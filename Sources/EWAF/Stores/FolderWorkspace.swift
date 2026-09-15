import SwiftUI
import EWAFCore

@MainActor @Observable
final class FolderWorkspace {
    var start = try! CivilDate(date: Date())
    var end = try! CivilDate(date: Date())
    var weekday: Weekday
    var search = ""
    var destination: URL?
    var plan: FolderPlan?
    var preview: [CivilDate] = []
    var validation: String?
    var status = "Choose a date range and weekday, then review your folders."
    var isPlanning = false
    var isCreating = false
    var progress = CreationResult()
    var showImporter = false
    var showConfirmation = false
    var showResult = false
    private var createAfterSelection = false
    private var operation: Task<Void, Never>?
    private let service = FolderService()

    init(defaultWeekday: Int) {
        weekday = Weekday(rawValue: defaultWeekday) ?? .thursday
    }

    var canCreate: Bool { !isCreating && !isPlanning && (plan?.count ?? 0) > 0 }

    func refresh() async {
        isPlanning = true
        plan = nil
        validation = nil
        preview = []
        do {
            let next = try await service.plan(start: start, end: end, weekday: weekday)
            try Task.checkCancellation()
            let rows = await service.preview(next, matching: search)
            try Task.checkCancellation()
            plan = next
            preview = rows
            isPlanning = false
        } catch is CancellationError {
            // The next .task owns the replacement plan and loading state.
        } catch {
            validation = error.localizedDescription
            isPlanning = false
        }
    }

    func filterPreview() async {
        guard let plan else { return }
        let rows = await service.preview(plan, matching: search)
        guard !Task.isCancelled else { return }
        preview = rows
    }

    func requestCreation() {
        guard canCreate else { return }
        if plan?.requiresConfirmation == true { showConfirmation = true }
        else { selectDestinationOrCreate() }
    }

    func selectDestinationOrCreate() {
        if destination == nil { createAfterSelection = true; showImporter = true }
        else { create() }
    }

    func selectedDestination(_ result: Result<[URL], Error>) {
        defer { createAfterSelection = false }
        switch result {
        case .success(let urls):
            if let first = urls.first { destination = first; if createAfterSelection { create() } }
        case .failure(let error):
            if (error as NSError).code != NSUserCancelledError {
                status = "The folder could not be opened. Please choose it again."
            }
        }
    }

    func create() {
        guard canCreate, let plan, let destination else { return }
        isCreating = true
        progress = CreationResult()
        status = "Creating \(plan.count.formatted()) folders…"
        operation = Task {
            let result = await service.create(plan, in: destination) { [weak self] update in
                await self?.updateProgress(update, total: plan.count)
            }
            progress = result
            status = result.summary
            isCreating = false
            showResult = true
            operation = nil
        }
    }

    private func updateProgress(_ result: CreationResult, total: Int) {
        progress = result
        status = "Processed \(result.processed.formatted()) of \(total.formatted()) folders."
    }

    func cancel() { operation?.cancel() }
}
