import Foundation
import Darwin

public struct CreationResult: Sendable, Equatable {
    public var created = 0
    public var existing = 0
    public var cancelled = false
    public var failedName: String?
    public var failureReason: String?
    public var processed: Int { created + existing }
    public var summary: String {
        let counts = "Created \(created.formatted()) folders. Already existed: \(existing.formatted())."
        if let failureReason { return "\(counts) \(failureReason)" }
        if cancelled { return "Canceled. \(counts) You can safely run the same range again." }
        return counts
    }
    public init() {}
}

/// Serialized filesystem operations execute outside the main actor. mkdirat is
/// atomic and relative to an open destination: renaming the parent cannot divert
/// the operation. Existing files and symbolic links are never followed/replaced.
public actor FolderService {
    public init() {}

    public func plan(start: CivilDate, end: CivilDate, weekday: Weekday) throws -> FolderPlan {
        try FolderPlan(start: start, end: end, weekday: weekday)
    }

    public func preview(_ plan: FolderPlan, matching search: String) -> [CivilDate] {
        plan.preview(matching: search)
    }

    public func create(_ plan: FolderPlan, in destination: URL,
                       progress: @Sendable (CreationResult) async -> Void = { _ in }) async -> CreationResult {
        var result = CreationResult()
        guard destination.isFileURL else {
            result.failureReason = "Choose a folder on this Mac or a mounted drive."
            return result
        }
        let access = destination.startAccessingSecurityScopedResource()
        defer { if access { destination.stopAccessingSecurityScopedResource() } }
        let descriptor = destination.withUnsafeFileSystemRepresentation { open($0!, O_RDONLY | O_DIRECTORY | O_CLOEXEC) }
        guard descriptor >= 0 else {
            result.failureReason = "The destination is unavailable. Choose an existing folder you can write to."
            return result
        }
        defer { close(descriptor) }
        for date in plan.dates {
            if Task.isCancelled { result.cancelled = true; break }
            let name = date.folderName
            if mkdirat(descriptor, name, 0o777) == 0 {
                result.created += 1
            } else {
                let code = errno
                var information = stat()
                if code == EEXIST, fstatat(descriptor, name, &information, AT_SYMLINK_NOFOLLOW) == 0,
                   information.st_mode & S_IFMT == S_IFDIR {
                    result.existing += 1
                } else {
                    result.failedName = name
                    switch code {
                    case EEXIST:
                        result.failureReason = "Stopped at \(name): a file or symbolic link already uses this name. Move it or choose another destination, then retry."
                    case EACCES, EPERM, EROFS:
                        result.failureReason = "Stopped at \(name): the destination does not allow changes. Check its permissions or choose another folder."
                    case ENOSPC, EDQUOT:
                        result.failureReason = "Stopped at \(name): the drive is full. Free some space, then retry."
                    default:
                        result.failureReason = "Stopped at \(name): the folder could not be created. Check that the drive is connected and writable, then retry."
                    }
                    break
                }
            }
            if result.processed.isMultiple(of: 25) { await progress(result) }
        }
        await progress(result)
        return result
    }
}
