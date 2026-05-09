import SwiftUI
import AppKit

// FocusedValue key so Cmd+O reaches the active window
struct OpenLogFileKey: FocusedValueKey {
    typealias Value = () -> Void
}

struct FocusOnCallKey: FocusedValueKey {
    typealias Value = () -> Void
}

struct ClearFocusKey: FocusedValueKey {
    typealias Value = () -> Void
}

struct ShowMethodTimingKey: FocusedValueKey {
    typealias Value = () -> Void
}

struct ShowStatsKey: FocusedValueKey {
    typealias Value = () -> Void
}

struct ToggleDurationKey: FocusedValueKey {
    typealias Value = () -> Void
}

extension FocusedValues {
    var openLogFile: (() -> Void)? {
        get { self[OpenLogFileKey.self] }
        set { self[OpenLogFileKey.self] = newValue }
    }
    var focusOnCall: (() -> Void)? {
        get { self[FocusOnCallKey.self] }
        set { self[FocusOnCallKey.self] = newValue }
    }
    var clearFocus: (() -> Void)? {
        get { self[ClearFocusKey.self] }
        set { self[ClearFocusKey.self] = newValue }
    }
    var showMethodTiming: (() -> Void)? {
        get { self[ShowMethodTimingKey.self] }
        set { self[ShowMethodTimingKey.self] = newValue }
    }
    var showStats: (() -> Void)? {
        get { self[ShowStatsKey.self] }
        set { self[ShowStatsKey.self] = newValue }
    }
    var toggleDuration: (() -> Void)? {
        get { self[ToggleDurationKey.self] }
        set { self[ToggleDurationKey.self] = newValue }
    }
}

// Bridges system "open file" Apple Events into SwiftUI windows.
// Empty windows register as "takers" — they get URLs first; if none accepts,
// a new window is opened. Avoids the cold-start "empty + file" duplicate.
@MainActor
final class LogramAppDelegate: NSObject, NSApplicationDelegate, ObservableObject {
    private var pending: [URL] = []
    private var takers: [UUID: (URL) -> Bool] = [:]
    private var openNew: ((URL) -> Void)?

    nonisolated func application(_ application: NSApplication, open urls: [URL]) {
        MainActor.assumeIsolated {
            for url in urls { self.dispatch(url) }
        }
    }

    func setOpenNew(_ handler: @escaping (URL) -> Void) {
        guard openNew == nil else { return }
        openNew = handler
        let queued = pending
        pending.removeAll()
        for url in queued { dispatch(url) }
    }

    func registerTaker(_ id: UUID, _ taker: @escaping (URL) -> Bool) {
        takers[id] = taker
    }

    func unregisterTaker(_ id: UUID) {
        takers.removeValue(forKey: id)
    }

    private func dispatch(_ url: URL) {
        for taker in takers.values where taker(url) { return }
        if let openNew {
            openNew(url)
        } else {
            pending.append(url)
        }
    }
}

@main
struct LogramApp: App {
    @NSApplicationDelegateAdaptor(LogramAppDelegate.self) private var appDelegate
    @FocusedValue(\.openLogFile) var openLogFile
    @FocusedValue(\.focusOnCall) var focusOnCall
    @FocusedValue(\.clearFocus) var clearFocus
    @FocusedValue(\.showMethodTiming) var showMethodTiming
    @FocusedValue(\.showStats) var showStats
    @FocusedValue(\.toggleDuration) var toggleDuration
    // Same key as ContentView's @AppStorage — they share state automatically.
    @AppStorage("colorTheme") private var themeRaw = ColorTheme.tokyoNight.rawValue

    var body: some Scene {
        WindowGroup(for: URL?.self) { $url in
            ContentView(initialURL: url ?? nil)
                .environmentObject(appDelegate)
                .frame(minWidth: 900, minHeight: 600)
        } defaultValue: {
            URL?.none
        }
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("Open Log...") {
                    openLogFile?()
                }
                .keyboardShortcut("o", modifiers: .command)
            }
            CommandMenu("Navigate") {
                Button("Focus on Call") { focusOnCall?() }
                    .keyboardShortcut("e", modifiers: [.command, .shift])
                    .disabled(focusOnCall == nil)
                Button("Clear Focus") { clearFocus?() }
                    .keyboardShortcut(.escape, modifiers: [])
                    .disabled(clearFocus == nil)
            }
            CommandMenu("Tools") {
                Menu("Theme") {
                    ForEach(ColorTheme.allCases) { t in
                        Button {
                            themeRaw = t.rawValue
                        } label: {
                            if t.rawValue == themeRaw {
                                Label(t.displayName, systemImage: "checkmark")
                            } else {
                                Text(t.displayName)
                            }
                        }
                    }
                }
                Divider()
                Button("Toggle Duration Column") { toggleDuration?() }
                    .keyboardShortcut("d", modifiers: .command)
                    .disabled(toggleDuration == nil)
                Button("Statistics…") { showStats?() }
                    .keyboardShortcut("i", modifiers: .command)
                    .disabled(showStats == nil)
                Button("Method Timing…") { showMethodTiming?() }
                    .keyboardShortcut("m", modifiers: .command)
                    .disabled(showMethodTiming == nil)
            }
            CommandGroup(replacing: .appInfo) {
                Button("About Logram") {
                    NSApplication.shared.orderFrontStandardAboutPanel(options: [
                        .applicationName: "Logram",
                        .applicationVersion: "1.2",
                        .credits: NSAttributedString(
                            string: "UnityBase Log Analyzer\nhttps://logram.perek.rest",
                            attributes: [
                                .font: NSFont.systemFont(ofSize: 11),
                                .foregroundColor: NSColor.secondaryLabelColor
                            ]
                        )
                    ])
                }
            }
            CommandGroup(replacing: .help) {
                Button("Logram Documentation") {
                    if let url = URL(string: localizedDocsURL()) {
                        NSWorkspace.shared.open(url)
                    }
                }
                .keyboardShortcut("?", modifiers: .command)
            }
        }
    }
}

private func localizedDocsURL() -> String {
    let code = Locale.current.language.languageCode?.identifier ?? "en"
    switch code {
    case "uk": return "https://logram.perek.rest/docs/"
    case "ru": return "https://logram.perek.rest/ru/docs/"
    default:   return "https://logram.perek.rest/en/docs/"
    }
}