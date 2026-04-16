// golden_gen.swift — Generate reference parse output for cross-check with the C++ port.
//
// Usage:
//   swift tools/golden_gen.swift <path/to/input.log> > tests/golden/input.expected.ndjson
//
// Each output line is a JSON object describing one parsed line. Keys:
//   id, level (code), thread, epochCS, messageOffset, durationUS?, httpMethod?, httpPath?, httpStatus?
//
// This deliberately uses only Foundation + the same parser logic as the
// Mac app (inlined here so the tool can run stand-alone with `swift`).

import Foundation

struct GoldenLine: Encodable {
    let id: Int
    let level: String
    let thread: Int
    let epochCS: Int64
    let messageOffset: Int
    let durationUS: Int64?
    let httpMethod: String?
    let httpPath: String?
    let httpStatus: Int?
}

// MARK: Minimal inlined parser (mirrors Logram/Models/LogParser.swift)

enum LogLevel: UInt8 {
    case unknown = 0, info, debug, trace, warn, error, enterL, leaveL
    case osErr, exc, excOs, mem, stack, fail
    case sql, cache, res, db, http, clnt, srvr, call, ret, auth
    case cust1, cust2, cust3, cust4, rotat, dddER, dddIN, mon

    static let codes: [(String, LogLevel)] = [
        ("???   ", .unknown),
        ("info  ", .info),   ("debug ", .debug),   ("trace ", .trace),
        ("warn  ", .warn),   ("ERROR ", .error),
        (" +    ", .enterL), (" -    ", .leaveL),
        ("OSERR ", .osErr),  ("EXC   ", .exc),     ("EXCOS ", .excOs),
        ("mem   ", .mem),    ("stack ", .stack),   ("fail  ", .fail),
        ("SQL   ", .sql),    ("cache ", .cache),   ("res   ", .res),
        ("DB    ", .db),     ("http  ", .http),    ("clnt  ", .clnt),
        ("srvr  ", .srvr),   ("call  ", .call),    ("ret   ", .ret),
        ("auth  ", .auth),
        ("cust1 ", .cust1),  ("cust2 ", .cust2),
        ("cust3 ", .cust3),  ("cust4 ", .cust4),
        ("rotat ", .rotat),  ("dddER ", .dddER),   ("dddIN ", .dddIN),
        ("mon   ", .mon),
    ]
}

func levelCode(for level: LogLevel) -> String {
    LogLevel.codes.first { $0.1 == level }?.0 ?? "???   "
}

func pack6(_ bytes: [UInt8], at offset: Int) -> UInt64 {
    var k: UInt64 = 0
    for i in 0..<6 { k = (k << 8) | UInt64(bytes[offset + i]) }
    return k
}

let packedMap: [UInt64: LogLevel] = {
    var m: [UInt64: LogLevel] = [:]
    for (code, lv) in LogLevel.codes {
        let bytes = Array(code.utf8)
        m[pack6(bytes, at: 0)] = lv
    }
    return m
}()

func daysFromEpoch(_ year: Int, _ month: Int, _ day: Int) -> Int {
    var y = year
    if month <= 2 { y -= 1 }
    let era = (y >= 0 ? y : y - 399) / 400
    let yoe = y - era * 400
    let doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy
    return era * 146097 + doe - 719468
}

enum Format { case mormot1, mormot2, journald, console }

struct ParserState {
    var format: Format = .mormot1
    var thPos: Int = 19
    var hiResFreq: Int64 = 1000
    var startEpochCS: Int64 = -1
}

func detectFormat(_ headerLines: [String]) -> ParserState {
    var s = ParserState()
    if headerLines.count > 1 {
        if let r = headerLines[1].range(of: "Freq=") {
            let f = headerLines[1][r.upperBound...].prefix(while: { $0.isNumber })
            if let v = Int64(f), v > 0 { s.hiResFreq = v / 1000 }
        }
    }
    for i in 0..<min(headerLines.count, 5) {
        if let r = headerLines[i].range(of: "PRTL ") {
            let dateStr = String(headerLines[i][r.upperBound...])
                .trimmingCharacters(in: .whitespaces)
            let fm = ISO8601DateFormatter()
            fm.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
            if let d = fm.date(from: dateStr) {
                s.startEpochCS = Int64(d.timeIntervalSince1970 * 100)
            } else {
                fm.formatOptions = [.withInternetDateTime]
                if let d = fm.date(from: dateStr) {
                    s.startEpochCS = Int64(d.timeIntervalSince1970 * 100)
                }
            }
        }
    }
    let probeIdx = headerLines.count > 5 ? 4 :
                   (headerLines.count > 4 ? 4 : min(headerLines.count - 1, 1))
    if probeIdx >= 0 && probeIdx < headerLines.count {
        let probe = headerLines[probeIdx]
        if probe.hasPrefix("00000000000000") {
            s.format = .mormot2; s.thPos = 18
        } else if probe.count > 31 && probe.dropFirst(26).hasPrefix("+") {
            s.format = .journald
            if let b = probe.range(of: "]:") {
                s.thPos = probe.distance(from: probe.startIndex, to: b.upperBound) + 2
            }
        } else if probe.count > 2 && probe.hasPrefix("  ") {
            s.format = .console; s.thPos = 2
        }
    }
    return s
}

func parseTimestamp(_ buf: [UInt8], _ s: ParserState) -> Int64 {
    switch s.format {
    case .mormot1:
        guard buf.count >= 17, buf[0] >= 0x30, buf[0] <= 0x39 else { return -1 }
        func d2(_ i: Int) -> Int { Int(buf[i] - 0x30) * 10 + Int(buf[i+1] - 0x30) }
        func d4(_ i: Int) -> Int {
            Int(buf[i] - 0x30) * 1000 + Int(buf[i+1] - 0x30) * 100 +
            Int(buf[i+2] - 0x30) * 10 + Int(buf[i+3] - 0x30)
        }
        let days = daysFromEpoch(d4(0), d2(4), d2(6))
        return Int64(days) * 8_640_000 +
               Int64(d2(9)) * 360_000 + Int64(d2(11)) * 6_000 +
               Int64(d2(13)) * 100 + Int64(d2(15))
    case .mormot2:
        guard s.startEpochCS >= 0, buf.count >= 16 else { return -1 }
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
        return s.startEpochCS + Int64(Double(ticks) / Double(s.hiResFreq) / 10.0)
    case .journald:
        guard buf.count >= 31 else { return -1 }
        func d2(_ i: Int) -> Int { Int(buf[i] - 0x30) * 10 + Int(buf[i+1] - 0x30) }
        func d4(_ i: Int) -> Int {
            Int(buf[i] - 0x30) * 1000 + Int(buf[i+1] - 0x30) * 100 +
            Int(buf[i+2] - 0x30) * 10 + Int(buf[i+3] - 0x30)
        }
        var cs = 0
        if buf.count > 19, buf[19] == 0x2E {
            if buf.count > 20 { cs += Int(buf[20] - 0x30) * 10 }
            if buf.count > 21 { cs += Int(buf[21] - 0x30) }
        }
        var tzOffset: Int64 = 0
        for i in 19..<min(buf.count, 32) {
            if buf[i] == 0x2B || buf[i] == 0x2D {
                let sign: Int64 = buf[i] == 0x2D ? -1 : 1
                if i + 4 < buf.count {
                    tzOffset = sign * (Int64(d2(i+1)) * 360_000 + Int64(d2(i+3)) * 6_000)
                }
                break
            }
        }
        let days = daysFromEpoch(d4(0), d2(5), d2(8))
        return Int64(days) * 8_640_000 + Int64(d2(11)) * 360_000 +
               Int64(d2(14)) * 6_000 + Int64(d2(17)) * 100 + Int64(cs) - tzOffset
    case .console:
        return -1
    }
}

func parseDuration(_ buf: [UInt8], from start: Int) -> Int64 {
    var i = start
    while i < buf.count && buf[i] == 0x20 { i += 1 }
    var parts: [Int64] = []
    var current: Int64 = 0
    var hasDigits = false
    while i < buf.count && parts.count < 3 {
        let c = buf[i]
        if c >= 0x30 && c <= 0x39 {
            current = current * 10 + Int64(c - 0x30); hasDigits = true
        } else if c == 0x2E {
            guard hasDigits else { return -1 }
            parts.append(current); current = 0; hasDigits = false
        } else { break }
        i += 1
    }
    if hasDigits { parts.append(current) }
    guard parts.count == 3 else { return -1 }
    return parts[0] * 1_000_000 + parts[1] * 1_000 + parts[2]
}

func parseHTTP(_ buf: [UInt8], from start: Int, raw: String) -> (String?, String?, Int?) {
    let len = buf.count
    guard len > start + 4 else { return (nil, nil, nil) }
    for i in start..<(len - 3) {
        guard buf[i] == 0x20 && buf[i+2] == 0x20 else { continue }
        if buf[i+1] == 0x2D && i + 3 < len && buf[i+3] == 0x3E {
            let afterArrow = i + 4
            if afterArrow < len {
                let msg = String(raw.dropFirst(afterArrow))
                let parts = msg.split(separator: " ", maxSplits: 1)
                let m = parts.count >= 1 ? String(parts[0]) : nil
                let p = parts.count >= 2 ? String(parts[1]) : nil
                return (m, p, nil)
            }
            return (nil, nil, nil)
        } else if buf[i+1] == 0x3C && i + 3 < len && buf[i+3] == 0x2D {
            let afterArrow = i + 4
            var j = afterArrow
            while j < len && buf[j] == 0x20 { j += 1 }
            if j + 2 < len,
               buf[j] >= 0x30, buf[j] <= 0x39,
               buf[j+1] >= 0x30, buf[j+1] <= 0x39,
               buf[j+2] >= 0x30, buf[j+2] <= 0x39 {
                let s = Int(buf[j] - 0x30) * 100 +
                        Int(buf[j+1] - 0x30) * 10 +
                        Int(buf[j+2] - 0x30)
                return (nil, nil, s)
            }
            return (nil, nil, nil)
        }
    }
    return (nil, nil, nil)
}

// MARK: Driver

guard CommandLine.arguments.count >= 2 else {
    FileHandle.standardError.write("Usage: swift tools/golden_gen.swift <path/to/input.log>\n".data(using: .utf8)!)
    exit(2)
}

let url = URL(fileURLWithPath: CommandLine.arguments[1])
guard let data = try? Data(contentsOf: url),
      let text = String(data: data, encoding: .utf8) else {
    FileHandle.standardError.write("cannot read file\n".data(using: .utf8)!)
    exit(1)
}

let lines = text.split(separator: "\n", omittingEmptySubsequences: false)
    .map { $0.hasSuffix("\r") ? String($0.dropLast()) : String($0) }
let state = detectFormat(Array(lines.prefix(10)))

let enc = JSONEncoder()
enc.outputFormatting = [.sortedKeys]

for (idx, line) in lines.enumerated() {
    let bytes = Array(line.utf8)
    let len = bytes.count

    var level: LogLevel = .unknown
    var thread: Int = -1
    var epochCS: Int64 = -1
    var msgOffset: Int = 0
    var durationUS: Int64? = nil
    var httpMethod: String? = nil
    var httpPath: String? = nil
    var httpStatus: Int? = nil

    if len > state.thPos + 8 {
        let th = bytes[state.thPos]
        if th >= 0x21 && th < 0x21 + 64 {
            let levelStart = state.thPos + 2
            if len > levelStart + 6 {
                let key = pack6(bytes, at: levelStart)
                if let lv = packedMap[key] {
                    level = lv
                    thread = Int(th - 0x21)
                    msgOffset = min(levelStart + 6, len)
                    epochCS = parseTimestamp(bytes, state)
                    if level == .leaveL && msgOffset < len {
                        durationUS = parseDuration(bytes, from: msgOffset)
                    }
                    if level == .http && msgOffset < len {
                        let (m, p, s) = parseHTTP(bytes, from: msgOffset, raw: line)
                        httpMethod = m; httpPath = p; httpStatus = s
                    }
                }
            }
        }
    }

    let gl = GoldenLine(
        id: idx,
        level: levelCode(for: level),
        thread: thread,
        epochCS: epochCS,
        messageOffset: msgOffset,
        durationUS: durationUS,
        httpMethod: httpMethod,
        httpPath: httpPath,
        httpStatus: httpStatus
    )
    if let json = try? enc.encode(gl), let s = String(data: json, encoding: .utf8) {
        print(s)
    }
}