// SPDX-License-Identifier: GPL-3.0-or-later
#include "updateworker.h"
#include <QJsonDocument>

namespace EWAF {
UpdateWorker::UpdateWorker(QObject *parent) : QObject(parent) {
    m_deadline.setSingleShot(true);
    m_killTimer.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] { stop(tr("Update helper timed out.")); });
    connect(&m_killTimer, &QTimer::timeout, &m_process, &QProcess::kill);
    connect(&m_process, &QProcess::finished, this, &UpdateWorker::finish);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finish(-1, QProcess::CrashExit);
    });
}
UpdateWorker::~UpdateWorker() {
    m_process.disconnect(this);
    // Normal cancellation is asynchronous. Application teardown is bounded and
    // cannot wait for a network deadline or leave a running QThread behind.
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}
int UpdateWorker::deadlineFor(const QString &command) {
    // Download/install reauthenticate metadata (two 30s requests) before the
    // transport's 900s artifact allowance; leave headroom for verification/I/O.
    return command == QStringLiteral("download") || command == QStringLiteral("install-appimage")
        ? 1200000 : 180000;
}
void UpdateWorker::start(const QString &program, const QStringList &args, int deadlineMs) {
    m_deadline.start(deadlineMs);
    m_process.start(program, args);
}
void UpdateWorker::stop(const QString &reason) {
    if (m_done) return;
    m_failure = reason;
    m_deadline.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        m_killTimer.start(2000);
    }
}
void UpdateWorker::cancel() {
    m_cancelled = true;
    stop(tr("Update canceled."));
}
void UpdateWorker::finish(int exitCode, QProcess::ExitStatus status) {
    if (m_done) return;
    m_done = true;
    m_deadline.stop();
    m_killTimer.stop();
    // Parse only after exit: readyRead can split UTF-8, lines and JSON anywhere.
    const QByteArray output = m_process.readAllStandardOutput();
    const QString errors = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(output, &parseError);
    const auto result = document.object();
    QString failure = m_failure;
    if (failure.isEmpty() && (status != QProcess::NormalExit || exitCode != 0)) {
        failure = result.value(QStringLiteral("error")).toString();
        if (failure.isEmpty()) failure = errors.isEmpty() ? tr("Update helper exited unsuccessfully.") : errors;
    }
    if (failure.isEmpty() && (parseError.error != QJsonParseError::NoError || !document.isObject()))
        failure = errors.isEmpty() ? tr("Invalid response from update helper.") : errors;
    emit completed(result, failure, m_cancelled);
}
}
