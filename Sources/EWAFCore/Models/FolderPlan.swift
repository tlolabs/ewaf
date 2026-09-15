import Foundation

public struct FolderPlan: Sendable, Equatable {
    public let dates: [CivilDate]
    public var count: Int { dates.count }
    public var requiresConfirmation: Bool { count > 250 }

    public init(start: CivilDate, end: CivilDate, weekday: Weekday) throws {
        guard start <= end else { throw PlanError.reversedRange }
        let offset = (weekday.rawValue - start.weekday.rawValue + 7) % 7
        var dates: [CivilDate] = []
        var current = start.adding(days: offset)
        while let date = current, date <= end {
            try Task.checkCancellation()
            dates.append(date)
            current = date.adding(days: 7)
        }
        self.dates = dates
    }

    public func preview(matching search: String, limit: Int = 200) -> [CivilDate] {
        guard limit > 0 else { return [] }
        var matches: [CivilDate] = []
        for date in dates {
            if Task.isCancelled { return [] }
            if search.isEmpty || date.folderName.localizedStandardContains(search) {
                matches.append(date)
                if matches.count == limit { break }
            }
        }
        return matches
    }
}
