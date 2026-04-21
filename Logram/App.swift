import SwiftUI

// FocusedValue key so Cmd+O reaches the active window
struct OpenLogFileKey: FocusedValueKey {
    typealias Value = () -> Void
}

extension FocusedValues {
    var openLogFile: (() -> Void)? {
        get { self[OpenLogFileKey.self] }
        set { self[OpenLogFileKey.self] = newValue }
    }
}

@main
struct LogramApp: App {
    @FocusedValue(\.openLogFile) var openLogFile

    var body: some Scene {
        WindowGroup {
            ContentView()
                .frame(minWidth: 900, minHeight: 600)
        }
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("Open Log...") {
                    openLogFile?()
                }
                .keyboardShortcut("o", modifiers: .command)
            }
            CommandGroup(replacing: .appInfo) {
                Button("About Logram") {
                    NSApplication.shared.orderFrontStandardAboutPanel(options: [
                        .applicationName: "Logram",
                        .applicationVersion: "1.1",
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