import SwiftUI
import EWAFCore

struct PreviewView: View {
    @Bindable var workspace: FolderWorkspace
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Text("Folder Preview").font(.headline)
                Spacer()
                if let count = workspace.plan?.count {
                    Text("\(count.formatted()) folders").foregroundStyle(.secondary)
                        .accessibilityLabel("Planned folders: \(count.formatted())")
                        .accessibilityIdentifier("folderCount")
                }
            }.padding()
            if workspace.isPlanning {
                ProgressView("Calculating dates…").frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if let validation = workspace.validation {
                ContentUnavailableView("Check Your Dates", systemImage: "calendar.badge.exclamationmark", description: Text(validation)).frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if workspace.plan?.count == 0 {
                ContentUnavailableView("No Matching Dates", systemImage: "calendar", description: Text("Choose a wider range or another weekday.")).frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                List(workspace.preview) { date in
                    Label(date.folderName, systemImage: "folder")
                        .monospacedDigit()
                        .accessibilityLabel("Folder \(date.folderName)")
                        .accessibilityHint("Drag or right-click to copy folder name")
                        .accessibilityIdentifier("preview-\(date.folderName)")
                        .help("Drag or right-click to copy folder name")
                        .onTapGesture { workspace.selectedName = date.folderName }
                        .contextMenu {
                            ShareLink(item: date.folderName) { Label("Share Folder Name", systemImage: "square.and.arrow.up") }
                            Button("Copy Folder Name") {
                                NSPasteboard.general.clearContents()
                                NSPasteboard.general.setString(date.folderName, forType: .string)
                            }
                        }
                        .draggable(date.folderName)
                }
                .searchable(text: $workspace.search, placement: .automatic, prompt: "Find a folder date")
                .overlay {
                    if workspace.preview.isEmpty {
                        ContentUnavailableView.search(text: workspace.search)
                            .accessibilityElement(children: .combine)
                            .accessibilityLabel("No folders found matching \(workspace.search)")
                            .accessibilityIdentifier("emptySearch")
                    }
                }
                Text("Preview shows up to 200 matches in date order. Existing folders will be kept.")
                    .font(.caption).foregroundStyle(.secondary).padding()
            }
        }
    }
}
