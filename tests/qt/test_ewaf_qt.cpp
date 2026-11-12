// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.h"
#include "core.h"
#include "identity.h"
#include "mainwindow.h"
#include "preferences.h"
#include "updateclient.h"
#include "updatecoordinator.h"
#include "updateworker.h"
#include "workspace.h"

#include <QAction>
#include <QApplication>
#include <QCalendarWidget>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <QJsonDocument>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QPointer>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <cmath>
#include <iostream>
#if defined(Q_OS_WIN)
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

using namespace EWAF;

class MockDialogs : public IWorkspaceDialogs {
public:
    QString folder;
    bool accept = false;
    int confirmations = 0;
    std::function<void()> onMessage;

    QString chooseFolder() override {
        return folder;
    }
    bool confirm(const QString &, const QString &, const QString &) override {
        confirmations++;
        return accept;
    }
    void message(const QString &, const QString &) override { if (onMessage) onMessage(); }
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

    const auto arguments = app.arguments();
    if (arguments.size() >= 3 && arguments[1] == QStringLiteral("--update-helper-fixture")) {
        const auto mode = arguments[2];
        if (mode == "chunk") {
            std::cout << "{\"val" << std::flush;
            QThread::msleep(150);
            std::cerr << "diagnostic on stderr" << std::flush;
            std::cout << "ue\":42}" << std::endl;
        } else if (mode == "error") {
            std::cout << "{\"error\":\"fixture failure\"}" << std::endl;
            return 7;
        } else if (mode == "invalid") {
            std::cout << "not JSON" << std::endl;
        } else if (mode == "crash") {
            std::abort();
        } else {
            QThread::msleep(3000);
            std::cout << "{}" << std::endl;
        }
        return 0;
    }
    if (arguments.size() == 4 && arguments[1] == QStringLiteral("--installer-child-fixture")) {
        const auto parentPid = arguments[3].toLongLong();
#if defined(Q_OS_WIN)
        HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(parentPid));
        if (!parent) return 2;
#endif
        std::cout << "REA" << std::flush;
        QThread::msleep(20);
        std::cout << "DY" << std::endl;
        std::string command;
        if (!std::getline(std::cin, command) || command != "COMMIT") return 0;
        std::cout << "COM" << std::flush;
        QThread::msleep(20);
        std::cout << "MITTED" << std::endl;
        bool exited = false;
        for (int i = 0; i < 250; ++i) {
#if defined(Q_OS_WIN)
            exited = WaitForSingleObject(parent, 0) == WAIT_OBJECT_0;
#else
            exited = kill(static_cast<pid_t>(parentPid), 0) == -1 && errno == ESRCH;
#endif
            if (exited) break;
            QThread::msleep(20);
        }
#if defined(Q_OS_WIN)
        CloseHandle(parent);
#endif
        if (!exited) return 3;
        QFile marker(arguments[2]);
        if (!marker.open(QIODevice::WriteOnly)) return 4;
        marker.write("survived normal parent exit");
        return 0;
    }
    if (arguments.size() == 3 && arguments[1] == QStringLiteral("--installer-parent-exit-probe")) {
        auto child = std::make_unique<QProcess>();
        child->start(app.applicationFilePath(), {QStringLiteral("--installer-child-fixture"), arguments[2], QString::number(app.applicationPid())});
        PreparedInstall installer(std::move(child));
        QObject::connect(&installer, &PreparedInstall::ready, &installer, &PreparedInstall::commit);
        QObject::connect(&installer, &PreparedInstall::committed, &app, [&] { app.exit(0); });
        QObject::connect(&installer, &PreparedInstall::failed, &app, [&](const QString &message) { std::cerr << message.toStdString(); app.exit(2); });
        QTimer::singleShot(10000, &app, [&] { app.exit(3); });
        return app.exec(); // Real parent teardown, including PreparedInstall destruction.
    }

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

    QTemporaryDir nativePreferences;
    check(nativePreferences.isValid(), "Create isolated native preferences root");
    qputenv("XDG_CONFIG_HOME", nativePreferences.path().toUtf8());
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

    runCase("search preserves a normalized plan and date changes invalidate it immediately", [&]() {
        MemoryPreferences preferences;
        MockDialogs localDialogs;
        Workspace workspace(&localDialogs, &preferences);
        workspace.setStart(QStringLiteral(" 09-03-2026 "));
        workspace.setEnd(QStringLiteral("\t09-17-2026\n"));
        waitForRefresh(&workspace);
        check(workspace.canCreate() && workspace.total() == 3, "Whitespace-trimmed plan is valid");
        workspace.setSearch(QStringLiteral("09-10"));
        check(workspace.canCreate() && workspace.total() == 3, "Query preserves normalized plan before filtering completes");
        waitForRefresh(&workspace);
        check(workspace.preview() == QStringList{QStringLiteral("09-10-2026")}, "Normalized plan is filtered");
        workspace.setEnd(QStringLiteral("invalid"));
        check(!workspace.canCreate() && workspace.preview().isEmpty(), "Changing dates invalidates the cached plan immediately");
        waitForRefresh(&workspace);
        check(!workspace.canCreate(), "Invalid range cannot reuse a cached plan");
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

    runCase("updater independent deadlines, responsiveness, output, errors and cancellation", [&]() {
        check(UpdateWorker::deadlineFor("download") >= 960000, "Download must allow 900s plus metadata authentication");
        check(UpdateWorker::deadlineFor("install-appimage") > UpdateWorker::deadlineFor("check"), "Independent operation deadlines");
        auto runWorker = [&](const QString &mode, int timeout, bool cancel) {
            QEventLoop loop;
            auto *worker = new UpdateWorker();
            QPointer<UpdateWorker> lifetime(worker);
            QJsonObject result;
            QString error;
            bool cancelled = false, completed = false;
            int ticks = 0;
            QTimer heartbeat;
            heartbeat.setInterval(5);
            QObject::connect(&heartbeat, &QTimer::timeout, &loop, [&] { ++ticks; });
            heartbeat.start();
            QObject::connect(worker, &UpdateWorker::completed, &loop, [&](const QJsonObject &value, const QString &failure, bool wasCancelled) {
                result = value; error = failure; cancelled = wasCancelled; completed = true;
                worker->deleteLater(); loop.quit();
            });
            worker->start(app.applicationFilePath(), {"--update-helper-fixture", mode}, timeout);
            if (cancel) QTimer::singleShot(80, worker, &UpdateWorker::cancel);
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);
            loop.exec();
            check(completed, "Worker must settle within fixture deadline");
            if (mode == "chunk") check(result.value("value").toInt() == 42 && error.isEmpty() && ticks > 5, "Partial JSON and responsive GUI");
            else check(!error.isEmpty(), "Failure must not be mistaken for success");
            check(cancelled == cancel, "Cancellation result is explicit");
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            check(lifetime.isNull(), "Completed worker must be destroyed");
        };
        runWorker("chunk", 4000, false);
        runWorker("error", 4000, false);
        runWorker("invalid", 4000, false);
        runWorker("crash", 4000, false);
        runWorker("slow", 80, false);
        runWorker("slow", 4000, true);
        for (int i = 0; i < 5; ++i) runWorker("error", 4000, false);
        {
            auto failedProcess = std::make_unique<QProcess>();
            failedProcess->start(QDir::tempPath() + "/ewaf-nonexistent-installer-fixture-" + QUuid::createUuid().toString());
            check(!failedProcess->waitForStarted(1000), "Missing installer fixture must fail before wrapper connects");
            PreparedInstall installer(std::move(failedProcess));
            QEventLoop loop;
            bool failed = false;
            QObject::connect(&installer, &PreparedInstall::failed, &loop, [&](const QString &) { failed = true; loop.quit(); });
            QTimer::singleShot(1000, &loop, &QEventLoop::quit);
            loop.exec();
            check(failed, "Already-failed installer must report readiness failure immediately");
        }
        QElapsedTimer elapsed;
        elapsed.start();
        auto worker = std::make_unique<UpdateWorker>();
        worker->start(app.applicationFilePath(), {"--update-helper-fixture", "slow"}, 4000);
        delay(80);
        worker.reset();
        check(elapsed.elapsed() < 2000, "Shutdown must reap an active helper without a network-length wait");
    });

    runCase("committed installer survives actual normal parent exit", [&]() {
        QTemporaryDir directory;
        check(directory.isValid(), "Parent-exit fixture directory");
        const QString marker = directory.path() + "/survived.txt";
        QProcess parent;
        parent.start(app.applicationFilePath(), {"--installer-parent-exit-probe", marker});
        check(parent.waitForStarted(5000) && parent.waitForFinished(12000), "Real parent process must exit");
        check(parent.exitStatus() == QProcess::NormalExit && parent.exitCode() == 0,
              "READY/COMMIT/COMMITTED must complete before normal parent teardown");
        for (int i = 0; i < 250 && !QFile::exists(marker); ++i) delay(20);
        QFile file(marker);
        check(file.open(QIODevice::ReadOnly) && file.readAll() == "survived normal parent exit", "Child must perform work after original parent is gone");
    });

    runCase("failed atomic preferences save retains persisted and in-memory values", [&]() {
#if !defined(Q_OS_WIN)
        QTemporaryDir directory;
        const auto previousRoot = qgetenv("XDG_CONFIG_HOME");
        const auto previousSession = qgetenv("EWAF_TEST_SESSION");
        qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
        qputenv("EWAF_TEST_SESSION", "save-failure");
        DesktopPreferences prefs;
        check(prefs.write("key", "old"), "Initial durable preference");
        const QString path = directory.path() + "/" + Identity::applicationId() + "/save-failure";
        check(QDir().rename(path, path + "-retained"), "Move only isolated fixture");
        QFile blocker(path);
        check(blocker.open(QIODevice::WriteOnly), "Block fixture directory recreation");
        blocker.close();
        check(!prefs.write("key", "new"), "Write failure must be reported");
        check(prefs.read("key", "") == "old", "Failed write must not change in-memory value");
        check(QFile::remove(path) && QDir().rename(path + "-retained", path), "Restore fixture directory");
        check(DesktopPreferences().read("key", "") == "old", "Old JSON must remain reloadable");
        qputenv("XDG_CONFIG_HOME", previousRoot);
        qputenv("EWAF_TEST_SESSION", previousSession);
#endif
    });

    runCase("Windows registry string decoding and atomic preferences", [&]() {
        QString fb = QStringLiteral("default_fallback");

        // 1. Valid terminated string
        std::u16string s1 = u"dark";
        s1.push_back(u'\0');
        check(DesktopPreferences::decodeRegistryString(reinterpret_cast<const unsigned char *>(s1.data()), s1.size() * sizeof(char16_t), 1, fb) == QStringLiteral("dark"), "Valid terminated REG_SZ");

        // 2. Unterminated string (no trailing NUL)
        std::u16string s2 = u"light";
        check(DesktopPreferences::decodeRegistryString(reinterpret_cast<const unsigned char *>(s2.data()), s2.size() * sizeof(char16_t), 1, fb) == QStringLiteral("light"), "Unterminated string handled safely");

        // 3. Empty string (0 bytes)
        check(DesktopPreferences::decodeRegistryString(nullptr, 0, 1, fb) == QStringLiteral(""), "Empty 0-byte registry value");

        // 4. Null terminator only
        std::u16string s4 = u"";
        s4.push_back(u'\0');
        check(DesktopPreferences::decodeRegistryString(reinterpret_cast<const unsigned char *>(s4.data()), sizeof(char16_t), 1, fb) == QStringLiteral(""), "Single NUL character registry value");

        // 5. Interior NUL
        std::u16string s5 = u"hello";
        s5.push_back(u'\0');
        s5 += u"world";
        check(DesktopPreferences::decodeRegistryString(reinterpret_cast<const unsigned char *>(s5.data()), s5.size() * sizeof(char16_t), 1, fb) == QStringLiteral("hello"), "Interior NUL character bounded decode");

        // 6. Malformed: odd byte count
        unsigned char oddBytes[] = { 'a', 'b', 'c' };
        check(DesktopPreferences::decodeRegistryString(oddBytes, 3, 1, fb) == fb, "Malformed odd byte length falls back");

        // 7. Invalid type: REG_DWORD (4)
        check(DesktopPreferences::decodeRegistryString(reinterpret_cast<const unsigned char *>(s1.data()), s1.size() * sizeof(char16_t), 4, fb) == fb, "Invalid type falls back");

        // 8. REG_EXPAND_SZ (2) accepted
        check(DesktopPreferences::decodeRegistryString(reinterpret_cast<const unsigned char *>(s1.data()), s1.size() * sizeof(char16_t), 2, fb) == QStringLiteral("dark"), "REG_EXPAND_SZ accepted");

        // 9. Maximum-length bound (> 64 KiB)
        std::vector<unsigned char> oversized(70000, 'A');
        check(DesktopPreferences::decodeRegistryString(oversized.data(), oversized.size(), 1, fb) == fb, "Overly large registry string rejected");

        const unsigned char unaligned[] = { 0xff, 'd', 0, 'a', 0, 'r', 0, 'k', 0 };
        check(DesktopPreferences::decodeRegistryString(unaligned + 1, 8, 1, fb) == QStringLiteral("dark"), "Unaligned UTF-16LE buffer");
        check(DesktopPreferences::decodeRegistryString(nullptr, 2, 1, fb) == fb, "Null nonempty registry buffer");

        // Test CQ-03: Atomic preferences persistence via DesktopPreferences
        const auto previousSession = qgetenv("EWAF_TEST_SESSION");
        QString testSession = QStringLiteral("atomic_test_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        qputenv("EWAF_TEST_SESSION", testSession.toUtf8());
        DesktopPreferences dp;
        check(dp.write(QStringLiteral("testKey"), QStringLiteral("testVal")), "DesktopPreferences write returns true");
        check(dp.read(QStringLiteral("testKey"), QString()) == QStringLiteral("testVal"), "DesktopPreferences read back value");
        check(DesktopPreferences().read(QStringLiteral("testKey"), QString()) == QStringLiteral("testVal"), "Native preferences backend reloads persisted value");
        qputenv("EWAF_TEST_SESSION", previousSession);
    });

    runCase("Bounded preview updates retain rows and discard obsolete searches", [&]() {
        MainWindow window;
        auto *workspace = window.model();
        workspace->setStart(QStringLiteral("01-01-0001"));
        workspace->setEnd(QStringLiteral("12-31-9999"));
        workspace->setWeekday(3);
        workspace->setSearch(QString());
        waitForRefresh(workspace);
        auto *list = window.findChild<QListWidget *>(QStringLiteral("PreviewList"));
        check(list && list->count() == 200 && workspace->total() == 521723,
              "Maximum range keeps the bounded preview and complete count");
        list->setCurrentRow(100);
        auto *selected = list->currentItem();
        const auto name = selected->text();
        for (int i = 0; i < 50; ++i) {
            workspace->setStatus(QStringLiteral("Progress update %1").arg(i));
            check(list->currentItem() == selected && workspace->selectedName() == name,
                  "Progress updates retain row identity and selection");
        }
        workspace->setSearch(name);
        waitForRefresh(workspace);
        check(list->count() == 1 && list->currentItem() && list->currentItem()->text() == name,
              "Changed preview retains a still-visible selected name");
        workspace->setSearch(QStringLiteral("9999"));
        workspace->setSearch(QStringLiteral("12345"));
        workspace->setSearch(QStringLiteral("０９-１０"));
        workspace->setSearch(QString());
        waitForRefresh(workspace);
        check(list->count() == 200 && list->item(0)->text() == QStringLiteral("01-04-0001") &&
              workspace->total() == 521723 && workspace->canCreate(),
              "Rapid input publishes the latest preview without filtering creation");
        workspace->setSearch(QStringLiteral("no matches"));
        waitForRefresh(workspace);
        check(list->count() == 0 && workspace->selectedName().isEmpty() && workspace->canCreate(),
              "Empty preview clears selection while preserving the complete plan");
    });

    runCase("Qt UI bindings, accessible controls and keyboard focus", [&]() {
        check(!QIcon(QStringLiteral(":/ewaf.png")).isNull(), "Application artwork is compiled into the Qt UI");
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

        // Verify Menu actions enabled synchronization (QT-UI-02)
        check(window.chooseDestinationAction() != nullptr && window.chooseDestinationAction()->isEnabled(), "Choose destination action enabled");
        check(window.createAction() != nullptr && window.createAction()->isEnabled(), "Create action enabled with valid plan");
        check(window.cancelAction() != nullptr && !window.cancelAction()->isEnabled(), "Cancel action disabled when idle");
        check(window.settingsAction() != nullptr && window.settingsAction()->isEnabled(), "Settings action enabled when idle");
        check(window.copyAction() != nullptr && window.copyAction()->isEnabled(), "Copy action enabled with preview items");

        auto *createBtn = window.findChild<QPushButton *>(QStringLiteral("CreateFolders"));
        check(createBtn != nullptr && !createBtn->toolTip().isEmpty() && !createBtn->accessibleDescription().isEmpty(), "Create button accessibility");
        auto *cancelBtn = window.findChild<QPushButton *>(QStringLiteral("CancelButton"));
        check(cancelBtn != nullptr && !cancelBtn->toolTip().isEmpty() && !cancelBtn->accessibleDescription().isEmpty(), "Cancel button accessibility");
        auto *destBtn = window.findChild<QPushButton *>(QStringLiteral("ChooseDestination"));
        check(destBtn != nullptr && !destBtn->toolTip().isEmpty() && !destBtn->accessibleDescription().isEmpty(), "Choose destination accessibility");
        auto *openBtn = window.findChild<QPushButton *>(QStringLiteral("OpenFolder"));
        check(openBtn != nullptr && !openBtn->toolTip().isEmpty() && !openBtn->accessibleDescription().isEmpty(), "Open folder accessibility");

        startEdit->setFocus();
        // Xvfb does not run a window manager, so hasFocus() can remain false
        // even when this window's keyboard focus target is correct.
        check(window.focusWidget() == startEdit, "Start field is the window's keyboard focus target");

        auto *searchEdit = window.findChild<QLineEdit *>(QStringLiteral("SearchBox"));
        check(searchEdit != nullptr, "SearchBox widget");
        check(searchEdit->accessibleName() == QStringLiteral("Find a folder date"), "Search accessible name");
        check(!searchEdit->toolTip().isEmpty(), "Search tooltip");
        check(!searchEdit->accessibleDescription().isEmpty(), "Search accessible description");

        // Verify Search filter typing preserves valid plan immediately without resetting total (QT-UI-05)
        searchEdit->setText(QStringLiteral("09-10"));
        check(window.model()->canCreate(), "Typing in search box preserves canCreate during query");
        check(window.model()->total() == 3, "Typing in search box preserves total planned folders during query");
        waitForRefresh(window.model());
        check(window.model()->canCreate(), "Plan remains valid after search query finishes");
        check(window.model()->total() == 3, "Total planned count preserved after search query finishes");

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
        auto *progress = window.findChild<QProgressBar *>(QStringLiteral("ProgressBar"));
        check(startCalendar != nullptr && endCalendar != nullptr, "Calendar controls exist");
        check(progress != nullptr, "Accessible progress control exists");
        window.model()->runWithDialog([&]() {
            check(!startEdit->isEnabled() && !endEdit->isEnabled() &&
                  !startCalendar->isEnabled() && !endCalendar->isEnabled(),
                  "Calendar controls remained editable during a modal operation");
            check(progress->accessibleDescription() == QStringLiteral("0 of 3 folders processed"),
                  "Accessible progress must use processed rather than created wording");
            check(!window.chooseDestinationAction()->isEnabled(), "Choose destination menu action disabled during modal dialog");
            check(!window.createAction()->isEnabled(), "Create menu action disabled during modal dialog");
            check(!window.settingsAction()->isEnabled(), "Settings menu action disabled during modal dialog");
        });
        check(startCalendar->isEnabled() && endCalendar->isEnabled(), "Calendar controls did not recover");

        // Verify calendar picker date range configuration (QT-UI-06)
        QTimer::singleShot(0, [&window] {
            auto *calendar = window.findChild<QCalendarWidget *>();
            check(calendar != nullptr, "Calendar widget found in picker dialog");
            check(calendar->minimumDate() == QDate(1, 1, 1), "Calendar picker minimum date is year 1");
            check(calendar->maximumDate() == QDate(9999, 12, 31), "Calendar picker maximum date is year 9999");
            if (auto *dlg = window.findChild<QDialog *>()) {
                dlg->reject();
            }
        });
        window.pickDate(startEdit);

        // Exact input permits surrounding whitespace. Opening a picker must
        // retain that validated date instead of silently selecting today.
        startEdit->setText(QStringLiteral(" 01-02-0001 "));
        waitForRefresh(window.model());
        QTimer::singleShot(0, [&window] {
            auto *calendar = window.findChild<QCalendarWidget *>();
            check(calendar && calendar->selectedDate() == QDate(1, 1, 2), "Picker retains whitespace-trimmed historical date");
            if (auto *dialog = window.findChild<QDialog *>()) dialog->accept();
        });
        window.pickDate(startEdit);
        check(startEdit->text() == QStringLiteral("01-02-0001"), "Accepted calendar date is canonical");
        startEdit->setText(QStringLiteral("09-03-2026"));
        waitForRefresh(window.model());

        check(createBtn->nextInFocusChain() == cancelBtn, "CreateFolders tab chain to CancelButton");

        // Verify right-click context menu selects clicked row (QT-UI-04)
        searchEdit->clear();
        waitForRefresh(window.model());
        auto *previewListWidget = qobject_cast<PreviewListWidget *>(previewList);
        check(previewListWidget != nullptr, "Preview list is PreviewListWidget");
        check(previewListWidget->count() >= 2, "Preview list has multiple items for context menu test");
        previewListWidget->setCurrentRow(0);
        check(window.model()->selectedName() == previewListWidget->item(0)->text(), "Initial selection is row 0");

        QRect item1Rect = previewListWidget->visualItemRect(previewListWidget->item(1));
        QTimer::singleShot(0, [] {
            if (auto *popup = QApplication::activePopupWidget()) {
                popup->close();
            }
        });
        emit previewListWidget->customContextMenuRequested(item1Rect.center());
        check(window.model()->selectedName() == previewListWidget->item(1)->text(), "Context menu right-click selects clicked item");
        window.copyName();
        check(QApplication::clipboard()->text() == previewListWidget->item(1)->text(), "Copy Folder Name copies context-menu selected item");

        window.model()->setSelectedName(QStringLiteral("09-10-2026"));
        window.copyName();
        check(QApplication::clipboard()->text() == QStringLiteral("09-10-2026"), "Clipboard copied");

        // Verify fallback to first item when selectedName is empty (QT-UI-03)
        window.model()->setSelectedName(QString());
        window.copyName();
        check(QApplication::clipboard()->text() == previewListWidget->item(0)->text(), "Clipboard copied first preview row");

        // Verify Drag & Drop mime data provides plain text folder names (QT-UI-01)
#if QT_CONFIG(draganddrop)
        auto *mime = previewListWidget->model()->mimeData(previewListWidget->model()->match(
            previewListWidget->model()->index(0, 0), Qt::DisplayRole, QStringLiteral("09-10-2026")));
        check(mime != nullptr && mime->hasText() && mime->text() == QStringLiteral("09-10-2026"), "Preview drag provides text folder name");
        delete mime;
#endif

        // Verify true JSON string/variant decoding for numbers and booleans in Preferences (QT-PREF-01)
#if !defined(Q_OS_WIN)
        // Windows persists native REG_SZ values and does not read this JSON.
        {
            QTemporaryDir prefDir;
            QString prevRoot = qEnvironmentVariable("XDG_CONFIG_HOME");
            QString prevSession = qEnvironmentVariable("EWAF_TEST_SESSION");
            qputenv("XDG_CONFIG_HOME", prefDir.path().toUtf8());
            qputenv("EWAF_TEST_SESSION", "pref-type-test");

            QString targetDir = prefDir.path() + "/" + Identity::applicationId() + "/pref-type-test";
            QDir().mkpath(targetDir);
            QFile jsonFile(targetDir + "/preferences.json");
            check(jsonFile.open(QIODevice::WriteOnly), "Open test preferences.json for writing");
            jsonFile.write(R"({
                "defaultWeekday": 5,
                "numericPref": 123,
                "boolPref": true,
                "stringPref": "hello"
            })");
            jsonFile.close();

            DesktopPreferences dp;
            check(dp.read(QStringLiteral("defaultWeekday"), QString()) == QStringLiteral("5"), "Decode numeric JSON integer to string");
            check(dp.read(QStringLiteral("numericPref"), QString()) == QStringLiteral("123"), "Decode numeric JSON number to string");
            check(dp.read(QStringLiteral("boolPref"), QString()) == QStringLiteral("true"), "Decode boolean JSON to string");
            check(dp.read(QStringLiteral("stringPref"), QString()) == QStringLiteral("hello"), "Decode string JSON");

            qputenv("XDG_CONFIG_HOME", prevRoot.toUtf8());
            qputenv("EWAF_TEST_SESSION", prevSession.toUtf8());
        }
#endif

        window.close();
    });

    runCase("appearance switching, style hints, and WCAG contrast (AX-01)", [&]() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        const auto platform = QGuiApplication::platformName();
        const bool nativeTheme = platform != QStringLiteral("offscreen") && platform != QStringLiteral("minimal");
        if (!nativeTheme) std::cout << "SKIP native color-scheme hints: headless platform " << platform.toStdString() << std::endl;
#endif
        auto relLum = [](const QColor &color) -> double {
            auto ch = [](double c) -> double {
                return (c <= 0.04045) ? (c / 12.92) : std::pow((c + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * ch(color.redF()) + 0.7152 * ch(color.greenF()) + 0.0722 * ch(color.blueF());
        };
        auto contrast = [&](const QColor &c1, const QColor &c2) -> double {
            double l1 = relLum(c1);
            double l2 = relLum(c2);
            return (std::max(l1, l2) + 0.05) / (std::min(l1, l2) + 0.05);
        };

        App::applyAppearance(QStringLiteral("system"));
        const QColor originalWindow = QApplication::palette().color(QPalette::Window);
        App::applyAppearance(QStringLiteral("light"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (nativeTheme) check(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light, "Light appearance");
#endif
        const QColor lightWindow = QApplication::palette().color(QPalette::Window);
        QPalette lightPal = QApplication::palette();
        double lightSelContrast = contrast(lightPal.color(QPalette::HighlightedText), lightPal.color(QPalette::Highlight));
        check(lightSelContrast >= 4.5, "Light appearance selection contrast meets WCAG AA (>= 4.5:1)");

        App::applyAppearance(QStringLiteral("dark"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (nativeTheme) check(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark, "Dark appearance");
#endif
        const QColor darkWindow = QApplication::palette().color(QPalette::Window);
        check(darkWindow.lightness() < lightWindow.lightness(), "Appearance setting did not change the widget palette");
        QPalette darkPal = QApplication::palette();
        double darkSelContrast = contrast(darkPal.color(QPalette::HighlightedText), darkPal.color(QPalette::Highlight));
        check(darkSelContrast >= 4.5, "Dark appearance selection contrast meets WCAG AA (>= 4.5:1)");
        double darkTextContrast = contrast(darkPal.color(QPalette::Text), darkPal.color(QPalette::Base));
        check(darkTextContrast >= 4.5, "Dark appearance normal text contrast meets WCAG AA (>= 4.5:1)");

        // Native high-contrast setting simulation: must preserve native platform palette
        qputenv("EWAF_FORCE_HIGH_CONTRAST", "1");
        App::applyAppearance(QStringLiteral("dark"));
        check(QApplication::palette().color(QPalette::Window) == originalWindow,
              "High-contrast mode respects platform palette override");
        qunsetenv("EWAF_FORCE_HIGH_CONTRAST");

        App::applyAppearance(QStringLiteral("system"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        auto scheme = QGuiApplication::styleHints()->colorScheme();
        check(scheme == Qt::ColorScheme::Light || scheme == Qt::ColorScheme::Dark || scheme == Qt::ColorScheme::Unknown, "System appearance resolved");
#endif
        check(QApplication::palette().color(QPalette::Window).isValid(), "System appearance palette is invalid");
        check(QApplication::palette().color(QPalette::Window) == originalWindow,
              "System appearance did not restore the platform palette");
    });

    runCase("enlarged Qt text preserves readable controls and resizing", [&]() {
        const auto original = app.font();
        auto enlarged = original;
        enlarged.setPointSizeF(original.pointSizeF() > 0 ? original.pointSizeF() * 1.75 : 21);
        app.setFont(enlarged);
        MainWindow window;
        window.show();
        waitForRefresh(window.model());
        window.resize(window.minimumSizeHint().expandedTo(QSize(640, 480)));
        QCoreApplication::processEvents();
        const auto create = window.findChild<QPushButton *>("CreateFolders");
        check(create && create->width() >= create->fontMetrics().horizontalAdvance(create->text()), "Enlarged button text must fit");
        check(create->height() >= create->fontMetrics().height(), "Enlarged button height must fit");
        window.resize(window.width() + 200, window.height() + 100);
        check(window.findChild<QListWidget *>("PreviewList")->width() > 0, "Preview remains visible after resize");
        window.close();
        app.setFont(original);
    });

    runCase("closing a window during modal callbacks safely unwinds dialogs", [&]() {
        QPointer<MainWindow> window = App::newWindow();
        window->show();
        waitForRefresh(window->model());
        QTimer::singleShot(20, window, [window] {
            check(App::hasActiveWork(), "Every modal dialog must block application update handoff");
            window->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        });
        check(!window->confirm("Fixture", "Close while confirmation is active", "Continue"), "Closing parent cancels consent");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        check(window.isNull(), "Parent is released after confirmation unwinds");
        window = App::newWindow();
        window->show();
        waitForRefresh(window->model());
        QTimer::singleShot(20, window, [window] {
            check(App::hasActiveWork(), "Every modal dialog must block application update handoff");
            window->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        });
        window->showSettings();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        check(window.isNull(), "Parent is released after settings/workspace callbacks unwind");
    });

    runCase("closing from a creation result callback safely settles worker ownership", [&]() {
        MockDialogs closingDialogs;
        MemoryPreferences preferences;
        Workspace closing(&closingDialogs, &preferences);
        closing.setStart("09-03-2026");
        closing.setEnd("09-17-2026");
        waitForRefresh(&closing);
        QTemporaryDir destination;
        closingDialogs.folder = destination.path();
        check(closing.choose(), "Select isolated callback fixture destination");
        closingDialogs.onMessage = [&] { closing.close(); };
        closing.create();
        waitForOperation(&closing);
        check(closing.isClosed() && !closing.isBusy() && closing.processed() == 3, "Result callback may close workspace without dereferencing a cleared worker");
    });

    runCase("Qt window lifecycle and automatic deletion on close (CQ-06)", [&]() {
        QPointer<MainWindow> win = App::newWindow();
        check(!win.isNull(), "Window created via factory");
        win->show();
        check(App::windows().contains(win.data()), "Window registered in App");
        win->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        check(win.isNull(), "Closed top-level window automatically deleted on close");
        check(!App::windows().contains(win.data()), "Window unregistered from App");
        for (int i = 0; i < 30; ++i) {
            QPointer<MainWindow> repeated = App::newWindow();
            repeated->show();
            repeated->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            check(repeated.isNull(), "Repeated windows must release widgets and observers");
        }
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
