#include "core/LogLevel.h"

static constexpr LogLevelInfo kLevelInfos[] = {
    {"???   ", "Unknown",    false},  // Unknown
    {"info  ", "Info",       false},
    {"debug ", "Debug",      false},
    {"trace ", "Trace",      false},
    {"warn  ", "Warn",       false},
    {"ERROR ", "Error",      true },
    {" +    ", "Enter",      false},  // Enter
    {" -    ", "Leave",      false},  // Leave
    {"OSERR ", "OSError",    true },
    {"EXC   ", "Exception",  true },
    {"EXCOS ", "ExcOS",      true },
    {"mem   ", "Memory",     false},
    {"stack ", "Stack",      false},
    {"fail  ", "Fail",       true },
    {"SQL   ", "SQL",        false},
    {"cache ", "Cache",      false},
    {"res   ", "Result",     false},
    {"DB    ", "DB",         false},
    {"http  ", "HTTP",       false},
    {"clnt  ", "Client",     false},
    {"srvr  ", "Server",     false},
    {"call  ", "Call",       false},
    {"ret   ", "Return",     false},
    {"auth  ", "Auth",       false},
    {"cust1 ", "Params",     false},
    {"cust2 ", "SlowSQL",    false},
    {"cust3 ", "Custom3",    false},
    {"cust4 ", "Custom4",    false},
    {"rotat ", "LogRotate",  false},
    {"dddER ", "DDDErr",     true },
    {"dddIN ", "DDDIn",      false},
    {"mon   ", "Monitor",    false},
};

static_assert(sizeof(kLevelInfos) / sizeof(kLevelInfos[0]) == kLogLevelCount,
              "kLevelInfos must match LogLevel::COUNT");

const LogLevelInfo& GetLogLevelInfo(LogLevel level) {
    return kLevelInfos[static_cast<int>(level)];
}

const std::unordered_map<uint64_t, LogLevel>& GetPackedLevelMap() {
    static const auto map = [] {
        std::unordered_map<uint64_t, LogLevel> m;
        for (int i = 0; i < kLogLevelCount; ++i) {
            const auto* code = reinterpret_cast<const uint8_t*>(kLevelInfos[i].code);
            uint64_t key = Pack6(code);
            m[key] = static_cast<LogLevel>(i);
        }
        return m;
    }();
    return map;
}
