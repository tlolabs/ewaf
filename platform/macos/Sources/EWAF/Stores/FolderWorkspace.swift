import SwiftUI
import EWAFCore

@MainActor @Observable
final class FolderWorkspace {
    static var activeCreations = 0
    var start = try! CivilDate(date: Date())
    var end = try! CivilDate(date: Date())
    var weekday: Weekday
    var search = ""
    var isSearching = false
    var selectedName: String?
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
    private var planGeneration = UUID()
    private let previewRows: @Sendable (FolderPlan, String) async -> [CivilDate]
    var announcementObserver: ((String) -> Void)?

    var selectedOrFirstName: String? {
        selectedName ?? preview.first?.folderName
    }

    init(defaultWeekday: Int, previewRows: (@Sendable (FolderPlan, String) async -> [CivilDate])? = nil) {
        weekday = Weekday(rawValue: defaultWeekday) ?? .thursday
        let service = self.service
        self.previewRows = previewRows ?? { plan, query in await service.preview(plan, matching: query) }
    }

    var canCreate: Bool { !isCreating && !isPlanning && (plan?.count ?? 0) > 0 }

    func reconcileSelection() {
        if let current = selectedName {
            if !preview.contains(where: { $0.folderName == current }) {
                selectedName = nil
            }
        }
    }

    func refresh() async {
        let generation = UUID()
        planGeneration = generation
        let hadValidation = validation != nil
        isPlanning = true
        plan = nil
        validation = nil
        preview = []
        do {
            let next = try await service.plan(start: start, end: end, weekday: weekday)
            try Task.checkCancellation()
            guard generation == planGeneration else { return }
            // Publish the validated plan independently of search. A query change
            // while planning must not strand the workspace in a loading state.
            plan = next
            isPlanning = false
            await filterPreview()
            guard !Task.isCancelled, generation == planGeneration else { return }
            if hadValidation {
                announceAccessibility("Valid date range restored. \(next.count) folders planned.")
            }
        } catch is CancellationError {
            // The next .task owns the replacement plan and loading state.
        } catch {
            guard generation == planGeneration else { return }
            validation = error.localizedDescription
            reconcileSelection()
            isPlanning = false
            announceAccessibility(error.localizedDescription)
        }
    }

    func filterPreview() async {
        guard let currentPlan = plan else { return }
        let generation = planGeneration
        let query = search
        let rows = await previewRows(currentPlan, query)
        guard !Task.isCancelled, generation == planGeneration, query == search else { return }
        preview = rows
        reconcileSelection()
        if rows.isEmpty && !query.isEmpty {
            announceAccessibility("No folders found matching \(query).")
        }
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
        Self.activeCreations += 1
        progress = CreationResult()
        status = "Creating \(plan.count.formatted()) folders…"
        announceAccessibility(status)
        operation = Task {
            let result = await service.create(plan, in: destination) { [weak self] update in
                await self?.updateProgress(update, total: plan.count)
            }
            progress = result
            status = result.summary
            announceCreationResult(result)
            isCreating = false
            Self.activeCreations -= 1
            showResult = true
            operation = nil
        }
    }

    private func updateProgress(_ result: CreationResult, total: Int) {
        progress = result
        status = "Processed \(result.processed.formatted()) of \(total.formatted()) folders."
    }

    func announceCreationResult(_ result: CreationResult) {
        // Task cancellation can race with an already completed Rust operation.
        // Only the backend result establishes whether cancellation took effect.
        if result.failureReason != nil {
            announceAccessibility("Folder creation stopped: \(result.summary)")
        } else {
            announceAccessibility(result.summary)
        }
    }

    func cancel() {
        guard isCreating else { return }
        announceAccessibility("Canceling folder creation…")
        operation?.cancel()
    }

    func announceAccessibility(_ text: String) {
        announcementObserver?(text)
        NSAccessibility.post(
            element: NSApp as Any,
            notification: .announcementRequested,
            userInfo: [
                .announcement: text,
                .priority: NSAccessibilityPriorityLevel.high.rawValue
            ]
        )
    }
}
