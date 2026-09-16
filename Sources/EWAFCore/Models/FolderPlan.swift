import Foundation

public struct FolderPlan: Sendable, Equatable {
    public let dates: [CivilDate]
    public let requiresConfirmation: Bool
    let start: CivilDate
    let end: CivilDate
    let weekday: Weekday
    var request: [String: Any] { ["start": start.folderName, "end": end.folderName, "weekday": weekday.rawValue] }
    public var count: Int { dates.count }
    private struct Response: Decodable { let dates: [String]; let requiresConfirmation: Bool }
    public init(start: CivilDate, end: CivilDate, weekday: Weekday) throws {
        try Task.checkCancellation()
        let response: Response = try RustCore.call(["op": "plan", "plan": ["start": start.folderName, "end": end.folderName, "weekday": weekday.rawValue], "all": true])
        try Task.checkCancellation()
        self.start = start; self.end = end; self.weekday = weekday
        dates = try response.dates.map { try CivilDate(folderName: $0) }
        requiresConfirmation = response.requiresConfirmation
    }
    public func preview(matching search: String, limit: Int = 200) -> [CivilDate] {
        guard limit > 0, !Task.isCancelled else { return [] }
        // A valid immutable plan cannot produce a domain validation error.
        guard let response = try? RustCore.call(["op": "plan", "plan": request, "search": search, "limit": limit], as: Response.self), !Task.isCancelled else { return [] }
        return response.dates.compactMap { try? CivilDate(folderName: $0) }
    }
}
