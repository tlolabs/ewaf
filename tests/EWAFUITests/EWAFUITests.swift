import XCTest

@MainActor
final class EWAFUITests: XCTestCase {
    private var app: XCUIApplication!

    override func setUpWithError() throws {
        continueAfterFailure = false
        app = XCUIApplication()
        let weekday = Calendar(identifier: .gregorian).component(.weekday, from: Date())
        app.launchArguments = ["-AppleLanguages", "(en)", "-AppleLocale", "en_US", "-defaultWeekday", String(weekday), "-NSQuitAlwaysKeepsWindows", "NO"]
        app.launch()
        XCTAssertTrue(app.windows.firstMatch.waitForExistence(timeout: 10))
    }
    override func tearDownWithError() throws { app.terminate() }

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
        XCTAssertTrue(app.staticTexts["No Results"].waitForExistence(timeout: 5))
    }

    func testCreationRetryAndExistingUserData() throws {
        let base = FileManager.default.temporaryDirectory.appendingPathComponent("EWAF-UI-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: base, withIntermediateDirectories: false)
        defer { try? FileManager.default.removeItem(at: base) }
        app.buttons["createFolders"].click()
        let dialog = app.dialogs.firstMatch
        XCTAssertTrue(dialog.waitForExistence(timeout: 5))
        app.typeKey("g", modifierFlags: [.command, .shift])
        app.typeText(base.path)
        app.typeKey(.return, modifierFlags: [])
        let open = app.buttons["Open"].firstMatch
        XCTAssertTrue(open.waitForExistence(timeout: 5))
        open.click()
        let ok = app.buttons["OK"].firstMatch
        XCTAssertTrue(ok.waitForExistence(timeout: 10))
        ok.click()
        let directories = try FileManager.default.contentsOfDirectory(at: base, includingPropertiesForKeys: nil)
        XCTAssertEqual(directories.count, 1)
        let userFile = try XCTUnwrap(directories.first).appendingPathComponent("keep.txt")
        try Data("keep me".utf8).write(to: userFile)
        app.buttons["createFolders"].click()
        XCTAssertTrue(ok.waitForExistence(timeout: 10))
        XCTAssertTrue(app.staticTexts.containing(NSPredicate(format: "label CONTAINS %@", "Already existed: 1")).firstMatch.exists)
        ok.click()
        XCTAssertEqual(try String(contentsOf: userFile, encoding: .utf8), "keep me")
    }

    func testSettingsKeyboardShortcut() {
        app.typeKey(",", modifierFlags: .command)
        XCTAssertTrue(app.popUpButtons["Default weekday"].waitForExistence(timeout: 5))
    }
}
