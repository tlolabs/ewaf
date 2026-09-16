import Foundation
import CEWAF

/// Gregorian date validated and calculated by Rust. Foundation is used only for
/// native time-zone and localized display adapters, never for domain arithmetic.
public struct CivilDate: Hashable, Comparable, Sendable, Codable, Identifiable {
    private let ordinal: Int32
    public var year: Int { Int(ewaf_date_component(ordinal, 0)) }
    public var month: Int { Int(ewaf_date_component(ordinal, 1)) }
    public var day: Int { Int(ewaf_date_component(ordinal, 2)) }
    public var id: String { folderName }
    public static var calendar: Calendar {
        var calendar = Calendar(identifier: .iso8601)
        calendar.timeZone = TimeZone(secondsFromGMT: 0)!
        calendar.locale = Locale(identifier: "en_US_POSIX")
        return calendar
    }
    private init(ordinal: Int32) throws {
        guard ewaf_date_component(ordinal, 0) != 0 else { throw PlanError.invalidDate }
        self.ordinal = ordinal
    }
    public init(year: Int, month: Int, day: Int) throws {
        guard let y = Int32(exactly: year), let m = UInt32(exactly: month), let d = UInt32(exactly: day) else { throw PlanError.invalidDate }
        try self.init(ordinal: ewaf_date_ordinal(y, m, d))
    }
    public init(date: Date, timeZone: TimeZone = .current) throws {
        let localSeconds = date.timeIntervalSince1970 + Double(timeZone.secondsFromGMT(for: date))
        guard let value = Int32(exactly: floor(localSeconds / 86400) + 719163) else { throw PlanError.invalidDate }
        try self.init(ordinal: value)
    }
    public init(folderName: String) throws {
        let bytes = Array(folderName.utf8)
        let value = bytes.withUnsafeBufferPointer { ewaf_date_parse($0.baseAddress, $0.count) }
        try self.init(ordinal: value)
    }
    public var folderName: String {
        var bytes = [UInt8](repeating: 0, count: 10)
        _ = bytes.withUnsafeMutableBufferPointer { ewaf_date_name(ordinal, $0.baseAddress) }
        return String(decoding: bytes, as: UTF8.self)
    }
    public var date: Date { Date(timeIntervalSince1970: Double(ordinal - 719163) * 86400) }
    public var weekday: Weekday { Weekday(rawValue: Int(ewaf_date_component(ordinal, 3)))! }
    public func adding(days: Int) -> CivilDate? {
        guard let days = Int32(exactly: days) else { return nil }
        let (value, overflow) = ordinal.addingReportingOverflow(days)
        return overflow ? nil : try? CivilDate(ordinal: value)
    }
    public static func < (lhs: Self, rhs: Self) -> Bool { lhs.ordinal < rhs.ordinal }
    private enum CodingKeys: String, CodingKey { case year, month, day }
    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        try self.init(year: values.decode(Int.self, forKey: .year), month: values.decode(Int.self, forKey: .month), day: values.decode(Int.self, forKey: .day))
    }
    public func encode(to encoder: Encoder) throws {
        var values = encoder.container(keyedBy: CodingKeys.self)
        try values.encode(year, forKey: .year); try values.encode(month, forKey: .month); try values.encode(day, forKey: .day)
    }
}

public enum Weekday: Int, CaseIterable, Sendable, Codable, Identifiable {
    case sunday = 1, monday, tuesday, wednesday, thursday, friday, saturday
    public var id: Int { rawValue }
    public static let displayOrder: [Weekday] = [.monday, .tuesday, .wednesday, .thursday, .friday, .saturday, .sunday]
    public func name(locale: Locale = .current) -> String {
        var calendar = CivilDate.calendar; calendar.locale = locale
        return calendar.weekdaySymbols[rawValue - 1]
    }
}
public enum PlanError: Error, LocalizedError, Equatable {
    case invalidDate, reversedRange
    public var errorDescription: String? {
        switch self {
        case .invalidDate: "Choose a valid date between the years 1 and 9999."
        case .reversedRange: "The end date must be on or after the start date."
        }
    }
}
