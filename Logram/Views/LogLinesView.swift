import SwiftUI
import AppKit

// MARK: - SwiftUI wrapper

/// High-performance log line list backed by NSTableView (handles millions of rows)
struct LogLinesView: View {
    let allLines: [LogLine]
    let indices: [Int]
    let theme: ColorTheme
    @Binding var selectedId: Int?

    var body: some View {
        LogTableView(allLines: allLines, indices: indices, theme: theme, selectedId: $selectedId)
    }
}

// MARK: - NSTableView subclass with copy support

class LogNSTableView: NSTableView {
    var onCopy: ((_ selectedRows: IndexSet) -> Void)?

    override func keyDown(with event: NSEvent) {
        if event.modifierFlags.contains(.command) && event.keyCode == 8 /* C key */ {
            let rows = selectedRowIndexes
            if !rows.isEmpty {
                onCopy?(rows)
                return
            }
        }
        super.keyDown(with: event)
    }
}

// MARK: - NSViewRepresentable

struct LogTableView: NSViewRepresentable {
    let allLines: [LogLine]
    let indices: [Int]
    let theme: ColorTheme
    @Binding var selectedId: Int?

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSScrollView()
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = true
        scrollView.autohidesScrollers = true

        let tableView = LogNSTableView()
        tableView.style = .plain
        tableView.rowHeight = 20
        tableView.intercellSpacing = NSSize(width: 0, height: 1)
        tableView.headerView = nil
        tableView.allowsMultipleSelection = true
        tableView.gridStyleMask = []

        // Single column — we draw everything in one row view
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("line"))
        column.title = ""
        column.resizingMask = .autoresizingMask
        tableView.addTableColumn(column)

        tableView.dataSource = context.coordinator
        tableView.delegate = context.coordinator
        let coordinator = context.coordinator
        tableView.onCopy = { [weak coordinator] rows in
            coordinator?.copyRows(rows)
        }

        context.coordinator.tableView = tableView

        scrollView.documentView = tableView
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let coord = context.coordinator
        let tableView = coord.tableView!

        // Check if data or theme changed
        let oldIndices = coord.currentIndices
        let themeChanged = coord.currentTheme != theme
        let dataChanged: Bool
        if let old = oldIndices, old.count == indices.count,
           old.first == indices.first, old.last == indices.last {
            dataChanged = false
        } else {
            dataChanged = true
        }
        coord.parent = self
        coord.currentIndices = indices
        coord.currentTheme = theme

        if dataChanged || themeChanged {
            tableView.reloadData()
        }

        // Scroll to selected row
        if let selectedId = selectedId {
            if let rowIndex = indices.firstIndex(of: selectedId) {
                let currentSelection = tableView.selectedRow
                if currentSelection != rowIndex {
                    tableView.selectRowIndexes(IndexSet(integer: rowIndex), byExtendingSelection: false)
                    tableView.scrollRowToVisible(rowIndex)
                }
            }
        }
    }

    // MARK: - Coordinator

    class Coordinator: NSObject, NSTableViewDataSource, NSTableViewDelegate {
        var parent: LogTableView
        weak var tableView: LogNSTableView?
        var currentIndices: [Int]?
        var currentTheme: ColorTheme = .tokyoNight

        init(parent: LogTableView) {
            self.parent = parent
        }

        func numberOfRows(in tableView: NSTableView) -> Int {
            parent.indices.count
        }

        func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
            let indices = parent.indices
            guard row < indices.count else { return nil }
            let lineIdx = indices[row]
            guard lineIdx < parent.allLines.count else { return nil }
            let line = parent.allLines[lineIdx]

            // Reuse or create cell
            let cellId = NSUserInterfaceItemIdentifier("LogCell")
            let cell: LogCellView
            if let reused = tableView.makeView(withIdentifier: cellId, owner: nil) as? LogCellView {
                cell = reused
            } else {
                cell = LogCellView()
                cell.identifier = cellId
            }

            cell.configure(with: line, isSelected: parent.selectedId == line.id, theme: parent.theme)
            return cell
        }

        func tableView(_ tableView: NSTableView, heightOfRow row: Int) -> CGFloat {
            20
        }

        func tableViewSelectionDidChange(_ notification: Notification) {
            guard let tableView = tableView else { return }
            let row = tableView.selectedRow
            if row >= 0 && row < parent.indices.count {
                let lineIdx = parent.indices[row]
                if lineIdx < parent.allLines.count {
                    DispatchQueue.main.async {
                        self.parent.selectedId = self.parent.allLines[lineIdx].id
                    }
                }
            } else {
                DispatchQueue.main.async {
                    self.parent.selectedId = nil
                }
            }
        }

        func copyRows(_ rows: IndexSet) {
            let indices = parent.indices
            let allLines = parent.allLines
            var lines: [String] = []
            lines.reserveCapacity(rows.count)
            for row in rows {
                guard row < indices.count else { continue }
                let lineIdx = indices[row]
                guard lineIdx < allLines.count else { continue }
                lines.append(allLines[lineIdx].raw)
            }
            let text = lines.joined(separator: "\n")
            NSPasteboard.general.clearContents()
            NSPasteboard.general.setString(text, forType: .string)
        }
    }
}

// MARK: - Native cell view

final class LogCellView: NSView {
    private let lineNoField = NSTextField(labelWithString: "")
    private let timeField = NSTextField(labelWithString: "")
    private let threadField = NSTextField(labelWithString: "")
    private let levelField = NSTextField(labelWithString: "")
    private let durationField = NSTextField(labelWithString: "")
    private let messageField = NSTextField(labelWithString: "")

    private let levelBg = NSView()

    private static let monoFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
    private static let smallMonoFont = NSFont.monospacedSystemFont(ofSize: 10, weight: .medium)
    private static let boldMonoFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setup()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setup()
    }

    private func setup() {
        let fields = [lineNoField, timeField, threadField, levelField, durationField, messageField]
        for f in fields {
            f.isEditable = false
            f.isBordered = false
            f.drawsBackground = false
            f.font = Self.monoFont
            f.lineBreakMode = .byTruncatingTail
            f.translatesAutoresizingMaskIntoConstraints = false
            addSubview(f)
        }

        lineNoField.alignment = .right
        lineNoField.textColor = .tertiaryLabelColor
        timeField.textColor = .secondaryLabelColor
        threadField.alignment = .center
        threadField.font = Self.boldMonoFont
        levelField.font = Self.smallMonoFont
        durationField.alignment = .right

        // Level background
        levelBg.wantsLayer = true
        levelBg.layer?.cornerRadius = 3
        levelBg.translatesAutoresizingMaskIntoConstraints = false
        addSubview(levelBg, positioned: .below, relativeTo: levelField)

        // Layout
        NSLayoutConstraint.activate([
            lineNoField.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 4),
            lineNoField.widthAnchor.constraint(equalToConstant: 60),
            lineNoField.centerYAnchor.constraint(equalTo: centerYAnchor),

            timeField.leadingAnchor.constraint(equalTo: lineNoField.trailingAnchor, constant: 6),
            timeField.widthAnchor.constraint(equalToConstant: 80),
            timeField.centerYAnchor.constraint(equalTo: centerYAnchor),

            threadField.leadingAnchor.constraint(equalTo: timeField.trailingAnchor),
            threadField.widthAnchor.constraint(equalToConstant: 16),
            threadField.centerYAnchor.constraint(equalTo: centerYAnchor),

            levelBg.leadingAnchor.constraint(equalTo: threadField.trailingAnchor, constant: 2),
            levelBg.widthAnchor.constraint(equalToConstant: 65),
            levelBg.topAnchor.constraint(equalTo: topAnchor, constant: 2),
            levelBg.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -2),

            levelField.leadingAnchor.constraint(equalTo: levelBg.leadingAnchor, constant: 3),
            levelField.trailingAnchor.constraint(equalTo: levelBg.trailingAnchor, constant: -3),
            levelField.centerYAnchor.constraint(equalTo: centerYAnchor),

            durationField.leadingAnchor.constraint(equalTo: levelBg.trailingAnchor, constant: 2),
            durationField.widthAnchor.constraint(equalToConstant: 60),
            durationField.centerYAnchor.constraint(equalTo: centerYAnchor),

            messageField.leadingAnchor.constraint(equalTo: durationField.trailingAnchor, constant: 4),
            messageField.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -4),
            messageField.centerYAnchor.constraint(equalTo: centerYAnchor),
        ])
    }

    func configure(with line: LogLine, isSelected: Bool, theme: ColorTheme) {
        lineNoField.stringValue = "\(line.id + 1)"
        timeField.stringValue = line.timeFormatted ?? ""

        if line.thread >= 0 {
            threadField.stringValue = String(Character(UnicodeScalar(0x21 + line.thread)!))
            threadField.textColor = theme.threadNSColor(line.thread)
        } else {
            threadField.stringValue = ""
        }

        levelField.stringValue = line.level.label
        levelBg.layer?.backgroundColor = theme.levelBadgeNSColor(for: line.level).withAlphaComponent(0.4).cgColor

        if let dur = line.durationFormatted {
            durationField.stringValue = dur
            durationField.textColor = theme.durationNSColor(line.durationUS)
            durationField.font = line.durationUS >= 1_000_000 ? Self.boldMonoFont : Self.monoFont
        } else {
            durationField.stringValue = ""
        }

        messageField.stringValue = line.message
        messageField.textColor = theme.messageColor(for: line.level)

        // Row background
        wantsLayer = true
        if isSelected {
            layer?.backgroundColor = NSColor.controlAccentColor.withAlphaComponent(0.25).cgColor
        } else {
            layer?.backgroundColor = theme.rowBackground(for: line.level)
        }
    }
}