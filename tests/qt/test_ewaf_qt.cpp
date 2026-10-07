// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.h"
#include "core.h"
#include "identity.h"
#include "mainwindow.h"
#include "preferences.h"
#include "updateclient.h"
#include "updatecoordinator.h"
#include "workspace.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <iostream>

using namespace EWAF;

class MockDialogs : public IWorkspaceDialogs {
public:
    QString folder;
    bool accept = false;
    int confirmations = 0;

    QString chooseFolder() override {
        return folder;
    }
    bool confirm(const QString &, const QString &, const QString &) override {
        confirmations++;
        return accept;
    }
    void message(const QString &, const QString &) override {}
};

static void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        exit(1);
    }
}

static void waitForRefresh(Workspace *ws) {
    if (!ws->isRefreshing()) {
        QCoreApplication::processEvents();
        if (!ws->isRefreshing()) {
            return;
        }
    }
    QEventLoop loop;
    QObject::connect(ws, &Workspace::refreshCompleted, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
}

static void waitForOperation(Workspace *ws) {
    QEventLoop loop;
    QObject::connect(ws, &Workspace::operationCompleted, &loop, &QEventLoop::quit);
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();
}

static void delay(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--preferences-migration") {
#if !defined(Q_OS_LINUX)
            std::cerr << "Migration probe requires actual Linux" << std::endl;
            return 1;
#else
            qunsetenv("EWAF_TEST_SESSION");
            DesktopPreferences prefs;
            check(prefs.read("defaultWeekday", "") == "2", "dconf weekday import");
            check(prefs.read("rangeStart", "") == "09-03-2026", "dconf date import");
            check(prefs.read("automaticUpdates", "") == "0", "dconf boolean import");
            check(prefs.read("updateLastSuccess", "") == "123", "dconf timestamp import");
            prefs.write("defaultWeekday", "7");
            check(DesktopPreferences().read("defaultWeekday", "") == "7", "JSON was overwritten by repeat migration");
            std::cout << "PASS actual Linux dconf import and one-time JSON persistence" << std::endl;
            return 0;
#endif
        }
    }

    qputenv("EWAF_TEST_SESSION", QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
    auto globalStore = std::make_shared<MemoryPreferences>();
    Preferences::setStore(globalStore);

    int passed = 0;
    auto runCase = [&](const std::string &name, const std::function<void()> &body) {
        body();
        passed++;
        std::cout << "PASS " << name << std::endl;
    };

    auto localPrefs = std::make_shared<MemoryPreferences>();
    MockDialogs dialogs;
    auto model = std::make_unique<Workspace>(&dialogs, localPrefs.get());

    runCase("planning and Unicode search preserve complete plan", [&]() {
        model->setStart(QStringLiteral("09-03-2026"));
        model->setEnd(QStringLiteral("09-17-2026"));
        waitForRefresh(model.get());
        check(model->total() == 3 && model->canCreate(), "September plan");

        model->setSearch(QString::fromUtf8("\xEF\xBC\x90\xEF\xBC\x99-\xEF\xBC\x91\xEF\xBC\x90")); // ０９-１０
        waitForRefresh(model.get());
        check(model->total() == 3 && model->preview() == QStringList{QStringLiteral("09-10-2026")}, "Unicode filter or full plan");
    });

    runCase("invalid dates and latest asynchronous request", [&]() {
        model->setStart(QStringLiteral("02-29-2025"));
        waitForRefresh(model.get());
        check(!model->canCreate() && model->status().contains(QStringLiteral("valid date")), "Invalid date");

        model->setStart(QStringLiteral("01-01-0001"));
        model->setEnd(QStringLiteral("12-31-9999"));
        model->setStart(QStringLiteral("09-03-2026"));
        model->setEnd(QStringLiteral("09-17-2026"));
        waitForRefresh(model.get());
        delay(100);
        check(model->total() == 3, "Stale response overwrote newest plan");
    });

    QString tempPath = QDir::tempPath() + QStringLiteral("/ewaf-qt-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(tempPath);

    runCase("creation ignores search; retry preserves contents", [&]() {
        dialogs.folder = tempPath;
        model->choose();
        model->create();
        waitForOperation(model.get());

        QDir dir(tempPath);
        check(dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() == 3, "Filtered creation");

        QString keepFile = tempPath + QStringLiteral("/09-03-2026/keep.txt");
        QFile f(keepFile);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("preserved");
            f.close();
        }

        model->create();
        waitForOperation(model.get());
        check(model->status().contains(QStringLiteral("Already existed: 3")), "Retry counts");

        QFile fRead(keepFile);
        check(fRead.open(QIODevice::ReadOnly) && fRead.readAll() == "preserved", "Retry preservation");
    });

    runCase("confirmation defaults to cancel without filesystem work", [&]() {
        model->setStart(QStringLiteral("01-01-2000"));
        model->setEnd(QStringLiteral("12-31-2010"));
        waitForRefresh(model.get());

        dialogs.accept = false;
        model->create();
        check(dialogs.confirmations == 1 && !model->isWorking(), "Cancel confirmation");
        check(QDir(tempPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() == 3, "Filesystem untouched");
    });

    runCase("cancelled chooser does not create", [&]() {
        MockDialogs emptyDialogs;
        Workspace second(&emptyDialogs, localPrefs.get());
        second.setStart(QStringLiteral("09-03-2026"));
        second.setEnd(QStringLiteral("09-17-2026"));
        waitForRefresh(&second);

        second.create();
        check(!second.hasDestination() && !second.isWorking(), "Chooser cancellation");
        second.close();
    });

    runCase("conflict summary preserves existing data", [&]() {
        QString conflictPath = tempPath + QStringLiteral("/09-24-2026");
        QFile cf(conflictPath);
        if (cf.open(QIODevice::WriteOnly)) {
            cf.write("do not replace");
            cf.close();
        }

        model->setStart(QStringLiteral("09-24-2026"));
        model->setEnd(QStringLiteral("09-24-2026"));
        waitForRefresh(model.get());

        model->create();
        waitForOperation(model.get());

        QFile cfRead(conflictPath);
        check(cfRead.open(QIODevice::ReadOnly) && cfRead.readAll() == "do not replace", "Conflict preserved");
        check(model->status().contains(QStringLiteral("09-24-2026")), "Conflict status");
    });

    runCase("close requests cancellation and releases creation", [&]() {
        MockDialogs cancelDialogs;
        cancelDialogs.folder = tempPath;
        cancelDialogs.accept = true;
        Workspace cancelWs(&cancelDialogs, localPrefs.get());
        cancelWs.setStart(QStringLiteral("01-01-0001"));
        cancelWs.setEnd(QStringLiteral("12-31-9999"));
        waitForRefresh(&cancelWs);

        cancelWs.choose();
        cancelWs.create();
        cancelWs.close();
        check(cancelWs.isClosed() && !cancelWs.isBusy() && !cancelWs.canCreate(), "Close/cancel");
    });

    QDir(tempPath).removeRecursively();

    runCase("independent windows and persisted defaults/ranges", [&]() {
        auto winPrefs = std::make_shared<MemoryPreferences>();
        winPrefs->write(QStringLiteral("defaultWeekday"), QStringLiteral("99"));
        Workspace fallback(&dialogs, winPrefs.get());
        check(fallback.weekday() == 3, "Invalid default fallback to 3");

        winPrefs->write(QStringLiteral("defaultWeekday"), QStringLiteral("2"));
        Workspace next(&dialogs, winPrefs.get());
        check(next.weekday() == 0 && fallback.weekday() == 3, "Window independence");

        next.setStart(QStringLiteral("01-01-2026"));
        next.setEnd(QStringLiteral("02-01-2026"));
        waitForRefresh(&next);
        next.close();

        Workspace restored(&dialogs, winPrefs.get());
        check(restored.start() == next.start() && !restored.hasDestination(), "Restoration and destination lifetime");
    });

    runCase("legacy Linux preference decoding", [&]() {
        check(DesktopPreferences::decodeLegacy(QStringLiteral("'09-03-2026'")) == QStringLiteral("09-03-2026"), "Date unquoting");
        check(DesktopPreferences::decodeLegacy(QStringLiteral("int64 123")) == QStringLiteral("123"), "int64 parsing");
        check(DesktopPreferences::decodeLegacy(QStringLiteral("false")) == QStringLiteral("0"), "false boolean");
    });

    runCase("Qt UI bindings, accessible controls and keyboard focus", [&]() {
        MainWindow window;
        window.show();

        auto *startEdit = window.findChild<QLineEdit *>(QStringLiteral("StartDate"));
        auto *endEdit = window.findChild<QLineEdit *>(QStringLiteral("EndDate"));
        check(startEdit != nullptr && endEdit != nullptr, "Date edit widgets");

        startEdit->setText(QStringLiteral("09-03-2026"));
        waitForRefresh(window.model());
        endEdit->setText(QStringLiteral("09-17-2026"));
        waitForRefresh(window.model());

        check(window.model()->total() == 3, "Text bindings");
        check(startEdit->accessibleName() == QStringLiteral("Start date"), "Start accessible name");
        check(!startEdit->toolTip().isEmpty(), "Start tooltip");
        check(!startEdit->accessibleDescription().isEmpty(), "Start accessible description");
        check(endEdit->accessibleName() == QStringLiteral("End date"), "End accessible name");
        check(!endEdit->toolTip().isEmpty(), "End tooltip");
        check(!endEdit->accessibleDescription().isEmpty(), "End accessible description");

        auto *createBtn = window.findChild<QPushButton *>(QStringLiteral("CreateFolders"));
        check(createBtn != nullptr && !createBtn->toolTip().isEmpty() && !createBtn->accessibleDescription().isEmpty(), "Create button accessibility");
        auto *cancelBtn = window.findChild<QPushButton *>(QStringLiteral("CancelButton"));
        check(cancelBtn != nullptr && !cancelBtn->toolTip().isEmpty() && !cancelBtn->accessibleDescription().isEmpty(), "Cancel button accessibility");
        auto *destBtn = window.findChild<QPushButton *>(QStringLiteral("ChooseDestination"));
        check(destBtn != nullptr && !destBtn->toolTip().isEmpty() && !destBtn->accessibleDescription().isEmpty(), "Choose destination accessibility");
        auto *openBtn = window.findChild<QPushButton *>(QStringLiteral("OpenFolder"));
        check(openBtn != nullptr && !openBtn->toolTip().isEmpty() && !openBtn->accessibleDescription().isEmpty(), "Open folder accessibility");

        startEdit->setFocus();
        check(startEdit->hasFocus(), "Start field has focus");

        auto *searchEdit = window.findChild<QLineEdit *>(QStringLiteral("SearchBox"));
        check(searchEdit != nullptr, "SearchBox widget");
        check(searchEdit->accessibleName() == QStringLiteral("Find a folder date"), "Search accessible name");
        check(!searchEdit->toolTip().isEmpty(), "Search tooltip");
        check(!searchEdit->accessibleDescription().isEmpty(), "Search accessible description");

        searchEdit->setText(QStringLiteral("09-10"));
        waitForRefresh(window.model());

        auto *previewList = window.findChild<QListWidget *>(QStringLiteral("PreviewList"));
        check(previewList != nullptr && previewList->count() == 1 && window.model()->total() == 3, "Preview list binding");
        check(!previewList->toolTip().isEmpty(), "Preview list tooltip");
        check(!previewList->accessibleDescription().isEmpty(), "Preview list accessible description");

        previewList->setCurrentRow(0);
        window.model()->setStatus(QStringLiteral("Status changed without changing preview"));
        check(previewList->currentItem() != nullptr && previewList->currentItem()->text() == QStringLiteral("09-10-2026"),
              "Status update discarded keyboard preview selection");

        auto *startCalendar = window.findChild<QToolButton *>(QStringLiteral("StartCalendar"));
        auto *endCalendar = window.findChild<QToolButton *>(QStringLiteral("EndCalendar"));
        check(startCalendar != nullptr && endCalendar != nullptr, "Calendar controls exist");
        window.model()->runWithDialog([&]() {
            check(!startEdit->isEnabled() && !endEdit->isEnabled() &&
                  !startCalendar->isEnabled() && !endCalendar->isEnabled(),
                  "Calendar controls remained editable during a modal operation");
        });
        check(startCalendar->isEnabled() && endCalendar->isEnabled(), "Calendar controls did not recover");

        check(createBtn->nextInFocusChain() == cancelBtn, "CreateFolders tab chain to CancelButton");

        window.model()->setSelectedName(QStringLiteral("09-10-2026"));
        window.copyName();
        check(QApplication::clipboard()->text() == QStringLiteral("09-10-2026"), "Clipboard copied");

        window.close();
    });

    runCase("appearance switching and style hints", [&]() {
        App::applyAppearance(QStringLiteral("light"));
        check(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light, "Light appearance");
        const QColor lightWindow = QApplication::palette().color(QPalette::Window);
        App::applyAppearance(QStringLiteral("dark"));
        check(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark, "Dark appearance");
        const QColor darkWindow = QApplication::palette().color(QPalette::Window);
        check(darkWindow.lightness() < lightWindow.lightness(), "Appearance setting did not change the widget palette");
        App::applyAppearance(QStringLiteral("system"));
        auto scheme = QGuiApplication::styleHints()->colorScheme();
        check(scheme == Qt::ColorScheme::Light || scheme == Qt::ColorScheme::Dark || scheme == Qt::ColorScheme::Unknown, "System appearance resolved");
    });

    runCase("two-window updater guard, confirmation and settings lifecycle", [&]() {
        auto *first = new MainWindow();
        auto *second = new MainWindow();
        App::registerWindow(first);
        App::registerWindow(second);
        first->show();
        second->show();

        first->model()->setStart(QStringLiteral("01-01-2000"));
        first->model()->setEnd(QStringLiteral("12-31-2010"));
        waitForRefresh(first->model());

        check(!App::hasActiveWork(), "Idle large range does not block updates");

        QTimer::singleShot(0, [&]() {
            check(App::hasActiveWork() && !first->model()->isEditable(), "Open confirmation did not block updates");
            UpdateCoordinator::check(second, true);
            check(!UpdateClient::isRunning(), "Updater started during another window dialog");

            for (QWidget *top : QApplication::topLevelWidgets()) {
                if (auto *dlg = qobject_cast<QDialog *>(top)) {
                    dlg->reject();
                }
            }
        });
        first->model()->create();
        check(!App::hasActiveWork() && first->model()->canCreate(), "Cancelled dialog left stale busy state");

        QTimer::singleShot(0, [&]() {
            check(App::hasActiveWork(), "Settings did not block updates");
            for (QWidget *top : QApplication::topLevelWidgets()) {
                if (auto *dlg = qobject_cast<QDialog *>(top)) {
                    if (auto *combo = dlg->findChild<QComboBox *>(QStringLiteral("DefaultWeekdayCombo"))) {
                        combo->setCurrentIndex(0);
                    }
                    if (auto *appCombo = dlg->findChild<QComboBox *>(QStringLiteral("AppearanceCombo"))) {
                        appCombo->setCurrentIndex(2); // dark
                    }
                    dlg->accept();
                }
            }
        });
        first->showSettings();
        check(Preferences::read(QStringLiteral("defaultWeekday"), QStringLiteral("5")) == QStringLiteral("2") && !App::hasActiveWork(), "Settings did not persist/close");
        check(Preferences::read(QStringLiteral("appearance"), QStringLiteral("system")) == QStringLiteral("dark"), "Appearance setting did persist");

        first->close();
        second->close();
        delete first;
        delete second;
    });

    runCase("internal Mac cannot invoke production update helper", [&]() {
#if defined(Q_OS_MACOS)
        check(Identity::isInternal() && Identity::applicationId().endsWith(QStringLiteral("qt-internal")), "Internal identity");
        bool threw = false;
        try {
            UpdateClient::run({QStringLiteral("check"), QStringLiteral("99")});
        } catch (const std::exception &) {
            threw = true;
        }
        check(threw, "Updater allowed on internal Mac");
#endif
    });

    std::cout << "PASS: " << passed << " shared presentation/FFI/headless scenarios" << std::endl;
    return 0;
}
