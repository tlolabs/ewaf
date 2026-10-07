// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_APP_H
#define EWAF_APP_H

#include <QApplication>
#include <QList>

namespace EWAF {

class MainWindow;

class App {
public:
    static int run(int argc, char *argv[]);
    static MainWindow *newWindow();
    static bool hasActiveWork();
    static void closeForUpdate();

    static const QList<MainWindow *> &windows() { return s_windows; }
    static void registerWindow(MainWindow *w);
    static void unregisterWindow(MainWindow *w);

    static void applyAppearance(const QString &appearance);

    static bool isSmoke() { return s_smoke; }

private:
    static QList<MainWindow *> s_windows;
    static bool s_smoke;
};

} // namespace EWAF

#endif // EWAF_APP_H
