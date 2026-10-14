// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "updateclient.h"
#include "identity.h"
#include "updateworker.h"
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

// Self-owned until finished, with no qApp parent. Closing the parent process
// releases its OS handles; it must never invoke QProcess::~QProcess on a live,
// committed installer. While EWAF remains open, finished reaps every Qt resource.
static void reapProcess(std::unique_ptr<QProcess> process, bool aborting, const QString &cleanupDirectory) {
    if (!process) return;
    auto *child = process.release();
    child->disconnect();
    child->setParent(nullptr);
    QObject::connect(child, &QProcess::finished, child, [child, cleanupDirectory] {
        if (!cleanupDirectory.isEmpty()) QDir(cleanupDirectory).removeRecursively();
        child->deleteLater();
    });
    if (child->state() == QProcess::NotRunning) {
        if (!cleanupDirectory.isEmpty()) QDir(cleanupDirectory).removeRecursively();
        child->deleteLater();
        return;
    }
    if (aborting) {
        child->write("ABORT\n");
        child->closeWriteChannel();
        QTimer::singleShot(2000, child, [child] {
            if (child->state() != QProcess::NotRunning) child->kill();
        });
    } else {
        child->closeWriteChannel();
    }
}

PreparedInstall::PreparedInstall(std::unique_ptr<QProcess> process, QString cleanupDirectory)
    : m_process(std::move(process)), m_cleanupDirectory(std::move(cleanupDirectory)) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(tr("Installer handshake timed out.")); });
    connect(m_process.get(), &QProcess::readyReadStandardOutput, this, &PreparedInstall::readProtocol);
    connect(m_process.get(), &QProcess::finished, this, [this] {
        readProtocol();
        if (!m_committed) fail(tr("Installer exited before handoff: ") +
            (m_process ? QString::fromUtf8(m_process->readAllStandardError()) : QString()));
    });
    connect(m_process.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) fail(tr("Could not start Windows Installer verification."));
    });
    m_timeout.start(120000);
    // The caller connects before the queued probe; it also covers already-buffered READY.
    QTimer::singleShot(0, this, [this] {
        readProtocol();
        // start() may synchronously fail, or the helper may exit, before this
        // wrapper connected its signals. Do not strand that case until timeout.
        if (m_process && m_process->state() == QProcess::NotRunning)
            fail(tr("Installer exited before readiness: ") + m_process->errorString());
    });
}
PreparedInstall::~PreparedInstall() { abort(); }
void PreparedInstall::readProtocol() {
    while (m_process && m_process->canReadLine()) {
        const auto line = m_process->readLine().trimmed();
        if (!m_ready && line == "READY") {
            m_ready = true;
            m_timeout.stop();
            emit ready();
        } else if (m_commitSent && line == "COMMITTED") {
            m_committed = true;
            m_timeout.stop();
            reapProcess(std::move(m_process), false, m_cleanupDirectory);
            emit committed();
            return;
        } else {
            fail(tr("Unexpected installer handshake response."));
            return;
        }
    }
}
void PreparedInstall::commit() {
    if (!m_ready || m_commitSent || !m_process || m_process->state() != QProcess::Running) {
        fail(tr("Installer is not ready for handoff."));
        return;
    }
    m_commitSent = true;
    if (m_process->write("COMMIT\n") != 7) { fail(tr("Could not commit installation.")); return; }
    m_timeout.start(10000);
}
void PreparedInstall::abort() {
    m_timeout.stop();
    if (!m_committed) reapProcess(std::move(m_process), true, m_cleanupDirectory);
}
void PreparedInstall::fail(const QString &message) {
    if (m_failed || m_committed) return;
    m_failed = true;
    abort();
    emit failed(message);
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

    if (!process.waitForFinished(UpdateWorker::deadlineFor(args.value(0)))) {
        process.kill();
        process.waitForFinished(1000);
        throw std::runtime_error("Update helper timed out.");
    }

    QByteArray stdoutData = process.readAllStandardOutput();
    QByteArray stderrData = process.readAllStandardError();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(stdoutData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        throw std::runtime_error(stderrData.isEmpty() ? "Invalid response from update helper." : stderrData.toStdString());
    }

    QJsonObject root = doc.object();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
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
    if (!QDir().mkpath(directory) || QFileInfo(directory).isSymLink())
        throw std::runtime_error("Unsafe Windows Installer staging directory.");

    QString helperPath = directory + QStringLiteral("/ewaf-installer.exe");
    QString sourceHelper = QCoreApplication::applicationDirPath() + QStringLiteral("/updater-installer/ewaf-installer.exe");
    if (!QFile::copy(sourceHelper, helperPath))
        throw std::runtime_error("Could not stage Windows Installer verifier.");

    auto process = std::make_unique<QProcess>();
    process->setWorkingDirectory(directory);

    qint64 pid = QCoreApplication::applicationPid();
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    HANDLE hProc = GetCurrentProcess();
    if (!GetProcessTimes(hProc, &ftCreation, &ftExit, &ftKernel, &ftUser))
        throw std::runtime_error("Could not identify the EWAF process.");
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
    return std::make_unique<PreparedInstall>(std::move(process), pkgInfo.absolutePath());
#endif
}

} // namespace EWAF
