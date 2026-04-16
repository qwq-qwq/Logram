#pragma once
#include "core/LogLevel.h"
#include "core/LogLine.h"
#include "core/LogParser.h"
#include "core/MappedFile.h"
#include "core/Observer.h"
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <regex>
#include <functional>
#include <atomic>

struct MethodTiming {
    uint32_t lineId;    // line index of enter (+)
    int thread;
    double durationMS;
    std::string method;
};

class LogDocument {
public:
    LogDocument() = default;

    // Load file (blocking, call from worker thread; progress callback from worker)
    bool Load(const wchar_t* path, std::function<void(double)> onProgress = nullptr);

    // --- Data accessors ---
    const uint8_t* MappedBase() const { return file_.Data(); }
    uint64_t FileSize() const { return file_.Size(); }
    const std::wstring& FilePath() const { return file_.Path(); }
    std::string FileName() const;

    const std::vector<LogLineHot>& AllLines() const { return allLines_; }
    const std::vector<uint32_t>& FilteredIndices() const { return filteredIndices_; }
    const LogParser* Parser() const { return parser_.get(); }

    // --- Header info ---
    const std::string& UBVersion() const { return parser_ ? parser_->GetUBVersion() : emptyStr_; }
    const std::string& HostInfo() const { return parser_ ? parser_->GetHostInfo() : emptyStr_; }
    int64_t StartEpochCS() const { return startEpochCS_; }
    int64_t EndEpochCS() const { return endEpochCS_; }

    // --- Statistics ---
    const int* PerThreadCount() const { return perThreadCount_.data(); }
    const int* PerLevelCount() const { return perLevelCount_.data(); }
    const std::vector<int>& ActiveThreads() const { return activeThreads_; }
    int TotalEvents() const { return static_cast<int>(allLines_.size()); }
    int FilteredCount() const { return static_cast<int>(filteredIndices_.size()); }
    int HttpRequests() const { return perLevelCount_[static_cast<int>(LogLevel::Http)]; }
    int SqlQueries() const { return perLevelCount_[static_cast<int>(LogLevel::Sql)]; }
    int ErrorCount() const;
    double DurationSeconds() const;
    std::string DurationFormatted() const;

    // --- HTTP/Duration sparse data ---
    const std::vector<LogLineHttp>& HttpLines() const { return httpLines_; }
    const std::vector<LogLineDuration>& DurationLines() const { return durationLines_; }
    int64_t GetDuration(uint32_t lineId) const;
    const LogLineHttp* GetHttp(uint32_t lineId) const;

    // --- Filters ---
    uint64_t EnabledLevelMask() const { return enabledLevelMask_; }
    uint64_t EnabledThreadMask() const { return enabledThreadMask_; }
    const std::string& SearchPattern() const { return searchPattern_; }
    bool SearchRegex() const { return searchRegex_; }

    void SetEnabledLevelMask(uint64_t mask);
    void SetEnabledThreadMask(uint64_t mask);
    void SetSearchPattern(const std::string& pattern, bool isRegex);
    void ApplyFilters();

    // --- Selection ---
    int SelectedLineId() const { return selectedLineId_; }
    void SetSelectedLineId(int id);

    // --- Method timing ---
    const std::vector<MethodTiming>& Timings() const { return methodTimings_; }
    void BuildMethodTimings();

    // --- Navigation ---
    int FindMatchingPair(int lineId) const;

    enum class SearchDirection { Forward, Backward };
    int FindNext(const std::string& pattern, SearchDirection dir, int from = -1) const;

    // --- Observer ---
    DocumentListeners listeners;

private:
    MappedFile file_;
    std::unique_ptr<LogParser> parser_;

    std::vector<LogLineHot> allLines_;
    std::vector<uint32_t> filteredIndices_;
    std::vector<LogLineHttp> httpLines_;
    std::vector<LogLineDuration> durationLines_;

    // Line byte ranges for raw access
    std::vector<uint32_t> lineOffsets_; // parallel to allLines_

    std::array<int, kMaxThreads> perThreadCount_{};
    std::array<int, kLogLevelCount> perLevelCount_{};
    std::vector<int> activeThreads_;

    int64_t startEpochCS_ = -1;
    int64_t endEpochCS_ = -1;

    // Filters
    uint64_t enabledLevelMask_ = ~uint64_t(0);
    uint64_t enabledThreadMask_ = ~uint64_t(0);
    std::string searchPattern_;
    bool searchRegex_ = false;

    int selectedLineId_ = -1;

    std::vector<MethodTiming> methodTimings_;

    static inline const std::string emptyStr_;

    bool MatchLine(std::string_view raw, const std::string& pattern,
                   const std::regex* re) const;
};
