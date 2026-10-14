// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_UPDATEWORKER_H
#define EWAF_UPDATEWORKER_H
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QJsonObject>

namespace EWAF {
// QProcess and both timers stay on their owning event-loop thread.
class UpdateWorker : public QObject {
    Q_OBJECT
public:
    explicit UpdateWorker(QObject *parent = nullptr);
    ~UpdateWorker() override;
    static int deadlineFor(const QString &command);
    void start(const QString &program, const QStringList &args, int deadlineMs);
    void cancel();
signals:
    void completed(const QJsonObject &result, const QString &error, bool cancelled);
private:
    void stop(const QString &reason);
    void finish(int exitCode, QProcess::ExitStatus status);
    QProcess m_process;
    QTimer m_deadline;
    QTimer m_killTimer;
    QString m_failure;
    bool m_cancelled = false;
    bool m_done = false;
};
}
#endif
