import SwiftUI

@main
struct LogramApp: App {
    @State private var document = LogDocument()

    var body: some Scene {
        WindowGroup {
            ContentView(document: document)
                .frame(minWidth: 900, minHeight: 600)
        }
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("Open Log...") {
                    openFile()
                }
                .keyboardShortcut("o", modifiers: .command)
            }
        }
    }

    private func openFile() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.plainText, .log]
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.message = "Select a UB server log file"

        if panel.runModal() == .OK, let url = panel.url {
            Task {
                await document.load(from: url)
            }
        }
    }
}