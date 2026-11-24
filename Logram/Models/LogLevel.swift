import Foundation

/// All 32 UB log levels with display metadata
enum LogLevel: Int, CaseIterable, Identifiable, Sendable {
    case unknown = 0
    case info, debug, trace, warn, error
    case enter, leave
    case osErr, exc, excOs
    case mem, stack, fail
    case sql, cache, res, db
    case http, clnt, srvr, call, ret
    case auth
    case cust1, cust2, cust3, cust4
    case rotat
    case dddER, dddIN
    case mon

    var id: Int { rawValue }

    /// 6-char code as it appears in log file (with trailing spaces)
    var code: String {
        switch self {
        case .unknown: return "??? "
        case .info:    return "info  "
        case .debug:   return "debug "
        case .trace:   return "trace "
        case .warn:    return "warn  "
        case .error:   return "ERROR "
        case .enter:   return " +    "
        case .leave:   return " -    "
        case .osErr:   return "OSERR "
        case .exc:     return "EXC   "
        case .excOs:   return "EXCOS "
        case .mem:     return "mem   "
        case .stack:   return "stack "
        case .fail:    return "fail  "
        case .sql:     return "SQL   "
        case .cache:   return "cache "
        case .res:     return "res   "
        case .db:      return "DB    "
        case .http:    return "http  "
        case .clnt:    return "clnt  "
        case .srvr:    return "srvr  "
        case .call:    return "call  "
        case .ret:     return "ret   "
        case .auth:    return "auth  "
        case .cust1:   return "cust1 "
        case .cust2:   return "cust2 "
        case .cust3:   return "cust3 "
        case .cust4:   return "cust4 "
        case .rotat:   return "rotat "
        case .dddER:   return "dddER "
        case .dddIN:   return "dddIN "
        case .mon:     return "mon   "
        }
    }

    var label: String {
        switch self {
        case .unknown: return "Unknown"
        case .info:    return "Info"
        case .debug:   return "Debug"
        case .trace:   return "Trace"
        case .warn:    return "Warn"
        case .error:   return "Error"
        case .enter:   return "Enter →"
        case .leave:   return "Leave ←"
        case .osErr:   return "OSError"
        case .exc:     return "Exception"
        case .excOs:   return "ExcOS"
        case .mem:     return "Memory"
        case .stack:   return "Stack"
        case .fail:    return "Fail"
        case .sql:     return "SQL"
        case .cache:   return "Cache"
        case .res:     return "Result"
        case .db:      return "DB"
        case .http:    return "HTTP"
        case .clnt:    return "Client"
        case .srvr:    return "Server"
        case .call:    return "Call"
        case .ret:     return "Return"
        case .auth:    return "Auth"
        case .cust1:   return "Params"
        case .cust2:   return "SlowSQL"
        case .cust3:   return "Custom3"
        case .cust4:   return "Custom4"
        case .rotat:   return "LogRotate"
        case .dddER:   return "DDDErr"
        case .dddIN:   return "DDDIn"
        case .mon:     return "Monitor"
        }
    }

    var isError: Bool {
        switch self {
        case .error, .exc, .excOs, .osErr, .fail, .dddER: return true
        default: return false
        }
    }

    /// Build lookup table: 6-char code → LogLevel
    static let codeMap: [String: LogLevel] = {
        var map = [String: LogLevel]()
        for level in LogLevel.allCases {
            map[level.code] = level
        }
        return map
    }()

    /// Fast lookup: 6 bytes packed into UInt64 → LogLevel (no String allocation)
    static let packedLevelMap: [UInt64: LogLevel] = {
        var map = [UInt64: LogLevel]()
        for level in LogLevel.allCases {
            let bytes = Array(level.code.utf8)
            guard bytes.count == 6 else { continue }
            var key: UInt64 = 0
            for b in bytes { key = (key &<< 8) | UInt64(b) }
            map[key] = level
        }
        return map
    }()

    /// Pack 6 bytes at given offset into UInt64 for lookup
    @inline(__always)
    static func packBytes(_ buf: UnsafeBufferPointer<UInt8>, at offset: Int) -> UInt64 {
        var key: UInt64 = 0
        key = (key &<< 8) | UInt64(buf[offset])
        key = (key &<< 8) | UInt64(buf[offset + 1])
        key = (key &<< 8) | UInt64(buf[offset + 2])
        key = (key &<< 8) | UInt64(buf[offset + 3])
        key = (key &<< 8) | UInt64(buf[offset + 4])
        key = (key &<< 8) | UInt64(buf[offset + 5])
        return key
    }
}