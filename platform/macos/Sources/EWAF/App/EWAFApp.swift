// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI
import AppKit
import EWAFCore

extension AppAppearance {
    var colorScheme: ColorScheme? {
        switch self {
        case .system: return nil
        case .light: return .light
        case .dark: return .dark
        }
    }
}

@main
struct EWAFApp: App {
    @NSApplicationDelegateAdaptor(ApplicationLifecycle.self) private var lifecycle
    private let updater = ApplicationUpdater.shared
    @AppStorage("defaultWeekday") private var defaultWeekday = Weekday.thursday.rawValue
    @AppStorage("appearance") private var appearance = AppAppearance.system.rawValue

    private var colorScheme: ColorScheme? {
        AppAppearance(rawValue: appearance)?.colorScheme
    }

    var body: some Scene {
        WindowGroup("EWAF — Every Week a Folder") {
            ContentView(defaultWeekday: defaultWeekday)
                .preferredColorScheme(colorScheme)
                .onAppear { syncAppearance(appearance) }
                .onChange(of: appearance) { _, newAppearance in syncAppearance(newAppearance) }
        }
        .defaultSize(width: 780, height: 540)
        .commands { FolderCommands() }
        Settings {
            SettingsView()
                .preferredColorScheme(colorScheme)
                .onAppear { syncAppearance(appearance) }
                .onChange(of: appearance) { _, newAppearance in syncAppearance(newAppearance) }
        }
    }

    private func syncAppearance(_ value: String) {
        if let mode = AppAppearance(rawValue: value) {
            switch mode {
            case .system:
                NSApp?.appearance = nil
            case .light:
                NSApp?.appearance = NSAppearance(named: .aqua)
            case .dark:
                NSApp?.appearance = NSAppearance(named: .darkAqua)
            }
        }
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
            Divider()
            Button("Reveal Destination in Finder") {
                if let destination = workspace?.destination {
                    NSWorkspace.shared.activateFileViewerSelecting([destination])
                }
            }
            .keyboardShortcut("r", modifiers: .command)
            .disabled(workspace?.destination == nil)
        }
        CommandGroup(after: .pasteboard) {
            Button("Copy Folder Name") {
                if let name = workspace?.selectedOrFirstName {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(name, forType: .string)
                }
            }
            .keyboardShortcut("c", modifiers: [.command, .shift])
            .disabled(workspace?.preview.isEmpty ?? true)
        }
    }
}
