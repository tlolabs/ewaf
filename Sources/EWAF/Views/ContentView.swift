import SwiftUI
import UniformTypeIdentifiers
import EWAFCore

struct ContentView: View {
    @State private var workspace: FolderWorkspace
    @SceneStorage("rangeStart") private var savedStart = ""
    @SceneStorage("rangeEnd") private var savedEnd = ""
    @SceneStorage("rangeWeekday") private var savedWeekday = 0
    @Environment(\.openURL) private var openURL

    init(defaultWeekday: Int) {
        _workspace = State(initialValue: FolderWorkspace(defaultWeekday: defaultWeekday))
    }

    private var request: String { "\(workspace.start)-\(workspace.end)-\(workspace.weekday.rawValue)" }

    var body: some View {
        @Bindable var workspace = workspace
        VStack(spacing: 0) {
            HSplitView {
                RangeForm(workspace: workspace)
                    .frame(minWidth: 280, idealWidth: 310, maxWidth: 400)
                PreviewView(workspace: workspace)
                    .frame(minWidth: 280, maxWidth: .infinity, maxHeight: .infinity)
            }
            Divider()
            footer
        }
        .frame(minWidth: 640, minHeight: 480)
        .toolbar {
            ToolbarItem {
                Button { workspace.showImporter = true } label: { Label("Choose Destination", systemImage: "folder") }
                    .help("Choose the folder where weekly folders will be created (⌘O)")
                    .keyboardShortcut("o")
                    .disabled(workspace.isCreating)
                    .accessibilityIdentifier("chooseDestination")
            }
            ToolbarItem(placement: .primaryAction) {
                Button("Create Folders", systemImage: "folder.badge.plus") { workspace.requestCreation() }
                    .keyboardShortcut(.return, modifiers: .command)
                    .disabled(!workspace.canCreate)
                    .accessibilityIdentifier("createFolders")
            }
        }
        .fileImporter(isPresented: $workspace.showImporter, allowedContentTypes: [.folder], allowsMultipleSelection: false) {
            workspace.selectedDestination($0)
        }
        .confirmationDialog("Create \(workspace.plan?.count ?? 0) folders?", isPresented: $workspace.showConfirmation, titleVisibility: .visible) {
            Button("Continue") { workspace.selectDestinationOrCreate() }
        } message: {
            Text("This is a large operation. Existing folders and their contents will be preserved. You can cancel while it runs.")
        }
        .alert(workspace.progress.failureReason == nil ? (workspace.progress.cancelled ? "Creation Canceled" : "Complete") : "Folder Creation Stopped", isPresented: $workspace.showResult) {
            Button("OK", role: .cancel) {}
        } message: { Text(workspace.status) }
        .onAppear {
            if let start = try? CivilDate(folderName: savedStart) { workspace.start = start }
            if let end = try? CivilDate(folderName: savedEnd) { workspace.end = end }
            if let weekday = Weekday(rawValue: savedWeekday) { workspace.weekday = weekday }
        }
        .onChange(of: workspace.start) { _, value in savedStart = value.folderName }
        .onChange(of: workspace.end) { _, value in savedEnd = value.folderName }
        .onChange(of: workspace.weekday) { _, value in savedWeekday = value.rawValue }
        .task(id: request) { await workspace.refresh() }
        .task(id: workspace.search) { await workspace.filterPreview() }
        .onDisappear { workspace.cancel() }
        .focusedSceneValue(\.folderWorkspace, workspace)
    }

    private var footer: some View {
        HStack(alignment: .center, spacing: 16) {
            if workspace.isCreating {
                ProgressView(value: Double(workspace.progress.processed), total: Double(max(1, workspace.plan?.count ?? 1)))
                    .frame(width: 100)
                    .accessibilityLabel("Folder creation progress")
            }
            Text(workspace.status)
                .font(.callout)
                .textSelection(.enabled)
                .accessibilityIdentifier("operationStatus")
                .frame(maxWidth: .infinity, alignment: .leading)
            if workspace.isCreating {
                Button("Cancel") { workspace.cancel() }
                    .keyboardShortcut(".", modifiers: .command)
            } else if let destination = workspace.destination {
                Button("Open Folder") { openURL(destination) }
                    .help("Open the destination in Finder")
            }
        }
        .padding()
    }
}

struct WorkspaceKey: FocusedValueKey { typealias Value = FolderWorkspace }
extension FocusedValues {
    var folderWorkspace: FolderWorkspace? {
        get { self[WorkspaceKey.self] }
        set { self[WorkspaceKey.self] = newValue }
    }
}
