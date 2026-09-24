import XCTest
@testable import EWAFCore

final class PersistenceTests: XCTestCase, @unchecked Sendable {
    func temporaryDirectory() throws -> URL {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("EWAF-tests-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: false)
        addTeardownBlock { try FileManager.default.removeItem(at: url) }
        return url
    }
    func plan() throws -> FolderPlan {
        try FolderPlan(start: CivilDate(folderName: "09-03-2026"), end: CivilDate(folderName: "09-17-2026"), weekday: .thursday)
    }
    func testCreationAndIdempotentRetry() async throws {
        let base = try temporaryDirectory()
        let service = FolderService()
        let first = await service.create(try plan(), in: base)
        XCTAssertEqual(first.created, 3)
        XCTAssertNil(first.failureReason)
        let second = await service.create(try plan(), in: base)
        XCTAssertEqual(second.created, 0)
        XCTAssertEqual(second.existing, 3)
        XCTAssertEqual(try FileManager.default.contentsOfDirectory(atPath: base.path).sorted(), ["09-03-2026", "09-10-2026", "09-17-2026"])
    }
    func testLegacyFoldersAndUserContentsRemainUnchanged() async throws {
        let base = try temporaryDirectory()
        let folder = base.appendingPathComponent("09-03-2026")
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: false)
        let file = folder.appendingPathComponent("user-data.txt")
        let content = Data("Preserve my work 🗓️".utf8)
        try content.write(to: file)
        let result = await FolderService().create(try plan(), in: base)
        XCTAssertEqual(result.existing, 1)
        XCTAssertEqual(result.created, 2)
        XCTAssertEqual(try Data(contentsOf: file), content)
    }
    func testCollisionReportsPartialProgressAndRecovery() async throws {
        let base = try temporaryDirectory()
        let collision = base.appendingPathComponent("09-10-2026")
        let content = Data("Never overwrite".utf8)
        try content.write(to: collision)
        let result = await FolderService().create(try plan(), in: base)
        XCTAssertEqual(result.created, 1)
        XCTAssertEqual(result.failedName, "09-10-2026")
        XCTAssertEqual(try Data(contentsOf: collision), content)
        XCTAssertFalse(FileManager.default.fileExists(atPath: base.appendingPathComponent("09-17-2026").path))
        try FileManager.default.moveItem(at: collision, to: base.appendingPathComponent("saved-file"))
        let retry = await FolderService().create(try plan(), in: base)
        XCTAssertEqual(retry.created, 2)
        XCTAssertEqual(retry.existing, 1)
    }
    func testSymbolicLinkIsNeverAcceptedAsExistingFolder() async throws {
        let base = try temporaryDirectory()
        let elsewhere = try temporaryDirectory()
        try FileManager.default.createSymbolicLink(at: base.appendingPathComponent("09-03-2026"), withDestinationURL: elsewhere)
        let result = await FolderService().create(try plan(), in: base)
        XCTAssertEqual(result.processed, 0)
        XCTAssertNotNil(result.failureReason)
        XCTAssertTrue(try FileManager.default.contentsOfDirectory(atPath: elsewhere.path).isEmpty)
    }
    func testMissingOrFileDestination() async throws {
        let base = try temporaryDirectory()
        let file = base.appendingPathComponent("file")
        try Data().write(to: file)
        for url in [base.appendingPathComponent("missing"), file, URL(string: "https://example.com")!] {
            let result = await FolderService().create(try plan(), in: url)
            XCTAssertEqual(result.processed, 0)
            XCTAssertNotNil(result.failureReason)
            XCTAssertTrue(result.done)
            XCTAssertEqual(result.errorCode, "destination_unavailable")
        }
    }
    func testConcurrentCreatorsDoNotOverwrite() async throws {
        let base = try temporaryDirectory()
        let plan = try plan()
        async let one = FolderService().create(plan, in: base)
        async let two = FolderService().create(plan, in: base)
        let (a, b) = await (one, two)
        XCTAssertEqual(a.created + b.created, 3)
        XCTAssertEqual(a.existing + b.existing, 3)
        XCTAssertNil(a.failureReason)
        XCTAssertNil(b.failureReason)
    }
    func testCancellationAndRetryAfterPartialProgress() async throws {
        let base = try temporaryDirectory()
        let plan = try FolderPlan(start: CivilDate(folderName: "01-01-2020"), end: CivilDate(folderName: "12-31-2030"), weekday: .thursday)
        let result = await FolderService().create(plan, in: base) { update in
            if update.processed >= 25 { withUnsafeCurrentTask { $0?.cancel() } }
        }
        XCTAssertTrue(result.cancelled)
        XCTAssertEqual(result.created, 25)
        // Retry from a fresh, uncancelled task, just as a new UI operation does.
        let retry = await Task.detached { await FolderService().create(plan, in: base) }.value
        XCTAssertEqual(retry.existing, 25)
        XCTAssertEqual(retry.processed, plan.count)
    }
    func testEmptyPlanCreatesNothing() async throws {
        let base = try temporaryDirectory()
        let plan = try FolderPlan(start: CivilDate(folderName: "09-03-2026"), end: CivilDate(folderName: "09-03-2026"), weekday: .friday)
        let result = await FolderService().create(plan, in: base)
        XCTAssertEqual(result.processed, 0)
        XCTAssertTrue(try FileManager.default.contentsOfDirectory(atPath: base.path).isEmpty)
    }
}
