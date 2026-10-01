import XCTest
import AppKit
import EWAFCore
@testable import EWAF

@MainActor
final class UpdaterTests: XCTestCase {
    func testDevelopmentBuildDoesNotStartUpdater() {
        let updater = ApplicationUpdater()
        XCTAssertFalse(updater.configured)
        XCTAssertFalse(updater.automaticChecks)
    }
    func testCreationPreventsTerminationAcrossWindows() {
        let original = FolderWorkspace.activeCreations
        defer { FolderWorkspace.activeCreations = original }
        let lifecycle = ApplicationLifecycle()
        FolderWorkspace.activeCreations = 2
        XCTAssertEqual(lifecycle.applicationShouldTerminate(NSApplication.shared), .terminateCancel)
        FolderWorkspace.activeCreations = 0
        XCTAssertEqual(lifecycle.applicationShouldTerminate(NSApplication.shared), .terminateNow)
    }
    func testActualCreationsHoldAndReleaseGlobalTerminationGuard() async throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("EWAF-update-work-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: false)
        defer { try? FileManager.default.removeItem(at: root) }
        let original = FolderWorkspace.activeCreations
        var workspaces: [FolderWorkspace] = []
        for _ in 0..<2 {
            let workspace = FolderWorkspace(defaultWeekday: Weekday.thursday.rawValue)
            workspace.start = try CivilDate(folderName: "09-03-2026")
            workspace.end = try CivilDate(folderName: "09-17-2026")
            await workspace.refresh()
            workspace.destination = root
            workspaces.append(workspace)
        }
        for workspace in workspaces { workspace.create() }
        XCTAssertEqual(FolderWorkspace.activeCreations, original + 2)
        XCTAssertEqual(ApplicationLifecycle().applicationShouldTerminate(NSApplication.shared), .terminateCancel)
        for _ in 0..<200 {
            if workspaces.allSatisfy({ !$0.isCreating }) { break }
            try await Task.sleep(for: .milliseconds(10))
        }
        XCTAssertTrue(workspaces.allSatisfy({ !$0.isCreating }))
        XCTAssertEqual(FolderWorkspace.activeCreations, original)
        XCTAssertEqual(try FileManager.default.contentsOfDirectory(atPath: root.path).count, 3)
    }

}
