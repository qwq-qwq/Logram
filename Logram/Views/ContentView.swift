import SwiftUI

struct ContentView: View {
    @Bindable var document: LogDocument
    @AppStorage("colorTheme") private var themeRaw = ColorTheme.tokyoNight.rawValue
    @AppStorage("showDuration") private var showDuration = false
    @State private var showStats = false
    @State private var showTimings = false
    @State private var searchText = ""
    @State private var searchIdx: Int?

    private var theme: ColorTheme {
        ColorTheme(rawValue: themeRaw) ?? .tokyoNight
    }

    var body: some View {
        HSplitView {
            // Left: filters sidebar
            FilterSidebar(document: document, theme: theme)
                .frame(minWidth: 200, idealWidth: 220, maxWidth: 300)

            // Right: log lines + toolbar
            VStack(spacing: 0) {
                toolbar
                Divider()

                if document.isLoading {
                    loadingView
                } else if document.allLines.isEmpty {
                    emptyView
                } else {
                    VSplitView {
                        LogLinesView(
                            allLines: document.allLines,
                            indices: document.filteredIndices,
                            theme: theme,
                            showDuration: showDuration,
                            selectedId: $document.selectedLineId,
                            onJumpToPair: jumpToPair
                        )
                        .frame(minHeight: 200)

                        LineDetailView(
                            line: selectedLine,
                            paramsJSON: selectedLineParams,
                            theme: theme
                        )
                        .frame(minHeight: 80, idealHeight: 180, maxHeight: 400)
                    }
                }

                Divider()
                statusBar
            }
        }
        .onDrop(of: [.fileURL], isTargeted: nil) { providers in
            handleDrop(providers)
        }
        .sheet(isPresented: $showStats) {
            StatsView(document: document, theme: theme)
        }
        .sheet(isPresented: $showTimings) {
            MethodTimingView(document: document, theme: theme)
        }
    }

    // MARK: - Toolbar

    private var toolbar: some View {
        HStack(spacing: 8) {
            Button {
                openFile()
            } label: {
                Label("Open", systemImage: "doc")
            }

            Divider().frame(height: 20)

            // Search
            HStack(spacing: 4) {
                Image(systemName: "magnifyingglass")
                    .foregroundStyle(.secondary)
                TextField("Search...", text: $searchText)
                    .textFieldStyle(.roundedBorder)
                    .frame(minWidth: 200)
                    .onSubmit { doSearch(.forward) }

                Button { doSearch(.backward) } label: {
                    Image(systemName: "chevron.up")
                }
                .buttonStyle(.borderless)
                Button { doSearch(.forward) } label: {
                    Image(systemName: "chevron.down")
                }
                .buttonStyle(.borderless)
            }

            Divider().frame(height: 20)

            // Error navigation
            if document.errorCount > 0 {
                HStack(spacing: 2) {
                    Button { goToError(.backward) } label: {
                        Image(systemName: "chevron.up")
                    }
                    .buttonStyle(.borderless)
                    .help("Previous Error")

                    Button { goToError(.forward) } label: {
                        Image(systemName: "chevron.down")
                    }
                    .buttonStyle(.borderless)
                    .help("Next Error")

                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(.red)
                        .font(.system(size: 11))
                }
            }

            // Jump to matching +/-
            if let sel = selectedLine, (sel.level == .enter || sel.level == .leave) {
                Button { jumpToPair() } label: {
                    Image(systemName: sel.level == .enter ? "arrow.down.to.line" : "arrow.up.to.line")
                }
                .buttonStyle(.borderless)
                .help("Jump to matching +/- (Cmd+J)")
            }

            Spacer()

            Button {
                if !showDuration && document.methodTimings.isEmpty {
                    document.buildMethodTimings()
                }
                showDuration.toggle()
            } label: {
                Image(systemName: showDuration ? "timer" : "timer.circle")
            }
            .buttonStyle(.borderless)
            .help("Toggle duration column")

            Menu {
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
            } label: {
                Image(systemName: "paintpalette")
            }
            .menuStyle(.borderlessButton)
            .frame(width: 24)
            .help("Color theme")

            Divider().frame(height: 20)

            Button {
                document.buildMethodTimings()
                showTimings = true
            } label: {
                Label("Timing", systemImage: "clock")
            }

            Button {
                showStats = true
            } label: {
                Label("Stats", systemImage: "chart.bar")
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 6)
    }

    // MARK: - Status bar

    private var statusBar: some View {
        HStack(spacing: 16) {
            if !document.fileName.isEmpty {
                Text(document.fileName)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text(formatFileSize(document.fileSize))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            Spacer()
            Text("\(document.filteredCount) / \(document.allLines.count) lines")
                .font(.caption)
                .foregroundStyle(.secondary)
            if document.errorCount > 0 {
                HStack(spacing: 2) {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(.red)
                    Text("\(document.errorCount) errors")
                        .foregroundStyle(.red)
                }
                .font(.caption)
            }
            Text("Duration: \(document.durationFormatted)")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
    }

    // MARK: - Empty / Loading

    private var emptyView: some View {
        VStack(spacing: 16) {
            Image(systemName: "doc.text.magnifyingglass")
                .font(.system(size: 48))
                .foregroundStyle(.tertiary)
            Text("Open a UB log file or drag & drop")
                .font(.title3)
                .foregroundStyle(.secondary)
            Text("Cmd+O to open")
                .font(.caption)
                .foregroundStyle(.tertiary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var loadingView: some View {
        VStack(spacing: 12) {
            ProgressView(value: document.loadingProgress)
                .frame(width: 200)
            Text("Loading \(document.fileName)...")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    // MARK: - Computed

    private var selectedLine: LogLine? {
        guard let id = document.selectedLineId, id >= 0, id < document.allLines.count else { return nil }
        return document.allLines[id]
    }

    /// Find cust1 (Params) line before the selected SQL/error line on the same thread.
    /// Searches backwards counting only same-thread lines. Stops at another SQL/cust2 boundary.
    private var selectedLineParams: String? {
        guard let line = selectedLine,
              (line.level == .sql || line.level == .cust2 || (line.level.isError && (line.message.contains(" q=") || line.message.trimmingCharacters(in: .whitespaces).hasPrefix("q=")))),
              line.thread >= 0
        else { return nil }

        let lines = document.allLines
        let startIdx = line.id
        let th = line.thread
        var sameThreadSeen = 0
        for i in stride(from: startIdx - 1, through: max(0, startIdx - 50000), by: -1) {
            let prev = lines[i]
            guard prev.thread == th else { continue }
            sameThreadSeen += 1
            if sameThreadSeen > 200 { break }
            if prev.level == .cust1 {
                return prev.message.trimmingCharacters(in: .whitespaces)
            }
            // Another SQL/cust2 = different query boundary → stop
            if prev.level == .sql || prev.level == .cust2 {
                break
            }
        }
        return nil
    }

    // MARK: - Actions

    private func jumpToPair() {
        guard let selId = document.selectedLineId,
              let pairId = document.findMatchingPair(for: selId) else { return }
        document.selectedLineId = pairId
    }

    private func openFile() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.plainText, .log]
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false

        if panel.runModal() == .OK, let url = panel.url {
            Task { await document.load(from: url) }
        }
    }

    private func doSearch(_ direction: LogDocument.SearchDirection) {
        guard !searchText.isEmpty else { return }
        if let idx = document.findNext(searchText, direction: direction, from: searchIdx) {
            searchIdx = idx
            document.selectedLineId = document.allLines[document.filteredIndices[idx]].id
        }
    }

    private func goToError(_ direction: LogDocument.SearchDirection) {
        let indices = document.filteredIndices
        let lines = document.allLines
        guard !indices.isEmpty else { return }

        // Find current position in filtered indices
        let currentIdx: Int
        if let selId = document.selectedLineId,
           let pos = indices.firstIndex(of: selId) {
            currentIdx = pos
        } else {
            currentIdx = direction == .forward ? -1 : indices.count
        }

        if direction == .forward {
            for i in (currentIdx + 1)..<indices.count {
                if lines[indices[i]].level.isError {
                    document.selectedLineId = indices[i]
                    return
                }
            }
        } else {
            let upper = min(currentIdx, indices.count) - 1
            for i in stride(from: upper, through: 0, by: -1) {
                if lines[indices[i]].level.isError {
                    document.selectedLineId = indices[i]
                    return
                }
            }
        }
    }

    private func handleDrop(_ providers: [NSItemProvider]) -> Bool {
        guard let provider = providers.first else { return false }
        provider.loadItem(forTypeIdentifier: "public.file-url") { item, _ in
            guard let data = item as? Data,
                  let url = URL(dataRepresentation: data, relativeTo: nil)
            else { return }
            Task { @MainActor in
                await document.load(from: url)
            }
        }
        return true
    }

    private func formatFileSize(_ bytes: Int64) -> String {
        if bytes >= 1_000_000 {
            return String(format: "%.1f MB", Double(bytes) / 1_000_000)
        } else if bytes >= 1_000 {
            return String(format: "%.0f KB", Double(bytes) / 1_000)
        }
        return "\(bytes) B"
    }
}