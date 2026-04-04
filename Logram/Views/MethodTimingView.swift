import SwiftUI

struct MethodTimingView: View {
    @Bindable var document: LogDocument
    @Environment(\.dismiss) private var dismiss
    @State private var selected: Int?
    @State private var sortedItems: [LogDocument.MethodTiming] = []
    @State private var sortOrder = [KeyPathComparator(\LogDocument.MethodTiming.durationMS, order: .reverse)]

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Method Timing")
                    .font(.title2.bold())
                Spacer()
                Text("\(sortedItems.count) of \(document.methodTimings.count) methods >= 10ms")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            if sortedItems.isEmpty {
                VStack(spacing: 8) {
                    Image(systemName: "clock")
                        .font(.system(size: 32))
                        .foregroundStyle(.tertiary)
                    Text("No slow methods found")
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                Table(sortedItems, selection: $selected, sortOrder: $sortOrder) {
                    TableColumn("#") { timing in
                        Text("\(timing.id + 1)")
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundStyle(.secondary)
                    }
                    .width(50)

                    TableColumn("Thread") { timing in
                        let ch = Character(UnicodeScalar(0x21 + timing.thread)!)
                        Text(String(ch))
                            .font(.system(size: 12, weight: .bold, design: .monospaced))
                    }
                    .width(50)

                    TableColumn("Duration", sortUsing: KeyPathComparator(\LogDocument.MethodTiming.durationMS)) { timing in
                        Text(formatDuration(timing.durationMS))
                            .font(.system(size: 11, weight: .semibold, design: .monospaced))
                            .foregroundStyle(durationColor(timing.durationMS))
                    }
                    .width(80)

                    TableColumn("Method") { timing in
                        Text(timing.method)
                            .font(.system(size: 11, design: .monospaced))
                            .lineLimit(1)
                            .truncationMode(.tail)
                    }
                }
                .tableStyle(.bordered)
                .onChange(of: sortOrder) { _, newOrder in
                    sortedItems.sort(using: newOrder)
                }
                .onKeyPress(.return) { goToSelected(); return .handled }
            }

            HStack {
                if selected != nil {
                    Button("Go to Line") { goToSelected() }
                        .keyboardShortcut(.defaultAction)
                }
                Spacer()
                Button("Close") { dismiss() }
                    .keyboardShortcut(.cancelAction)
            }
        }
        .padding(20)
        .frame(minWidth: 700, minHeight: 400)
        .onAppear {
            let all = document.methodTimings.sorted(using: sortOrder)
            sortedItems = all.count > 1000 ? Array(all.prefix(1000)) : all
        }
    }

    private func goToSelected() {
        guard let lineId = selected else { return }
        // Filter to show only the thread of the selected method
        if let timing = sortedItems.first(where: { $0.id == lineId }) {
            document.enabledThreads = [timing.thread]
            document.applyFilters()
        }
        document.selectedLineId = lineId
        dismiss()
    }

    private func formatDuration(_ ms: Double) -> String {
        if ms >= 1000 {
            return String(format: "%.1f s", ms / 1000)
        }
        return String(format: "%.0f ms", ms)
    }

    private func durationColor(_ ms: Double) -> Color {
        if ms >= 10_000 { return .red }
        if ms >= 1_000 { return .orange }
        if ms >= 100 { return .yellow }
        return .primary
    }
}