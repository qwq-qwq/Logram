# Logram

macOS SwiftUI приложение для анализа логов UnityBase сервера.

## Сборка и запуск

```bash
# Через Xcode
open Logram.xcodeproj

# Через командную строку
xcodebuild -project Logram.xcodeproj -scheme Logram -configuration Debug build
open ~/Library/Developer/Xcode/DerivedData/Logram-*/Build/Products/Debug/Logram.app

# Через Swift Package Manager (только сборка, .app не создаётся)
swift build
```

**Target**: macOS 14+, Swift 5.9

## Архитектура

```
Logram/
├── App.swift                  — @main, WindowGroup, меню Open (Cmd+O)
├── Models/
│   ├── LogLevel.swift         — enum 32 уровня UB-логов (code, label, bgColor)
│   ├── LogLine.swift          — структура одной строки (thread, level, timestamp, HTTP, duration)
│   ├── LogParser.swift        — парсер: 4 формата (mORMot1, mORMot2 HiRes, journald, console)
│   └── LogDocument.swift      — @Observable модель: загрузка, фильтрация, поиск, method timing
└── Views/
    ├── ContentView.swift      — главное окно: toolbar + HSplitView (sidebar | log lines)
    ├── LogLinesView.swift     — List + ScrollViewReader, цветовая маркировка
    ├── FilterSidebar.swift    — чекбоксы уровней/потоков, пресеты (Errors, SQL, HTTP)
    ├── StatsView.swift        — sheet со статистикой (events/sec, распределение)
    └── MethodTimingView.swift — sheet с таблицей медленных методов (+/- пары)
```

## UB Log Format

Формат строки: `YYYYMMDD HHMMSSCC  T level  <TAB>message`
- `HHMMSSCC` — часы, минуты, секунды, сотые секунды
- `T` — символ потока: `!`=0, `"`=1, `#`=2, `$`=3 и т.д. (charCode - 0x21)
- `level` — 6-символьный код: `info  `, `SQL   `, `EXC   `, ` +    `, ` -    ` и др.

Поддерживаемые форматы:
- **mORMot1**: `20210727 08260933  $ SQL` (основной формат UB)
- **mORMot2 HiRes**: `0000000000000C72  ! SQL` (hex-таймер)
- **journald**: `2022-10-27T00:00:00.358664+0300 ub[657518]:  $ SQL`
- **console**: `  $ SQL` (без даты, stdin)

## Ключевые паттерны

- `LogParser` — `Sendable`, создаётся один раз при загрузке файла, определяет формат по заголовку
- `LogDocument` — `@Observable`, главный state holder, все мутации на MainActor
- Загрузка чанками по 50K строк с `Task.yield()` для responsiveness
- Фильтрация через `Set<LogLevel>` и `Set<Int>` (потоки) — O(n) один проход
- Method timing: для каждого `+` ищет парный `-` на том же потоке с учётом вложенности

## Референсные реализации

Парсер основан на двух существующих реализациях:
- **JS**: `@unitybase/logview` — `uLogUtils.js` (LogParser class, 572 строки) - /Users/sergeyperekrestov/WebstormProjects/UB/ldoc/node_modules/@unitybase/logview/public/forms/components/uLogUtils.js
- **Go**: `ub-log-shipper` — `ubparse.go` (parseUBLine, глубокий HTTP/SQL парсинг) - /Users/sergeyperekrestov/WebstormProjects/UB/ldoc/docker/ub-log-shipper/src/ubparse.go
- **Pascal**: `LogViewMain.pas` (Synopse TSynLog viewer) - /Users/sergeyperekrestov/WebstormProjects/UB/core/libs/Synopse/SQLite3/Samples/11 - Exception logging/LogViewMain.pas

## Язык ответов

Отвечай на русском языке.