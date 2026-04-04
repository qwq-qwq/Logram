import SwiftUI
import AppKit

/// Detail panel showing full text of selected log line with SQL syntax highlighting
struct LineDetailView: View {
    let line: LogLine?
    /// JSON params from preceding cust1 line (for SQL parameter substitution)
    let paramsJSON: String?
    @State private var showSubstituted = false

    var body: some View {
        if let line = line {
            VStack(alignment: .leading, spacing: 0) {
                // Header: metadata
                HStack(spacing: 12) {
                    Text("#\(line.id + 1)")
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundStyle(.secondary)
                    if let time = line.timeFormatted {
                        Text(time)
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundStyle(.secondary)
                    }
                    if line.thread >= 0 {
                        Text("Thread \(line.thread)")
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundStyle(.secondary)
                    }
                    Text(line.level.label)
                        .font(.system(size: 11, weight: .medium, design: .monospaced))
                        .padding(.horizontal, 4)
                        .padding(.vertical, 1)
                        .background(
                            RoundedRectangle(cornerRadius: 3)
                                .fill(line.level.bgColor.opacity(0.4))
                        )
                    if let dur = line.durationFormatted {
                        Text(dur)
                            .font(.system(size: 11, weight: .semibold, design: .monospaced))
                    }

                    // Toggle for parameter substitution
                    if (isSQLLine(line.level) || containsSQL(line.message)) && paramsJSON != nil {
                        Toggle(isOn: $showSubstituted) {
                            Text("Params")
                                .font(.system(size: 10))
                        }
                        .toggleStyle(.switch)
                        .controlSize(.mini)
                    }

                    Spacer()

                    Button {
                        NSPasteboard.general.clearContents()
                        let text: String
                        if isSQLLine(line.level) || containsSQL(line.message) {
                            let parsed = SQLStats.parse(line.message)
                            var sql = parsed.sql
                            if showSubstituted, let json = paramsJSON {
                                sql = SQLParamSubstitution.substitute(sql: sql, paramsJSON: json)
                            }
                            text = SQLFormatter.format(sql)
                        } else {
                            text = line.message
                        }
                        NSPasteboard.general.setString(text, forType: .string)
                    } label: {
                        Label("Copy", systemImage: "doc.on.doc")
                            .font(.caption)
                    }
                    .buttonStyle(.borderless)
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .background(Color(nsColor: .controlBackgroundColor))

                Divider()

                // Body: full message with optional SQL formatting + highlighting
                if isSQLLine(line.level) {
                    SQLTextView(message: line.message, paramsJSON: showSubstituted ? paramsJSON : nil)
                } else if containsSQL(line.message) {
                    ErrorWithSQLTextView(message: line.message, paramsJSON: showSubstituted ? paramsJSON : nil)
                } else if line.level == .cust1 || looksLikeJSON(line.message) {
                    JSONTextView(text: formatJSON(line.message))
                } else {
                    PlainTextView(text: line.message, isError: line.level.isError)
                }
            }
        } else {
            Text("Select a log line to view details")
                .font(.caption)
                .foregroundStyle(.tertiary)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }

    private func isSQLLine(_ level: LogLevel) -> Bool {
        level == .sql || level == .cust2
    }

    /// Check if message contains embedded SQL (e.g. error lines with `q=SELECT...`)
    private func containsSQL(_ message: String) -> Bool {
        let trimmed = message.trimmingCharacters(in: .whitespaces)
        return trimmed.contains(" q=") || trimmed.hasPrefix("q=")
    }

    private func looksLikeJSON(_ text: String) -> Bool {
        let trimmed = text.trimmingCharacters(in: .whitespaces)
        return (trimmed.hasPrefix("[{") || trimmed.hasPrefix("{\""))
    }

    private func formatJSON(_ text: String) -> String {
        let trimmed = text.trimmingCharacters(in: .whitespaces)
        // Try standard JSON parsing
        if let data = trimmed.data(using: .utf8),
           let obj = try? JSONSerialization.jsonObject(with: data),
           let pretty = try? JSONSerialization.data(withJSONObject: obj, options: [.prettyPrinted, .sortedKeys]),
           let str = String(data: pretty, encoding: .utf8) {
            return str
        }
        // Truncated JSON — simple visual formatting
        return formatTruncatedJSON(trimmed)
    }

    /// Simple formatting for truncated JSON: newlines before `{` and after `,` at top level
    private func formatTruncatedJSON(_ text: String) -> String {
        var result = ""
        var depth = 0
        var inString = false
        let chars = Array(text)

        for i in 0..<chars.count {
            let c = chars[i]

            if c == "\"" && (i == 0 || chars[i-1] != "\\") {
                inString.toggle()
                result.append(c)
                continue
            }

            if inString {
                result.append(c)
                continue
            }

            switch c {
            case "{", "[":
                result.append(c)
                depth += 1
                if depth <= 2 {
                    result.append("\n")
                    result.append(String(repeating: "  ", count: depth))
                }
            case "}", "]":
                depth = max(0, depth - 1)
                if depth < 2 {
                    result.append("\n")
                    result.append(String(repeating: "  ", count: depth))
                }
                result.append(c)
            case ",":
                result.append(c)
                if depth <= 2 {
                    result.append("\n")
                    result.append(String(repeating: "  ", count: depth))
                }
            default:
                result.append(c)
            }
        }
        return result
    }
}

// MARK: - SQL Parameter Substitution

/// Substitutes bind parameters (:1, :2) with actual values from cust1 JSON
enum SQLParamSubstitution {
    /// Parse cust1 JSON and substitute into SQL
    /// JSON format: {"P1":value, "P2s8":"strvalue", "P3d":"2025-01-01", ...}
    /// P<N> → :<N>, P<N>s<len> → :<N> (string), P<N>d → :<N> (date)
    static func substitute(sql: String, paramsJSON: String) -> String {
        let trimmed = paramsJSON.trimmingCharacters(in: .whitespaces)
        guard let data = trimmed.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else { return sql }

        // Build map: param number → display value
        var paramMap: [Int: String] = [:]
        for (key, value) in obj {
            guard key.hasPrefix("P"), let num = extractParamNumber(key) else { continue }
            let displayValue: String
            if let str = value as? String {
                displayValue = "'\(str)'"
            } else if let num = value as? NSNumber {
                // Check if it's a boolean
                if CFGetTypeID(num) == CFBooleanGetTypeID() {
                    displayValue = num.boolValue ? "true" : "false"
                } else {
                    displayValue = "\(num)"
                }
            } else if value is NSNull {
                displayValue = "NULL"
            } else {
                displayValue = "\(value)"
            }
            paramMap[num] = displayValue
        }

        guard !paramMap.isEmpty else { return sql }

        // Detect format: `:N` (Oracle) or `?` (PostgreSQL/MSSQL)
        if sql.contains(":1") || sql.contains(":2") {
            // Oracle style :N — replace from highest to avoid :1 matching :10
            var result = sql
            for num in paramMap.keys.sorted(by: >) {
                guard let value = paramMap[num] else { continue }
                result = result.replacingOccurrences(of: ":\(num)", with: value)
            }
            return result
        } else if sql.contains("?") {
            // Positional ? — replace sequentially: first ? → P1, second ? → P2, ...
            var result = ""
            var paramIdx = 1
            var i = sql.startIndex
            while i < sql.endIndex {
                if sql[i] == "?" {
                    if let value = paramMap[paramIdx] {
                        result += value
                    } else {
                        result += "?"
                    }
                    paramIdx += 1
                } else {
                    result.append(sql[i])
                }
                i = sql.index(after: i)
            }
            return result
        }
        return sql
    }

    /// Extract parameter number from key like "P1", "P2s8", "P3d"
    private static func extractParamNumber(_ key: String) -> Int? {
        let chars = Array(key)
        guard chars.count >= 2, chars[0] == "P" else { return nil }
        var numStr = ""
        for i in 1..<chars.count {
            if chars[i].isNumber {
                numStr.append(chars[i])
            } else {
                break
            }
        }
        return Int(numStr)
    }
}

// MARK: - SQL Stats Parser

/// Parses trailing stats from SQL log messages: `r=1 t=785 fr=783 c=2`
enum SQLStats {
    struct Entry {
        let label: String
        let value: Int
        let unit: String
        let nsColor: NSColor

        var formattedValue: String {
            if unit == "us" {
                if value >= 1_000_000 {
                    return String(format: "%.1f s", Double(value) / 1_000_000)
                } else if value >= 1_000 {
                    return String(format: "%.1f ms", Double(value) / 1_000)
                } else {
                    return "\(value) us"
                }
            }
            return "\(value)"
        }
    }

    struct Result {
        let sql: String
        let entries: [Entry]
    }

    /// Known stat keys with labels and descriptions
    private static let statDefs: [(key: String, label: String, unit: String)] = [
        ("r",  "Rows",       ""),    // rows returned
        ("t",  "Total",      "us"),  // total time (microseconds)
        ("fr", "Fetch",      "us"),  // fetch/read time (microseconds)
        ("c",  "Cache",      ""),    // cache hits
        ("w",  "Write",      "us"),  // write time
        ("p",  "Prepare",    "us"),  // prepare time
        ("e",  "Exec",       "us"),  // execute time
        ("b",  "Bytes",      ""),    // bytes transferred
    ]

    /// Extract just the SQL part (strip leading stats)
    static func extractSQL(_ message: String) -> String {
        parse(message).sql
    }

    /// Parse UB SQL message: `r=1 t=821 fr=818 c=2 q=SELECT ...`
    /// Stats are at the BEGINNING, SQL follows after `q=`
    static func parse(_ message: String) -> Result {
        let trimmed = message.trimmingCharacters(in: .whitespaces)

        // Message starts with "q=" (no stats prefix)
        if trimmed.hasPrefix("q=") {
            let sqlPart = String(trimmed.dropFirst(2)).trimmingCharacters(in: .whitespaces)
            return Result(sql: sqlPart, entries: [])
        }

        // Look for " q=" which separates stats from SQL
        // Format: "r=1 t=821 fr=818 c=2 q=SELECT ..."
        if let qRange = trimmed.range(of: " q=") {
            let statsPart = String(trimmed[trimmed.startIndex..<qRange.lowerBound])
            let sqlPart = String(trimmed[qRange.upperBound...]).trimmingCharacters(in: .whitespaces)
            let entries = parseStats(statsPart)
            return Result(sql: sqlPart, entries: entries)
        }

        // No "q=" — try to find stats at the beginning anyway
        // (some lines might have just stats or just SQL)
        let entries = parseStats(trimmed)
        if !entries.isEmpty {
            return Result(sql: "", entries: entries)
        }

        return Result(sql: trimmed, entries: [])
    }

    /// Parse space-separated key=value pairs: "r=1 t=821 fr=818 c=2"
    private static func parseStats(_ s: String) -> [Entry] {
        let tokens = s.split(separator: " ")
        var entries: [Entry] = []
        for token in tokens {
            if let entry = parseKeyValue(String(token)) {
                entries.append(entry)
            }
        }
        return entries
    }

    private static func parseKeyValue(_ s: String) -> Entry? {
        guard let eqIdx = s.firstIndex(of: "=") else { return nil }
        let key = String(s[s.startIndex..<eqIdx])
        let valStr = String(s[s.index(after: eqIdx)...])
        guard let value = Int(valStr) else { return nil }
        // Only accept known short keys (avoid matching SQL fragments like TABLE=xxx)
        guard key.count <= 3 && key.allSatisfy({ $0.isLetter }) else { return nil }

        // Match against known stat keys
        for def in statDefs {
            if def.key == key {
                let color: NSColor
                if def.unit == "us" {
                    if value >= 1_000_000 { color = .systemRed }
                    else if value >= 100_000 { color = .systemOrange }
                    else if value >= 10_000 { color = .systemYellow }
                    else { color = .labelColor }
                } else if key == "r" {
                    color = value > 1000 ? .systemOrange : .labelColor
                } else {
                    color = .labelColor
                }
                return Entry(label: def.label, value: value, unit: def.unit, nsColor: color)
            }
        }

        // Unknown key — still show it
        return Entry(label: key, value: value, unit: "", nsColor: .secondaryLabelColor)
    }

    /// Render stats entries as a single NSAttributedString line
    static func formatStatsLine(_ entries: [Entry]) -> NSAttributedString {
        let result = NSMutableAttributedString()
        let labelFont = NSFont.monospacedSystemFont(ofSize: 11, weight: .regular)
        let valueFont = NSFont.monospacedSystemFont(ofSize: 11, weight: .bold)
        let separatorAttrs: [NSAttributedString.Key: Any] = [
            .foregroundColor: NSColor.separatorColor,
            .font: labelFont
        ]

        for (i, entry) in entries.enumerated() {
            if i > 0 {
                result.append(NSAttributedString(string: "  |  ", attributes: separatorAttrs))
            }
            result.append(NSAttributedString(
                string: "\(entry.label): ",
                attributes: [.foregroundColor: NSColor.secondaryLabelColor, .font: labelFont]
            ))
            result.append(NSAttributedString(
                string: entry.formattedValue,
                attributes: [.foregroundColor: entry.nsColor, .font: valueFont]
            ))
        }

        return result
    }
}

// MARK: - SQL Formatter

/// Simple SQL auto-formatter: adds newlines and indentation before major clauses
enum SQLFormatter {
    /// Keywords that start a new line at current indent
    private static let topKeywords: Set<String> = [
        "SELECT", "FROM", "WHERE", "ORDER", "GROUP", "HAVING",
        "LIMIT", "OFFSET", "UNION", "EXCEPT", "INTERSECT",
        "INSERT", "INTO", "VALUES", "UPDATE", "SET", "DELETE",
        "WITH", "RETURNING",
    ]

    /// Keywords that start a new line indented +1
    private static let subKeywords: Set<String> = [
        "AND", "OR", "LEFT", "RIGHT", "INNER", "OUTER",
        "CROSS", "FULL", "JOIN", "ON",
    ]

    /// PL/SQL block keywords that increase indent
    private static let blockOpenKeywords: Set<String> = [
        "DECLARE", "BEGIN", "LOOP", "EXCEPTION",
    ]

    /// PL/SQL keywords that start a new line at current indent
    private static let plsqlLineKeywords: Set<String> = [
        "IF", "ELSIF", "ELSE", "THEN", "WHEN", "END", "FOR", "WHILE",
        "RETURN", "RAISE",
    ]

    static func format(_ sql: String) -> String {
        let trimmed = sql.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return "" }

        let tokens = tokenize(trimmed)
        var result = ""
        var indent = 0
        var prevToken = ""
        var prevUpper = ""
        var lineStart = true

        for token in tokens {
            let upper = token.uppercased()

            // Semicolon — newline after
            if token == ";" {
                result += ";"
                result += "\n" + String(repeating: "    ", count: indent)
                prevToken = token; prevUpper = upper
                lineStart = true
                continue
            }

            // Opening paren — increase indent
            if token == "(" {
                result += token
                indent += 1
                prevToken = token; prevUpper = upper
                lineStart = false
                continue
            }

            // Closing paren — decrease indent
            if token == ")" {
                indent = max(0, indent - 1)
                result += token
                prevToken = token; prevUpper = upper
                lineStart = false
                continue
            }

            // Comma — newline after
            if token == "," {
                result += ","
                result += "\n" + String(repeating: "    ", count: indent + 1)
                prevToken = token; prevUpper = upper
                lineStart = true
                continue
            }

            // PL/SQL block open — newline before, then increase indent
            if blockOpenKeywords.contains(upper) {
                if !result.isEmpty {
                    result += "\n" + String(repeating: "    ", count: indent)
                }
                result += token
                indent += 1
                prevToken = token; prevUpper = upper
                lineStart = false
                continue
            }

            // END — decrease indent, newline before
            if upper == "END" {
                indent = max(0, indent - 1)
                result += "\n" + String(repeating: "    ", count: indent)
                result += token
                prevToken = token; prevUpper = upper
                lineStart = false
                continue
            }

            // PL/SQL line keywords — newline before at current indent
            if plsqlLineKeywords.contains(upper) {
                result += "\n" + String(repeating: "    ", count: indent)
                result += token
                prevToken = token; prevUpper = upper
                lineStart = false
                continue
            }

            // Top-level SQL keyword — new line at current indent
            if topKeywords.contains(upper) {
                // ORDER BY, GROUP BY — keep BY on same line
                if upper == "BY" && (prevUpper == "ORDER" || prevUpper == "GROUP") {
                    result += " " + token
                    prevToken = token; prevUpper = upper
                    lineStart = false
                    continue
                }
                if !result.isEmpty {
                    result += "\n" + String(repeating: "    ", count: indent)
                }
                result += token
                prevToken = token; prevUpper = upper
                lineStart = false
                continue
            }

            // Sub-keyword — new line indented
            if subKeywords.contains(upper) {
                // LEFT JOIN, INNER JOIN etc. — keep on same line
                if upper == "JOIN" && (prevUpper == "LEFT" || prevUpper == "RIGHT" ||
                                       prevUpper == "INNER" || prevUpper == "OUTER" ||
                                       prevUpper == "CROSS" || prevUpper == "FULL") {
                    result += " " + token
                    prevToken = token; prevUpper = upper
                    lineStart = false
                    continue
                }
                result += "\n" + String(repeating: "    ", count: indent) + "  " + token
                prevToken = token; prevUpper = upper
                lineStart = false
                continue
            }

            // Regular token
            if !lineStart && !result.isEmpty {
                result += " "
            }
            result += token
            prevToken = token; prevUpper = upper
            lineStart = false
        }

        return result
    }

    /// Tokenizer: preserves bind params (:1), assignment (:=), operators (<=, >=)
    private static func tokenize(_ sql: String) -> [String] {
        var tokens: [String] = []
        let chars = Array(sql)
        let len = chars.count
        var i = 0

        while i < len {
            if chars[i].isWhitespace { i += 1; continue }

            // String literal 'xxx'
            if chars[i] == "'" {
                var j = i + 1
                while j < len {
                    if chars[j] == "'" {
                        j += 1
                        if j < len && chars[j] == "'" { j += 1; continue }
                        break
                    }
                    j += 1
                }
                tokens.append(String(chars[i..<j]))
                i = j
                continue
            }

            // Quoted identifier [name]
            if chars[i] == "[" {
                var j = i + 1
                while j < len && chars[j] != "]" { j += 1 }
                if j < len { j += 1 }
                tokens.append(String(chars[i..<j]))
                i = j
                continue
            }
            // Quoted identifier "name"
            if chars[i] == "\"" {
                var j = i + 1
                while j < len && chars[j] != "\"" { j += 1 }
                if j < len { j += 1 }
                tokens.append(String(chars[i..<j]))
                i = j
                continue
            }

            // Colon: := (assignment) or :name/:1 (bind param)
            if chars[i] == ":" {
                if i + 1 < len && chars[i + 1] == "=" {
                    tokens.append(":=")
                    i += 2
                    continue
                }
                var j = i + 1
                while j < len && (chars[j].isLetter || chars[j].isNumber || chars[j] == "_") { j += 1 }
                tokens.append(String(chars[i..<j]))
                i = j
                continue
            }

            // Word / identifier / number (include dots for qualified names like ERD.table)
            if chars[i].isLetter || chars[i] == "_" || chars[i] == "@" || chars[i] == "#" || chars[i].isNumber {
                var j = i + 1
                while j < len && (chars[j].isLetter || chars[j].isNumber || chars[j] == "_" || chars[j] == "." || chars[j] == "@" || chars[j] == "#") { j += 1 }
                tokens.append(String(chars[i..<j]))
                i = j
                continue
            }

            // Multi-char operators: >=, <=, <>, !=, ||
            if i + 1 < len {
                let c0 = chars[i], c1 = chars[i + 1]
                if (c0 == ">" && c1 == "=") || (c0 == "<" && c1 == "=") ||
                   (c0 == "<" && c1 == ">") || (c0 == "!" && c1 == "=") ||
                   (c0 == "|" && c1 == "|") {
                    tokens.append(String(chars[i...i+1]))
                    i += 2
                    continue
                }
            }

            // Single char punctuation (; , ( ) = < > + - * / %)
            tokens.append(String(chars[i]))
            i += 1
        }

        return tokens
    }
}

// MARK: - Plain text view with word wrap

struct PlainTextView: NSViewRepresentable {
    let text: String
    let isError: Bool

    /// Format JS stack traces: space-separated `func@/path:line:col` entries → one per line.
    /// Handles: `Stack: func@...`, `stack: 'func@...'`, and `{ func@... }` formats.
    static func formatStackTrace(_ text: String) -> String {
        // Split into tokens by space, preserving spaces
        // Replace space before a stack entry token with newline+indent
        let chars = Array(text)
        let len = chars.count
        var result = ""
        var i = 0

        while i < len {
            if chars[i] == " " && i + 1 < len && chars[i + 1] != " " && chars[i + 1] != "\n" {
                // Peek at next token
                var j = i + 1
                while j < len && chars[j] != " " && chars[j] != "\n" { j += 1 }
                let token = String(chars[(i+1)..<j])
                if isJSStackEntry(token) {
                    result.append("\n  ")
                    i += 1
                    continue
                }
            }
            result.append(chars[i])
            i += 1
        }
        return result
    }

    /// Check if token looks like `funcName@/path/file.js:line:col`
    private static func isJSStackEntry(_ token: String) -> Bool {
        guard let atIdx = token.firstIndex(of: "@"),
              atIdx > token.startIndex  // must have func name before @
        else { return false }
        let path = String(token[token.index(after: atIdx)...])
        return path.contains("/") && (path.contains(".js:") || path.contains(".ts:") || path.contains(".mjs:"))
    }

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSTextView.scrollableTextView()
        let textView = scrollView.documentView as! NSTextView
        textView.isEditable = false
        textView.isSelectable = true
        textView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        textView.backgroundColor = .textBackgroundColor
        textView.textContainerInset = NSSize(width: 8, height: 6)
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticTextReplacementEnabled = false
        // Word wrap
        textView.isHorizontallyResizable = false
        textView.textContainer?.widthTracksTextView = true
        textView.textContainer?.lineBreakMode = .byWordWrapping
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let textView = scrollView.documentView as! NSTextView
        let color: NSColor = isError ? .systemRed : .labelColor
        let attrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.monospacedSystemFont(ofSize: 12, weight: .regular),
            .foregroundColor: color
        ]
        var displayText = text.replacingOccurrences(of: "\\n", with: "\n")
        displayText = Self.formatStackTrace(displayText)
        textView.textStorage?.setAttributedString(NSAttributedString(string: displayText, attributes: attrs))
    }
}

// MARK: - SQL Syntax Highlighting via NSTextView

struct SQLTextView: NSViewRepresentable {
    let message: String
    let paramsJSON: String?

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSTextView.scrollableTextView()
        let textView = scrollView.documentView as! NSTextView
        textView.isEditable = false
        textView.isSelectable = true
        textView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        textView.backgroundColor = .textBackgroundColor
        textView.textContainerInset = NSSize(width: 8, height: 6)
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticTextReplacementEnabled = false
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let textView = scrollView.documentView as! NSTextView
        let parsed = SQLStats.parse(message)
        let result = NSMutableAttributedString()

        // Stats header
        if !parsed.entries.isEmpty {
            let statsLine = SQLStats.formatStatsLine(parsed.entries)
            result.append(statsLine)
            result.append(NSAttributedString(string: "\n\n"))
        }

        // Formatted & highlighted SQL (with optional param substitution)
        var sql = parsed.sql
        if let json = paramsJSON {
            sql = SQLParamSubstitution.substitute(sql: sql, paramsJSON: json)
        }
        let formatted = SQLFormatter.format(sql)
        result.append(SQLHighlighter.highlight(formatted))

        textView.textStorage?.setAttributedString(result)
    }
}

// MARK: - Error with embedded SQL

/// Shows error text prefix + formatted SQL for error lines containing `q=...`
struct ErrorWithSQLTextView: NSViewRepresentable {
    let message: String
    let paramsJSON: String?

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSTextView.scrollableTextView()
        let textView = scrollView.documentView as! NSTextView
        textView.isEditable = false
        textView.isSelectable = true
        textView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        textView.backgroundColor = .textBackgroundColor
        textView.textContainerInset = NSSize(width: 8, height: 6)
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticTextReplacementEnabled = false
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let textView = scrollView.documentView as! NSTextView
        let trimmed = message.trimmingCharacters(in: .whitespaces)
        let result = NSMutableAttributedString()
        let errorFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        let errorColor = NSColor(red: 0.88, green: 0.42, blue: 0.46, alpha: 1)

        // Find where SQL starts: " q=" or "q=" at beginning
        let errorPart: String?
        let sqlMessage: String

        if trimmed.hasPrefix("q=") {
            errorPart = nil
            sqlMessage = trimmed
        } else if let qRange = trimmed.range(of: " q=") {
            errorPart = String(trimmed[trimmed.startIndex..<qRange.lowerBound])
            sqlMessage = String(trimmed[qRange.upperBound...])
        } else {
            // No SQL found — show as error text
            result.append(NSAttributedString(
                string: trimmed,
                attributes: [.foregroundColor: errorColor, .font: errorFont]
            ))
            textView.textStorage?.setAttributedString(result)
            return
        }

        // Error prefix (if any)
        if let err = errorPart {
            var displayErr = err.replacingOccurrences(of: "\\n", with: "\n")
            displayErr = PlainTextView.formatStackTrace(displayErr)
            result.append(NSAttributedString(
                string: displayErr + "\n\n",
                attributes: [.foregroundColor: errorColor, .font: errorFont]
            ))
        }

        // Parse stats + SQL
        let parsed = SQLStats.parse(sqlMessage)

        if !parsed.entries.isEmpty {
            result.append(SQLStats.formatStatsLine(parsed.entries))
            result.append(NSAttributedString(string: "\n\n"))
        }

        var sql = parsed.sql
        if let json = paramsJSON {
            sql = SQLParamSubstitution.substitute(sql: sql, paramsJSON: json)
        }
        let formatted = SQLFormatter.format(sql)
        result.append(SQLHighlighter.highlight(formatted))

        textView.textStorage?.setAttributedString(result)
    }
}

// MARK: - SQL Highlighter

enum SQLHighlighter {
    private static let keywords: Set<String> = [
        "SELECT", "FROM", "WHERE", "AND", "OR", "NOT", "IN", "IS", "NULL",
        "INSERT", "INTO", "VALUES", "UPDATE", "SET", "DELETE",
        "JOIN", "LEFT", "RIGHT", "INNER", "OUTER", "CROSS", "FULL", "ON",
        "ORDER", "BY", "ASC", "DESC", "GROUP", "HAVING",
        "LIMIT", "OFFSET", "TOP", "DISTINCT", "AS", "BETWEEN", "LIKE",
        "EXISTS", "ALL", "ANY", "UNION", "EXCEPT", "INTERSECT",
        "CREATE", "ALTER", "DROP", "TABLE", "INDEX", "VIEW",
        "CASE", "WHEN", "THEN", "ELSE", "END",
        "COUNT", "SUM", "AVG", "MIN", "MAX",
        "CAST", "COALESCE", "NULLIF", "ISNULL",
        "BEGIN", "COMMIT", "ROLLBACK", "TRANSACTION",
        "WITH", "RECURSIVE", "OVER", "PARTITION", "ROW", "ROWS",
        "FETCH", "NEXT", "FIRST", "LAST", "ONLY",
        "PRIMARY", "KEY", "FOREIGN", "REFERENCES", "CONSTRAINT",
        "DEFAULT", "CHECK", "UNIQUE", "CASCADE",
        "EXEC", "EXECUTE", "DECLARE", "CURSOR", "OPEN", "CLOSE",
        "IF", "WHILE", "RETURN", "RETURNS", "FUNCTION", "PROCEDURE",
        "DECLARE", "BEGIN", "EXCEPTION", "LOOP", "ELSIF", "RAISE",
        "THEN", "ELSEIF", "CURSOR", "FOR", "EXIT", "CONTINUE",
    ]

    private static let types: Set<String> = [
        "INT", "INTEGER", "BIGINT", "SMALLINT", "TINYINT",
        "VARCHAR", "NVARCHAR", "CHAR", "NCHAR", "TEXT", "NTEXT",
        "FLOAT", "REAL", "DECIMAL", "NUMERIC", "MONEY",
        "DATE", "DATETIME", "DATETIME2", "TIMESTAMP", "TIME",
        "BIT", "BOOLEAN", "BLOB", "CLOB", "BINARY", "VARBINARY",
        "UUID", "JSONB", "JSON", "XML",
    ]

    private static let keywordColor = NSColor.systemBlue
    private static let typeColor = NSColor.systemCyan
    private static let stringColor = NSColor.systemGreen
    private static let numberColor = NSColor.systemOrange
    private static let commentColor = NSColor.systemGray
    private static let operatorColor = NSColor.systemPurple
    private static let bindColor = NSColor.systemPink
    private static let defaultColor = NSColor.labelColor
    private static let font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
    private static let boldFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)

    static func highlight(_ sql: String) -> NSAttributedString {
        let result = NSMutableAttributedString()
        let defaultAttrs: [NSAttributedString.Key: Any] = [.foregroundColor: defaultColor, .font: font]

        let chars = Array(sql.unicodeScalars)
        let len = chars.count
        var i = 0

        while i < len {
            let c = chars[i]

            // Single-line comment --
            if c == "-" && i + 1 < len && chars[i+1] == "-" {
                let start = i
                i += 2
                while i < len && chars[i] != "\n" { i += 1 }
                let text = substring(sql, from: start, to: i)
                result.append(NSAttributedString(string: text, attributes: [.foregroundColor: commentColor, .font: font]))
                continue
            }

            // Block comment /* ... */
            if c == "/" && i + 1 < len && chars[i+1] == "*" {
                let start = i
                i += 2
                while i + 1 < len && !(chars[i] == "*" && chars[i+1] == "/") { i += 1 }
                if i + 1 < len { i += 2 } else { i = len }
                let text = substring(sql, from: start, to: i)
                result.append(NSAttributedString(string: text, attributes: [.foregroundColor: commentColor, .font: font]))
                continue
            }

            // String literal 'xxx'
            if c == "'" {
                let start = i
                i += 1
                while i < len {
                    if chars[i] == "'" {
                        i += 1
                        if i < len && chars[i] == "'" { i += 1; continue }
                        break
                    }
                    i += 1
                }
                let text = substring(sql, from: start, to: i)
                result.append(NSAttributedString(string: text, attributes: [.foregroundColor: stringColor, .font: font]))
                continue
            }

            // Colon: := (assignment operator) or :name/:1 (bind parameter)
            if c == ":" {
                if i + 1 < len && chars[i + 1] == "=" {
                    // := assignment
                    result.append(NSAttributedString(string: ":=", attributes: [.foregroundColor: operatorColor, .font: font]))
                    i += 2
                    continue
                }
                let start = i
                i += 1
                while i < len && (chars[i].properties.isAlphabetic || chars[i].properties.numericType != nil || chars[i] == "_") { i += 1 }
                let text = substring(sql, from: start, to: i)
                result.append(NSAttributedString(string: text, attributes: [.foregroundColor: bindColor, .font: boldFont]))
                continue
            }

            // Number
            if c.properties.numericType != nil || (c == "." && i + 1 < len && chars[i+1].properties.numericType != nil) {
                let start = i
                while i < len && (chars[i].properties.numericType != nil || chars[i] == ".") { i += 1 }
                let text = substring(sql, from: start, to: i)
                result.append(NSAttributedString(string: text, attributes: [.foregroundColor: numberColor, .font: font]))
                continue
            }

            // Word (identifier or keyword)
            if c.properties.isAlphabetic || c == "_" || c == "@" || c == "#" {
                let start = i
                i += 1
                while i < len && (chars[i].properties.isAlphabetic || chars[i].properties.numericType != nil || chars[i] == "_" || chars[i] == "@" || chars[i] == "#") { i += 1 }
                let text = substring(sql, from: start, to: i)
                let upper = text.uppercased()
                if keywords.contains(upper) {
                    result.append(NSAttributedString(string: text, attributes: [.foregroundColor: keywordColor, .font: boldFont]))
                } else if types.contains(upper) {
                    result.append(NSAttributedString(string: text, attributes: [.foregroundColor: typeColor, .font: font]))
                } else {
                    result.append(NSAttributedString(string: text, attributes: defaultAttrs))
                }
                continue
            }

            // Operators
            if c == "=" || c == "<" || c == ">" || c == "!" || c == "+" || c == "*" || c == "/" || c == "%" {
                result.append(NSAttributedString(string: String(c), attributes: [.foregroundColor: operatorColor, .font: font]))
                i += 1
                continue
            }

            // Default: punctuation, whitespace, etc.
            result.append(NSAttributedString(string: String(c), attributes: defaultAttrs))
            i += 1
        }

        return result
    }

    private static func substring(_ s: String, from: Int, to: Int) -> String {
        let scalars = s.unicodeScalars
        let start = scalars.index(scalars.startIndex, offsetBy: from)
        let end = scalars.index(scalars.startIndex, offsetBy: to)
        return String(scalars[start..<end])
    }
}

// MARK: - JSON Text View with syntax highlighting

struct JSONTextView: NSViewRepresentable {
    let text: String

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSTextView.scrollableTextView()
        let textView = scrollView.documentView as! NSTextView
        textView.isEditable = false
        textView.isSelectable = true
        textView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        textView.backgroundColor = .textBackgroundColor
        textView.textContainerInset = NSSize(width: 8, height: 6)
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.isAutomaticTextReplacementEnabled = false
        textView.isHorizontallyResizable = false
        textView.textContainer?.widthTracksTextView = true
        textView.textContainer?.lineBreakMode = .byWordWrapping
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let textView = scrollView.documentView as! NSTextView
        textView.textStorage?.setAttributedString(JSONHighlighter.highlight(text))
    }
}

// MARK: - JSON Highlighter

enum JSONHighlighter {
    private static let font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
    private static let boldFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)
    private static let keyColor = NSColor(red: 0.51, green: 0.63, blue: 0.78, alpha: 1)     // steel blue
    private static let stringColor = NSColor(red: 0.51, green: 0.67, blue: 0.51, alpha: 1)   // sage green
    private static let numberColor = NSColor(red: 0.82, green: 0.68, blue: 0.39, alpha: 1)   // warm amber
    private static let boolColor = NSColor(red: 0.63, green: 0.57, blue: 0.75, alpha: 1)     // muted lavender
    private static let bracketColor = NSColor.secondaryLabelColor
    private static let defaultColor = NSColor.labelColor

    static func highlight(_ text: String) -> NSAttributedString {
        let result = NSMutableAttributedString()
        let chars = Array(text)
        let len = chars.count
        var i = 0
        // Track if next string is a key (after `{`, `,`, or at start, before `:`)
        var expectKey = true

        while i < len {
            let c = chars[i]

            // Whitespace / newlines
            if c.isWhitespace || c.isNewline {
                let start = i
                while i < len && (chars[i].isWhitespace || chars[i].isNewline) { i += 1 }
                result.append(NSAttributedString(
                    string: String(chars[start..<i]),
                    attributes: [.font: font, .foregroundColor: defaultColor]
                ))
                continue
            }

            // String
            if c == "\"" {
                let start = i
                i += 1
                while i < len && chars[i] != "\"" {
                    if chars[i] == "\\" && i + 1 < len { i += 1 }
                    i += 1
                }
                if i < len { i += 1 } // closing quote

                let str = String(chars[start..<i])
                // Look ahead: is this a key? (followed by optional whitespace then `:`)
                var j = i
                while j < len && chars[j].isWhitespace { j += 1 }
                let isKey = j < len && chars[j] == ":"

                let color = isKey ? keyColor : stringColor
                let f = isKey ? boldFont : font
                result.append(NSAttributedString(string: str, attributes: [.font: f, .foregroundColor: color]))
                if isKey { expectKey = false }
                continue
            }

            // Number
            if c == "-" || c.isNumber {
                let start = i
                if c == "-" { i += 1 }
                while i < len && (chars[i].isNumber || chars[i] == "." || chars[i] == "e" || chars[i] == "E" || chars[i] == "+" || chars[i] == "-") {
                    // Avoid consuming `-` that's not part of exponent
                    if (chars[i] == "+" || chars[i] == "-") && i > start + 1 && chars[i-1] != "e" && chars[i-1] != "E" { break }
                    i += 1
                }
                let str = String(chars[start..<i])
                result.append(NSAttributedString(string: str, attributes: [.font: font, .foregroundColor: numberColor]))
                continue
            }

            // true/false/null
            if c == "t" || c == "f" || c == "n" {
                let remaining = String(chars[i...])
                var matched: String?
                if remaining.hasPrefix("true") { matched = "true" }
                else if remaining.hasPrefix("false") { matched = "false" }
                else if remaining.hasPrefix("null") { matched = "null" }
                if let m = matched {
                    result.append(NSAttributedString(string: m, attributes: [.font: boldFont, .foregroundColor: boolColor]))
                    i += m.count
                    continue
                }
            }

            // Brackets and structural
            if c == "{" || c == "[" {
                result.append(NSAttributedString(string: String(c), attributes: [.font: font, .foregroundColor: bracketColor]))
                expectKey = (c == "{")
                i += 1
                continue
            }
            if c == "}" || c == "]" {
                result.append(NSAttributedString(string: String(c), attributes: [.font: font, .foregroundColor: bracketColor]))
                i += 1
                continue
            }
            if c == ":" {
                result.append(NSAttributedString(string: ":", attributes: [.font: font, .foregroundColor: bracketColor]))
                expectKey = false
                i += 1
                continue
            }
            if c == "," {
                result.append(NSAttributedString(string: ",", attributes: [.font: font, .foregroundColor: bracketColor]))
                expectKey = true
                i += 1
                continue
            }

            // Default
            result.append(NSAttributedString(string: String(c), attributes: [.font: font, .foregroundColor: defaultColor]))
            i += 1
        }

        return result
    }
}