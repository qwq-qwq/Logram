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
            // True Tokyo Night palette for badges
            switch level {
            case .error, .exc:               return NSColor(red: 0.969, green: 0.463, blue: 0.557, alpha: 1) // #f7768e
            case .osErr, .excOs, .fail:      return NSColor(red: 0.859, green: 0.443, blue: 0.537, alpha: 1) // #db7093
            case .warn, .cust2:              return NSColor(red: 0.878, green: 0.686, blue: 0.408, alpha: 1) // #e0af68
            case .enter, .leave:             return NSColor(red: 0.620, green: 0.808, blue: 0.416, alpha: 1) // #9ece6a
            case .sql, .cust1:               return NSColor(red: 0.478, green: 0.635, blue: 0.969, alpha: 1) // #7aa2f7
            case .db:                        return NSColor(red: 0.451, green: 0.843, blue: 0.612, alpha: 1) // #73daca
            case .http:                      return NSColor(red: 0.490, green: 0.812, blue: 1.000, alpha: 1) // #7dcfff
            case .srvr, .clnt:               return NSColor(red: 0.698, green: 0.894, blue: 0.925, alpha: 1) // #b4f9ec
            case .auth:                      return NSColor(red: 0.733, green: 0.604, blue: 0.969, alpha: 1) // #bb9af7
            case .debug, .trace:             return NSColor(red: 0.337, green: 0.373, blue: 0.537, alpha: 1) // #565f89
            default:                         return NSColor(red: 0.663, green: 0.694, blue: 0.839, alpha: 1) // #a9b1d6
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
            // True Tokyo Night palette — https://github.com/enkia/tokyo-night-vscode-theme
            switch level {
            case .error, .exc:                          return NSColor(red: 0.969, green: 0.463, blue: 0.557, alpha: 1) // #f7768e red
            case .osErr, .excOs, .fail, .dddER:         return NSColor(red: 0.859, green: 0.443, blue: 0.537, alpha: 1) // #db7093 palevioletred
            case .warn, .cust2:                          return NSColor(red: 0.878, green: 0.686, blue: 0.408, alpha: 1) // #e0af68 yellow
            case .sql, .cust1:                           return NSColor(red: 0.478, green: 0.635, blue: 0.969, alpha: 1) // #7aa2f7 blue
            case .enter, .leave:                         return NSColor(red: 0.620, green: 0.808, blue: 0.416, alpha: 1) // #9ece6a green
            case .http:                                  return NSColor(red: 0.490, green: 0.812, blue: 1.000, alpha: 1) // #7dcfff cyan
            case .db:                                    return NSColor(red: 0.451, green: 0.843, blue: 0.612, alpha: 1) // #73daca teal
            case .debug, .trace:                         return NSColor(red: 0.337, green: 0.373, blue: 0.537, alpha: 1) // #565f89 comment
            case .auth:                                  return NSColor(red: 0.733, green: 0.604, blue: 0.969, alpha: 1) // #bb9af7 magenta
            case .srvr, .clnt:                           return NSColor(red: 0.698, green: 0.894, blue: 0.925, alpha: 1) // #b4f9ec teal bright
            case .info:                                  return NSColor(red: 0.663, green: 0.694, blue: 0.839, alpha: 1) // #a9b1d6 foreground
            default:                                     return NSColor(red: 0.663, green: 0.694, blue: 0.839, alpha: 1) // #a9b1d6 foreground
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
                return NSColor(red: 0.97, green: 0.47, blue: 0.56, alpha: 0.07).cgColor
            case .osErr, .excOs, .fail:
                return NSColor(red: 0.90, green: 0.52, blue: 0.62, alpha: 0.06).cgColor
            case .warn, .cust2:
                return NSColor(red: 0.88, green: 0.69, blue: 0.41, alpha: 0.05).cgColor
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
            if us >= 10_000_000 { return NSColor(red: 0.97, green: 0.47, blue: 0.56, alpha: 1) }
            if us >= 1_000_000  { return NSColor(red: 0.88, green: 0.69, blue: 0.41, alpha: 1) }
            if us >= 100_000    { return NSColor(red: 0.73, green: 0.67, blue: 0.44, alpha: 1) }
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
            if ms >= 10_000 { return Color(red: 0.97, green: 0.47, blue: 0.56) }
            if ms >= 1_000  { return Color(red: 0.88, green: 0.69, blue: 0.41) }
            if ms >= 100    { return Color(red: 0.73, green: 0.67, blue: 0.44) }
            return .primary
        case .tty:
            if ms >= 10_000 { return Color(red: 0.90, green: 0.20, blue: 0.20) }
            if ms >= 1_000  { return Color(red: 1.00, green: 1.00, blue: 0.30) }
            if ms >= 100    { return Color(red: 0.80, green: 0.80, blue: 0.00) }
            return Color(red: 0.20, green: 0.80, blue: 0.20)
        }
    }
}