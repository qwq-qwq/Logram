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

    /// Timestamp as "YYYY-MM-DD HH:MM:SS.cc" (epochCS already shifted to local time)
    var fullTimestampFormatted: String? {
        guard epochCS >= 0 else { return nil }
        let dayCS: Int64 = 86400 * 100
        var days = epochCS / dayCS
        var timeCS = epochCS - days * dayCS
        if timeCS < 0 { timeCS += dayCS; days -= 1 }
        let (y, mo, d) = LogLine.civilFromDays(Int(days))
        let cs = Int(timeCS % 100)
        let totalSecs = Int(timeCS / 100)
        let s = totalSecs % 60
        let m = (totalSecs / 60) % 60
        let h = (totalSecs / 3600) % 24
        return String(format: "%04d-%02d-%02d %02d:%02d:%02d.%02d", y, mo, d, h, m, s, cs)
    }

    /// Clipboard text: compact UTC timestamp prefix replaced with a readable local
    /// "YYYY-MM-DD HH:MM:SS.cc", keeping thread/level/message. Raw fallback for
    /// lines without a timestamp (console, continuation, unknown).
    func clipboardText(threadPos: Int) -> String {
        guard let ts = fullTimestampFormatted, threadPos >= 2 else { return raw }
        return ts + String(raw.dropFirst(threadPos - 2))
    }

    /// Civil date from days since Unix epoch (inverse of LogParser.daysFromEpoch)
    static func civilFromDays(_ z0: Int) -> (year: Int, month: Int, day: Int) {
        var z = z0
        z += 719468
        let era = (z >= 0 ? z : z - 146096) / 146097
        let doe = z - era * 146097
        let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365
        let y = yoe + era * 400
        let doy = doe - (365 * yoe + yoe / 4 - yoe / 100)
        let mp = (5 * doy + 2) / 153
        let d = doy - (153 * mp + 2) / 5 + 1
        let m = mp < 10 ? mp + 3 : mp - 9
        return (m <= 2 ? y + 1 : y, m, d)
    }
}
