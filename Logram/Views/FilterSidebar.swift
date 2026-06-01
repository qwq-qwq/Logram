import SwiftUI

struct FilterSidebar: View {
    @Bindable var document: LogDocument
    let theme: ColorTheme

    @State private var showAllLevels = true
    @State private var showAllThreads = true

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header
            Text("Filters")
                .font(.headline)
                .padding(.horizontal, 12)
                .padding(.vertical, 8)

            Divider()

            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    levelFilters
                    Divider()
                    threadFilters
                }
                .padding(12)
            }

            Divider()

            // Apply button
            HStack {
                Button("Reset") {
                    document.enabledLevels = Set(LogLevel.allCases)
                    document.enabledThreads = Set(0..<LogParser.maxThreads)
                    document.searchPattern = ""
                    document.applyFilters()
                }
                .buttonStyle(.borderless)

                Spacer()

                Button("Apply") {
                    document.applyFilters()
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
            }
            .padding(8)
        }
        .background(Color(nsColor: .controlBackgroundColor))
    }

    // MARK: - Log Levels

    private var levelFilters: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("Log Levels")
                    .font(.subheadline.weight(.semibold))
                Spacer()
                Button(showAllLevels ? "None" : "All") {
                    if showAllLevels {
                        document.enabledLevels = []
                    } else {
                        document.enabledLevels = Set(LogLevel.allCases)
                    }
                    showAllLevels.toggle()
                    document.applyFilters()
                }
                .buttonStyle(.borderless)
                .font(.caption)
            }

            // Quick presets
            HStack(spacing: 4) {
                presetButton("Errors", levels: [.error, .exc, .excOs, .osErr, .fail])
                presetButton("SQL", levels: [.sql, .cust1, .cust2])
                presetButton("HTTP", levels: [.http])
                presetButton("+/-", levels: [.enter, .leave])
            }
            .font(.caption)

            // All levels
            ForEach(LogLevel.allCases.filter { levelHasEvents($0) }) { level in
                levelToggle(level)
            }
        }
    }

    private func levelToggle(_ level: LogLevel) -> some View {
        HStack(spacing: 6) {
            Toggle(isOn: Binding(
                get: { document.enabledLevels.contains(level) },
                set: { on in
                    if on { document.enabledLevels.insert(level) }
                    else { document.enabledLevels.remove(level) }
                    document.applyFilters()
                }
            )) {
                HStack(spacing: 4) {
                    Image(level.iconName)
                        .renderingMode(.template)
                        .resizable()
                        .interpolation(.high)
                        .frame(width: 14, height: 14)
                        .foregroundStyle(theme.levelBadgeColor(for: level))
                    Text(level.label)
                        .font(.system(size: 11, design: .monospaced))
                    Spacer()
                    Text("\(document.perLevelCount[level.rawValue])")
                        .font(.system(size: 10))
                        .foregroundStyle(.secondary)
                }
            }
            .toggleStyle(.checkbox)
        }
        .contentShape(Rectangle())
        .contextMenu {
            Button("All") {
                document.enabledLevels = Set(LogLevel.allCases)
                showAllLevels = true
                document.applyFilters()
            }
            Button("None") {
                document.enabledLevels = []
                showAllLevels = false
                document.applyFilters()
            }
            Button("Only this") {
                document.enabledLevels = [level]
                showAllLevels = false
                document.applyFilters()
            }
        }
    }

    private func presetButton(_ title: String, levels: Set<LogLevel>) -> some View {
        Button(title) {
            // Toggle: if this preset is exactly what's active, restore all
            // levels; otherwise narrow down to just the preset.
            if document.enabledLevels == levels {
                document.enabledLevels = Set(LogLevel.allCases)
                showAllLevels = true
            } else {
                document.enabledLevels = levels
                showAllLevels = false
            }
            document.applyFilters()
        }
        .buttonStyle(.bordered)
        .controlSize(.mini)
    }

    private func levelHasEvents(_ level: LogLevel) -> Bool {
        document.perLevelCount[level.rawValue] > 0
    }

    // MARK: - Thread Filters

    private var threadFilters: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("Threads")
                    .font(.subheadline.weight(.semibold))
                Spacer()
                Button(showAllThreads ? "None" : "All") {
                    // User is editing the thread filter — drop any active
                    // call-frame focus first, otherwise focusRange would keep
                    // hiding the threads they just enabled.
                    if document.focusRange != nil { document.clearFocus() }
                    if showAllThreads {
                        document.enabledThreads = []
                    } else {
                        document.enabledThreads = Set(0..<LogParser.maxThreads)
                    }
                    showAllThreads.toggle()
                    document.applyFilters()
                }
                .buttonStyle(.borderless)
                .font(.caption)
            }

            ForEach(document.activeThreads, id: \.self) { th in
                threadToggle(th)
            }
        }
    }

    private func threadToggle(_ th: Int) -> some View {
        let ch = Character(UnicodeScalar(0x21 + th)!)
        return HStack(spacing: 6) {
            Toggle(isOn: Binding(
                get: { document.enabledThreads.contains(th) },
                set: { on in
                    // Capture the visible thread mask before exiting focus —
                    // clearFocus would otherwise restore the pre-focus snapshot
                    // and we'd end up with "all threads + th" instead of
                    // honoring what the user actually sees in the checkboxes.
                    var visible = document.enabledThreads
                    if on { visible.insert(th) }
                    else { visible.remove(th) }
                    if document.focusRange != nil { document.clearFocus() }
                    document.enabledThreads = visible
                    document.applyFilters()
                }
            )) {
                HStack(spacing: 4) {
                    Text(String(ch))
                        .font(.system(size: 12, weight: .bold, design: .monospaced))
                        .foregroundStyle(theme.threadSwiftUIColor(th))
                    Text("Thread \(th)")
                        .font(.system(size: 11))
                    Spacer()
                    Text("\(document.perThreadCount[th])")
                        .font(.system(size: 10))
                        .foregroundStyle(.secondary)
                }
            }
            .toggleStyle(.checkbox)
        }
        .contentShape(Rectangle())
        .contextMenu {
            Button("All") {
                if document.focusRange != nil { document.clearFocus() }
                document.enabledThreads = Set(0..<LogParser.maxThreads)
                showAllThreads = true
                document.applyFilters()
            }
            Button("None") {
                if document.focusRange != nil { document.clearFocus() }
                document.enabledThreads = []
                showAllThreads = false
                document.applyFilters()
            }
            Button("Only this") {
                if document.focusRange != nil { document.clearFocus() }
                document.enabledThreads = [th]
                showAllThreads = false
                document.applyFilters()
            }
        }
    }

}