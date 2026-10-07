// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "updateclient.h"
#include "identity.h"
#include "preferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSysInfo>
#include <QDateTime>

#if defined(Q_OS_LINUX)
#include <gnu/libc-version.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace EWAF {

bool UpdateClient::s_running = false;

PreparedInstall::PreparedInstall(std::unique_ptr<QProcess> process)
    : m_process(std::move(process)) {}

PreparedInstall::~PreparedInstall() {
    abort();
}

void PreparedInstall::commit() {
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write("COMMIT\n");
        m_process->waitForBytesWritten();
        m_committed = true;
    }
}

void PreparedInstall::abort() {
    if (!m_committed && m_process && m_process->state() == QProcess::Running) {
        m_process->write("ABORT\n");
        m_process->waitForBytesWritten();
        m_process->closeWriteChannel();
        m_process->waitForFinished(5000);
    }
}

bool UpdateClient::isRunning() {
    return s_running;
}

void UpdateClient::setRunning(bool running) {
    s_running = running;
}

QJsonObject UpdateClient::run(const QStringList &args) {
    if (Identity::isInternal()) {
        throw std::runtime_error("Internal reference builds cannot use production updates.");
    }

    QString appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WIN)
    QString helperPath = appDir + QStringLiteral("/ewaf-update.exe");
#else
    QString helperPath = appDir + QStringLiteral("/ewaf-update");
#endif

    QProcess process;
    process.start(helperPath, args);
    if (!process.waitForStarted(5000)) {
        throw std::runtime_error("Could not start the update helper.");
    }

    process.waitForFinished(60000);

    QByteArray stdoutData = process.readAllStandardOutput();
    QByteArray stderrData = process.readAllStandardError();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(stdoutData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        throw std::runtime_error(stderrData.isEmpty() ? "Invalid response from update helper." : stderrData.toStdString());
    }

    QJsonObject root = doc.object();
    if (process.exitCode() != 0) {
        QString errStr = root.value(QStringLiteral("error")).toString();
        throw std::runtime_error(errStr.isEmpty() ? stderrData.toStdString() : errStr.toStdString());
    }

    return root;
}

QString UpdateClient::osVersion() {
#if defined(Q_OS_LINUX)
    return QString::fromLatin1(gnu_get_libc_version());
#elif defined(Q_OS_WIN)
    return QSysInfo::kernelVersion();
#else
    return QSysInfo::productVersion();
#endif
}

QJsonObject UpdateClient::due() {
    return run({
        QStringLiteral("due"),
        Preferences::read(QStringLiteral("updateLastSuccess"), QStringLiteral("0")),
        Preferences::read(QStringLiteral("updateLastAttempt"), QStringLiteral("0")),
        Preferences::read(QStringLiteral("automaticUpdates"), QStringLiteral("1"))
    });
}

QJsonObject UpdateClient::check() {
    return run({QStringLiteral("check"), osVersion()});
}

QJsonObject UpdateClient::download(const QString &version) {
    return run({QStringLiteral("download"), osVersion(), version});
}

std::unique_ptr<PreparedInstall> UpdateClient::prepareInstall(const QJsonObject &download) {
#if !defined(Q_OS_WIN)
    Q_UNUSED(download);
    throw std::runtime_error("MSI installation is only supported on Windows.");
#else
    QString package = download.value(QStringLiteral("path")).toString();
    QFileInfo pkgInfo(package);
    QString directory = pkgInfo.absolutePath() + QStringLiteral("/installer");
    QDir().mkpath(directory);

    QString helperPath = directory + QStringLiteral("/ewaf-installer.exe");
    QString sourceHelper = QCoreApplication::applicationDirPath() + QStringLiteral("/updater-installer/ewaf-installer.exe");
    QFile::copy(sourceHelper, helperPath);

    auto process = std::make_unique<QProcess>();
    process->setWorkingDirectory(directory);

    qint64 pid = QCoreApplication::applicationPid();
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    HANDLE hProc = GetCurrentProcess();
    GetProcessTimes(hProc, &ftCreation, &ftExit, &ftKernel, &ftUser);
    ULARGE_INTEGER uli;
    uli.LowPart = ftCreation.dwLowDateTime;
    uli.HighPart = ftCreation.dwHighDateTime;
    // Convert 100-ns intervals from 1601 to .NET ticks (100-ns intervals from 0001)
    // 504911232000000000 ticks difference
    qint64 parentStarted = static_cast<qint64>(uli.QuadPart) + 504911232000000000LL;

    QStringList args{
        QStringLiteral("--install"),
        package,
        download.value(QStringLiteral("sha256")).toString(),
        download.value(QStringLiteral("signer")).toString(),
        download.value(QStringLiteral("version")).toString(),
        download.value(QStringLiteral("architecture")).toString(),
        QString::number(pid),
        QString::number(parentStarted)
    };

    process->start(helperPath, args);
    if (!process->waitForStarted(5000)) {
        throw std::runtime_error("Could not start Windows Installer verification.");
    }

    if (!process->waitForReadyRead(120000)) {
        throw std::runtime_error("Timeout waiting for Windows Installer verifier readiness.");
    }

    QByteArray line = process->readLine().trimmed();
    if (line != "READY") {
        QByteArray errors = process->readAllStandardError();
        throw std::runtime_error(std::string("Update verification failed: ") + errors.toStdString());
    }

    return std::make_unique<PreparedInstall>(std::move(process));
#endif
}

} // namespace EWAF
