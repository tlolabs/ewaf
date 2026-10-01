import SwiftUI
import EWAFCore

struct SettingsView: View {
    @ObservedObject private var updater = ApplicationUpdater.shared
    @AppStorage("defaultWeekday") private var defaultWeekday = Weekday.thursday.rawValue
    var body: some View {
        Form {
            Toggle("Automatically check for updates", isOn: Binding(get: { updater.automaticChecks }, set: { updater.automaticChecks = $0 }))
                .disabled(!updater.configured)
            Picker("Default weekday", selection: $defaultWeekday) {
                ForEach(Weekday.displayOrder) { day in Text(day.name()).tag(day.rawValue) }
            }
            .accessibilityLabel("Default weekday")
            .accessibilityIdentifier("defaultWeekday")
            Text("Applies when you open a new window. Dates use your system time zone; folder names stay in MM-DD-YYYY format.")
                .foregroundStyle(.secondary)
        }
        .formStyle(.grouped)
        .frame(width: 430, height: 230)
    }
}
