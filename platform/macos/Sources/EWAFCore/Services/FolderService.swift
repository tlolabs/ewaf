import Foundation

public struct CreationResult: Sendable, Equatable, Decodable {
    public var created = 0
    public var existing = 0
    public var cancelled = false
    public var failedName: String?
    public var failureReason: String?
    public var errorCode: String?
    public var done = false
    public var processed: Int { created + existing }
    public var summary: String {
        let counts = "Created \(created.formatted()) folders. Already existed: \(existing.formatted())."
        if let failureReason { return "\(counts) \(failureReason)" }
        if cancelled { return "Canceled. \(counts) You can safely run the same range again." }
        return counts
    }
    public init() {}
}

/// Native scheduling and security-scoped access; all filesystem policy lives in Rust.
public actor FolderService {
    public init() {}
    public func plan(start: CivilDate, end: CivilDate, weekday: Weekday) throws -> FolderPlan { try FolderPlan(start: start, end: end, weekday: weekday) }
    public func preview(_ plan: FolderPlan, matching search: String) -> [CivilDate] { plan.preview(matching: search) }
    public func create(_ plan: FolderPlan, in destination: URL, progress: @Sendable (CreationResult) async -> Void = { _ in }) async -> CreationResult {
        var result = CreationResult()
        guard destination.isFileURL else {
            result.failureReason = "Choose a folder on this Mac or a mounted drive."
            result.errorCode = "destination_unavailable"
            result.done = true
            return result
        }
        let access = destination.startAccessingSecurityScopedResource()
        defer { if access { destination.stopAccessingSecurityScopedResource() } }
        struct Handle: Decodable, Sendable { let handle: UInt64 }
        struct Empty: Decodable {}
        do {
            let operation: Handle = try RustCore.call(["op": "begin", "plan": plan.request, "destination": destination.path(percentEncoded: false)])
            defer { _ = try? RustCore.call(["op": "release", "handle": operation.handle], as: Empty.self) }
            result = await withTaskCancellationHandler {
                var latest = CreationResult()
                do {
                    repeat {
                        latest = try RustCore.call(["op": "step", "handle": operation.handle, "cancelled": Task.isCancelled])
                        await progress(latest)
                        await Task.yield()
                    } while !latest.done
                } catch { latest.failureReason = error.localizedDescription; latest.errorCode = (error as? CoreFailure)?.code; latest.done = true }
                return latest
            } onCancel: {
                _ = try? RustCore.call(["op": "cancel", "handle": operation.handle], as: Empty.self)
            }
        } catch { result.failureReason = error.localizedDescription; result.errorCode = (error as? CoreFailure)?.code; result.done = true }
        return result
    }
}
