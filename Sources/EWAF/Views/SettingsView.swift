import SwiftUI
import EWAFCore

struct SettingsView: View {
    @AppStorage("defaultWeekday") private var defaultWeekday = Weekday.thursday.rawValue
    var body: some View {
        Form {
            Picker("Default weekday", selection: $defaultWeekday) {
                ForEach(Weekday.displayOrder) { day in Text(day.name()).tag(day.rawValue) }
            }
            Text("Applies when you open a new window. Dates use your system time zone; folder names stay in MM-DD-YYYY format.")
                .foregroundStyle(.secondary)
        }
        .formStyle(.grouped)
        .frame(width: 430, height: 180)
    }
}
