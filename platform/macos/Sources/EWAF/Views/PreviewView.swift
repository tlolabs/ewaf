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
                        .accessibilityIdentifier("preview-\(date.folderName)")
                        .contextMenu {
                            ShareLink(item: date.folderName) { Label("Share Folder Name", systemImage: "square.and.arrow.up") }
                        }
                        .draggable(date.folderName)
                }
                .searchable(text: $workspace.search, placement: .automatic, prompt: "Find a folder date")
                .overlay {
                    if workspace.preview.isEmpty {
                        ContentUnavailableView.search(text: workspace.search)
                            .accessibilityElement(children: .combine)
                            .accessibilityIdentifier("emptySearch")
                    }
                }
                Text("Preview shows up to 200 matches in date order. Existing folders will be kept.")
                    .font(.caption).foregroundStyle(.secondary).padding()
            }
        }
    }
}
