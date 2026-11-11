#pragma once
#include "core/LogLevel.h"
#include "core/LogLine.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class LogParser {
public:
    enum class Format { Mormot1, Mormot2, Journald, Console };

    explicit LogParser(const std::vector<std::string_view>& headerLines);

    Format GetFormat() const { return format_; }
    int GetThreadPos() const { return thPos_; }
    int64_t GetHiResFreq() const { return hiResFreq_; }
    // Log start, shifted to viewer-local time (matches per-line epochCS).
    int64_t GetStartEpochCS() const {
        return startEpochCS_ >= 0 ? startEpochCS_ + tzOffsetCS_ : -1;
    }
    const std::string& GetUBVersion() const { return ubVersion_; }
    const std::string& GetHostInfo() const { return hostInfo_; }

    static constexpr uint8_t kThBase = 0x21; // '!'

    // Время строки в миллисекундах от эпохи - ТОЛЬКО для расчётов.
    // epochCS хранит дробную часть ровно так, как она записана в логе (и так
    // же показывается). В формате mORMot1 последние две цифры метки
    // "YYYYMMDD hhmmsszz" - это миллисекунды shr 4 (00..62, шаг 16 мс), а не
    // сотые доли секунды (SynCommons.pas, TSynSystemTime.AddLogTime), поэтому
    // для расчётов zz умножается на 16. В остальных форматах дробная часть
    // настоящая. Возвращает -1 для неизвестного времени.
    int64_t TrueEpochMS(int64_t epochCS) const;

    // Длительность пары enter/leave в микросекундах.
    // leaveUS - значение из leave-строки (-1, если его нет), enterMS/leaveMS -
    // TrueEpochMS обеих строк. Значению из строки верим, пока оно согласуется
    // с метками: mORMot пишет его как SS.mmm.uuu с двумя цифрами секунд и
    // обрезает секунды до 99, а аргумент 32-битный и заворачивается по модулю
    // ~4295 с (TTextWriter.AddMicroSec). Если между метками прошло больше, чем
    // leaveUS плюс допуск, берётся разность меток. Возвращает -1, если
    // длительность определить нельзя.
    static int64_t PairDurationUS(int64_t leaveUS, int64_t enterMS, int64_t leaveMS);

    // Допуск сверки длительности из leave-строки с метками времени, мкс.
    static constexpr int64_t kDurationSlackUS = 1'000'000;

    // Parse a single log line from raw bytes.
    // Returns populated LogLineHot. HTTP/Duration stored in out params if present.
    struct ParseResult {
        LogLineHot hot;
        LogLineHttp* http = nullptr;   // caller-owned, set if HTTP line
        int64_t durationUS = -1;       // set if leave line
    };

    ParseResult ParseLine(const uint8_t* buf, uint32_t len,
                          uint32_t rawOffset, uint32_t index) const;

    // Days since Unix epoch (1970-01-01) using Hinnant's algorithm
    static int DaysFromEpoch(int year, int month, int day);

    // HTTP field extraction is public because LogDocument calls it
    // lazily for Http lines during parallel parsing.
    static void ParseHTTPFields(const uint8_t* buf, uint32_t len, int start,
                                const char* rawPtr, LogLineHttp& http);

private:
    int64_t ParseTimestamp(const uint8_t* buf, uint32_t len) const;
    int64_t ParseMormot1(const uint8_t* buf, uint32_t len) const;
    int64_t ParseMormot2(const uint8_t* buf, uint32_t len) const;
    int64_t ParseJournald(const uint8_t* buf, uint32_t len) const;
    static int64_t ParseDuration(const uint8_t* buf, uint32_t len, int start);

    Format format_ = Format::Mormot1;
    int thPos_ = 19;
    int64_t hiResFreq_ = 1000;
    int64_t startEpochCS_ = -1;   // unshifted UTC anchor (base for mORMot2 ticks)
    int64_t tzOffsetCS_ = 0;      // viewer-local UTC offset, added to every timestamp
    std::string ubVersion_;
    std::string hostInfo_;
};
