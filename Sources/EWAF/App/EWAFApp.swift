import SwiftUI
import EWAFCore

@main
struct EWAFApp: App {
    @AppStorage("defaultWeekday") private var defaultWeekday = Weekday.thursday.rawValue
    var body: some Scene {
        WindowGroup("E.W.A.F. — Every Week a Folder") {
            ContentView(defaultWeekday: defaultWeekday)
        }
        .defaultSize(width: 780, height: 540)
        .commands { FolderCommands() }
        Settings { SettingsView() }
    }
}

struct FolderCommands: Commands {
    @FocusedValue(\.folderWorkspace) private var workspace
    var body: some Commands {
        CommandGroup(after: .newItem) {
            Button("Choose Destination…") { workspace?.showImporter = true }
                .keyboardShortcut("o")
                .disabled(workspace == nil || workspace?.isCreating == true)
            Button("Create Folders") { workspace?.requestCreation() }
                .keyboardShortcut(.return, modifiers: .command)
                .disabled(workspace?.canCreate != true)
            Button("Cancel Folder Creation") { workspace?.cancel() }
                .keyboardShortcut(".", modifiers: .command)
                .disabled(workspace?.isCreating != true)
        }
    }
}
