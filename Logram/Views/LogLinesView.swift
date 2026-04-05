import SwiftUI
import AppKit

// MARK: - SwiftUI wrapper

/// High-performance log line list backed by NSTableView (handles millions of rows)
struct LogLinesView: View {
    let allLines: [LogLine]
    let indices: [Int]
    let theme: ColorTheme
    let showDuration: Bool
    @Binding var selectedId: Int?
    var onJumpToPair: (() -> Void)?

    var body: some View {
        LogTableView(
            allLines: allLines, indices: indices, theme: theme,
            showDuration: showDuration, selectedId: $selectedId,
            onJumpToPair: onJumpToPair
        )
    }
}

// MARK: - NSTableView subclass with keyboard support

class LogNSTableView: NSTableView {
    var onCopy: ((_ selectedRows: IndexSet) -> Void)?
    var onJumpToPair: (() -> Void)?

    override func keyDown(with event: NSEvent) {
        let cmd = event.modifierFlags.contains(.command)
        switch event.keyCode {
        case 8 where cmd:  // Cmd+C
            let rows = selectedRowIndexes
            if !rows.isEmpty { onCopy?(rows); return }
        case 38 where cmd: // Cmd+J — jump to matching +/-
            onJumpToPair?(); return
        default:
            break
        }
        super.keyDown(with: event)
    }
}

// MARK: - NSViewRepresentable

struct LogTableView: NSViewRepresentable {
    let allLines: [LogLine]
    let indices: [Int]
    let theme: ColorTheme
    let showDuration: Bool
    @Binding var selectedId: Int?
    var onJumpToPair: (() -> Void)?

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
        tableView.onJumpToPair = onJumpToPair

        context.coordinator.tableView = tableView

        scrollView.documentView = tableView
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let coord = context.coordinator
        let tableView = coord.tableView!

        // Check if data, theme, or display options changed
        let oldIndices = coord.currentIndices
        let themeChanged = coord.currentTheme != theme
        let durationChanged = coord.currentShowDuration != showDuration
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
        coord.currentShowDuration = showDuration
        tableView.onJumpToPair = onJumpToPair

        if dataChanged || themeChanged || durationChanged {
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
        var currentShowDuration: Bool = true

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

            cell.configure(with: line, isSelected: parent.selectedId == line.id,
                           theme: parent.theme, showDuration: parent.showDuration)
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

    // Toggleable constraint for duration column
    private var durationWidthConstraint: NSLayoutConstraint!
    private var durationGapConstraint: NSLayoutConstraint!

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

        // Toggleable constraints
        durationWidthConstraint = durationField.widthAnchor.constraint(equalToConstant: 60)
        durationGapConstraint = durationField.leadingAnchor.constraint(equalTo: levelBg.trailingAnchor, constant: 2)

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

            durationGapConstraint,
            durationWidthConstraint,
            durationField.centerYAnchor.constraint(equalTo: centerYAnchor),

            messageField.leadingAnchor.constraint(equalTo: durationField.trailingAnchor, constant: 4),
            messageField.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -4),
            messageField.centerYAnchor.constraint(equalTo: centerYAnchor),
        ])
    }

    func configure(with line: LogLine, isSelected: Bool, theme: ColorTheme, showDuration: Bool) {
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

        // Duration column — toggleable
        durationField.isHidden = !showDuration
        durationWidthConstraint.constant = showDuration ? 60 : 0
        durationGapConstraint.constant = showDuration ? 2 : 0

        if showDuration, let dur = line.durationFormatted {
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