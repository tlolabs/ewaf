// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.h"
#include "core.h"
#include "identity.h"
#include "mainwindow.h"
#include "preferences.h"

#include <QJsonDocument>
#include <QIcon>
#include <QPalette>
#include <QStyleHints>
#include <QUuid>
#include <iostream>

namespace EWAF {

QList<MainWindow *> App::s_windows;
bool App::s_smoke = false;

void App::applyAppearance(const QString &appearance) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (appearance == QStringLiteral("dark")) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    } else if (appearance == QStringLiteral("light")) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    } else {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
    }
#else
    // Qt before 6.8 has no color-scheme override. An unresolved palette returns
    // control to the platform style when System is selected.
    if (appearance != QStringLiteral("dark") && appearance != QStringLiteral("light")) {
        QApplication::setPalette(QPalette());
        return;
    }

    const bool dark = appearance == QStringLiteral("dark");
    QPalette palette;
    const QColor window = dark ? QColor(38, 38, 38) : QColor(248, 248, 248);
    const QColor base = dark ? QColor(30, 30, 30) : QColor(255, 255, 255);
    const QColor text = dark ? QColor(242, 242, 242) : QColor(24, 24, 24);
    const QColor button = dark ? QColor(53, 53, 53) : QColor(240, 240, 240);
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, button);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::ToolTipBase, base);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Highlight, dark ? QColor(86, 156, 214) : QColor(35, 103, 184));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, dark ? QColor(145, 145, 145) : QColor(112, 112, 112));
    palette.setColor(QPalette::Disabled, QPalette::Text, dark ? QColor(145, 145, 145) : QColor(112, 112, 112));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, dark ? QColor(145, 145, 145) : QColor(112, 112, 112));
    QApplication::setPalette(palette);
#endif
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
    app.setWindowIcon(QIcon(QStringLiteral(":/ewaf.png")));

    applyAppearance(Preferences::read(QStringLiteral("appearance"), QStringLiteral("system")));

    MainWindow *w = newWindow();
    w->show();

    return app.exec();
}

} // namespace EWAF
