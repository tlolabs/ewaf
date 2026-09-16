import XCTest
@testable import EWAFCore

final class CalendarTests: XCTestCase {
    func date(_ value: String) throws -> CivilDate { try CivilDate(folderName: value) }
    func names(_ start: String, _ end: String, _ day: Weekday) throws -> [String] {
        try FolderPlan(start: date(start), end: date(end), weekday: day).dates.map(\.folderName)
    }

    func testInclusiveBoundaries() throws {
        XCTAssertEqual(try names("09-03-2026", "09-17-2026", .thursday), ["09-03-2026", "09-10-2026", "09-17-2026"])
    }
    func testAdvancesAndEmptyRange() throws {
        XCTAssertEqual(try names("09-04-2026", "09-10-2026", .thursday), ["09-10-2026"])
        XCTAssertEqual(try names("09-04-2026", "09-05-2026", .thursday), [])
    }
    func testReversedRange() {
        XCTAssertThrowsError(try names("09-05-2026", "09-04-2026", .thursday)) { XCTAssertEqual($0 as? PlanError, .reversedRange) }
    }
    func testLeapYearAndCenturyRules() throws {
        XCTAssertEqual(try names("02-22-2024", "03-07-2024", .thursday), ["02-22-2024", "02-29-2024", "03-07-2024"])
        XCTAssertNoThrow(try date("02-29-2000"))
        for value in ["02-29-1900", "02-29-2100", "02-29-2025", "02-30-2024", "13-01-2024", "00-01-2024", "01-00-2024", "01-01-0000"] {
            XCTAssertThrowsError(try date(value), value)
        }
    }
    func testUnsafeAndMalformedNamesAreRejected() {
        for value in ["", "..", "../09-03-2026", "/09-03-2026", "09-03-2026/child", "09-03-2026\0", "9-3-2026", "09-03-26", " 09-03-2026", "０９-０３-２０２６"] {
            XCTAssertThrowsError(try date(value), value)
        }
    }
    func testFullSupportedBoundaries() throws {
        XCTAssertEqual(try date("01-01-0001").weekday, .monday)
        XCTAssertEqual(try names("12-31-9999", "12-31-9999", .friday), ["12-31-9999"])
        XCTAssertEqual(try names("12-31-9999", "12-31-9999", .saturday), [])
        XCTAssertNil(try date("12-31-9999").adding(days: 1))
        XCTAssertNil(try date("01-01-0001").adding(days: -1))
    }
    func testProlepticGregorianCompatibility() throws {
        // Python uses the Gregorian calendar even before the 1582 reform.
        XCTAssertEqual(try names("10-01-1582", "10-31-1582", .sunday), ["10-03-1582", "10-10-1582", "10-17-1582", "10-24-1582", "10-31-1582"])
        XCTAssertThrowsError(try date("02-29-1500"))
    }
    func testDSTDoesNotSkipOrDuplicateWeeks() throws {
        XCTAssertEqual(try names("03-01-2026", "03-22-2026", .sunday), ["03-01-2026", "03-08-2026", "03-15-2026", "03-22-2026"])
        XCTAssertEqual(try names("10-25-2026", "11-08-2026", .sunday), ["10-25-2026", "11-01-2026", "11-08-2026"])
        XCTAssertEqual(try names("12-23-2011", "01-06-2012", .friday), ["12-23-2011", "12-30-2011", "01-06-2012"])
    }
    func testLocalDateExtractionAcrossTimeZones() throws {
        let instant = ISO8601DateFormatter().date(from: "2026-03-08T07:30:00Z")!
        XCTAssertEqual(try CivilDate(date: instant, timeZone: TimeZone(identifier: "America/Los_Angeles")!).folderName, "03-07-2026")
        XCTAssertEqual(try CivilDate(date: instant, timeZone: TimeZone(identifier: "Pacific/Kiritimati")!).folderName, "03-08-2026")
        for zone in ["America/Los_Angeles", "Europe/London", "Australia/Lord_Howe", "Asia/Kathmandu"] {
            var calendar = CivilDate.calendar
            calendar.timeZone = TimeZone(identifier: zone)!
            let local = calendar.date(from: DateComponents(year: 2026, month: 11, day: 1, hour: 12))!
            XCTAssertEqual(try CivilDate(date: local, timeZone: calendar.timeZone).folderName, "11-01-2026")
        }
    }
    func testLocaleIndependentNamesAndLocalizedWeekdays() throws {
        XCTAssertEqual(try date("09-03-2026").folderName, "09-03-2026")
        XCTAssertEqual(Weekday.thursday.name(locale: Locale(identifier: "fr_FR")), "jeudi")
    }
    func testChronologicalSortingAcrossYears() throws {
        let dates = try ["01-01-2027", "12-31-2026", "02-01-2026"].map(date).sorted()
        XCTAssertEqual(dates.map(\.folderName), ["02-01-2026", "12-31-2026", "01-01-2027"])
    }
    func testSearchAndPreviewLimit() throws {
        let plan = try FolderPlan(start: date("01-01-2020"), end: date("12-31-2030"), weekday: .thursday)
        XCTAssertEqual(plan.preview(matching: "").count, 200)
        XCTAssertTrue(plan.preview(matching: "2026").allSatisfy { $0.year == 2026 })
        XCTAssertEqual(plan.preview(matching: "nonsense"), [])
        XCTAssertEqual(plan.preview(matching: "", limit: 0), [])
    }
    func testLargeOperationConfirmationThreshold() throws {
        let start = try date("01-01-2026")
        XCTAssertFalse(try FolderPlan(start: start, end: start.adding(days: 7 * 249)!, weekday: .thursday).requiresConfirmation)
        XCTAssertTrue(try FolderPlan(start: start, end: start.adding(days: 7 * 250)!, weekday: .thursday).requiresConfirmation)
    }
    func testCodableRoundtripAndInvalidData() throws {
        let value = try date("02-29-2024")
        XCTAssertEqual(try JSONDecoder().decode(CivilDate.self, from: JSONEncoder().encode(value)), value)
        XCTAssertThrowsError(try JSONDecoder().decode(CivilDate.self, from: Data(#"{"year":2025,"month":2,"day":29}"#.utf8)))
    }
    func testAllWeekdaysProduceWeeklyOrderedResults() throws {
        for weekday in Weekday.allCases {
            let plan = try FolderPlan(start: date("01-01-2024"), end: date("12-31-2026"), weekday: weekday)
            XCTAssertEqual(plan.dates, plan.dates.sorted())
            XCTAssertEqual(Set(plan.dates).count, plan.count)
            XCTAssertTrue(plan.dates.allSatisfy { $0.weekday == weekday })
            for pair in zip(plan.dates, plan.dates.dropFirst()) { XCTAssertEqual(pair.0.adding(days: 7), pair.1) }
        }
    }
    func testLocalizedSearchCompatibility() throws {
        let plan = try FolderPlan(start: date("09-03-2026"), end: date("09-17-2026"), weekday: .thursday)
        for query in ["０９", "０９－１０", "²", "①", "٠٩", "09–10", "09-10", "２０２６", "\u{00ad}2026", " 2026", "2026\n", "09\u{200b}-10"] {
            let expected = plan.dates.filter { $0.folderName.localizedStandardContains(query) }
            XCTAssertEqual(plan.preview(matching: query), expected, "Search compatibility for \(query.debugDescription)")
        }
    }

}
