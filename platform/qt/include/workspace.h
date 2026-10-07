// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_WORKSPACE_H
#define EWAF_WORKSPACE_H

#include "core.h"
#include "preferences.h"

#include <QFutureWatcher>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

namespace EWAF {

class IWorkspaceDialogs {
public:
    virtual ~IWorkspaceDialogs() = default;
    virtual QString chooseFolder() = 0;
    virtual bool confirm(const QString &title, const QString &message, const QString &accept = QStringLiteral("Continue")) = 0;
    virtual void message(const QString &title, const QString &message) = 0;
};

class Workspace : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(Workspace)
public:
    static const std::vector<quint32> Days;

    Workspace(IWorkspaceDialogs *dialogs, IPreferences *preferences = nullptr, QObject *parent = nullptr);
    ~Workspace() override;

    QStringList dayNames() const;

    const QString &start() const { return m_start; }
    void setStart(const QString &start);

    const QString &end() const { return m_end; }
    void setEnd(const QString &end);

    const QString &search() const { return m_search; }
    void setSearch(const QString &search);

    int weekday() const { return m_weekday; }
    void setWeekday(int weekday);

    const QString &status() const { return m_status; }
    void setStatus(const QString &status);

    QString destinationDisplay() const;
    const QString &destination() const { return m_destination; }
    bool hasDestination() const { return !m_destination.isEmpty(); }

    bool isBusy() const { return m_busy; }
    bool isDialogOpen() const { return m_dialogOpen; }
    bool isEditable() const { return !m_busy && !m_dialogOpen && !m_closed; }
    bool isWorking() const { return m_busy || m_dialogOpen; }
    bool isClosed() const { return m_closed; }

    int total() const { return m_total; }
    int processed() const { return m_processed; }
    int progressMaximum() const;
    QString countText() const;

    const QStringList &preview() const { return m_preview; }
    const QString &selectedName() const { return m_selectedName; }
    void setSelectedName(const QString &name) { m_selectedName = name; }

    bool canCreate() const;
    bool canChoose() const;
    bool canCancel() const;
    bool isRefreshing() const { return m_currentWatcher != nullptr; }

    void refresh();
    bool choose();
    void create();
    void cancel();
    void close();
    void runWithDialog(const std::function<void()> &fn);

signals:
    void stateChanged();
    void refreshCompleted();
    void operationCompleted();

private:
    void refreshAsync();
    void state();

    IWorkspaceDialogs *m_dialogs;
    IPreferences *m_preferences;
    QString m_start;
    QString m_end;
    QString m_search;
    QString m_status;
    QString m_destination;
    QString m_selectedName;
    int m_weekday = 3;
    int m_generation = 0;
    int m_total = 0;
    int m_processed = 0;
    bool m_busy = false;
    bool m_dialogOpen = false;
    bool m_closed = false;
    bool m_confirmation = false;
    bool m_planValid = false;
    PlanInput m_plan;
    QStringList m_preview;
    CreationWorker *m_worker = nullptr;
    QPointer<QFutureWatcher<QJsonObject>> m_currentWatcher;
};

} // namespace EWAF

#endif // EWAF_WORKSPACE_H
