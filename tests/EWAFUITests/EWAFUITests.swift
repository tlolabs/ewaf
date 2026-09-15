import XCTest

@MainActor
final class EWAFUITests: XCTestCase {
    private var app: XCUIApplication!

    override func setUp() async throws {
        await MainActor.run { prepareApplication() }
    }

    private func prepareApplication() {
        continueAfterFailure = false
        app = XCUIApplication()
        let weekday = Calendar(identifier: .gregorian).component(.weekday, from: Date())
        app.launchArguments = ["-AppleLanguages", "(en)", "-AppleLocale", "en_US", "-defaultWeekday", String(weekday), "-NSQuitAlwaysKeepsWindows", "NO", "-ApplePersistenceIgnoreState", "YES"]
        app.launch()
        XCTAssertTrue(app.windows.firstMatch.waitForExistence(timeout: 10))
    }
    override func tearDown() async throws {
        await MainActor.run { app.terminate() }
    }

    func testAccessibleControlsAndEmptySearch() {
        XCTAssertTrue(app.datePickers["startDate"].exists)
        XCTAssertTrue(app.datePickers["endDate"].exists)
        XCTAssertTrue(app.popUpButtons["weekday"].exists)
        XCTAssertTrue(app.buttons["chooseDestination"].exists)
        XCTAssertTrue(app.buttons["createFolders"].isEnabled)
        let search = app.searchFields.firstMatch
        XCTAssertTrue(search.waitForExistence(timeout: 5))
        search.click()
        search.typeText("no-matching-folder")
        XCTAssertTrue(app.descendants(matching: .any)["emptySearch"].waitForExistence(timeout: 5))
    }

    func testCreationRetryAndExistingUserData() throws {
        let base = FileManager.default.temporaryDirectory.appendingPathComponent("EWAF-UI-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: base, withIntermediateDirectories: false)
        defer { try? FileManager.default.removeItem(at: base) }
        app.buttons["createFolders"].click()
        let dialog = app.sheets["open-panel"]
        XCTAssertTrue(dialog.waitForExistence(timeout: 5))
        app.typeKey("g", modifierFlags: [.command, .shift])
        app.typeText(base.path)
        app.typeKey(.return, modifierFlags: [])
        let open = dialog.buttons["Open"].firstMatch
        XCTAssertTrue(open.waitForExistence(timeout: 5))
        open.click()
        let ok = app.windows.buttons["OK"].firstMatch
        XCTAssertTrue(ok.waitForExistence(timeout: 10))
        ok.click()
        let directories = try FileManager.default.contentsOfDirectory(at: base, includingPropertiesForKeys: nil)
        XCTAssertEqual(directories.count, 1)
        let userFile = try XCTUnwrap(directories.first).appendingPathComponent("keep.txt")
        try Data("keep me".utf8).write(to: userFile)
        app.buttons["createFolders"].click()
        XCTAssertTrue(ok.waitForExistence(timeout: 10))
        ok.click()
        XCTAssertTrue((app.staticTexts["operationStatus"].value as? String)?.contains("Already existed: 1") == true)
        XCTAssertEqual(try String(contentsOf: userFile, encoding: .utf8), "keep me")
    }

    private func enterDates(start: String, end: String) {
        app.buttons["exactDates"].click()
        let first = app.textFields["exactStart"]
        XCTAssertTrue(first.waitForExistence(timeout: 5))
        first.click()
        first.typeKey("a", modifierFlags: .command)
        first.typeText(start)
        let last = app.textFields["exactEnd"]
        last.click()
        last.typeKey("a", modifierFlags: .command)
        last.typeText(end)
        app.buttons["Apply Dates"].click()
    }

    func testExactDatesAndWeekdayPreview() {
        enterDates(start: "09-03-2026", end: "09-17-2026")
        app.popUpButtons["weekday"].click()
        app.menuItems["Thursday"].click()
        for date in ["09-03-2026", "09-10-2026", "09-17-2026"] {
            XCTAssertTrue(app.descendants(matching: .any)["preview-\(date)"].waitForExistence(timeout: 5))
        }
    }

    func testInvalidExactDatesKeepEditorOpen() {
        enterDates(start: "02-29-2025", end: "03-01-2025")
        XCTAssertTrue(app.textFields["exactStart"].exists)
        XCTAssertTrue(app.descendants(matching: .any)["exactDateError"].exists)
    }

    func testLargeRangeRequiresConfirmation() {
        enterDates(start: "01-01-2020", end: "12-31-2030")
        app.buttons["createFolders"].click()
        XCTAssertTrue(app.buttons["Continue"].waitForExistence(timeout: 5))
        app.typeKey(.escape, modifierFlags: [])
        XCTAssertFalse(app.buttons["Continue"].exists)
    }

    func testSettingsKeyboardShortcut() {
        app.typeKey(",", modifierFlags: .command)
        XCTAssertTrue(app.popUpButtons["defaultWeekday"].waitForExistence(timeout: 5))
    }
}
