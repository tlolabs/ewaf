// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "updatecoordinator.h"
#include "app.h"
#include "identity.h"
#include "mainwindow.h"
#include "preferences.h"
#include "updateclient.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QThread>
#include <QUrl>
#include <atomic>
#include <mutex>

namespace EWAF {

class UpdateWorker : public QThread {
    Q_OBJECT
public:
    explicit UpdateWorker(QStringList args, QObject *parent = nullptr)
        : QThread(parent), m_args(std::move(args)) {}

    ~UpdateWorker() override {
        cancel();
        wait();
    }

    void cancel() {
        m_cancelled.store(true);
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_process) {
            m_process->terminate();
        }
    }

signals:
    void finished(const QJsonObject &result);
    void error(const QString &message);

protected:
    void run() override {
        if (m_cancelled.load()) return;

        QString appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WIN)
        QString helperPath = appDir + QStringLiteral("/ewaf-update.exe");
#else
        QString helperPath = appDir + QStringLiteral("/ewaf-update");
#endif

        QProcess process;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_cancelled.load()) return;
            m_process = &process;
        }

        process.start(helperPath, m_args);
        if (!process.waitForStarted(10000)) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_process = nullptr;
            }
            if (!m_cancelled.load()) {
                emit error(QStringLiteral("Could not start update helper."));
            }
            return;
        }

        const int timeoutMs = 180000;
        const int sliceMs = 100;
        int elapsed = 0;
        while (!process.waitForFinished(sliceMs)) {
            if (m_cancelled.load()) {
                process.terminate();
                if (!process.waitForFinished(2000)) {
                    process.kill();
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_process = nullptr;
                }
                return;
            }
            elapsed += sliceMs;
            if (elapsed >= timeoutMs) {
                process.terminate();
                if (!process.waitForFinished(2000)) {
                    process.kill();
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_process = nullptr;
                }
                if (!m_cancelled.load()) {
                    emit error(QStringLiteral("Update helper timed out."));
                }
                return;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_process = nullptr;
        }

        if (m_cancelled.load()) return;

        QByteArray stdoutData = process.readAllStandardOutput();
        QByteArray stderrData = process.readAllStandardError();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(stdoutData, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit error(stderrData.isEmpty() ? QStringLiteral("Invalid response from update helper.") : QString::fromUtf8(stderrData));
            return;
        }

        QJsonObject root = doc.object();
        if (process.exitCode() != 0) {
            QString errStr = root.value(QStringLiteral("error")).toString();
            emit error(errStr.isEmpty() ? QString::fromUtf8(stderrData) : errStr);
            return;
        }

        emit finished(root);
    }

private:
    QStringList m_args;
    std::atomic<bool> m_cancelled{false};
    QProcess *m_process = nullptr;
    std::mutex m_mutex;
};

UpdateCoordinator *UpdateCoordinator::s_instance = nullptr;

UpdateCoordinator *UpdateCoordinator::instance() {
    if (!s_instance) {
        s_instance = new UpdateCoordinator(qApp);
    }
    return s_instance;
}

UpdateCoordinator::UpdateCoordinator(QObject *parent)
    : QObject(parent) {}

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

    if (!manual) {
        try {
            QJsonObject dueRes = UpdateClient::due();
            if (!dueRes.value(QStringLiteral("due")).toBool()) {
                return;
            }
        } catch (...) {
            return;
        }
    }

    m_targetWindow = window;
    m_manual = manual;
    m_state = State::Checking;
    UpdateClient::setRunning(true);

    qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    Preferences::write(QStringLiteral("updateLastAttempt"), QString::number(now));

    if (m_worker) {
        m_worker->cancel();
        m_worker->deleteLater();
    }

    m_worker = new UpdateWorker({QStringLiteral("check"), UpdateClient::osVersion()}, this);
    connect(m_worker, &UpdateWorker::finished, this, &UpdateCoordinator::onCheckFinished);
    connect(m_worker, &UpdateWorker::error, this, &UpdateCoordinator::onWorkerError);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    m_worker->start();
}

void UpdateCoordinator::cancel() {
    if (m_worker) {
        m_worker->cancel();
        m_worker = nullptr;
    }
    cleanupStaging();
    reset();
}

void UpdateCoordinator::cleanupStaging() {
    if (!m_stagedDir.isEmpty()) {
        QDir(m_stagedDir).removeRecursively();
        m_stagedDir.clear();
    }
}

void UpdateCoordinator::reset() {
    m_state = State::Idle;
    UpdateClient::setRunning(false);
    m_targetWindow.clear();
    m_worker = nullptr;
}

void UpdateCoordinator::onCheckFinished(const QJsonObject &update) {
    if (!m_targetWindow || m_targetWindow->model()->isClosed() || App::hasActiveWork()) {
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
                                    QStringLiteral("Download and Update")) && !App::hasActiveWork() && !m_targetWindow->model()->isClosed()) {
            m_state = State::Downloading;
            m_worker = new UpdateWorker({QStringLiteral("install-appimage"), UpdateClient::osVersion(), m_version}, this);
            connect(m_worker, &UpdateWorker::finished, this, [this](const QJsonObject &installed) {
                if (m_targetWindow && !m_targetWindow->model()->isClosed()) {
                    m_targetWindow->message(QStringLiteral("EWAF updated"),
                                            QStringLiteral("Reopen EWAF when ready. Previous version: ") + installed.value(QStringLiteral("backup")).toString());
                }
                reset();
            });
            connect(m_worker, &UpdateWorker::error, this, &UpdateCoordinator::onWorkerError);
            connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
            m_worker->start();
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

    m_state = State::Downloading;
    m_worker = new UpdateWorker({QStringLiteral("download"), UpdateClient::osVersion(), m_version}, this);
    connect(m_worker, &UpdateWorker::finished, this, &UpdateCoordinator::onDownloadFinished);
    connect(m_worker, &UpdateWorker::error, this, &UpdateCoordinator::onWorkerError);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    m_worker->start();
#else
    reset();
#endif
}

void UpdateCoordinator::onDownloadFinished(const QJsonObject &download) {
    if (!m_targetWindow || m_targetWindow->model()->isClosed() || App::hasActiveWork()) {
        reset();
        return;
    }

    QString packagePath = download.value(QStringLiteral("path")).toString();
    m_stagedDir = QFileInfo(packagePath).absolutePath();

#if defined(Q_OS_WIN)
    bool handedOff = false;
    try {
        if (m_targetWindow->confirm(QStringLiteral("Install EWAF update?"),
                                    QStringLiteral("EWAF will close normally. Windows Installer will verify the publisher and install the update. Your settings and created folders are preserved. Reopen EWAF when installation finishes."),
                                    QStringLiteral("Close and Install")) && !App::hasActiveWork()) {
            auto installer = UpdateClient::prepareInstall(download);
            if (!m_targetWindow || m_targetWindow->model()->isClosed() || App::hasActiveWork()) {
                cleanupStaging();
                reset();
                return;
            }
            installer->commit();
            handedOff = true;
            App::closeForUpdate();
        }
    } catch (const std::exception &e) {
        if (m_manual && m_targetWindow && !m_targetWindow->model()->isClosed()) {
            m_targetWindow->message(QStringLiteral("Update unavailable"), QString::fromUtf8(e.what()));
        }
    }

    if (!handedOff) {
        cleanupStaging();
    }
#endif
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

#include "updatecoordinator.moc"
