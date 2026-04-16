#include "ui/TimingDialog.h"
#include "infra/Utf.h"
#include <cstdio>

void ShowTimingDialog(HWND parent, LogDocument& doc) {
    doc.BuildMethodTimings();

    const auto& timings = doc.Timings();
    std::string text;
    char buf[512];

    snprintf(buf, sizeof(buf), "Slow Methods (%zu total, >= 10ms):\r\n\r\n",
             timings.size());
    text += buf;

    int shown = 0;
    for (const auto& t : timings) {
        if (shown >= 100) {
            text += "\r\n... and more\r\n";
            break;
        }
        snprintf(buf, sizeof(buf), "%8.1f ms  T%d  %s\r\n",
                 t.durationMS, t.thread, t.method.c_str());
        text += buf;
        shown++;
    }

    auto wide = Utf8ToWide(text);
    MessageBoxW(parent, wide.c_str(), L"Method Timing", MB_OK | MB_ICONINFORMATION);
}
