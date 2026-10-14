// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_UPDATECOORDINATOR_H
#define EWAF_UPDATECOORDINATOR_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QJsonObject>

namespace EWAF {

class MainWindow;
class UpdateWorker;

class UpdateCoordinator : public QObject {
    Q_OBJECT
public:
    enum class State { Idle, Checking, Downloading, ReadyToInstall, Error };

    static UpdateCoordinator *instance();
    static void check(MainWindow *window, bool manual);

    State state() const { return m_state; }

public slots:
    void cancel();

private slots:
    void onCheckFinished(const QJsonObject &result);
    void onDownloadFinished(const QJsonObject &result);
    void onWorkerError(const QString &message);

private:
    explicit UpdateCoordinator(QObject *parent = nullptr);
    void startCheck(MainWindow *window, bool manual);
    void cleanupStaging();
    void reset();

    static UpdateCoordinator *s_instance;
    State m_state{State::Idle};
    QPointer<MainWindow> m_targetWindow;
    bool m_manual = false;
    QString m_version;
    QString m_notes;
    QString m_stagedDir;
    UpdateWorker *m_worker = nullptr;
};

} // namespace EWAF

#endif // EWAF_UPDATECOORDINATOR_H
