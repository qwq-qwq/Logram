import SwiftUI

struct FilterSidebar: View {
    @Bindable var document: LogDocument

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
                    RoundedRectangle(cornerRadius: 2)
                        .fill(level.bgColor)
                        .frame(width: 12, height: 12)
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
    }

    private func presetButton(_ title: String, levels: Set<LogLevel>) -> some View {
        Button(title) {
            // Toggle: if all selected, deselect; otherwise select only these
            let allEnabled = levels.isSubset(of: document.enabledLevels)
            if allEnabled {
                document.enabledLevels.subtract(levels)
            } else {
                document.enabledLevels = levels
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
                    if on { document.enabledThreads.insert(th) }
                    else { document.enabledThreads.remove(th) }
                    document.applyFilters()
                }
            )) {
                HStack(spacing: 4) {
                    Text(String(ch))
                        .font(.system(size: 12, weight: .bold, design: .monospaced))
                        .foregroundStyle(threadColor(th))
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
    }

    private func threadColor(_ idx: Int) -> Color {
        let colors: [Color] = [
            .red, .blue, .green, .orange, .purple,
            .cyan, .pink, .mint, .indigo, .brown
        ]
        return colors[idx % colors.count]
    }
}