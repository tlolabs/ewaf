// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI
import EWAFCore

@main
struct EWAFApp: App {
    @NSApplicationDelegateAdaptor(ApplicationLifecycle.self) private var lifecycle
    private let updater = ApplicationUpdater.shared
    @AppStorage("defaultWeekday") private var defaultWeekday = Weekday.thursday.rawValue
    var body: some Scene {
        WindowGroup("EWAF — Every Week a Folder") {
            ContentView(defaultWeekday: defaultWeekday)
        }
        .defaultSize(width: 780, height: 540)
        .commands { FolderCommands() }
        Settings { SettingsView() }
    }
}

struct FolderCommands: Commands {
    @ObservedObject private var updater = ApplicationUpdater.shared
    @FocusedValue(\.folderWorkspace) private var workspace
    var body: some Commands {
        CommandGroup(after: .appInfo) {
            Button("Check for Updates…") { updater.check() }
                .disabled(updater.configured && !updater.canCheck)
        }
        CommandGroup(replacing: .help) {
            Link("EWAF Help", destination: URL(string: "https://github.com/tlolabs/ewaf#use")!)
        }
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
