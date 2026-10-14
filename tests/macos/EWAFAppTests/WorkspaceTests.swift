import XCTest
import EWAFCore
@testable import EWAF

@MainActor
final class WorkspaceTests: XCTestCase {
    private func workspace() throws -> FolderWorkspace {
        let value = FolderWorkspace(defaultWeekday: Weekday.thursday.rawValue)
        value.start = try CivilDate(folderName: "09-03-2026")
        value.end = try CivilDate(folderName: "09-17-2026")
        return value
    }

    func testValidAndReversedInputsUpdateCreationAvailability() async throws {
        let value = try workspace()
        await value.refresh()
        XCTAssertTrue(value.canCreate)
        XCTAssertEqual(value.preview.count, 3)
        value.end = try CivilDate(folderName: "09-02-2026")
        await value.refresh()
        XCTAssertFalse(value.canCreate)
        XCTAssertNotNil(value.validation)
        XCTAssertNil(value.plan)
        XCTAssertTrue(value.preview.isEmpty)
    }

    func testSearchingDoesNotChangeCreationPlan() async throws {
        let value = try workspace()
        await value.refresh()
        value.search = "09-10"
        await value.filterPreview()
        XCTAssertEqual(value.preview.map(\.folderName), ["09-10-2026"])
        XCTAssertEqual(value.plan?.count, 3)
        XCTAssertTrue(value.canCreate)
    }

    func testCreationRequiresLargeRangeConfirmationBeforeOpeningChooser() async throws {
        let value = try workspace()
        value.start = try CivilDate(folderName: "01-01-2020")
        value.end = try CivilDate(folderName: "12-31-2030")
        await value.refresh()
        value.requestCreation()
        XCTAssertTrue(value.showConfirmation)
        XCTAssertFalse(value.showImporter)
        XCTAssertFalse(value.isCreating)
    }

    func testDestinationSelectionCompletesRequestedCreationAndRetry() async throws {
        let destination = FileManager.default.temporaryDirectory.appendingPathComponent("EWAF-workspace-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: false)
        defer { try? FileManager.default.removeItem(at: destination) }
        let value = try workspace()
        await value.refresh()
        value.requestCreation()
        XCTAssertTrue(value.showImporter)
        value.selectedDestination(.success([destination]))
        try await waitForCreation(value)
        XCTAssertTrue(value.showResult)
        XCTAssertEqual(value.progress.created, 3)
        value.create()
        try await waitForCreation(value)
        XCTAssertEqual(value.progress.existing, 3)
        XCTAssertEqual(value.progress.created, 0)
    }

    func testWindowsOwnSeparateWorkspacesAndValidateDefault() async throws {
        let first = try workspace()
        let second = FolderWorkspace(defaultWeekday: 99)
        first.weekday = .monday
        XCTAssertEqual(second.weekday, .thursday)
        XCTAssertNil(second.destination)
        XCTAssertNil(second.plan)
    }

    func testAppAppearanceValuesAndDisplayNames() {
        XCTAssertEqual(AppAppearance.allCases.count, 3)
        XCTAssertEqual(AppAppearance.system.rawValue, "system")
        XCTAssertEqual(AppAppearance.light.rawValue, "light")
        XCTAssertEqual(AppAppearance.dark.rawValue, "dark")
        XCTAssertEqual(AppAppearance.system.displayName, "System")
        XCTAssertEqual(AppAppearance.light.displayName, "Light")
        XCTAssertEqual(AppAppearance.dark.displayName, "Dark")
        XCTAssertNil(AppAppearance.system.colorScheme)
        XCTAssertEqual(AppAppearance.light.colorScheme, .light)
        XCTAssertEqual(AppAppearance.dark.colorScheme, .dark)
    }

    func testSelectionAndVoiceOverAnnouncementState() async throws {
        let value = try workspace()
        await value.refresh()
        XCTAssertNil(value.selectedName)
        XCTAssertEqual(value.selectedOrFirstName, "09-03-2026")
        value.selectedName = "09-10-2026"
        XCTAssertEqual(value.selectedOrFirstName, "09-10-2026")
        value.announceAccessibility("Test announcement")
    }

    func testPreviewSelectionReconciliationAcrossSearchAndRangeChanges() async throws {
        let value = try workspace()
        await value.refresh()
        XCTAssertEqual(value.preview.count, 3)

        // Select the second item
        value.selectedName = "09-10-2026"
        XCTAssertEqual(value.selectedOrFirstName, "09-10-2026")

        // Search matching selected item keeps selection
        value.search = "09-10"
        await value.filterPreview()
        XCTAssertEqual(value.selectedName, "09-10-2026")
        XCTAssertEqual(value.selectedOrFirstName, "09-10-2026")

        // Search excluding selected item reconciles selection to nil
        value.search = "09-17"
        await value.filterPreview()
        XCTAssertNil(value.selectedName)
        XCTAssertEqual(value.selectedOrFirstName, "09-17-2026")

        // Empty search results reconciles selection to nil
        value.search = "no-match"
        await value.filterPreview()
        XCTAssertTrue(value.preview.isEmpty)
        XCTAssertNil(value.selectedName)
        XCTAssertNil(value.selectedOrFirstName)

        // Reset search restores full preview
        value.search = ""
        await value.filterPreview()
        XCTAssertEqual(value.preview.count, 3)
        XCTAssertNil(value.selectedName)
        XCTAssertEqual(value.selectedOrFirstName, "09-03-2026")

        // Range change removing selected item reconciles selection
        value.selectedName = "09-17-2026"
        value.end = try CivilDate(folderName: "09-10-2026")
        await value.refresh()
        XCTAssertEqual(value.preview.map(\.folderName), ["09-03-2026", "09-10-2026"])
        XCTAssertNil(value.selectedName)

        // Weekday change reconciles selection
        value.selectedName = "09-10-2026"
        value.weekday = .friday
        await value.refresh()
        XCTAssertFalse(value.preview.map(\.folderName).contains("09-10-2026"))
        XCTAssertNil(value.selectedName)
    }

    func testAsynchronousSearchRaceIgnoresStaleQueries() async throws {
        let value = try workspace()
        await value.refresh()
        XCTAssertEqual(value.preview.count, 3)

        // Simulate concurrent filter operations where stale query arrives second
        value.search = "09-10"
        let t1 = Task { await value.filterPreview() }
        value.search = "09-17"
        let t2 = Task { await value.filterPreview() }
        _ = await (t1.value, t2.value)

        // Must match the final active search query ("09-17")
        XCTAssertEqual(value.preview.map(\.folderName), ["09-17-2026"])
    }

    func testCancellationAnnouncementIntegrity() throws {
        let value = try workspace()
        // Canceling when not creating must not emit premature "Folder creation canceled."
        value.cancel()
        XCTAssertFalse(value.isCreating)
    }

    private func waitForCreation(_ value: FolderWorkspace) async throws {
        for _ in 0..<200 {
            if !value.isCreating { return }
            try await Task.sleep(for: .milliseconds(10))
        }
        XCTFail("Folder creation did not finish within two seconds")
    }
}
