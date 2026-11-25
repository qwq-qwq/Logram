# Changelog

All notable changes to Logram are listed here, newest first.
Versions match the git tags (`v1.2.15` etc.); release builds for all three platforms are on the [Releases](https://github.com/qwq-qwq/Logram/releases) page.
Platform labels show where a change applies; unlabeled entries apply to macOS, Windows and Linux.

## [Unreleased]

## [1.2.16] - 2026-11-26

### Added
- **macOS:** the update window shows what is new in the offered version (its section of this changelog); the "You're up to date" alert has a Version History button that opens the full changelog.

## [1.2.15] - 2026-11-26

### Fixed
- **macOS:** a Logram window no longer re-renders about 200 times per second while it is focused. The loop used a full CPU core, kept main menu submenus from opening and made the system Edit submenus (AutoFill, Writing Tools) flicker.
- **macOS:** mouse drag-select no longer snaps back to the previously selected row when you click one row and then drag from another; the detail panel now follows the drag.

### Changed
- **macOS:** Theme is a submenu again: View → Theme → Tokyo Night / TTY.

## [1.2.14] - 2026-11-25

### Fixed
- **macOS:** selecting a range of rows with the mouse works again; rows no longer flicker while dragging.

## [1.2.13] - 2026-11-24

### Changed
- **macOS:** theme and Show Duration Column (Cmd+D) moved to the View menu and work even without an active window; Tools keeps Statistics and Method Timing.
- **macOS:** themes paint the whole window like on Windows and Linux (table, detail text, sidebar, toolbar and status bar backgrounds); the window always uses dark appearance.
- **macOS:** level icons in the filter sidebar replaced with small squares in the level color.

### Fixed
- **macOS:** the theme could not be switched from the menu.
- **macOS:** turning on the duration column computes timings in every open window, not only the focused one.

## [1.2.12] - 2026-11-23

### Changed
- Documentation: corrected the description of mORMot1 timestamps; added Linux paths and shortcuts, Statistics and Install & updates sections, open calls and the 99 s leave duration cap in Method Timing.

## [1.2.11] - 2026-11-11

### Fixed
- Durations use the real mORMot1 line time: the last two timestamp digits are milliseconds shr 4 (16 ms steps), not centiseconds. The time column still shows them as written.
- Calls longer than 99 s are timed correctly: mORMot caps the duration on the leave line, so Logram falls back to the line timestamps when more time has passed.

## [1.2.10] - 2026-09-06

### Added
- **macOS:** automatic updates via Sparkle and a Logram → Check for Updates… menu item. About shows the real app version.

## [1.2.9] - 2026-08-11

### Fixed
- Method timings are no longer computed twice.

## [1.2.8] - 2026-08-10

### Fixed
- Call durations prefer the high-resolution value from the leave line.

## [1.2.7] - 2026-08-10

### Fixed
- Durations of nested leave lines indented with TAB are parsed correctly.

## [1.2.6] - 2026-06-15

### Changed
- Copied lines (Cmd/Ctrl+C) start with a readable local timestamp `YYYY-MM-DD HH:MM:SS.cc` instead of the compact UTC one.

## [1.2.5] - 2026-06-13

### Added
- Method Timing lists open calls (a `+` with no matching `-`: hung, still running or cut off by the end of the log), with a toggle to show them.

### Changed
- Times are shown in the viewer's local time zone (UB writes logs in UTC).
- **Linux:** the package is named `Logram-linux.deb`.

### Fixed
- **macOS:** opening a file from Finder no longer leaves stray empty windows; empty windows from earlier sessions are not restored.

## [1.2.4] - 2026-06-01

### Fixed
- **macOS:** the Statistics and Method Timing toolbar buttons opened each other's dialogs.

## [1.2.3] - 2026-05-21

### Added
- Statistics shows the start and end time of the log.

## [1.2.2] - 2026-05-14

### Added
- Toolbar buttons for Statistics and Method Timing.
- **Windows:** the detail panel shows the line metadata (time, thread, level) above the message.

### Changed
- Search and error navigation clear an active Focus on Call first.

## [1.2.1] - 2026-05-11

### Fixed
- **macOS:** signing entitlements for the Developer ID / notarized build.

## [1.2.0] - 2026-05-09

### Added
- **Linux:** first release (GTK 4, Ubuntu 24.04+, `.deb` package) with filters, search, detail panel with SQL/JSON highlighting and parameter substitution, method timing, Focus on Call, themes and drag-select.
- Statistics dialog (Cmd/Ctrl+I).
- **macOS:** the app is signed with Developer ID and notarized.

### Changed
- **Windows:** better display of SQL parameters in the detail panel.

## [1.1.16] - 2026-05-07

### Fixed
- Clearing Focus on Call keeps the thread checkboxes the way they were shown.

## [1.1.15] - 2026-05-05

### Fixed
- **Windows:** the filter sidebar no longer shows a horizontal scrollbar.

## [1.1.14] - 2026-05-05

### Fixed
- **Windows:** filter sidebar links no longer overlap the scrollbar.

## [1.1.13] - 2026-05-04

### Fixed
- **Windows:** filter sidebar margin next to the scrollbar.

## [1.1.12] - 2026-05-02

### Added
- Focus on Call (Cmd/Ctrl+Shift+E, Esc to clear): narrows the view to the enclosing `+/-` call on the same thread.

## [1.1.11] - 2026-05-02

### Fixed
- **Windows:** no flicker while resizing the window or dragging the splitters.

## [1.1.10] - 2026-05-01

### Added
- Right-click menu in the filter sidebar: All, None, Only this.

## [1.1.9] - 2026-05-01

### Changed
- "No matches" message in English.
- **Windows:** smoother vertical scrolling.

## [1.1.8] - 2026-04-21

### Added
- Help → Logram Documentation opens the docs in your language.

### Fixed
- **Windows:** filter group labels and All/None state follow the visible rows.

## [1.1.7] - 2026-04-20

### Fixed
- **Windows:** error navigation buttons are drawn correctly.

## [1.1.6] - 2026-04-20

### Added
- **Windows:** mouse drag-select with auto-scroll.

## [1.1.5] - 2026-04-20

### Added
- **macOS:** level icons in the filter sidebar.

### Changed
- The selected line stays selected and centered when filter checkboxes are toggled.

## [1.1.4] - 2026-04-18

### Fixed
- **Windows:** switching the theme repaints the detail panel and the filter sidebar.

## [1.1.3] - 2026-04-18

### Fixed
- **macOS:** Method Timing follows the duration column setting.

## [1.1.2] - 2026-04-18

### Changed
- Release builds are made by CI with Xcode 26.

## [1.1.1] - 2026-04-18

### Added
- CI builds for macOS and Windows.
- **macOS:** search wraps around and reports when nothing matches.

### Fixed
- **Windows:** selection and search fixes.

## [1.1] - 2026-04-17

### Added
- First tagged release: native macOS app and Windows port.
- Four log formats detected automatically (mORMot1, mORMot2 HiRes, journald, console), 32 UB log levels.
- Filters by level and thread with presets, search, error navigation.
- Detail panel with SQL and JSON highlighting and cust1 parameter substitution.
- Method Timing, jump to the matching `+/-` (Cmd/Ctrl+J), duration column, copy of selected lines.
- Tokyo Night and TTY themes.

[Unreleased]: https://github.com/qwq-qwq/Logram/compare/v1.2.16...HEAD
[1.2.16]: https://github.com/qwq-qwq/Logram/compare/v1.2.15...v1.2.16
[1.2.15]: https://github.com/qwq-qwq/Logram/compare/v1.2.14...v1.2.15
[1.2.14]: https://github.com/qwq-qwq/Logram/compare/v1.2.13...v1.2.14
[1.2.13]: https://github.com/qwq-qwq/Logram/compare/v1.2.12...v1.2.13
[1.2.12]: https://github.com/qwq-qwq/Logram/compare/v1.2.11...v1.2.12
[1.2.11]: https://github.com/qwq-qwq/Logram/compare/v1.2.10...v1.2.11
[1.2.10]: https://github.com/qwq-qwq/Logram/compare/v1.2.9...v1.2.10
[1.2.9]: https://github.com/qwq-qwq/Logram/compare/v1.2.8...v1.2.9
[1.2.8]: https://github.com/qwq-qwq/Logram/compare/v1.2.7...v1.2.8
[1.2.7]: https://github.com/qwq-qwq/Logram/compare/v1.2.6...v1.2.7
[1.2.6]: https://github.com/qwq-qwq/Logram/compare/v1.2.5...v1.2.6
[1.2.5]: https://github.com/qwq-qwq/Logram/compare/v1.2.4...v1.2.5
[1.2.4]: https://github.com/qwq-qwq/Logram/compare/v1.2.3...v1.2.4
[1.2.3]: https://github.com/qwq-qwq/Logram/compare/v1.2.2...v1.2.3
[1.2.2]: https://github.com/qwq-qwq/Logram/compare/v1.2.1...v1.2.2
[1.2.1]: https://github.com/qwq-qwq/Logram/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/qwq-qwq/Logram/compare/v1.1.16...v1.2.0
[1.1.16]: https://github.com/qwq-qwq/Logram/compare/v1.1.15...v1.1.16
[1.1.15]: https://github.com/qwq-qwq/Logram/compare/v1.1.14...v1.1.15
[1.1.14]: https://github.com/qwq-qwq/Logram/compare/v1.1.13...v1.1.14
[1.1.13]: https://github.com/qwq-qwq/Logram/compare/v1.1.12...v1.1.13
[1.1.12]: https://github.com/qwq-qwq/Logram/compare/v1.1.11...v1.1.12
[1.1.11]: https://github.com/qwq-qwq/Logram/compare/v1.1.10...v1.1.11
[1.1.10]: https://github.com/qwq-qwq/Logram/compare/v1.1.9...v1.1.10
[1.1.9]: https://github.com/qwq-qwq/Logram/compare/v1.1.8...v1.1.9
[1.1.8]: https://github.com/qwq-qwq/Logram/compare/v1.1.7...v1.1.8
[1.1.7]: https://github.com/qwq-qwq/Logram/compare/v1.1.6...v1.1.7
[1.1.6]: https://github.com/qwq-qwq/Logram/compare/v1.1.5...v1.1.6
[1.1.5]: https://github.com/qwq-qwq/Logram/compare/v1.1.4...v1.1.5
[1.1.4]: https://github.com/qwq-qwq/Logram/compare/v1.1.3...v1.1.4
[1.1.3]: https://github.com/qwq-qwq/Logram/compare/v1.1.2...v1.1.3
[1.1.2]: https://github.com/qwq-qwq/Logram/compare/v1.1.1...v1.1.2
[1.1.1]: https://github.com/qwq-qwq/Logram/compare/v1.1...v1.1.1
[1.1]: https://github.com/qwq-qwq/Logram/releases/tag/v1.1
