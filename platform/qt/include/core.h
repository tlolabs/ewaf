// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_CORE_H
#define EWAF_CORE_H

#include <QJsonObject>
#include <QString>
#include <QThread>
#include <atomic>
#include <exception>

namespace EWAF {

struct PlanInput {
    QString start;
    QString end;
    quint32 weekday = 0;

    QJsonObject toJson() const {
        return QJsonObject{
            {QStringLiteral("start"), start},
            {QStringLiteral("end"), end},
            {QStringLiteral("weekday"), static_cast<qint64>(weekday)}
        };
    }
};

class CoreException : public std::exception {
public:
    CoreException(QString code, QString message)
        : m_code(std::move(code)), m_message(std::move(message)), m_what(m_message.toStdString()) {}

    const char *what() const noexcept override {
        return m_what.c_str();
    }

    const QString &code() const { return m_code; }
    const QString &message() const { return m_message; }

private:
    QString m_code;
    QString m_message;
    std::string m_what;
};

class Core {
public:
    static QJsonObject call(const QJsonObject &request);
    static QString summary(const QJsonObject &result);
};

class CreationWorker : public QThread {
    Q_OBJECT
public:
    CreationWorker(PlanInput plan, QString destination, QObject *parent = nullptr);
    ~CreationWorker() override;

    void cancel();
    bool isCancelled() const { return m_cancelled.load(); }

signals:
    void progress(const QJsonObject &stepResult);
    void finished(const QJsonObject &finalResult);
    void error(const QString &message, const QString &code);

protected:
    void run() override;

private:
    PlanInput m_plan;
    QString m_destination;
    std::atomic<bool> m_cancelled{false};
    std::atomic<quint64> m_handle{0};
};

} // namespace EWAF

#endif // EWAF_CORE_H
