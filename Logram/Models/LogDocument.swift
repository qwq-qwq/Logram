import Foundation
import Observation

/// Main model: holds all parsed lines, statistics, and filter state
@Observable
final class LogDocument {
    // MARK: - Data
    var allLines: [LogLine] = []
    var filteredIndices: [Int] = []
    var fileName: String = ""
    var fileSize: Int64 = 0
    var parser: LogParser?

    // MARK: - Header info
    var ubVersion: String?
    var hostInfo: String?
    var startEpochCS: Int64 = -1
    var endEpochCS: Int64 = -1

    // MARK: - Statistics
    var perThreadCount: [Int] = Array(repeating: 0, count: LogParser.maxThreads)
    var perLevelCount: [Int] = Array(repeating: 0, count: LogLevel.allCases.count)
    var activeThreads: [Int] = [] // thread indices that have events

    // MARK: - Filters
    var enabledLevels: Set<LogLevel> = Set(LogLevel.allCases)
    var enabledThreads: Set<Int> = Set(0..<LogParser.maxThreads)
    var searchPattern: String = ""
    var searchRegex: Bool = false

    // MARK: - Selection
    var selectedLineId: Int?

    // MARK: - Method timing
    struct MethodTiming: Identifiable {
        let id: Int       // line index of enter (+)
        let thread: Int
        let durationMS: Double
        let method: String
    }
    var methodTimings: [MethodTiming] = []

    // MARK: - Loading

    var isLoading = false
    var loadingProgress: Double = 0

    /// Per-chunk parse result for parallel aggregation
    private struct ChunkResult: Sendable {
        let chunkIndex: Int
        let lines: [LogLine]
        let threadCounts: [Int]  // size: maxThreads
        let levelCounts: [Int]   // size: LogLevel.allCases.count
    }

    func load(from url: URL) async {
        isLoading = true
        loadingProgress = 0
        fileName = url.lastPathComponent

        do {
            let attrs = try FileManager.default.attributesOfItem(atPath: url.path)
            fileSize = attrs[.size] as? Int64 ?? 0
        } catch {}

        do {
            // Memory-map the file — no copy into RAM
            let data = try Data(contentsOf: url, options: .mappedIfSafe)
            let totalBytes = data.count

            // Find newline positions at byte level
            let lineRanges = data.withUnsafeBytes { (rawBuf: UnsafeRawBufferPointer) -> [Range<Int>] in
                guard let base = rawBuf.baseAddress?.assumingMemoryBound(to: UInt8.self) else { return [] }
                var ranges: [Range<Int>] = []
                ranges.reserveCapacity(totalBytes / 80) // rough estimate
                var start = 0
                for i in 0..<totalBytes {
                    if base[i] == 0x0A { // '\n'
                        ranges.append(start..<i)
                        start = i + 1
                    }
                }
                if start < totalBytes {
                    ranges.append(start..<totalBytes)
                }
                return ranges
            }

            let total = lineRanges.count

            // Initialize parser from header lines
            let headerCount = min(total, 10)
            var headerLines: [String] = []
            headerLines.reserveCapacity(headerCount)
            for i in 0..<headerCount {
                let range = lineRanges[i]
                let line = data.withUnsafeBytes { (rawBuf: UnsafeRawBufferPointer) -> String in
                    let ptr = rawBuf.baseAddress!.assumingMemoryBound(to: UInt8.self).advanced(by: range.lowerBound)
                    let buf = UnsafeBufferPointer(start: ptr, count: range.count)
                    return String(decoding: buf, as: UTF8.self)
                }
                headerLines.append(line)
            }
            let logParser = LogParser(headerLines: headerLines)
            self.parser = logParser
            self.ubVersion = logParser.ubVersion
            self.hostInfo = logParser.hostInfo
            self.startEpochCS = logParser.startEpochCS

            // Parallel parsing across CPU cores
            let cpuCount = max(ProcessInfo.processInfo.activeProcessorCount, 1)
            let chunkSize = max(total / cpuCount, 1)

            var chunkRanges: [(start: Int, end: Int)] = []
            for c in 0..<cpuCount {
                let start = c * chunkSize
                let end = c == cpuCount - 1 ? total : (c + 1) * chunkSize
                if start < total {
                    chunkRanges.append((start, end))
                }
            }

            loadingProgress = 0.1

            // Parse chunks in parallel
            let results: [ChunkResult] = await withTaskGroup(of: ChunkResult.self) { group in
                for (ci, chunkRange) in chunkRanges.enumerated() {
                    group.addTask {
                        var lines: [LogLine] = []
                        lines.reserveCapacity(chunkRange.end - chunkRange.start)
                        var threadCounts = Array(repeating: 0, count: LogParser.maxThreads)
                        var levelCounts = Array(repeating: 0, count: LogLevel.allCases.count)

                        for i in chunkRange.start..<chunkRange.end {
                            let byteRange = lineRanges[i]
                            // Strip trailing CR if present
                            var endIdx = byteRange.upperBound
                            if endIdx > byteRange.lowerBound {
                                let lastByte = data.withUnsafeBytes { buf in
                                    buf.load(fromByteOffset: endIdx - 1, as: UInt8.self)
                                }
                                if lastByte == 0x0D { endIdx -= 1 }
                            }
                            let lineStr = data.withUnsafeBytes { (rawBuf: UnsafeRawBufferPointer) -> String in
                                let ptr = rawBuf.baseAddress!.assumingMemoryBound(to: UInt8.self).advanced(by: byteRange.lowerBound)
                                let count = endIdx - byteRange.lowerBound
                                let buf = UnsafeBufferPointer(start: ptr, count: count)
                                return String(decoding: buf, as: UTF8.self)
                            }

                            let line = logParser.parseLine(lineStr, index: i)
                            lines.append(line)
                            if line.level != .unknown && line.thread >= 0 {
                                threadCounts[line.thread] += 1
                                levelCounts[line.level.rawValue] += 1
                            }
                        }
                        return ChunkResult(
                            chunkIndex: ci, lines: lines,
                            threadCounts: threadCounts, levelCounts: levelCounts
                        )
                    }
                }

                var collected: [ChunkResult] = []
                collected.reserveCapacity(chunkRanges.count)
                for await result in group {
                    collected.append(result)
                }
                return collected.sorted { $0.chunkIndex < $1.chunkIndex }
            }

            loadingProgress = 0.9

            // Merge results
            var parsed: [LogLine] = []
            parsed.reserveCapacity(total)
            var mergedThreadCounts = Array(repeating: 0, count: LogParser.maxThreads)
            var mergedLevelCounts = Array(repeating: 0, count: LogLevel.allCases.count)

            for result in results {
                parsed.append(contentsOf: result.lines)
                for j in 0..<LogParser.maxThreads {
                    mergedThreadCounts[j] += result.threadCounts[j]
                }
                for j in 0..<LogLevel.allCases.count {
                    mergedLevelCounts[j] += result.levelCounts[j]
                }
            }

            perThreadCount = mergedThreadCounts
            perLevelCount = mergedLevelCounts

            // Find end epochCS from last valid line
            for i in stride(from: parsed.count - 1, through: 0, by: -1) {
                if parsed[i].epochCS >= 0 {
                    endEpochCS = parsed[i].epochCS
                    break
                }
            }

            // Compute active threads
            activeThreads = perThreadCount.enumerated()
                .filter { $0.element > 0 }
                .map { $0.offset }

            allLines = parsed
            applyFilters()
            loadingProgress = 1.0
            isLoading = false

        } catch {
            isLoading = false
            print("Failed to load log: \(error)")
        }
    }

    // MARK: - Filtering

    func applyFilters() {
        let levels = enabledLevels
        let threads = enabledThreads
        let pattern = searchPattern
        let useRegex = searchRegex

        var regex: NSRegularExpression?
        if !pattern.isEmpty && useRegex {
            regex = try? NSRegularExpression(pattern: pattern, options: .caseInsensitive)
        }

        var indices: [Int] = []
        indices.reserveCapacity(allLines.count)

        for i in 0..<allLines.count {
            let line = allLines[i]
            // Level filter
            guard levels.contains(line.level) else { continue }
            // Thread filter
            if line.thread >= 0 {
                guard threads.contains(line.thread) else { continue }
            }
            // Text filter
            if !pattern.isEmpty {
                if let re = regex {
                    let range = NSRange(line.raw.startIndex..., in: line.raw)
                    if re.firstMatch(in: line.raw, range: range) == nil { continue }
                } else {
                    if !line.raw.localizedCaseInsensitiveContains(pattern) { continue }
                }
            }
            indices.append(i)
        }

        filteredIndices = indices
    }

    // MARK: - Search

    enum SearchDirection { case forward, backward }

    func findNext(_ pattern: String, direction: SearchDirection = .forward, from: Int? = nil) -> Int? {
        let indices = filteredIndices
        guard !indices.isEmpty, !pattern.isEmpty else { return nil }
        let re = try? NSRegularExpression(pattern: pattern, options: .caseInsensitive)
        let startIdx = from ?? (direction == .forward ? -1 : indices.count)

        if direction == .forward {
            for i in (startIdx + 1)..<indices.count {
                if matchLine(allLines[indices[i]].raw, pattern: pattern, regex: re) {
                    return i
                }
            }
        } else {
            let upper = min(startIdx, indices.count) - 1
            for i in stride(from: upper, through: 0, by: -1) {
                if matchLine(allLines[indices[i]].raw, pattern: pattern, regex: re) {
                    return i
                }
            }
        }
        return nil
    }

    private func matchLine(_ line: String, pattern: String, regex: NSRegularExpression?) -> Bool {
        if let re = regex {
            let range = NSRange(line.startIndex..., in: line)
            return re.firstMatch(in: line, range: range) != nil
        }
        return line.localizedCaseInsensitiveContains(pattern)
    }

    // MARK: - Method Timing

    func buildMethodTimings() {
        guard parser != nil else { return }
        var timings = [MethodTiming]()

        let lines = allLines
        let count = lines.count

        for i in 0..<count {
            guard lines[i].level == .enter else { continue }
            let th = lines[i].thread
            var depth = 0

            for j in (i + 1)..<count {
                guard lines[j].thread == th else { continue }
                if lines[j].level == .enter {
                    depth += 1
                } else if lines[j].level == .leave {
                    if depth == 0 {
                        let csStart = lines[i].epochCS
                        let csEnd = lines[j].epochCS
                        if csStart >= 0 && csEnd >= 0 {
                            let durationMS = Double(csEnd - csStart) * 10.0
                            if durationMS >= 10 {
                                let method = lines[i].message.trimmingCharacters(in: .whitespaces)
                                timings.append(MethodTiming(
                                    id: i, thread: th,
                                    durationMS: durationMS,
                                    method: method
                                ))
                            }
                        }
                        break
                    } else {
                        depth -= 1
                    }
                }
            }
        }

        timings.sort { $0.durationMS > $1.durationMS }
        methodTimings = timings
    }

    // MARK: - Computed stats

    var totalEvents: Int { allLines.count }

    var filteredCount: Int { filteredIndices.count }

    var httpRequests: Int {
        perLevelCount[LogLevel.http.rawValue]
    }

    var sqlQueries: Int {
        perLevelCount[LogLevel.sql.rawValue]
    }

    var errorCount: Int {
        perLevelCount[LogLevel.error.rawValue] +
        perLevelCount[LogLevel.exc.rawValue] +
        perLevelCount[LogLevel.excOs.rawValue] +
        perLevelCount[LogLevel.osErr.rawValue] +
        perLevelCount[LogLevel.fail.rawValue]
    }

    var duration: TimeInterval? {
        guard startEpochCS >= 0, endEpochCS >= 0 else { return nil }
        return Double(endEpochCS - startEpochCS) / 100.0
    }

    var durationFormatted: String {
        guard let d = duration else { return "—" }
        let h = Int(d) / 3600
        let m = (Int(d) % 3600) / 60
        let s = Int(d) % 60
        return String(format: "%d:%02d:%02d", h, m, s)
    }
}
