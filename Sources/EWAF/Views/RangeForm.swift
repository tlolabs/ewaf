import SwiftUI
import EWAFCore

struct RangeForm: View {
    @Bindable var workspace: FolderWorkspace
    @State private var showExactDates = false
    private var localGregorian: Calendar {
        var value = CivilDate.calendar
        value.timeZone = TimeZone(secondsFromGMT: 0)!
        value.locale = .current
        return value
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Label("Every Week a Folder", systemImage: "folder.badge.plus")
                .font(.title2.weight(.semibold))
                .padding([.horizontal, .top], 20)
            Text("A little structure for every week.")
                .foregroundStyle(.secondary)
                .padding(.horizontal, 20)
                .padding(.top, 6)
            Form {
                Section("Date Range") {
                    dateControl("Start date", date: $workspace.start, identifier: "startDate")
                    dateControl("End date", date: $workspace.end, identifier: "endDate")
                    Button("Enter Exact Dates…") { showExactDates = true }
                        .accessibilityIdentifier("exactDates")
                    Picker("Weekday", selection: $workspace.weekday) {
                        ForEach(Weekday.displayOrder) { day in Text(day.name()).tag(day) }
                    }
                    .accessibilityIdentifier("weekday")
                    Text("Both dates are included. Folder names always use MM-DD-YYYY.")
                        .font(.callout).foregroundStyle(.secondary)
                }
                Section("Destination") {
                    if let destination = workspace.destination {
                        Label(destination.lastPathComponent, systemImage: "folder.fill")
                            .lineLimit(2)
                        Text(destination.path(percentEncoded: false))
                            .font(.caption).foregroundStyle(.secondary)
                            .textSelection(.enabled)
                    } else {
                        Text("Choose an existing folder on your Mac or a mounted drive.")
                            .foregroundStyle(.secondary)
                    }
                    Button(workspace.destination == nil ? "Choose Folder…" : "Change Folder…") { workspace.showImporter = true }
                        .accessibilityIdentifier("destinationButton")
                }
            }
            .formStyle(.grouped)
            .environment(\.calendar, localGregorian)
            .environment(\.timeZone, CivilDate.calendar.timeZone)
            .disabled(workspace.isCreating)
        }
        .sheet(isPresented: $showExactDates) { ExactDatesView(workspace: workspace) }
    }

    @ViewBuilder
    private func dateControl(_ label: String, date: Binding<CivilDate>, identifier: String) -> some View {
        if date.wrappedValue.year >= 1600 {
            DatePicker(label, selection: Binding(get: { date.wrappedValue.date }, set: { value in
                if let selected = try? CivilDate(date: value, timeZone: CivilDate.calendar.timeZone) { date.wrappedValue = selected }
            }), displayedComponents: .date)
            .accessibilityIdentifier(identifier)
        } else {
            LabeledContent(label, value: date.wrappedValue.folderName)
        }
    }
}
