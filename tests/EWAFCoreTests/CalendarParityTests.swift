import XCTest
@testable import EWAFCore

final class CalendarParityTests: XCTestCase {
    private struct Fixture: Decodable {
        let start: String
        let end: String
        let weekday: Int
        let expected: [String]
    }

    func testReferenceFixturesMatchExactly() throws {
        let url = try XCTUnwrap(Bundle.module.url(forResource: "calendar-dates", withExtension: "json", subdirectory: "Fixtures"))
        let fixtures = try JSONDecoder().decode([Fixture].self, from: Data(contentsOf: url))
        XCTAssertEqual(fixtures.count, 112)
        for fixture in fixtures {
            let plan = try FolderPlan(start: CivilDate(folderName: fixture.start), end: CivilDate(folderName: fixture.end), weekday: XCTUnwrap(Weekday(rawValue: fixture.weekday)))
            XCTAssertEqual(plan.dates.map(\.folderName), fixture.expected, "\(fixture.start)–\(fixture.end)")
        }
    }

    func testMaximumRangeMatchesReferenceCount() throws {
        let plan = try FolderPlan(start: CivilDate(folderName: "01-01-0001"), end: CivilDate(folderName: "12-31-9999"), weekday: .thursday)
        XCTAssertEqual(plan.count, 521_723)
        XCTAssertEqual(plan.dates.first?.folderName, "01-04-0001")
        XCTAssertEqual(plan.dates.last?.folderName, "12-30-9999")
    }
}
