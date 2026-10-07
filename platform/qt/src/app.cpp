// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.h"
#include "core.h"
#include "identity.h"
#include "mainwindow.h"
#include "preferences.h"

#include <QJsonDocument>
#include <QStyleHints>
#include <QUuid>
#include <iostream>

namespace EWAF {

QList<MainWindow *> App::s_windows;
bool App::s_smoke = false;

void App::applyAppearance(const QString &appearance) {
    if (appearance == QStringLiteral("dark")) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    } else if (appearance == QStringLiteral("light")) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    } else {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
    }
}

MainWindow *App::newWindow() {
    auto *w = new MainWindow();
    registerWindow(w);
    return w;
}

bool App::hasActiveWork() {
    for (MainWindow *w : s_windows) {
        if (w && w->model() && w->model()->isWorking()) {
            return true;
        }
    }
    return false;
}

void App::closeForUpdate() {
    if (!hasActiveWork()) {
        QList<MainWindow *> toClose = s_windows;
        for (MainWindow *w : toClose) {
            w->close();
        }
    }
}

void App::registerWindow(MainWindow *w) {
    if (w && !s_windows.contains(w)) {
        s_windows.append(w);
    }
}

void App::unregisterWindow(MainWindow *w) {
    s_windows.removeAll(w);
}

int App::run(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == QStringLiteral("--core-smoke")) {
            try {
                QJsonObject info = Core::call({{QStringLiteral("op"), QStringLiteral("info")}});
                std::cout << "EWAF " << info.value(QStringLiteral("version")).toString().toStdString()
                          << " ABI " << QJsonDocument(info).toJson(QJsonDocument::Compact).toStdString() << std::endl;

                QJsonObject planReq{
                    {QStringLiteral("op"), QStringLiteral("plan")},
                    {QStringLiteral("plan"), QJsonObject{
                        {QStringLiteral("start"), QStringLiteral("09-03-2026")},
                        {QStringLiteral("end"), QStringLiteral("09-17-2026")},
                        {QStringLiteral("weekday"), 5}
                    }},
                    {QStringLiteral("search"), QString()}
                };
                QJsonObject planRes = Core::call(planReq);
                return (planRes.value(QStringLiteral("count")).toInt() == 3) ? 0 : 1;
            } catch (const std::exception &e) {
                std::cerr << "Core smoke error: " << e.what() << std::endl;
                return 1;
            }
        } else if (arg == QStringLiteral("--ui-smoke")) {
            s_smoke = true;
            qputenv("EWAF_TEST_SESSION", QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName(Identity::applicationId());
    app.setApplicationDisplayName(Identity::title());

    applyAppearance(Preferences::read(QStringLiteral("appearance"), QStringLiteral("system")));

    MainWindow *w = newWindow();
    w->show();

    return app.exec();
}

} // namespace EWAF
