// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "updatecoordinator.h"
#include "app.h"
#include "identity.h"
#include "mainwindow.h"
#include "preferences.h"
#include "updateclient.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace EWAF {

void UpdateCoordinator::check(MainWindow *window, bool manual) {
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

    UpdateClient::setRunning(true);
    QString stagedDir;
    bool handedOff = false;

    try {
        if (!manual) {
            QJsonObject dueRes = UpdateClient::due();
            if (!dueRes.value(QStringLiteral("due")).toBool()) {
                UpdateClient::setRunning(false);
                return;
            }
        }

        qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        Preferences::write(QStringLiteral("updateLastAttempt"), QString::number(now));

        QJsonObject update = UpdateClient::check();
        Preferences::write(QStringLiteral("updateLastSuccess"),
                           QString::number(update.value(QStringLiteral("checked_at")).toInteger()));

        if (window->model()->isClosed() || App::hasActiveWork()) {
            UpdateClient::setRunning(false);
            return;
        }

        if (!update.value(QStringLiteral("available")).toBool()) {
            if (manual) {
                window->message(QStringLiteral("EWAF Updates"),
                                QStringLiteral("No compatible newer stable release is available."));
            }
            UpdateClient::setRunning(false);
            return;
        }

        QString version = update.value(QStringLiteral("version")).toString();
        QString notes = QStringLiteral("\nRelease notes: ") + update.value(QStringLiteral("notes")).toString();

#if defined(Q_OS_LINUX)
        QString appImage = qEnvironmentVariable("APPIMAGE");
        if (appImage.isEmpty()) {
            if (window->confirm(QStringLiteral("EWAF ") + version + QStringLiteral(" is available"),
                                QStringLiteral("This installation is managed by your package manager. Install the authenticated AppImage from GitHub to enable automatic replacement.") + notes,
                                QStringLiteral("Open Releases"))) {
                QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/tlolabs/ewaf/releases")));
            }
        } else {
            if (window->confirm(QStringLiteral("EWAF ") + version + QStringLiteral(" is available"),
                                QStringLiteral("Download, authenticate and replace this AppImage? A previous copy will be retained. Reopen EWAF when ready; this does not close your windows.") + notes,
                                QStringLiteral("Download and Update")) && !App::hasActiveWork() && !window->model()->isClosed()) {
                QJsonObject installed = UpdateClient::run({QStringLiteral("install-appimage"), UpdateClient::osVersion(), version});
                window->message(QStringLiteral("EWAF updated"),
                                QStringLiteral("Reopen EWAF when ready. Previous version: ") + installed.value(QStringLiteral("backup")).toString());
            }
        }
        UpdateClient::setRunning(false);
        return;
#endif

#if defined(Q_OS_WIN)
        if (!window->confirm(QStringLiteral("EWAF ") + version + QStringLiteral(" is available"),
                             QStringLiteral("Download and verify this update? Installation requires closing all EWAF windows.") + notes,
                             QStringLiteral("Download"))) {
            UpdateClient::setRunning(false);
            return;
        }

        QJsonObject download = UpdateClient::download(version);
        QString packagePath = download.value(QStringLiteral("path")).toString();
        stagedDir = QFileInfo(packagePath).absolutePath();

        if (window->model()->isClosed() || App::hasActiveWork()) {
            UpdateClient::setRunning(false);
            if (!stagedDir.isEmpty()) QDir(stagedDir).removeRecursively();
            return;
        }

        if (window->confirm(QStringLiteral("Install EWAF update?"),
                             QStringLiteral("EWAF will close normally. Windows Installer will verify the publisher and install the update. Your settings and created folders are preserved. Reopen EWAF when installation finishes."),
                             QStringLiteral("Close and Install")) && !App::hasActiveWork()) {
            auto installer = UpdateClient::prepareInstall(download);
            if (window->model()->isClosed() || App::hasActiveWork()) {
                UpdateClient::setRunning(false);
                if (!stagedDir.isEmpty()) QDir(stagedDir).removeRecursively();
                return;
            }
            installer->commit();
            handedOff = true;
            App::closeForUpdate();
        }
#endif
    } catch (const std::exception &e) {
        if (manual && !window->model()->isClosed()) {
            window->message(QStringLiteral("Update unavailable"), QString::fromUtf8(e.what()));
        }
    }

    UpdateClient::setRunning(false);
    if (!handedOff && !stagedDir.isEmpty()) {
        QDir(stagedDir).removeRecursively();
    }
}

} // namespace EWAF
