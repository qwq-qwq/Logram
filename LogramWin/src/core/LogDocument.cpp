#include "core/LogDocument.h"
#include <thread>
#include <algorithm>
#include <cstring>
#include <cmath>

std::string LogDocument::FileName() const {
    auto& p = file_.Path();
    auto pos = p.find_last_of(L"\\/");
    if (pos == std::wstring::npos) pos = 0; else pos++;
    std::string result;
    for (size_t i = pos; i < p.size(); ++i)
        result.push_back(static_cast<char>(p[i]));
    return result;
}

int LogDocument::ErrorCount() const {
    return perLevelCount_[static_cast<int>(LogLevel::Error)] +
           perLevelCount_[static_cast<int>(LogLevel::Exc)] +
           perLevelCount_[static_cast<int>(LogLevel::ExcOs)] +
           perLevelCount_[static_cast<int>(LogLevel::OsErr)] +
           perLevelCount_[static_cast<int>(LogLevel::Fail)];
}

double LogDocument::DurationSeconds() const {
    if (startEpochCS_ < 0 || endEpochCS_ < 0) return -1.0;
    return static_cast<double>(endEpochCS_ - startEpochCS_) / 100.0;
}

std::string LogDocument::DurationFormatted() const {
    double d = DurationSeconds();
    if (d < 0) return "\xe2\x80\x94"; // em-dash
    int total = static_cast<int>(d);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    return buf;
}

int64_t LogDocument::GetDuration(uint32_t lineId) const {
    // Binary search in sorted durationLines_
    auto it = std::lower_bound(durationLines_.begin(), durationLines_.end(), lineId,
        [](const LogLineDuration& d, uint32_t id) { return d.lineId < id; });
    if (it != durationLines_.end() && it->lineId == lineId) return it->durationUS;
    return -1;
}

const LogLineHttp* LogDocument::GetHttp(uint32_t lineId) const {
    auto it = std::lower_bound(httpLines_.begin(), httpLines_.end(), lineId,
        [](const LogLineHttp& h, uint32_t id) { return h.lineId < id; });
    if (it != httpLines_.end() && it->lineId == lineId) return &(*it);
    return nullptr;
}

bool LogDocument::Load(const wchar_t* path, std::function<void(double)> onProgress) {
    MappedFile newFile;
    if (!newFile.Open(path)) return false;

    const uint8_t* base = newFile.Data();
    uint64_t totalBytes = newFile.Size();

    // Find newline positions
    std::vector<uint32_t> lineStarts;
    lineStarts.reserve(static_cast<size_t>(totalBytes / 80));
    lineStarts.push_back(0);
    for (uint64_t i = 0; i < totalBytes; ++i) {
        if (base[i] == 0x0A) {
            if (i + 1 < totalBytes)
                lineStarts.push_back(static_cast<uint32_t>(i + 1));
        }
    }

    size_t totalLines = lineStarts.size();

    // Compute line lengths (strip \r)
    struct LineRange { uint32_t offset; uint32_t length; };
    std::vector<LineRange> ranges(totalLines);
    for (size_t i = 0; i < totalLines; ++i) {
        uint32_t start = lineStarts[i];
        uint32_t end;
        if (i + 1 < totalLines) {
            end = lineStarts[i + 1] - 1; // before \n
        } else {
            end = static_cast<uint32_t>(totalBytes);
        }
        // Strip trailing \r
        if (end > start && base[end - 1] == 0x0D) --end;
        ranges[i] = {start, end - start};
    }

    // Initialize parser from header lines
    size_t headerCount = std::min(totalLines, size_t(10));
    std::vector<std::string_view> headerLines;
    headerLines.reserve(headerCount);
    for (size_t i = 0; i < headerCount; ++i) {
        headerLines.emplace_back(
            reinterpret_cast<const char*>(base + ranges[i].offset),
            ranges[i].length);
    }
    auto logParser = std::make_unique<LogParser>(headerLines);

    if (onProgress) onProgress(0.1);

    // Parallel parsing
    size_t cpuCount = std::max(1u, std::thread::hardware_concurrency());
    size_t chunkSize = std::max(totalLines / cpuCount, size_t(1));

    struct ChunkResult {
        std::array<int, kMaxThreads> threadCounts{};
        std::array<int, kLogLevelCount> levelCounts{};
        std::vector<LogLineHttp> httpLines;
        std::vector<LogLineDuration> durationLines;
    };

    std::vector<LogLineHot> parsed(totalLines);
    std::vector<ChunkResult> chunkResults(cpuCount);
    std::atomic<size_t> progressCounter{0};

    auto parseChunk = [&](size_t chunkIdx) {
        size_t start = chunkIdx * chunkSize;
        size_t end = (chunkIdx == cpuCount - 1) ? totalLines : (chunkIdx + 1) * chunkSize;
        if (start >= totalLines) return;
        end = std::min(end, totalLines);

        auto& cr = chunkResults[chunkIdx];

        for (size_t i = start; i < end; ++i) {
            auto& r = ranges[i];
            auto result = logParser->ParseLine(
                base + r.offset, r.length, r.offset, static_cast<uint32_t>(i));

            parsed[i] = result.hot;

            auto level = static_cast<LogLevel>(result.hot.level);
            int thread = result.hot.thread;

            if (level != LogLevel::Unknown && thread >= 0) {
                cr.threadCounts[thread]++;
                cr.levelCounts[static_cast<int>(level)]++;
            }

            if (result.durationUS >= 0) {
                cr.durationLines.push_back({static_cast<uint32_t>(i), result.durationUS});
            }

            if (level == LogLevel::Http && result.hot.messageOffset < r.length) {
                LogLineHttp http;
                http.lineId = static_cast<uint32_t>(i);
                LogParser::ParseHTTPFields(
                    base + r.offset, r.length, result.hot.messageOffset,
                    reinterpret_cast<const char*>(base + r.offset), http);
                if (!http.method.empty() || http.status >= 0) {
                    cr.httpLines.push_back(std::move(http));
                }
            }

            if ((i & 0x3FFF) == 0) { // every 16K lines
                progressCounter.store(i, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::jthread> threads;
    for (size_t c = 1; c < cpuCount; ++c) {
        threads.emplace_back([&, c] { parseChunk(c); });
    }
    parseChunk(0); // main thread does chunk 0
    threads.clear(); // join all

    if (onProgress) onProgress(0.9);

    // Merge results
    std::array<int, kMaxThreads> mergedThreadCounts{};
    std::array<int, kLogLevelCount> mergedLevelCounts{};
    std::vector<LogLineHttp> mergedHttp;
    std::vector<LogLineDuration> mergedDuration;

    for (auto& cr : chunkResults) {
        for (int j = 0; j < kMaxThreads; ++j) mergedThreadCounts[j] += cr.threadCounts[j];
        for (int j = 0; j < kLogLevelCount; ++j) mergedLevelCounts[j] += cr.levelCounts[j];
        mergedHttp.insert(mergedHttp.end(),
                          std::make_move_iterator(cr.httpLines.begin()),
                          std::make_move_iterator(cr.httpLines.end()));
        mergedDuration.insert(mergedDuration.end(),
                              cr.durationLines.begin(), cr.durationLines.end());
    }

    // Sort sparse arrays by lineId for binary search
    std::sort(mergedHttp.begin(), mergedHttp.end(),
              [](const LogLineHttp& a, const LogLineHttp& b) { return a.lineId < b.lineId; });
    std::sort(mergedDuration.begin(), mergedDuration.end(),
              [](const LogLineDuration& a, const LogLineDuration& b) { return a.lineId < b.lineId; });

    // Find end epochCS
    int64_t endCS = -1;
    for (size_t i = parsed.size(); i > 0; --i) {
        if (parsed[i - 1].epochCS >= 0) {
            endCS = parsed[i - 1].epochCS;
            break;
        }
    }

    // Active threads
    std::vector<int> active;
    for (int i = 0; i < kMaxThreads; ++i) {
        if (mergedThreadCounts[i] > 0) active.push_back(i);
    }

    // Commit
    file_ = std::move(newFile);
    parser_ = std::move(logParser);
    allLines_ = std::move(parsed);
    httpLines_ = std::move(mergedHttp);
    durationLines_ = std::move(mergedDuration);
    perThreadCount_ = mergedThreadCounts;
    perLevelCount_ = mergedLevelCounts;
    activeThreads_ = std::move(active);
    startEpochCS_ = parser_->GetStartEpochCS();
    endEpochCS_ = endCS;

    enabledLevelMask_ = ~uint64_t(0);
    enabledThreadMask_ = ~uint64_t(0);
    searchPattern_.clear();
    searchRegex_ = false;
    selectedLineId_ = -1;
    methodTimings_.clear();

    ApplyFilters();

    if (onProgress) onProgress(1.0);
    return true;
}

void LogDocument::SetEnabledLevelMask(uint64_t mask) { enabledLevelMask_ = mask; }
void LogDocument::SetEnabledThreadMask(uint64_t mask) { enabledThreadMask_ = mask; }
void LogDocument::SetSearchPattern(const std::string& pattern, bool isRegex) {
    searchPattern_ = pattern;
    searchRegex_ = isRegex;
}
void LogDocument::SetSelectedLineId(int id) { selectedLineId_ = id; }

void LogDocument::ApplyFilters() {
    uint64_t levelMask = enabledLevelMask_;
    uint64_t threadMask = enabledThreadMask_;
    const auto& pattern = searchPattern_;
    bool useRegex = searchRegex_;

    std::regex* re = nullptr;
    std::regex compiledRegex;
    if (!pattern.empty() && useRegex) {
        try {
            compiledRegex = std::regex(pattern, std::regex_constants::icase);
            re = &compiledRegex;
        } catch (...) {}
    }

    std::vector<uint32_t> indices;
    indices.reserve(allLines_.size());

    const uint8_t* base = file_.Data();

    for (size_t i = 0; i < allLines_.size(); ++i) {
        const auto& line = allLines_[i];
        auto level = static_cast<LogLevel>(line.level);

        // Level filter
        if (!(levelMask & (uint64_t(1) << static_cast<int>(level)))) continue;

        // Thread filter
        if (line.thread >= 0) {
            if (!(threadMask & (uint64_t(1) << line.thread))) continue;
        }

        // Text filter
        if (!pattern.empty()) {
            auto raw = GetRawLine(base, line);
            if (!MatchLine(raw, pattern, re)) continue;
        }

        indices.push_back(static_cast<uint32_t>(i));
    }

    filteredIndices_ = std::move(indices);
}

bool LogDocument::MatchLine(std::string_view raw, const std::string& pattern,
                            const std::regex* re) const {
    if (re) {
        return std::regex_search(raw.begin(), raw.end(), *re);
    }
    // Case-insensitive ASCII contains
    if (pattern.empty()) return true;
    auto it = std::search(raw.begin(), raw.end(), pattern.begin(), pattern.end(),
        [](char a, char b) {
            return (a >= 'A' && a <= 'Z' ? a + 32 : a) == (b >= 'A' && b <= 'Z' ? b + 32 : b);
        });
    return it != raw.end();
}

int LogDocument::FindMatchingPair(int lineId) const {
    if (lineId < 0 || lineId >= static_cast<int>(allLines_.size())) return -1;
    const auto& line = allLines_[lineId];
    auto level = static_cast<LogLevel>(line.level);
    int th = line.thread;
    if (th < 0) return -1;

    if (level == LogLevel::Enter) {
        int depth = 0;
        for (size_t i = lineId + 1; i < allLines_.size(); ++i) {
            const auto& l = allLines_[i];
            if (l.thread != th) continue;
            auto lv = static_cast<LogLevel>(l.level);
            if (lv == LogLevel::Enter) {
                depth++;
            } else if (lv == LogLevel::Leave) {
                if (depth == 0) return static_cast<int>(i);
                depth--;
            }
        }
    } else if (level == LogLevel::Leave) {
        int depth = 0;
        for (int i = lineId - 1; i >= 0; --i) {
            const auto& l = allLines_[i];
            if (l.thread != th) continue;
            auto lv = static_cast<LogLevel>(l.level);
            if (lv == LogLevel::Leave) {
                depth++;
            } else if (lv == LogLevel::Enter) {
                if (depth == 0) return i;
                depth--;
            }
        }
    }
    return -1;
}

void LogDocument::BuildMethodTimings() {
    methodTimings_.clear();
    if (!parser_) return;

    const uint8_t* base = file_.Data();
    size_t count = allLines_.size();

    // Single-pass O(n) with per-thread stacks.
    // Each stack entry is the line index of an Enter.
    std::array<std::vector<uint32_t>, kMaxThreads> stacks;

    for (size_t i = 0; i < count; ++i) {
        auto lv = static_cast<LogLevel>(allLines_[i].level);
        int th = allLines_[i].thread;
        if (th < 0 || th >= kMaxThreads) continue;

        if (lv == LogLevel::Enter) {
            stacks[th].push_back(static_cast<uint32_t>(i));
        } else if (lv == LogLevel::Leave && !stacks[th].empty()) {
            uint32_t enterIdx = stacks[th].back();
            stacks[th].pop_back();

            // Only top-level methods (stack now empty = outermost call)
            if (!stacks[th].empty()) continue;

            int64_t csStart = allLines_[enterIdx].epochCS;
            int64_t csEnd = allLines_[i].epochCS;
            if (csStart >= 0 && csEnd >= 0) {
                double durationMS = static_cast<double>(csEnd - csStart) * 10.0;
                if (durationMS >= 10.0) {
                    auto msg = GetMessage(base, allLines_[enterIdx]);
                    while (!msg.empty() && (msg.front() == ' ' || msg.front() == '\t'))
                        msg.remove_prefix(1);
                    while (!msg.empty() && (msg.back() == ' ' || msg.back() == '\t' ||
                                             msg.back() == '\r' || msg.back() == '\n'))
                        msg.remove_suffix(1);

                    methodTimings_.push_back({
                        enterIdx, th, durationMS, std::string(msg)
                    });
                }
            }
        }
    }

    std::sort(methodTimings_.begin(), methodTimings_.end(),
              [](const MethodTiming& a, const MethodTiming& b) {
                  return a.durationMS > b.durationMS;
              });
}

int LogDocument::FindNext(const std::string& pattern, SearchDirection dir, int from) const {
    const auto& indices = filteredIndices_;
    if (indices.empty() || pattern.empty()) return -1;

    std::regex* re = nullptr;
    std::regex compiledRegex;
    try {
        compiledRegex = std::regex(pattern, std::regex_constants::icase);
        re = &compiledRegex;
    } catch (...) {}

    const uint8_t* base = file_.Data();

    if (dir == SearchDirection::Forward) {
        int startIdx = from + 1;
        for (int i = startIdx; i < static_cast<int>(indices.size()); ++i) {
            auto raw = GetRawLine(base, allLines_[indices[i]]);
            if (MatchLine(raw, pattern, re)) return i;
        }
    } else {
        int upper = std::min(from, static_cast<int>(indices.size())) - 1;
        for (int i = upper; i >= 0; --i) {
            auto raw = GetRawLine(base, allLines_[indices[i]]);
            if (MatchLine(raw, pattern, re)) return i;
        }
    }
    return -1;
}
