import Foundation

public struct FolderPlan: Sendable, Equatable {
    public let count: Int
    public let requiresConfirmation: Bool
    public let start: CivilDate
    public let end: CivilDate
    public let weekday: Weekday
    var request: [String: Any] { ["start": start.folderName, "end": end.folderName, "weekday": weekday.rawValue] }
    private struct Response: Decodable { let count: Int?; let dates: [String]; let requiresConfirmation: Bool }
    public var dates: [CivilDate] {
        guard let response = try? RustCore.call(["op": "plan", "plan": request, "all": true], as: Response.self) else { return [] }
        var result = [CivilDate]()
        result.reserveCapacity(response.dates.count)
        for dateString in response.dates {
            if let date = try? CivilDate(folderName: dateString) {
                result.append(date)
            }
        }
        return result
    }
    public init(start: CivilDate, end: CivilDate, weekday: Weekday) throws {
        try Task.checkCancellation()
        let response: Response = try RustCore.call(["op": "plan", "plan": ["start": start.folderName, "end": end.folderName, "weekday": weekday.rawValue]])
        try Task.checkCancellation()
        self.start = start; self.end = end; self.weekday = weekday
        count = response.count ?? 0
        requiresConfirmation = response.requiresConfirmation
    }
    public func preview(matching search: String, limit: Int = 200) -> [CivilDate] {
        guard limit > 0, !Task.isCancelled else { return [] }
        // A valid immutable plan cannot produce a domain validation error.
        guard let response = try? RustCore.call(["op": "plan", "plan": request, "search": search, "limit": limit], as: Response.self), !Task.isCancelled else { return [] }
        return response.dates.compactMap { try? CivilDate(folderName: $0) }
    }
    public static func == (lhs: FolderPlan, rhs: FolderPlan) -> Bool {
        lhs.start == rhs.start && lhs.end == rhs.end && lhs.weekday == rhs.weekday && lhs.count == rhs.count && lhs.requiresConfirmation == rhs.requiresConfirmation
    }
}
