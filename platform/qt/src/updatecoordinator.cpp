// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "updatecoordinator.h"
#include "app.h"
#include "identity.h"
#include "mainwindow.h"
#include "preferences.h"
#include "updateclient.h"
#include "updateworker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QUrl>

namespace EWAF {

UpdateCoordinator *UpdateCoordinator::s_instance = nullptr;

UpdateCoordinator *UpdateCoordinator::instance() {
    if (!s_instance) {
        s_instance = new UpdateCoordinator(qApp);
    }
    return s_instance;
}

UpdateCoordinator::UpdateCoordinator(QObject *parent)
    : QObject(parent) {
    connect(qApp, &QCoreApplication::aboutToQuit, this, &UpdateCoordinator::cancel);
}

UpdateCoordinator::~UpdateCoordinator() {
    // Reap the writer before the staging directory's RAII cleanup runs.
    if (m_worker) delete m_worker.data();
    if (m_installer) m_installer->abort();
}

void UpdateCoordinator::check(MainWindow *window, bool manual) {
    instance()->startCheck(window, manual);
}

void UpdateCoordinator::startCheck(MainWindow *window, bool manual) {
    if (!window || UpdateClient::isRunning() || App::hasActiveWork() || window->model()->isWorking() || window->model()->isClosed()) {
        return;
    }

    if (Identity::isInternal()) {
        if (manual) {
            window->message(QStringLiteral("Internal reference build"),
                            QStringLiteral("Production updates are disabled for this internal Qt build."));
        }
        return;
    }

    m_targetWindow = window;
    m_manual = manual;
    m_state = State::Checking;
    UpdateClient::setRunning(true);
    if (!manual) {
        startWorker({QStringLiteral("due"),
                     Preferences::read(QStringLiteral("updateLastSuccess"), QStringLiteral("0")),
                     Preferences::read(QStringLiteral("updateLastAttempt"), QStringLiteral("0")),
                     Preferences::read(QStringLiteral("automaticUpdates"), QStringLiteral("1"))}, true);
    } else {
        Preferences::write(QStringLiteral("updateLastAttempt"), QString::number(QDateTime::currentSecsSinceEpoch()));
        startWorker({QStringLiteral("check"), UpdateClient::osVersion()});
    }
}

void UpdateCoordinator::startWorker(const QStringList &args, bool due) {
    auto *worker = new UpdateWorker(this);
    m_worker = worker;
    connect(worker, &UpdateWorker::completed, this,
            [this, worker, args, due](const QJsonObject &result, const QString &error, bool cancelled) {
        worker->deleteLater();
        if (m_worker != worker) return;
        m_worker.clear();
        if (cancelled || !m_targetWindow || m_targetWindow->model()->isClosed()) {
            cleanupStaging();
            reset();
        } else if (!error.isEmpty()) {
            onWorkerError(error);
        } else if (due) {
            if (!result.value(QStringLiteral("due")).toBool()) { reset(); return; }
            Preferences::write(QStringLiteral("updateLastAttempt"), QString::number(QDateTime::currentSecsSinceEpoch()));
            startWorker({QStringLiteral("check"), UpdateClient::osVersion()});
        } else if (args.first() == QStringLiteral("check")) {
            onCheckFinished(result);
        } else if (args.first() == QStringLiteral("download")) {
            onDownloadFinished(result);
        } else {
            if (m_targetWindow && !m_targetWindow->model()->isClosed())
                m_targetWindow->message(QStringLiteral("EWAF updated"),
                    QStringLiteral("Reopen EWAF when ready. Previous version: ") + result.value(QStringLiteral("backup")).toString());
            reset();
        }
    });
#if defined(Q_OS_WIN)
    const QString helper = QCoreApplication::applicationDirPath() + QStringLiteral("/ewaf-update.exe");
#else
    const QString helper = QCoreApplication::applicationDirPath() + QStringLiteral("/ewaf-update");
#endif
    worker->start(helper, args, UpdateWorker::deadlineFor(args.first()));
}

void UpdateCoordinator::cancelFor(MainWindow *window) {
    if (m_targetWindow == window) cancel();
}

void UpdateCoordinator::cancel() {
    if (m_installer) {
        m_installer->abort();
        m_installer->deleteLater();
        m_installer.clear();
    }
    if (m_worker) {
        // Keep operation ownership until the child is reaped. Never remove a
        // directory while the downloader may still be writing into it.
        m_worker->cancel();
        return;
    }
    cleanupStaging();
    reset();
}

void UpdateCoordinator::cleanupStaging() {
    if (!m_stagedDir.isEmpty()) {
        QDir(m_stagedDir).removeRecursively();
        m_stagedDir.clear();
    }
    m_downloadDirectory.reset();
}

void UpdateCoordinator::reset() {
    m_state = State::Idle;
    UpdateClient::setRunning(false);
    m_targetWindow.clear();
    m_worker = nullptr;
}

void UpdateCoordinator::onCheckFinished(const QJsonObject &update) {
    if (!m_targetWindow || m_targetWindow->model()->isClosed() || App::hasActiveWork()) {
        cleanupStaging();
        reset();
        return;
    }

    Preferences::write(QStringLiteral("updateLastSuccess"),
                       QString::number(update.value(QStringLiteral("checked_at")).toInteger()));

    if (!update.value(QStringLiteral("available")).toBool()) {
        if (m_manual && m_targetWindow) {
            m_targetWindow->message(QStringLiteral("EWAF Updates"),
                                    QStringLiteral("No compatible newer stable release is available."));
        }
        reset();
        return;
    }

    m_version = update.value(QStringLiteral("version")).toString();
    m_notes = QStringLiteral("\nRelease notes: ") + update.value(QStringLiteral("notes")).toString();

#if defined(Q_OS_LINUX)
    QString appImage = qEnvironmentVariable("APPIMAGE");
    if (appImage.isEmpty()) {
        if (m_targetWindow->confirm(QStringLiteral("EWAF ") + m_version + QStringLiteral(" is available"),
                                    QStringLiteral("This installation is managed by your package manager. Install the authenticated AppImage from GitHub to enable automatic replacement.") + m_notes,
                                    QStringLiteral("Open Releases"))) {
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/tlolabs/ewaf/releases")));
        }
        reset();
        return;
    } else {
        if (m_targetWindow->confirm(QStringLiteral("EWAF ") + m_version + QStringLiteral(" is available"),
                                    QStringLiteral("Download, authenticate and replace this AppImage? A previous copy will be retained. Reopen EWAF when ready; this does not close your windows.") + m_notes,
                                    QStringLiteral("Download and Update")) && !App::hasActiveWork() && m_targetWindow && !m_targetWindow->model()->isClosed()) {
            m_state = State::Downloading;
            startWorker({QStringLiteral("install-appimage"), UpdateClient::osVersion(), m_version});
            return;
        }
        reset();
        return;
    }
#endif

#if defined(Q_OS_WIN)
    if (!m_targetWindow->confirm(QStringLiteral("EWAF ") + m_version + QStringLiteral(" is available"),
                                 QStringLiteral("Download and verify this update? Installation requires closing all EWAF windows.") + m_notes,
                                 QStringLiteral("Download"))) {
        reset();
        return;
    }

    if (!m_targetWindow || m_targetWindow->model()->isClosed() || App::hasActiveWork()) { reset(); return; }
    m_downloadDirectory = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/ewaf-update-XXXXXX"));
    if (!m_downloadDirectory->isValid()) { onWorkerError(QStringLiteral("Could not create update staging directory.")); return; }
    m_stagedDir = m_downloadDirectory->path();
    m_state = State::Downloading;
    startWorker({QStringLiteral("download"), UpdateClient::osVersion(), m_version, QFileInfo(m_stagedDir).canonicalFilePath()});
#else
    reset();
#endif
}

void UpdateCoordinator::onDownloadFinished(const QJsonObject &download) {
    if (!m_targetWindow || m_targetWindow->model()->isClosed() || App::hasActiveWork()) {
        cleanupStaging();
        reset();
        return;
    }

    if (m_stagedDir.isEmpty()) {
        onWorkerError(QStringLiteral("Update helper returned an invalid staging directory."));
        return;
    }

#if defined(Q_OS_WIN)
    try {
        const bool consent = m_targetWindow->confirm(QStringLiteral("Install EWAF update?"),
            QStringLiteral("EWAF will close normally. Windows Installer will verify the publisher and install the update. Your settings and created folders are preserved. Reopen EWAF when installation finishes."),
            QStringLiteral("Close and Install"));
        if (!consent || !m_targetWindow || m_targetWindow->model()->isClosed() || App::hasActiveWork()) {
            cleanupStaging(); reset(); return;
        }
        m_state = State::ReadyToInstall;
        m_installer = UpdateClient::prepareInstall(download).release();
        m_installer->setParent(this);
        auto *installer = m_installer.data();
        connect(installer, &PreparedInstall::ready, this, [this, installer] {
            if (m_installer != installer) return;
            if (!m_targetWindow || m_targetWindow->model()->isClosed() || App::hasActiveWork()) { cancel(); return; }
            installer->commit();
        });
        connect(installer, &PreparedInstall::committed, this, [this, installer] {
            if (m_installer != installer) return;
            m_installer.clear();
            installer->deleteLater();
            if (m_downloadDirectory) m_downloadDirectory->setAutoRemove(false);
            m_downloadDirectory.reset();
            m_stagedDir.clear(); // Must transfer ownership BEFORE closing windows.
            App::closeForUpdate();
            reset();
        });
        connect(installer, &PreparedInstall::failed, this, [this, installer](const QString &error) {
            if (m_installer != installer) return;
            m_installer.clear();
            installer->deleteLater();
            onWorkerError(error);
        });
        return;
    } catch (const std::exception &e) {
        onWorkerError(QString::fromUtf8(e.what()));
        return;
    }
#endif
    cleanupStaging();
    reset();
}

void UpdateCoordinator::onWorkerError(const QString &message) {
    if (m_manual && m_targetWindow && !m_targetWindow->model()->isClosed()) {
        m_targetWindow->message(QStringLiteral("Update unavailable"), message);
    }
    cleanupStaging();
    reset();
}

} // namespace EWAF
