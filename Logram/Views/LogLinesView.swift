import SwiftUI
import AppKit

// MARK: - SwiftUI wrapper

/// High-performance log line list backed by NSTableView (handles millions of rows)
struct LogLinesView: View {
    let allLines: [LogLine]
    let indices: [Int]
    @Binding var selectedId: Int?

    var body: some View {
        LogTableView(allLines: allLines, indices: indices, selectedId: $selectedId)
    }
}

// MARK: - NSViewRepresentable

struct LogTableView: NSViewRepresentable {
    let allLines: [LogLine]
    let indices: [Int]
    @Binding var selectedId: Int?

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSScrollView()
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = true
        scrollView.autohidesScrollers = true

        let tableView = NSTableView()
        tableView.style = .plain
        tableView.rowHeight = 20
        tableView.intercellSpacing = NSSize(width: 0, height: 1)
        tableView.headerView = nil
        tableView.allowsMultipleSelection = false
        tableView.gridStyleMask = []

        // Single column — we draw everything in one row view
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("line"))
        column.title = ""
        column.resizingMask = .autoresizingMask
        tableView.addTableColumn(column)

        tableView.dataSource = context.coordinator
        tableView.delegate = context.coordinator

        context.coordinator.tableView = tableView

        scrollView.documentView = tableView
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let coord = context.coordinator
        let tableView = coord.tableView!

        // Quick identity check: count + first/last element
        let oldIndices = coord.currentIndices
        let dataChanged: Bool
        if let old = oldIndices, old.count == indices.count,
           old.first == indices.first, old.last == indices.last {
            dataChanged = false
        } else {
            dataChanged = true
        }
        coord.parent = self
        coord.currentIndices = indices

        if dataChanged {
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
        weak var tableView: NSTableView?
        var currentIndices: [Int]?

        // Pre-computed colors
        static let threadNSColors: [NSColor] = [
            .systemRed, .systemBlue, .systemGreen, .systemOrange, .systemPurple,
            .systemTeal, .systemPink, .systemMint, .systemIndigo, .systemBrown,
            .systemCyan, .systemYellow
        ]
        static let monoFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        static let monoFontSmall = NSFont.monospacedSystemFont(ofSize: 10, weight: .medium)
        static let monoFontBold = NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)
        static let tertiaryColor = NSColor.tertiaryLabelColor
        static let secondaryColor = NSColor.secondaryLabelColor

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

            cell.configure(with: line, isSelected: parent.selectedId == line.id)
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

    private static let threadColors: [NSColor] = [
        .systemRed, .systemBlue, .systemGreen, .systemOrange, .systemPurple,
        .systemTeal, .systemPink, .systemMint, .systemIndigo, .systemBrown,
        .systemCyan, .systemYellow
    ]
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

    func configure(with line: LogLine, isSelected: Bool) {
        lineNoField.stringValue = "\(line.id + 1)"
        timeField.stringValue = line.timeFormatted ?? ""

        if line.thread >= 0 {
            threadField.stringValue = String(Character(UnicodeScalar(0x21 + line.thread)!))
            threadField.textColor = Self.threadColors[line.thread % Self.threadColors.count]
        } else {
            threadField.stringValue = ""
        }

        levelField.stringValue = line.level.label
        levelBg.layer?.backgroundColor = NSColor(line.level.bgColor.opacity(0.4)).cgColor

        if let dur = line.durationFormatted {
            durationField.stringValue = dur
            durationField.textColor = durationNSColor(line.durationUS)
            durationField.font = line.durationUS >= 1_000_000 ? Self.boldMonoFont : Self.monoFont
        } else {
            durationField.stringValue = ""
        }

        messageField.stringValue = line.message
        messageField.textColor = Self.messageColor(for: line.level)

        // Row background
        wantsLayer = true
        if isSelected {
            layer?.backgroundColor = NSColor.controlAccentColor.withAlphaComponent(0.25).cgColor
        } else {
            layer?.backgroundColor = Self.rowBackground(for: line.level)
        }
    }

    // MARK: - Dark-theme color palette (Tokyo Night inspired, muted & warm-shifted)

    /// Row background — barely-there tints, only for error/warning levels
    private static func rowBackground(for level: LogLevel) -> CGColor? {
        switch level {
        case .error, .exc:
            return NSColor(red: 0.88, green: 0.42, blue: 0.46, alpha: 0.07).cgColor
        case .osErr, .excOs, .fail:
            return NSColor(red: 0.82, green: 0.47, blue: 0.55, alpha: 0.06).cgColor
        case .warn, .cust2:
            return NSColor(red: 0.82, green: 0.68, blue: 0.39, alpha: 0.05).cgColor
        default:
            return nil
        }
    }

    /// Message text color — muted, AA+ contrast on ~#1E1E1E
    private static func messageColor(for level: LogLevel) -> NSColor {
        switch level {
        case .error, .exc:                          // warm coral
            return NSColor(red: 0.88, green: 0.42, blue: 0.46, alpha: 1)
        case .osErr, .excOs, .fail, .dddER:         // dusty rose
            return NSColor(red: 0.82, green: 0.47, blue: 0.55, alpha: 1)
        case .warn, .cust2:                          // warm amber
            return NSColor(red: 0.82, green: 0.68, blue: 0.39, alpha: 1)
        case .sql, .cust1:                           // steel blue
            return NSColor(red: 0.51, green: 0.63, blue: 0.78, alpha: 1)
        case .enter, .leave:                         // dim warm gray
            return NSColor(red: 0.40, green: 0.42, blue: 0.46, alpha: 1)
        case .http:                                  // muted teal
            return NSColor(red: 0.55, green: 0.71, blue: 0.71, alpha: 1)
        case .db:                                    // sage green
            return NSColor(red: 0.51, green: 0.67, blue: 0.51, alpha: 1)
        case .debug, .trace:                         // near-invisible
            return NSColor(red: 0.35, green: 0.37, blue: 0.40, alpha: 1)
        case .auth:                                  // muted lavender
            return NSColor(red: 0.63, green: 0.57, blue: 0.75, alpha: 1)
        case .srvr, .clnt:                           // muted cyan
            return NSColor(red: 0.51, green: 0.67, blue: 0.71, alpha: 1)
        default:
            return .labelColor
        }
    }

    private func durationNSColor(_ us: Int64) -> NSColor {
        if us >= 10_000_000 { return NSColor(red: 0.88, green: 0.42, blue: 0.46, alpha: 1) } // coral
        if us >= 1_000_000  { return NSColor(red: 0.82, green: 0.68, blue: 0.39, alpha: 1) } // amber
        if us >= 100_000    { return NSColor(red: 0.68, green: 0.63, blue: 0.44, alpha: 1) } // muted gold
        return .secondaryLabelColor
    }
}