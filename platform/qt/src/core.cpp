// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core.h"
#include "ewaf.h"

#include <QJsonDocument>
#include <QLocale>
#include <QDebug>

namespace EWAF {

QJsonObject Core::call(const QJsonObject &request) {
    QJsonDocument doc(request);
    QByteArray bytes = doc.toJson(QJsonDocument::Compact);
    char *response = ewaf_request(reinterpret_cast<const uint8_t *>(bytes.constData()), bytes.size());
    if (!response) {
        throw CoreException(QStringLiteral("no_response"), QStringLiteral("No core response."));
    }

    QByteArray respBytes(response);
    ewaf_string_free(response);

    QJsonParseError parseError;
    QJsonDocument respDoc = QJsonDocument::fromJson(respBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !respDoc.isObject()) {
        throw CoreException(QStringLiteral("invalid_json"), QStringLiteral("Invalid response from core."));
    }

    QJsonObject root = respDoc.object();
    if (!root.value(QStringLiteral("ok")).toBool()) {
        QJsonObject error = root.value(QStringLiteral("error")).toObject();
        throw CoreException(error.value(QStringLiteral("code")).toString(),
                            error.value(QStringLiteral("message")).toString());
    }

    return root.value(QStringLiteral("value")).toObject();
}

QString Core::summary(const QJsonObject &result) {
    int created = result.value(QStringLiteral("created")).toInt();
    int existing = result.value(QStringLiteral("existing")).toInt();
    QLocale locale = QLocale::system();
    QString counts = QStringLiteral("Created %1 folders. Already existed: %2.")
                         .arg(locale.toString(created))
                         .arg(locale.toString(existing));

    QJsonValue failureReason = result.value(QStringLiteral("failureReason"));
    if (failureReason.isString()) {
        return counts + QStringLiteral(" ") + failureReason.toString();
    }

    if (result.value(QStringLiteral("cancelled")).toBool()) {
        return QStringLiteral("Canceled. %1 You can safely run the same range again.").arg(counts);
    }

    return counts;
}

CreationWorker::CreationWorker(PlanInput plan, QString destination, QObject *parent)
    : QThread(parent), m_plan(std::move(plan)), m_destination(std::move(destination)) {}

CreationWorker::~CreationWorker() {
    cancel();
    wait();
}

void CreationWorker::cancel() {
    m_cancelled.store(true);
    quint64 handle = m_handle.load();
    if (handle != 0) {
        try {
            Core::call({
                {QStringLiteral("op"), QStringLiteral("cancel")},
                {QStringLiteral("handle"), static_cast<qint64>(handle)}
            });
        } catch (...) {
            // Ignore cancel errors if handle already released
        }
    }
}

void CreationWorker::run() {
    quint64 handle = 0;
    try {
        QJsonObject beginReq{
            {QStringLiteral("op"), QStringLiteral("begin")},
            {QStringLiteral("plan"), m_plan.toJson()},
            {QStringLiteral("destination"), m_destination}
        };
        QJsonObject beginRes = Core::call(beginReq);
        handle = static_cast<quint64>(beginRes.value(QStringLiteral("handle")).toInteger());
        m_handle.store(handle);
    } catch (const CoreException &e) {
        emit error(e.message(), e.code());
        return;
    } catch (const std::exception &e) {
        emit error(QString::fromUtf8(e.what()), QStringLiteral("runtime"));
        return;
    }

    if (m_cancelled.load()) {
        try {
            Core::call({
                {QStringLiteral("op"), QStringLiteral("cancel")},
                {QStringLiteral("handle"), static_cast<qint64>(handle)}
            });
        } catch (...) {}
    }

    try {
        QJsonObject result;
        bool done = false;
        do {
            QJsonObject stepReq{
                {QStringLiteral("op"), QStringLiteral("step")},
                {QStringLiteral("handle"), static_cast<qint64>(handle)},
                {QStringLiteral("cancelled"), m_cancelled.load()}
            };
            result = Core::call(stepReq);
            done = result.value(QStringLiteral("done")).toBool();
            emit progress(result);
        } while (!done);

        emit finished(result);
    } catch (const CoreException &e) {
        emit error(e.message(), e.code());
    } catch (const std::exception &e) {
        emit error(QString::fromUtf8(e.what()), QStringLiteral("runtime"));
    }

    if (handle != 0) {
        try {
            Core::call({
                {QStringLiteral("op"), QStringLiteral("release")},
                {QStringLiteral("handle"), static_cast<qint64>(handle)}
            });
        } catch (...) {}
        m_handle.store(0);
    }
}

} // namespace EWAF
