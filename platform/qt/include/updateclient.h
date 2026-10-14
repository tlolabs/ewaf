// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_UPDATECLIENT_H
#define EWAF_UPDATECLIENT_H

#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <memory>

namespace EWAF {

// Own verification until READY, then transfer a committed child to an event-loop
// reaper. The reaper has no application parent, so normal EWAF exit cannot kill it.
class PreparedInstall : public QObject {
    Q_OBJECT
public:
    explicit PreparedInstall(std::unique_ptr<QProcess> process, QString cleanupDirectory = {});
    ~PreparedInstall() override;
    void commit();
    void abort();
signals:
    void ready();
    void committed();
    void failed(const QString &message);
private:
    void readProtocol();
    void fail(const QString &message);
    std::unique_ptr<QProcess> m_process;
    QTimer m_timeout;
    QString m_cleanupDirectory;
    bool m_ready = false;
    bool m_commitSent = false;
    bool m_committed = false;
    bool m_failed = false;
};

class UpdateClient {
public:
    static bool isRunning();
    static void setRunning(bool running);

    static QJsonObject run(const QStringList &args);
    static QString osVersion();
    static QJsonObject due();
    static QJsonObject check();
    static QJsonObject download(const QString &version);
    static std::unique_ptr<PreparedInstall> prepareInstall(const QJsonObject &download);

private:
    static bool s_running;
};

} // namespace EWAF

#endif // EWAF_UPDATECLIENT_H
