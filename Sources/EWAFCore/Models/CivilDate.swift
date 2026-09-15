import Foundation

/// A Gregorian date, with no time of day or time zone. Folder names are a stable
/// interchange format; locale and DST must never change them.
public struct CivilDate: Hashable, Comparable, Sendable, Codable, Identifiable {
    public let year: Int
    public let month: Int
    public let day: Int
    public var id: String { folderName }

    // Foundation Calendar has a historical Julian/Gregorian cutover. The public
    // DateFormatter cutover API supplies proleptic dates before 1600, matching
    // Python datetime. Modern dates retain the fast Calendar path.
    private static let historicalFormatter = formatter(timeZone: TimeZone(secondsFromGMT: 0)!)
    private static let modernBoundary = Date(timeIntervalSince1970: -11_676_096_000)
    private static func formatter(timeZone: TimeZone) -> DateFormatter {
        let value = DateFormatter()
        value.locale = Locale(identifier: "en_US_POSIX")
        value.calendar = Calendar(identifier: .gregorian)
        value.timeZone = timeZone
        value.dateFormat = "MM-dd-yyyy G"
        value.isLenient = false
        value.gregorianStartDate = Date(timeIntervalSince1970: -1_000_000_000_000)
        return value
    }

    private static func absoluteDate(year: Int, month: Int, day: Int) -> Date? {
        if year < 1600 {
            let text = String(format: "%02d-%02d-%04d AD", month, day, year)
            guard let date = historicalFormatter.date(from: text), historicalFormatter.string(from: date) == text else { return nil }
            return date
        }
        guard let date = calendar.date(from: DateComponents(year: year, month: month, day: day)),
              calendar.dateComponents([.year, .month, .day], from: date) == DateComponents(year: year, month: month, day: day) else { return nil }
        return date
    }

    public static var calendar: Calendar {
        var calendar = Calendar(identifier: .iso8601)
        calendar.timeZone = TimeZone(secondsFromGMT: 0)!
        calendar.locale = Locale(identifier: "en_US_POSIX")
        return calendar
    }

    public init(year: Int, month: Int, day: Int) throws {
        guard (1...9999).contains(year), (1...12).contains(month), (1...31).contains(day),
              Self.absoluteDate(year: year, month: month, day: day) != nil
        else { throw PlanError.invalidDate }
        self.year = year
        self.month = month
        self.day = day
    }

    public init(date: Date, timeZone: TimeZone = .current) throws {
        if date < Self.modernBoundary {
            let formatter = timeZone.secondsFromGMT(for: date) == 0 ? Self.historicalFormatter : Self.formatter(timeZone: timeZone)
            let value = formatter.string(from: date)
            guard value.hasSuffix(" AD") else { throw PlanError.invalidDate }
            try self.init(folderName: String(value.dropLast(3)))
            return
        }
        var calendar = Self.calendar
        calendar.timeZone = timeZone
        let parts = calendar.dateComponents([.year, .month, .day], from: date)
        try self.init(year: parts.year!, month: parts.month!, day: parts.day!)
    }

    public init(folderName: String) throws {
        let parts = folderName.split(separator: "-", omittingEmptySubsequences: false)
        guard parts.count == 3, parts[0].count == 2, parts[1].count == 2, parts[2].count == 4,
              folderName.utf8.allSatisfy({ $0 == 45 || (48...57).contains($0) }),
              let month = Int(parts[0]), let day = Int(parts[1]), let year = Int(parts[2])
        else { throw PlanError.invalidDate }
        try self.init(year: year, month: month, day: day)
    }

    public var folderName: String { String(format: "%02d-%02d-%04d", month, day, year) }
    public var date: Date { Self.absoluteDate(year: year, month: month, day: day)! }
    public var weekday: Weekday { Weekday(rawValue: Self.calendar.component(.weekday, from: date))! }

    public func adding(days: Int) -> CivilDate? {
        guard let date = Self.calendar.date(byAdding: .day, value: days, to: date) else { return nil }
        return try? CivilDate(date: date, timeZone: Self.calendar.timeZone)
    }

    public static func < (lhs: Self, rhs: Self) -> Bool {
        (lhs.year, lhs.month, lhs.day) < (rhs.year, rhs.month, rhs.day)
    }

    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        try self.init(year: values.decode(Int.self, forKey: .year), month: values.decode(Int.self, forKey: .month), day: values.decode(Int.self, forKey: .day))
    }
}

public enum Weekday: Int, CaseIterable, Sendable, Codable, Identifiable {
    case sunday = 1, monday, tuesday, wednesday, thursday, friday, saturday
    public var id: Int { rawValue }
    public static let displayOrder: [Weekday] = [.monday, .tuesday, .wednesday, .thursday, .friday, .saturday, .sunday]
    public func name(locale: Locale = .current) -> String {
        var calendar = CivilDate.calendar
        calendar.locale = locale
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
