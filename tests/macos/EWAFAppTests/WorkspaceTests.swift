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

    private func waitForCreation(_ value: FolderWorkspace) async throws {
        for _ in 0..<200 {
            if !value.isCreating { return }
            try await Task.sleep(for: .milliseconds(10))
        }
        XCTFail("Folder creation did not finish within two seconds")
    }
}
