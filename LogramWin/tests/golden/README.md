
# Golden tests

Эталонные `.log` файлы парсятся Mac-версией через `tools/golden_gen.swift`
и результат сохраняется в одноимённый `*.expected.ndjson`. При сборке
Windows-порта `parser_tests` читает оба файла и сравнивает поля
line-by-line.

## Пополнение набора

```bash
# macOS:
cd LogramWin
swift tools/golden_gen.swift samples/my.log > tests/golden/my.expected.ndjson
cp samples/my.log tests/golden/my.log
```

Каждый test-case — пара файлов:

| Файл                   | Источник                          |
| ---------------------- | --------------------------------- |
| `foo.log`              | фрагмент реального UB-лога        |
| `foo.expected.ndjson`  | результат Swift-парсера           |

NDJSON: одна JSON-запись на строку лога. Ключи:
`id`, `level` (6-char code), `thread`, `epochCS`, `messageOffset`,
`durationUS?`, `httpMethod?`, `httpPath?`, `httpStatus?`.

## Критерий прохождения

100% совпадение всех непроизвольных полей. Расхождения в `httpPath`
допускаются только при наличии нестандартных пробелов — в этом случае
расширяем `ParseHTTPFields`.