import SwiftUI

struct StatsView: View {
    let document: LogDocument
    let theme: ColorTheme
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Log Statistics")
                .font(.title2.bold())

            Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 8) {
                GridRow {
                    Text("File:").foregroundStyle(.secondary)
                    Text(document.fileName)
                }
                GridRow {
                    Text("Size:").foregroundStyle(.secondary)
                    Text(formatSize(document.fileSize))
                }
                if let ver = document.ubVersion {
                    GridRow {
                        Text("UB Version:").foregroundStyle(.secondary)
                        Text(ver).lineLimit(1)
                    }
                }
                if let host = document.hostInfo {
                    GridRow {
                        Text("Host:").foregroundStyle(.secondary)
                        Text(host).lineLimit(1)
                    }
                }
                GridRow {
                    Text("Duration:").foregroundStyle(.secondary)
                    Text(document.durationFormatted)
                }
                GridRow {
                    Text("Total Events:").foregroundStyle(.secondary)
                    Text("\(document.totalEvents)")
                }
                GridRow {
                    Text("Threads:").foregroundStyle(.secondary)
                    Text("\(document.activeThreads.count)")
                }

                Divider().gridCellColumns(2)

                GridRow {
                    Text("HTTP Requests:").foregroundStyle(.secondary)
                    Text("\(document.httpRequests)")
                }
                GridRow {
                    Text("SQL Queries:").foregroundStyle(.secondary)
                    Text("\(document.sqlQueries)")
                }
                GridRow {
                    Text("Errors:").foregroundStyle(.secondary)
                    Text("\(document.errorCount)")
                        .foregroundStyle(document.errorCount > 0 ? .red : .primary)
                }

                if let d = document.duration, d > 0 {
                    Divider().gridCellColumns(2)

                    GridRow {
                        Text("HTTP req/sec:").foregroundStyle(.secondary)
                        Text("\(Int(Double(document.httpRequests) / 2 / d))")
                    }
                    GridRow {
                        Text("SQL q/sec:").foregroundStyle(.secondary)
                        Text("\(Int(Double(document.sqlQueries) / d))")
                    }
                }
            }

            Divider()

            // Per-level breakdown
            Text("Events by Level")
                .font(.headline)

            LazyVGrid(columns: [
                GridItem(.fixed(100), alignment: .leading),
                GridItem(.fixed(80), alignment: .trailing)
            ], spacing: 4) {
                ForEach(LogLevel.allCases.filter { document.perLevelCount[$0.rawValue] > 0 }) { level in
                    HStack(spacing: 4) {
                        RoundedRectangle(cornerRadius: 2)
                            .fill(theme.levelBadgeColor(for: level))
                            .frame(width: 10, height: 10)
                        Text(level.label)
                            .font(.system(size: 11, design: .monospaced))
                    }
                    Text("\(document.perLevelCount[level.rawValue])")
                        .font(.system(size: 11, design: .monospaced))
                }
            }

            Spacer()

            HStack {
                Spacer()
                Button("Close") { dismiss() }
                    .keyboardShortcut(.cancelAction)
            }
        }
        .padding(20)
        .frame(minWidth: 400, minHeight: 500)
    }

    private func formatSize(_ bytes: Int64) -> String {
        if bytes >= 1_000_000 {
            return String(format: "%.1f MB", Double(bytes) / 1_000_000)
        }
        return String(format: "%.0f KB", Double(bytes) / 1_000)
    }
}