// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "workspace.h"

#include <QDate>
#include <QJsonArray>
#include <QLocale>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace EWAF {

const std::vector<quint32> Workspace::Days = {2, 3, 4, 5, 6, 7, 1};

Workspace::Workspace(IWorkspaceDialogs *dialogs, IPreferences *preferences, QObject *parent)
    : QObject(parent), m_dialogs(dialogs),
      m_preferences(preferences ? preferences : &Preferences::store()),
      m_status(QStringLiteral("Choose a date range and weekday, then review your folders.")) {
    QString today = QDate::currentDate().toString(QStringLiteral("MM-dd-yyyy"));
    m_start = m_preferences->read(QStringLiteral("rangeStart"), today);
    m_end = m_preferences->read(QStringLiteral("rangeEnd"), today);
    if (m_start.isEmpty()) m_start = today;
    if (m_end.isEmpty()) m_end = today;

    bool ok = false;
    quint32 saved = m_preferences->read(QStringLiteral("defaultWeekday"), QStringLiteral("5")).toUInt(&ok);
    if (!ok) saved = 5;

    m_weekday = 3;
    for (size_t i = 0; i < Days.size(); ++i) {
        if (Days[i] == saved) {
            m_weekday = static_cast<int>(i);
            break;
        }
    }
}

Workspace::~Workspace() {
    disconnect();
    close();
}

QStringList Workspace::dayNames() const {
    QStringList names;
    QLocale loc = QLocale::system();
    // Days array: Monday=2, Tuesday=3, ..., Sunday=1.
    // In Qt: Monday=1, ..., Sunday=7.
    for (quint32 day : Days) {
        int qtDay = (day == 1) ? 7 : static_cast<int>(day - 1);
        names.append(loc.dayName(qtDay, QLocale::LongFormat));
    }
    return names;
}

void Workspace::setStart(const QString &start) {
    if (m_start != start) {
        m_start = start;
        refresh();
    }
}

void Workspace::setEnd(const QString &end) {
    if (m_end != end) {
        m_end = end;
        refresh();
    }
}

void Workspace::setSearch(const QString &search) {
    if (m_search != search) {
        m_search = search;
        refresh();
    }
}

void Workspace::setWeekday(int weekday) {
    if (m_weekday != weekday) {
        m_weekday = weekday;
        refresh();
    }
}

void Workspace::setStatus(const QString &status) {
    if (m_status != status) {
        m_status = status;
        state();
    }
}

QString Workspace::destinationDisplay() const {
    return m_destination.isEmpty()
               ? QStringLiteral("Choose an existing destination folder.")
               : m_destination;
}

int Workspace::progressMaximum() const {
    return std::max(1, m_total);
}

QString Workspace::countText() const {
    if (!m_planValid) {
        return QStringLiteral("Check Your Dates");
    }
    return QStringLiteral("%1 folders").arg(QLocale::system().toString(m_total));
}

bool Workspace::canCreate() const {
    return isEditable() && m_planValid && m_total > 0;
}

bool Workspace::canChoose() const {
    return isEditable();
}

bool Workspace::canCancel() const {
    return m_busy;
}

void Workspace::state() {
    emit stateChanged();
}

void Workspace::refresh() {
    refreshAsync();
}

void Workspace::refreshAsync() {
    if (!isEditable() || m_weekday < 0 || m_weekday >= static_cast<int>(Days.size())) {
        return;
    }

    if (m_currentWatcher) {
        m_currentWatcher->disconnect();
        m_currentWatcher->deleteLater();
        m_currentWatcher = nullptr;
    }

    int current = ++m_generation;
    m_planValid = false;
    m_preview.clear();
    state();

    QString first = m_start;
    QString last = m_end;
    quint32 day = Days[m_weekday];
    QString query = m_search;

    auto *watcher = new QFutureWatcher<QJsonObject>(this);
    m_currentWatcher = watcher;
    connect(watcher, &QFutureWatcher<QJsonObject>::finished, this, [this, watcher, current, day]() {
        if (m_currentWatcher == watcher) {
            m_currentWatcher = nullptr;
        }
        watcher->deleteLater();
        if (m_closed || current != m_generation) {
            return;
        }

        try {
            QJsonObject res = watcher->result();
            if (res.contains(QStringLiteral("error"))) {
                m_status = res.value(QStringLiteral("error")).toString();
                m_planValid = false;
                m_total = 0;
                m_preview.clear();
            } else {
                m_plan.start = res.value(QStringLiteral("start")).toString();
                m_plan.end = res.value(QStringLiteral("end")).toString();
                m_plan.weekday = day;
                m_planValid = true;

                m_total = res.value(QStringLiteral("count")).toInt();
                m_confirmation = res.value(QStringLiteral("requiresConfirmation")).toBool();

                QJsonArray datesArray = res.value(QStringLiteral("dates")).toArray();
                m_preview.clear();
                for (const auto &item : datesArray) {
                    m_preview.append(item.toString());
                }

                if (m_total == 0) {
                    m_status = QStringLiteral("No matching dates. Choose a wider range or another weekday.");
                } else if (m_preview.isEmpty()) {
                    m_status = QStringLiteral("No search matches. Creation still uses the full date range.");
                } else {
                    m_status = QStringLiteral("Review your folders, then choose Create Folders.");
                }
            }
        } catch (const std::exception &e) {
            m_status = QString::fromUtf8(e.what());
            m_planValid = false;
            m_total = 0;
            m_preview.clear();
        }

        state();
        emit refreshCompleted();
    });

    QFuture<QJsonObject> future = QtConcurrent::run([first, last, day, query]() -> QJsonObject {
        try {
            QJsonObject exactRes = Core::call({
                {QStringLiteral("op"), QStringLiteral("exact")},
                {QStringLiteral("start"), first},
                {QStringLiteral("end"), last}
            });

            PlanInput input{
                exactRes.value(QStringLiteral("start")).toString(),
                exactRes.value(QStringLiteral("end")).toString(),
                day
            };

            QJsonObject planRes = Core::call({
                {QStringLiteral("op"), QStringLiteral("plan")},
                {QStringLiteral("plan"), input.toJson()},
                {QStringLiteral("search"), query}
            });

            QJsonObject result = planRes;
            result.insert(QStringLiteral("start"), input.start);
            result.insert(QStringLiteral("end"), input.end);
            return result;
        } catch (const CoreException &e) {
            return QJsonObject{{QStringLiteral("error"), e.message()}};
        } catch (const std::exception &e) {
            return QJsonObject{{QStringLiteral("error"), QString::fromUtf8(e.what())}};
        }
    });

    watcher->setFuture(future);
}

bool Workspace::choose() {
    if (!isEditable() || !m_dialogs) {
        return false;
    }
    m_dialogOpen = true;
    state();

    QString folder;
    try {
        folder = m_dialogs->chooseFolder();
    } catch (const std::exception &e) {
        m_status = QStringLiteral("The folder could not be opened. ") + QString::fromUtf8(e.what());
    }

    m_dialogOpen = false;
    if (!folder.isEmpty() && !m_closed) {
        m_destination = folder;
        state();
        return true;
    }
    state();
    return false;
}

void Workspace::create() {
    if (!canCreate()) return;

    PlanInput snapshot = m_plan;
    int count = m_total;

    if (m_confirmation && m_dialogs) {
        m_dialogOpen = true;
        state();
        bool accept = m_dialogs->confirm(
            QStringLiteral("Create %1 folders?").arg(QLocale::system().toString(count)),
            QStringLiteral("Existing folders and their contents will be preserved. You can cancel while this runs.")
        );
        m_dialogOpen = false;
        state();
        if (!accept) return;
    }

    if (m_closed) return;
    if (m_destination.isEmpty() && !choose()) {
        return;
    }
    if (m_closed) return;

    m_busy = true;
    ++m_generation;
    m_processed = 0;
    state();

    m_worker = new CreationWorker(snapshot, m_destination, this);

    connect(m_worker, &CreationWorker::progress, this, [this, count](const QJsonObject &r) {
        if (!m_closed && m_busy) {
            m_processed = r.value(QStringLiteral("created")).toInt() + r.value(QStringLiteral("existing")).toInt();
            m_status = QStringLiteral("Processed %1 of %2 folders.")
                           .arg(QLocale::system().toString(m_processed))
                           .arg(QLocale::system().toString(count));
            state();
        }
    });

    connect(m_worker, &CreationWorker::finished, this, [this](const QJsonObject &result) {
        m_processed = result.value(QStringLiteral("created")).toInt() + result.value(QStringLiteral("existing")).toInt();
        m_status = Core::summary(result);
        QString title;
        if (result.value(QStringLiteral("failureReason")).isString()) {
            title = QStringLiteral("Folder Creation Stopped");
        } else if (result.value(QStringLiteral("cancelled")).toBool()) {
            title = QStringLiteral("Creation Canceled");
        } else {
            title = QStringLiteral("Complete");
        }

        m_busy = false;
        state();

        if (m_dialogs && !m_closed) {
            m_dialogOpen = true;
            state();
            m_dialogs->message(title, m_status);
            m_dialogOpen = false;
            state();
        }

        m_worker->deleteLater();
        m_worker = nullptr;
        emit operationCompleted();
    });

    connect(m_worker, &CreationWorker::error, this, [this](const QString &message, const QString &) {
        m_status = message;
        m_busy = false;
        state();

        if (m_dialogs && !m_closed) {
            m_dialogOpen = true;
            state();
            m_dialogs->message(QStringLiteral("Folder Creation Stopped"), m_status);
            m_dialogOpen = false;
            state();
        }

        m_worker->deleteLater();
        m_worker = nullptr;
        emit operationCompleted();
    });

    m_worker->start();
}

void Workspace::cancel() {
    if (m_worker && m_worker->isRunning()) {
        m_worker->cancel();
    }
}

void Workspace::close() {
    if (m_closed) {
        return;
    }
    m_closed = true;
    disconnect();
    ++m_generation;

    if (m_currentWatcher) {
        m_currentWatcher->disconnect();
        m_currentWatcher->deleteLater();
        m_currentWatcher = nullptr;
    }
    if (m_worker) {
        if (m_worker->isRunning()) {
            m_worker->cancel();
            m_worker->wait();
        }
        m_worker->deleteLater();
        m_worker = nullptr;
    }
    m_busy = false;
    m_dialogOpen = false;

    try {
        m_preferences->write(QStringLiteral("rangeStart"), m_start);
        m_preferences->write(QStringLiteral("rangeEnd"), m_end);
    } catch (...) {}
}

void Workspace::runWithDialog(const std::function<void()> &fn) {
    m_dialogOpen = true;
    state();
    try {
        fn();
    } catch (...) {
        m_dialogOpen = false;
        state();
        throw;
    }
    m_dialogOpen = false;
    state();
}

} // namespace EWAF
