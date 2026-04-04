import Foundation

/// Single parsed log line
struct LogLine: Identifiable, Sendable {
    let id: Int          // line index in file (0-based)
    let raw: String      // original line text
    let level: LogLevel
    let thread: Int      // thread index (0-based), -1 if unknown
    let epochCS: Int64   // centiseconds since Unix epoch, -1 if unknown
    let messageOffset: UInt16 // byte offset where message starts in raw

    // HTTP fields (parsed from http-level lines)
    var httpMethod: String?
    var httpPath: String?
    var httpStatus: Int?
    var isHttpOpen: Bool { raw.contains(" -> ") && level == .http }
    var isHttpClose: Bool { raw.contains(" <- ") && level == .http }

    // Duration from leave lines (microseconds), -1 if not a leave line
    var durationUS: Int64 = -1

    /// Message text after level code (computed from raw + offset)
    var message: String {
        let offset = Int(messageOffset)
        guard offset > 0, offset < raw.count else { return raw }
        return String(raw.dropFirst(offset))
    }

    /// Duration formatted as human-readable string
    var durationFormatted: String? {
        guard durationUS >= 0 else { return nil }
        if durationUS >= 1_000_000 {
            return String(format: "%.1fs", Double(durationUS) / 1_000_000)
        } else if durationUS >= 1_000 {
            return String(format: "%.1fms", Double(durationUS) / 1_000)
        } else {
            return "\(durationUS)µs"
        }
    }

    /// Timestamp formatted as HH:MM:SS.cc (pure arithmetic, no Calendar)
    var timeFormatted: String? {
        guard epochCS >= 0 else { return nil }
        let dayCS: Int64 = 86400 * 100
        let timeCS = ((epochCS % dayCS) + dayCS) % dayCS // handle negatives
        let cs = Int(timeCS % 100)
        let totalSecs = Int(timeCS / 100)
        let s = totalSecs % 60
        let m = (totalSecs / 60) % 60
        let h = (totalSecs / 3600) % 24
        return String(format: "%02d:%02d:%02d.%02d", h, m, s, cs)
    }
}
