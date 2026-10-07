// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_UPDATECLIENT_H
#define EWAF_UPDATECLIENT_H

#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <memory>

namespace EWAF {

class PreparedInstall {
public:
    explicit PreparedInstall(std::unique_ptr<QProcess> process);
    ~PreparedInstall();

    void commit();
    void abort();

private:
    std::unique_ptr<QProcess> m_process;
    bool m_committed = false;
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
