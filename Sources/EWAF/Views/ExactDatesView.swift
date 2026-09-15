import SwiftUI
import EWAFCore

/// An explicit Gregorian entry path also supports dates that the system's
/// historical DatePicker interprets using a Julian/Gregorian cutover.
struct ExactDatesView: View {
    let workspace: FolderWorkspace
    @Environment(\.dismiss) private var dismiss
    @State private var start: String
    @State private var end: String
    @State private var error: String?

    init(workspace: FolderWorkspace) {
        self.workspace = workspace
        _start = State(initialValue: workspace.start.folderName)
        _end = State(initialValue: workspace.end.folderName)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            Text("Enter Exact Dates").font(.headline)
            Text("Use MM-DD-YYYY, including four digits for the year. Both dates are included.")
                .foregroundStyle(.secondary)
            Form {
                TextField("Start date", text: $start).accessibilityIdentifier("exactStart")
                TextField("End date", text: $end).accessibilityIdentifier("exactEnd")
            }
            if let error { Label(error, systemImage: "exclamationmark.triangle").foregroundStyle(.red).accessibilityIdentifier("exactDateError") }
            HStack {
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }.keyboardShortcut(.cancelAction)
                Button("Apply Dates") { apply() }.keyboardShortcut(.defaultAction)
            }
        }
        .padding(24)
        .frame(width: 430)
    }

    private func apply() {
        do {
            let first = try CivilDate(folderName: start.trimmingCharacters(in: .whitespacesAndNewlines))
            let last = try CivilDate(folderName: end.trimmingCharacters(in: .whitespacesAndNewlines))
            guard first <= last else { throw PlanError.reversedRange }
            workspace.start = first
            workspace.end = last
            dismiss()
        } catch { self.error = error.localizedDescription }
    }
}
