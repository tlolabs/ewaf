import XCTest
@testable import EWAFCore

final class RangeCoverageTests: XCTestCase {
    func testMaximumSupportedRange() throws {
        let plan = try FolderPlan(start: CivilDate(folderName: "01-01-0001"), end: CivilDate(folderName: "12-31-9999"), weekday: .thursday)
        XCTAssertEqual(plan.count, 521_723)
        XCTAssertEqual(plan.dates.first?.folderName, "01-04-0001")
        XCTAssertEqual(plan.dates.last?.folderName, "12-30-9999")
    }
}
