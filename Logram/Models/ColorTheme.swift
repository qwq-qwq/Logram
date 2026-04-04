import AppKit
import SwiftUI

enum ColorTheme: String, CaseIterable, Identifiable {
    case tokyoNight
    case tty

    var id: String { rawValue }

    var displayName: String {
        switch self {
        case .tokyoNight: return "Tokyo Night"
        case .tty: return "TTY"
        }
    }

    // MARK: - Thread Colors

    var threadNSColors: [NSColor] {
        switch self {
        case .tokyoNight:
            return [
                .systemRed, .systemBlue, .systemGreen, .systemOrange, .systemPurple,
                .systemTeal, .systemPink, .systemMint, .systemIndigo, .systemBrown,
                .systemCyan, .systemYellow
            ]
        case .tty:
            return [
                NSColor(red: 0.90, green: 0.20, blue: 0.20, alpha: 1), // red
                NSColor(red: 0.40, green: 0.50, blue: 1.00, alpha: 1), // blue
                NSColor(red: 0.20, green: 0.80, blue: 0.20, alpha: 1), // green
                NSColor(red: 0.80, green: 0.80, blue: 0.00, alpha: 1), // yellow
                NSColor(red: 0.80, green: 0.20, blue: 0.80, alpha: 1), // magenta
                NSColor(red: 0.20, green: 0.80, blue: 0.80, alpha: 1), // cyan
                NSColor(red: 1.00, green: 0.40, blue: 0.60, alpha: 1), // pink
                NSColor(red: 0.20, green: 0.90, blue: 0.60, alpha: 1), // mint
                NSColor(red: 0.50, green: 0.30, blue: 1.00, alpha: 1), // indigo
                NSColor(red: 0.70, green: 0.50, blue: 0.30, alpha: 1), // brown
                NSColor(red: 0.30, green: 0.70, blue: 1.00, alpha: 1), // light blue
                NSColor(red: 1.00, green: 1.00, blue: 0.30, alpha: 1), // bright yellow
            ]
        }
    }

    func threadNSColor(_ idx: Int) -> NSColor {
        let colors = threadNSColors
        return colors[idx % colors.count]
    }

    func threadSwiftUIColor(_ idx: Int) -> Color {
        Color(nsColor: threadNSColor(idx))
    }

    // MARK: - Level Badge

    func levelBadgeNSColor(for level: LogLevel) -> NSColor {
        switch self {
        case .tokyoNight:
            switch level {
            case .warn, .cust2:              return NSColor(red: 0.75, green: 0.50, blue: 0.50, alpha: 1)
            case .error, .exc:               return NSColor(red: 1.00, green: 0.50, blue: 0.50, alpha: 1)
            case .osErr, .excOs, .fail:      return NSColor(red: 0.94, green: 0.50, blue: 0.75, alpha: 1)
            case .enter:                     return NSColor(red: 0.75, green: 0.86, blue: 0.75, alpha: 1)
            case .leave:                     return NSColor(red: 0.75, green: 0.86, blue: 0.86, alpha: 1)
            case .sql, .cust1:               return NSColor(red: 0.78, green: 0.78, blue: 1.00, alpha: 1)
            case .db:                        return NSColor(red: 0.50, green: 0.86, blue: 0.50, alpha: 1)
            case .http:                      return NSColor(red: 0.50, green: 0.50, blue: 0.86, alpha: 1)
            case .srvr:                      return NSColor(red: 0.00, green: 0.82, blue: 0.86, alpha: 1)
            case .debug, .trace, .auth:      return NSColor(red: 0.86, green: 0.86, blue: 0.86, alpha: 1)
            default:                         return NSColor(red: 0.75, green: 0.75, blue: 0.86, alpha: 1)
            }
        case .tty:
            switch level {
            case .error, .exc, .osErr, .excOs, .fail:
                return NSColor(red: 0.80, green: 0.00, blue: 0.00, alpha: 1)
            case .warn, .cust2:
                return NSColor(red: 0.80, green: 0.80, blue: 0.00, alpha: 1)
            case .sql:
                return NSColor(red: 0.70, green: 0.70, blue: 0.00, alpha: 1)
            case .cust1:
                return NSColor(red: 0.70, green: 0.00, blue: 0.70, alpha: 1)
            case .enter, .leave:
                return NSColor(red: 0.00, green: 0.60, blue: 0.00, alpha: 1)
            case .http:
                return NSColor(red: 0.00, green: 0.60, blue: 0.60, alpha: 1)
            case .db:
                return NSColor(red: 0.00, green: 0.60, blue: 0.00, alpha: 1)
            case .debug, .trace:
                return NSColor(red: 0.40, green: 0.40, blue: 0.40, alpha: 1)
            case .auth:
                return NSColor(red: 0.60, green: 0.00, blue: 0.60, alpha: 1)
            case .srvr, .clnt:
                return NSColor(red: 0.00, green: 0.60, blue: 0.60, alpha: 1)
            default:
                return NSColor(red: 0.50, green: 0.50, blue: 0.50, alpha: 1)
            }
        }
    }

    func levelBadgeColor(for level: LogLevel) -> Color {
        Color(nsColor: levelBadgeNSColor(for: level))
    }

    // MARK: - Message Color

    func messageColor(for level: LogLevel) -> NSColor {
        switch self {
        case .tokyoNight:
            switch level {
            case .error, .exc:                          return NSColor(red: 0.88, green: 0.42, blue: 0.46, alpha: 1)
            case .osErr, .excOs, .fail, .dddER:         return NSColor(red: 0.82, green: 0.47, blue: 0.55, alpha: 1)
            case .warn, .cust2:                          return NSColor(red: 0.82, green: 0.68, blue: 0.39, alpha: 1)
            case .sql, .cust1:                           return NSColor(red: 0.51, green: 0.63, blue: 0.78, alpha: 1)
            case .enter, .leave:                         return NSColor(red: 0.40, green: 0.42, blue: 0.46, alpha: 1)
            case .http:                                  return NSColor(red: 0.55, green: 0.71, blue: 0.71, alpha: 1)
            case .db:                                    return NSColor(red: 0.51, green: 0.67, blue: 0.51, alpha: 1)
            case .debug, .trace:                         return NSColor(red: 0.35, green: 0.37, blue: 0.40, alpha: 1)
            case .auth:                                  return NSColor(red: 0.63, green: 0.57, blue: 0.75, alpha: 1)
            case .srvr, .clnt:                           return NSColor(red: 0.51, green: 0.67, blue: 0.71, alpha: 1)
            default:                                     return .labelColor
            }
        case .tty:
            switch level {
            case .error, .exc, .osErr, .excOs, .fail, .dddER:
                return NSColor(red: 0.90, green: 0.20, blue: 0.20, alpha: 1)
            case .warn, .cust2:
                return NSColor(red: 1.00, green: 1.00, blue: 0.30, alpha: 1)
            case .sql:
                return NSColor(red: 0.80, green: 0.80, blue: 0.00, alpha: 1)
            case .cust1:
                return NSColor(red: 0.80, green: 0.20, blue: 0.80, alpha: 1)
            case .enter, .leave:
                return NSColor(red: 0.20, green: 0.80, blue: 0.20, alpha: 1)
            case .http:
                return NSColor(red: 0.20, green: 0.80, blue: 0.80, alpha: 1)
            case .db:
                return NSColor(red: 0.60, green: 0.80, blue: 0.20, alpha: 1)
            case .debug, .trace:
                return NSColor(red: 0.50, green: 0.50, blue: 0.50, alpha: 1)
            case .auth:
                return NSColor(red: 0.70, green: 0.40, blue: 0.90, alpha: 1)
            case .srvr, .clnt:
                return NSColor(red: 0.20, green: 0.80, blue: 0.80, alpha: 1)
            case .info:
                return NSColor(red: 0.85, green: 0.85, blue: 0.85, alpha: 1)
            default:
                return NSColor(red: 0.75, green: 0.75, blue: 0.75, alpha: 1)
            }
        }
    }

    // MARK: - Row Background

    func rowBackground(for level: LogLevel) -> CGColor? {
        switch self {
        case .tokyoNight:
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
        case .tty:
            switch level {
            case .error, .exc, .osErr, .excOs, .fail:
                return NSColor(red: 0.90, green: 0.00, blue: 0.00, alpha: 0.10).cgColor
            case .warn, .cust2:
                return NSColor(red: 0.80, green: 0.80, blue: 0.00, alpha: 0.06).cgColor
            default:
                return nil
            }
        }
    }

    // MARK: - Duration Color

    func durationNSColor(_ us: Int64) -> NSColor {
        switch self {
        case .tokyoNight:
            if us >= 10_000_000 { return NSColor(red: 0.88, green: 0.42, blue: 0.46, alpha: 1) }
            if us >= 1_000_000  { return NSColor(red: 0.82, green: 0.68, blue: 0.39, alpha: 1) }
            if us >= 100_000    { return NSColor(red: 0.68, green: 0.63, blue: 0.44, alpha: 1) }
            return .secondaryLabelColor
        case .tty:
            if us >= 10_000_000 { return NSColor(red: 0.90, green: 0.20, blue: 0.20, alpha: 1) }
            if us >= 1_000_000  { return NSColor(red: 1.00, green: 1.00, blue: 0.30, alpha: 1) }
            if us >= 100_000    { return NSColor(red: 0.80, green: 0.80, blue: 0.00, alpha: 1) }
            return NSColor(red: 0.20, green: 0.80, blue: 0.20, alpha: 1)
        }
    }

    func durationSwiftUIColor(_ ms: Double) -> Color {
        switch self {
        case .tokyoNight:
            if ms >= 10_000 { return Color(red: 0.88, green: 0.42, blue: 0.46) }
            if ms >= 1_000  { return Color(red: 0.82, green: 0.68, blue: 0.39) }
            if ms >= 100    { return Color(red: 0.68, green: 0.63, blue: 0.44) }
            return .primary
        case .tty:
            if ms >= 10_000 { return Color(red: 0.90, green: 0.20, blue: 0.20) }
            if ms >= 1_000  { return Color(red: 1.00, green: 1.00, blue: 0.30) }
            if ms >= 100    { return Color(red: 0.80, green: 0.80, blue: 0.00) }
            return Color(red: 0.20, green: 0.80, blue: 0.20)
        }
    }
}