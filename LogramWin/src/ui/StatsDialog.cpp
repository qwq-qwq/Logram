#include "ui/StatsDialog.h"
#include "infra/Utf.h"
#include <cstdio>

void ShowStatsDialog(HWND parent, const LogDocument& doc) {
    std::string text;
    char buf[256];

    if (!doc.UBVersion().empty()) {
        snprintf(buf, sizeof(buf), "UB Version: %s\r\n", doc.UBVersion().c_str());
        text += buf;
    }
    if (!doc.HostInfo().empty()) {
        snprintf(buf, sizeof(buf), "Host: %s\r\n", doc.HostInfo().c_str());
        text += buf;
    }

    text += "\r\n";

    snprintf(buf, sizeof(buf), "File: %s\r\n", doc.FileName().c_str());
    text += buf;

    double sizeMB = static_cast<double>(doc.FileSize()) / 1'000'000.0;
    snprintf(buf, sizeof(buf), "Size: %.1f MB\r\n", sizeMB);
    text += buf;

    snprintf(buf, sizeof(buf), "Total Events: %d\r\n", doc.TotalEvents());
    text += buf;

    snprintf(buf, sizeof(buf), "HTTP Requests: %d\r\n", doc.HttpRequests());
    text += buf;

    snprintf(buf, sizeof(buf), "SQL Queries: %d\r\n", doc.SqlQueries());
    text += buf;

    snprintf(buf, sizeof(buf), "Errors: %d\r\n", doc.ErrorCount());
    text += buf;

    snprintf(buf, sizeof(buf), "Duration: %s\r\n", doc.DurationFormatted().c_str());
    text += buf;

    text += "\r\nActive Threads: ";
    for (int t : doc.ActiveThreads()) {
        snprintf(buf, sizeof(buf), "%d ", t);
        text += buf;
    }
    text += "\r\n";

    auto wide = Utf8ToWide(text);
    MessageBoxW(parent, wide.c_str(), L"Statistics", MB_OK | MB_ICONINFORMATION);
}
