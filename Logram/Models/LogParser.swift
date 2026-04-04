import Foundation

/// Detects log format and parses UB log lines.
/// Supports mORMot1 (YYYYMMDD HHMMSSCC), mORMot2 (hex HiRes), journald (ISO8601), console (no date).
final class LogParser: Sendable {
    enum Format {
        case mormot1    // 20210727 08260933  $ SQL   ...
        case mormot2    // 0000000000000C72  ! SQL   ...
        case journald   // 2022-10-27T00:00:00.358664+0300 ub[657518]:  $ SQL ...
        case console    // " SQL   ... (no date, from stdin)
    }

    let format: Format
    /// Position of thread character in line
    let thPos: Int
    /// For mORMot2 HiRes: timer frequency in ms
    let hiResFreq: Int64
    /// Log start timestamp (centiseconds since epoch)
    let startEpochCS: Int64
    /// Log start timestamp as Date (for header display)
    let startedAt: Date?

    // Header info
    let ubVersion: String?
    let hostInfo: String?

    /// Thread char base: '!' = 0x21
    static let thBase: UInt8 = 0x21 // '!'
    static let maxThreads = 64

    init(headerLines: [String]) {
        var version: String?
        var host: String?
        var startDate: Date?
        var startCS: Int64 = -1
        var freq: Int64 = 1000
        var fmt: Format = .mormot1
        var threadPos = 19 // default for mORMot1

        if headerLines.count > 0 {
            version = headerLines[0]
        }
        if headerLines.count > 1 {
            host = headerLines[1]
            if let freqRange = headerLines[1].range(of: "Freq=") {
                let freqStr = headerLines[1][freqRange.upperBound...]
                    .prefix(while: { $0.isNumber })
                if let f = Int64(freqStr), f > 0 {
                    freq = f / 1000
                }
            }
        }

        // Find PRTL line for start date
        for i in 0..<min(headerLines.count, 5) {
            let line = headerLines[i]
            if let prtlRange = line.range(of: "PRTL ") {
                let dateStr = String(line[prtlRange.upperBound...]).trimmingCharacters(in: .whitespaces)
                startDate = Self.parseISO(dateStr)
                if let d = startDate {
                    startCS = Int64(d.timeIntervalSince1970 * 100)
                }
            }
        }

        // Detect format from first data line
        let probeIdx = headerLines.count > 5 ? 4 : (headerLines.count > 4 ? 4 : min(headerLines.count - 1, 1))
        if probeIdx >= 0 && probeIdx < headerLines.count {
            let probe = headerLines[probeIdx]
            if probe.hasPrefix("00000000000000") {
                fmt = .mormot2
                threadPos = 18
            } else if probe.count > 31 && probe.dropFirst(26).hasPrefix("+") {
                fmt = .journald
                if let bracketIdx = probe.range(of: "]:") {
                    threadPos = probe.distance(from: probe.startIndex, to: bracketIdx.upperBound) + 2
                }
            } else if probe.count > 2 && probe.hasPrefix("  ") {
                fmt = .console
                threadPos = 2
            }
        }

        self.format = fmt
        self.thPos = threadPos
        self.hiResFreq = freq
        self.startEpochCS = startCS
        self.startedAt = startDate
        self.ubVersion = version
        self.hostInfo = host
    }

    /// Parse a single log line — fast byte-level parsing
    func parseLine(_ line: String, index: Int) -> LogLine {
        return line.withCString(encodedAs: UTF8.self) { ptr in
            let len = line.utf8.count
            let buf = UnsafeBufferPointer(start: ptr, count: len)
            return parseLineBytes(buf, raw: line, index: index)
        }
    }

    /// Core parser working on raw bytes
    private func parseLineBytes(_ buf: UnsafeBufferPointer<UInt8>, raw: String, index: Int) -> LogLine {
        let len = buf.count
        let unknown = LogLine(
            id: index, raw: raw, level: .unknown,
            thread: -1, epochCS: -1, messageOffset: 0
        )

        guard len > thPos + 8 else { return unknown }

        // Extract thread
        let thChar = buf[thPos]
        guard thChar >= LogParser.thBase && thChar < LogParser.thBase + UInt8(LogParser.maxThreads) else {
            return unknown
        }
        let threadIdx = Int(thChar - LogParser.thBase)

        // Extract level (6 chars starting at thPos + 2)
        let levelStart = thPos + 2
        guard len > levelStart + 6 else { return unknown }

        let packedKey = LogLevel.packBytes(buf, at: levelStart)
        guard let level = LogLevel.packedLevelMap[packedKey] else { return unknown }

        // Message offset (after level code)
        let msgStart = levelStart + 6
        let msgOffset = UInt16(min(msgStart, len))

        // Parse timestamp
        let epochCS = parseTimestampBytes(buf)

        var result = LogLine(
            id: index, raw: raw, level: level,
            thread: threadIdx, epochCS: epochCS, messageOffset: msgOffset
        )

        // Parse duration from leave lines
        if level == .leave && msgStart < len {
            result.durationUS = parseDurationBytes(buf, from: msgStart)
        }

        // Parse HTTP fields
        if level == .http && msgStart < len {
            parseHTTPFieldsBytes(buf, from: msgStart, raw: raw, into: &result)
        }

        return result
    }

    // MARK: - Timestamp Parsing

    private func parseTimestampBytes(_ buf: UnsafeBufferPointer<UInt8>) -> Int64 {
        switch format {
        case .mormot1:  return parseMormot1Bytes(buf)
        case .mormot2:  return parseMormot2Bytes(buf)
        case .journald: return parseJournaldBytes(buf)
        case .console:  return -1
        }
    }

    /// YYYYMMDD HHMMSSCC → epochCS (pure arithmetic, no Calendar)
    private func parseMormot1Bytes(_ buf: UnsafeBufferPointer<UInt8>) -> Int64 {
        guard buf.count >= 17 else { return -1 }
        let b = buf
        // Quick check: first char is digit
        guard b[0] >= 0x30 && b[0] <= 0x39 else { return -1 }

        @inline(__always) func d2(_ i: Int) -> Int {
            Int(b[i] - 0x30) * 10 + Int(b[i+1] - 0x30)
        }
        @inline(__always) func d4(_ i: Int) -> Int {
            Int(b[i] - 0x30) * 1000 + Int(b[i+1] - 0x30) * 100 +
            Int(b[i+2] - 0x30) * 10 + Int(b[i+3] - 0x30)
        }

        let year   = d4(0)
        let month  = d2(4)
        let day    = d2(6)
        let hour   = d2(9)
        let minute = d2(11)
        let sec    = d2(13)
        let cs     = d2(15)

        let days = Self.daysFromEpoch(year: year, month: month, day: day)
        let baseCS = Int64(days) * 8_640_000 + Int64(hour) * 360_000
        return baseCS + Int64(minute) * 6_000 + Int64(sec) * 100 + Int64(cs)
    }

    /// Hex HiRes timer → epochCS (offset from startEpochCS)
    private func parseMormot2Bytes(_ buf: UnsafeBufferPointer<UInt8>) -> Int64 {
        guard startEpochCS >= 0, buf.count >= 16 else { return -1 }
        var ticks: UInt64 = 0
        for i in 0..<16 {
            let c = buf[i]
            let v: UInt64
            if c >= 0x30 && c <= 0x39 { v = UInt64(c - 0x30) }
            else if c >= 0x41 && c <= 0x46 { v = UInt64(c - 0x41 + 10) }
            else if c >= 0x61 && c <= 0x66 { v = UInt64(c - 0x61 + 10) }
            else { return -1 }
            ticks = ticks &* 16 &+ v
        }
        let ms = Double(ticks) / Double(hiResFreq)
        return startEpochCS + Int64(ms / 10.0)
    }

    /// ISO8601 with timezone → epochCS
    private func parseJournaldBytes(_ buf: UnsafeBufferPointer<UInt8>) -> Int64 {
        guard buf.count >= 31 else { return -1 }
        // 2022-10-27T00:00:00.358664+0300
        @inline(__always) func d2(_ i: Int) -> Int {
            Int(buf[i] - 0x30) * 10 + Int(buf[i+1] - 0x30)
        }
        @inline(__always) func d4(_ i: Int) -> Int {
            Int(buf[i] - 0x30) * 1000 + Int(buf[i+1] - 0x30) * 100 +
            Int(buf[i+2] - 0x30) * 10 + Int(buf[i+3] - 0x30)
        }

        let year  = d4(0)
        let month = d2(5)
        let day   = d2(8)
        let hour  = d2(11)
        let minute = d2(14)
        let sec   = d2(17)

        // Fractional seconds (optional)
        var cs = 0
        if buf.count > 19 && buf[19] == 0x2E { // '.'
            // Parse up to 2 digits for centiseconds
            if buf.count > 20 { cs += Int(buf[20] - 0x30) * 10 }
            if buf.count > 21 { cs += Int(buf[21] - 0x30) }
        }

        // Timezone offset: +HHMM or -HHMM
        var tzOffsetCS: Int64 = 0
        // Find +/- after the time part
        for i in 19..<min(buf.count, 32) {
            if buf[i] == 0x2B || buf[i] == 0x2D { // '+' or '-'
                let sign: Int64 = buf[i] == 0x2D ? -1 : 1
                if i + 4 < buf.count {
                    let tzH = d2(i + 1)
                    let tzM = d2(i + 3)
                    tzOffsetCS = sign * (Int64(tzH) * 360_000 + Int64(tzM) * 6_000)
                }
                break
            }
        }

        let days = Self.daysFromEpoch(year: year, month: month, day: day)
        let baseCS = Int64(days) * 8_640_000 + Int64(hour) * 360_000 +
                     Int64(minute) * 6_000 + Int64(sec) * 100 + Int64(cs)
        return baseCS - tzOffsetCS
    }

    // MARK: - Duration Parsing

    /// Parse duration from leave line: "SS.MMM.UUU" → microseconds
    private func parseDurationBytes(_ buf: UnsafeBufferPointer<UInt8>, from start: Int) -> Int64 {
        // Skip leading whitespace
        var i = start
        while i < buf.count && buf[i] == 0x20 { i += 1 }

        // Parse "SS.MMM.UUU" — three dot-separated groups
        var parts: [Int64] = []
        var current: Int64 = 0
        var hasDigits = false
        while i < buf.count && parts.count < 3 {
            let c = buf[i]
            if c >= 0x30 && c <= 0x39 {
                current = current * 10 + Int64(c - 0x30)
                hasDigits = true
            } else if c == 0x2E { // '.'
                guard hasDigits else { return -1 }
                parts.append(current)
                current = 0
                hasDigits = false
            } else {
                break
            }
            i += 1
        }
        if hasDigits { parts.append(current) }

        guard parts.count == 3 else { return -1 }
        return parts[0] * 1_000_000 + parts[1] * 1_000 + parts[2]
    }

    // MARK: - HTTP Parsing

    private func parseHTTPFieldsBytes(_ buf: UnsafeBufferPointer<UInt8>, from start: Int, raw: String, into line: inout LogLine) {
        // Look for " -> " or " <- " pattern
        let len = buf.count
        guard len > start + 4 else { return }

        for i in start..<(len - 3) {
            if buf[i] == 0x20 && buf[i+2] == 0x20 { // " ? "
                if buf[i+1] == 0x2D && i + 3 < len && buf[i+3] == 0x3E { // " -> "
                    // Request: after " -> " find METHOD path
                    let afterArrow = i + 4
                    if afterArrow < len {
                        let msg = String(raw.dropFirst(afterArrow))
                        let parts = msg.split(separator: " ", maxSplits: 1)
                        if parts.count >= 1 { line.httpMethod = String(parts[0]) }
                        if parts.count >= 2 { line.httpPath = String(parts[1]) }
                    }
                    return
                } else if buf[i+1] == 0x3C && i + 3 < len && buf[i+3] == 0x2D { // " <- "
                    // Response: after " <- " parse status code (3 digits)
                    let afterArrow = i + 4
                    // Skip whitespace
                    var j = afterArrow
                    while j < len && buf[j] == 0x20 { j += 1 }
                    if j + 2 < len &&
                       buf[j] >= 0x30 && buf[j] <= 0x39 &&
                       buf[j+1] >= 0x30 && buf[j+1] <= 0x39 &&
                       buf[j+2] >= 0x30 && buf[j+2] <= 0x39 {
                        line.httpStatus = Int(buf[j] - 0x30) * 100 +
                                          Int(buf[j+1] - 0x30) * 10 +
                                          Int(buf[j+2] - 0x30)
                    }
                    return
                }
            }
        }
    }

    // MARK: - Date Arithmetic

    /// Days since Unix epoch (1970-01-01) using Hinnant's algorithm
    /// Pure integer arithmetic, no Calendar needed
    @inline(__always)
    static func daysFromEpoch(year: Int, month: Int, day: Int) -> Int {
        var y = year
        if month <= 2 { y -= 1 }
        let era = (y >= 0 ? y : y - 399) / 400
        let yoe = y - era * 400
        let doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1
        let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy
        return era * 146097 + doe - 719468
    }

    private static func parseISO(_ str: String) -> Date? {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        if let d = formatter.date(from: str) { return d }
        formatter.formatOptions = [.withInternetDateTime]
        if let d = formatter.date(from: str) { return d }
        let df = DateFormatter()
        df.dateFormat = "yyyy-MM-dd'T'HH:mm:ss"
        df.timeZone = TimeZone(identifier: "UTC")
        return df.date(from: str)
    }
}
