// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import Sparkle
import SwiftUI

@MainActor
final class ApplicationUpdater: NSObject, ObservableObject, SPUUpdaterDelegate {
    static let shared = ApplicationUpdater()
    @Published private(set) var canCheck = false
    private var observation: NSKeyValueObservation?
    private var controller: SPUStandardUpdaterController?
    override init() {
        super.init()
        // Development/test bundles carry no production key and perform no background requests.
        guard let key = Bundle.main.object(forInfoDictionaryKey: "SUPublicEDKey") as? String,
              Data(base64Encoded: key)?.count == 32 else { return }
        let controller = SPUStandardUpdaterController(startingUpdater: false, updaterDelegate: self, userDriverDelegate: nil)
        self.controller = controller
        observation = controller.updater.observe(\.canCheckForUpdates, options: [.initial, .new]) { [weak self] _, change in
            let value = change.newValue ?? false
            Task { @MainActor in self?.canCheck = value }
        }
        controller.startUpdater()
    }
    var automaticChecks: Bool {
        get { controller?.updater.automaticallyChecksForUpdates ?? false }
        set { controller?.updater.automaticallyChecksForUpdates = newValue; objectWillChange.send() }
    }
    var configured: Bool { controller != nil }
    func check() {
        guard let controller else {
            let alert = NSAlert()
            alert.messageText = "Updates are not configured in this build"
            alert.informativeText = "Install an official EWAF release from GitHub to enable authenticated updates. Your current application remains usable."
            alert.addButton(withTitle: "OK")
            alert.runModal()
            return
        }
        controller.checkForUpdates(nil)
    }
    func allowedSystemProfileKeys(for updater: SPUUpdater) -> [String]? { [] }
    func feedParameters(for updater: SPUUpdater, sendingSystemProfile: Bool) -> [[String: String]] { [] }
    func updater(_ updater: SPUUpdater, mayPerform updateCheck: SPUUpdateCheck) throws {
        if FolderWorkspace.activeCreations > 0 {
            throw NSError(domain: "com.tlolabs.ewaf.updates", code: 1, userInfo: [NSLocalizedDescriptionKey: "Finish folder creation before updating EWAF."])
        }
    }
    func updater(_ updater: SPUUpdater, shouldPostponeRelaunchForUpdate item: SUAppcastItem, untilInvokingBlock installHandler: @escaping () -> Void) -> Bool {
        guard FolderWorkspace.activeCreations > 0 else { return false }
        Task { @MainActor in
            while FolderWorkspace.activeCreations > 0 { try? await Task.sleep(for: .seconds(1)) }
            installHandler()
        }
        return true
    }
}

@MainActor
final class ApplicationLifecycle: NSObject, NSApplicationDelegate {
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        FolderWorkspace.activeCreations == 0 ? .terminateNow : .terminateCancel
    }
}
