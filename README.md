# CodeSplit

CodeSplit — кроссплатформенная консольная утилита для автоматизированной декомпозиции крупных файлов исходного кода.

Проект ориентирован на безопасное разделение файлов C и C++ размером более 100 КБ на
логически связанные файлы меньшего размера.

## Статус проекта

MVP 0.1.0 реализует анализ и безопасное выделение реализации одной свободной функции с сохранением
исходной функции как forwarding wrapper:

```text
codesplit analyze <file> [--build-path <directory>] [--max-size-kb <number>]
                  [--format <text|json>]
codesplit plan-move <file> --symbol-id <usr> --target <file>
                    [--build-path <directory>] [--max-size-kb <number>]
                    [--format <text|json>]
codesplit dry-run-move <file> --symbol-id <usr> --target <file>
                      [--build-path <directory>] [--max-size-kb <number>]
                      [--format <text|json>]
codesplit apply-move <file> --symbol-id <usr> --target <file> --confirm
                    [--build-path <directory>] [--max-size-kb <number>]
                    [--format <text|json>]
```

Команда выводит путь, размер файла в байтах, количество строк, признак превышения заданного
порога и доступность команды компиляции. Если доступен Clang LibTooling, отчёт также содержит
найденные определения свободных функций и out-of-line методов, их строки, байтовые диапазоны,
размер и ограничения безопасности. Для сущностей также выводятся USR-идентификаторы, а для
out-of-line методов — диапазоны объявления и содержащего класса, если они доступны. По умолчанию
используется 100 КиБ. Предупреждения и ошибки frontend возвращаются как структурированные
диагностики с уровнем, сообщением и исходной позицией. Прямые вызовы между функциями и методами
и явные ссылки callable на пользовательские типы выводятся как ориентированные зависимости со
стабильными USR-идентификаторами обоих концов. Прямые обращения к глобальным данным различаются
как чтение и запись. Для прямых `#include` сохраняются написанное имя, разрешённый путь и точное
место директивы; транзитивные подключения не смешиваются с ними. Для callable также выводятся
категория связывания и отдельный признак anonymous namespace. Прямые макровызовы связываются с USR
callable и сохраняют диапазон определения и все места раскрытия.

Если сборка CodeSplit содержит Clang LibTooling, команда загружается из
`<build-path>/compile_commands.json`. Отсутствие базы или записи для файла не прерывает базовый
анализ, но явно отражается в отчёте. Будущие преобразования исходного кода будут требовать
доступной команды компиляции.

`plan-move` выбирает одну свободную функцию по USR. Публичная функция не удаляется и не меняет
сигнатуру: её прежнее тело переносится в переименованную implementation-функцию нового `.cpp`, а
на исходном месте остаётся forwarding wrapper с вызовом этой реализации. Поэтому существующие
call sites продолжают вызывать прежний API. Недоказанно безопасные сигнатуры блокируются с кодом 3.
`dry-run-move` строит точные половинно-открытые замены. Эти диагностические команды не изменяют
исходники и не создают целевой файл.

`apply-move` — первая изменяющая команда. Она требует `--confirm`, повторно сверяет исходный текст,
сначала записывает новые версии во временные файлы и восстанавливает backup исходника при
ошибке файлового commit. До удаления backup оба получившихся файла повторно анализируются Clang с
реальной командой проекта; frontend failure вызывает полный откат. Для поддерживаемого
CMake-проекта dry-run также добавляет декларативный `target_sources`, а apply собирает доказанную
цель. Существующие тесты проекта остаются применимы через неизменившийся публичный вызов; CodeSplit
не генерирует дублирующие тесты. Ошибка сборки восстанавливает исходники, `CMakeLists.txt` и
производный build graph. Для
автоматической обработки любой результат можно вывести в JSON:

Для поддерживаемого кандидата новый файл получает прямой include заголовка с каноническим
объявлением и исходную цепочку лексических namespace. Если такой include нельзя доказать,
преобразование блокируется вместо генерации предположительно корректного файла.

Текущий build-адаптер поддерживает CMake: корень берётся из `CMakeCache.txt`, а цель — из объектного
пути настоящей compilation command. Для других систем сборки и нераспознаваемой цели операция
консервативно блокируется.

```text
codesplit analyze src/large.cpp --format json
codesplit plan-move src/large.cpp --symbol-id <usr> --target src/isolated.cpp --format json
codesplit dry-run-move src/large.cpp --symbol-id <usr> --target src/isolated.cpp --format json
codesplit apply-move src/large.cpp --symbol-id <usr> --target src/isolated.cpp --confirm --format json
```

## Загрузка зависимостей

- [CMake 3.25 или новее](https://cmake.org/download/);
- компилятор с поддержкой C++20;
- [LLVM и Clang](https://github.com/llvm/llvm-project/releases) для семантического анализа;
- [Visual Studio или Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)
  с компонентом разработки классических приложений на C++ при сборке MSVC в Windows.

Для загрузки команд компиляции дополнительно требуются LLVM и Clang LibTooling с CMake-пакетами
`LLVMConfig.cmake` и `ClangConfig.cmake`. Независимое ядро и базовый анализ можно собрать без них.

Официальный Windows-инсталлятор LLVM ориентирован прежде всего на использование готовых
исполняемых файлов и может не содержать заголовки, библиотеки и CMake-конфигурацию LibTooling.
Для разработки CodeSplit выбирайте на странице релиза полный архив с именем вида
`clang+llvm-<version>-x86_64-pc-windows-msvc.tar.xz`. После распаковки его корень должен содержать
как минимум:

```text
include/clang/Tooling/CompilationDatabase.h
lib/clangTooling.lib
lib/cmake/llvm/LLVMConfig.cmake
lib/cmake/clang/ClangConfig.cmake
```

## Сборка и тестирование

### Базовая сборка без LibTooling

```text
cmake -S . -B build -DCODESPLIT_ENABLE_CLANG=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Эта конфигурация поддерживает CLI, подсчёт байтов и строк и проверку лимита, но не строит AST.

### Полная сборка с LibTooling

Распакуйте development-дистрибутив LLVM и передайте его корень через `CMAKE_PREFIX_PATH`:

```text
cmake -S . -B build-llvm -DCMAKE_PREFIX_PATH=<llvm-install>
cmake --build build-llvm --config Release
ctest --test-dir build-llvm -C Release --output-on-failure
```

`<llvm-install>` — каталог, внутри которого находятся `include`, `lib` и `lib/cmake`. В PowerShell
путь с пробелами нужно заключить в кавычки:

```powershell
cmake -S . -B build-llvm `
  -DCMAKE_PREFIX_PATH="C:/Tools/clang+llvm-22.1.8-x86_64-pc-windows-msvc"
```

CMake сообщает один из двух результатов:

```text
CodeSplit Clang LibTooling integration enabled
CodeSplit Clang LibTooling integration unavailable
```

Во втором случае проверьте путь к development-дистрибутиву. Интеграцию также можно отключить
явно через `-DCODESPLIT_ENABLE_CLANG=OFF`.

## Подготовка анализируемого проекта

`compile_commands.json` должен относиться к проекту, исходники которого анализирует CodeSplit.
Для CMake-проекта его обычно создают так:

```text
cmake -S <project> -B <project-build> -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Некоторые генераторы, включая Visual Studio, не создают эту базу. Для получения файла обычно
используют [Ninja](https://github.com/ninja-build/ninja/releases), Makefiles либо отдельный экспорт
команд из системы сборки проекта.

Параметр `--build-path` должен указывать каталог с полученным `compile_commands.json`:

```text
codesplit analyze <project>/src/large.cpp --build-path <project-build>
codesplit analyze <project>/src/large.cpp --build-path <project-build> --format json
```

Команда компиляции определяет фактический стандарт C или C++, include-каталоги, макросы и режим
compiler driver. CodeSplit не подменяет эти настройки собственным фиксированным `C++20`: C++20
требуется для сборки самой утилиты, но анализируемый проект может использовать другую версию языка.
Автоматическая матрица проверяет команды для C11, C17, C++17 и C++20.

## План распространения

До первого пользовательского релиза планируется убрать необходимость вручную собирать и
настраивать LLVM: подготовить готовые архивы или установочные пакеты CodeSplit, автоматическое
обнаружение зависимостей, понятную диагностику окружения и короткую проверку установки. Текущие
инструкции описывают окружение разработчика и будут уточняться по мере появления поддерживаемых
релизных пакетов.

Уже доступна стандартная установка собранного бинарника:

```text
cmake --install build-llvm --prefix <install-directory>
```

Docker не входит в MVP: утилите нужен прямой доступ одновременно к рабочему дереву,
`compile_commands.json`, build directory и host toolchain. Контейнеризация будет рассматриваться
после определения понятного контракта их монтирования, а не как замена обычной локальной установки.

## Документация

Материалы по обоснованию разработки:

- [архитектура и инварианты безопасности](docs/architecture.md);
- [инженерные проблемы и принятые решения](docs/development-challenges.md);
- [протокол ручной проверки](docs/manual-verification.md);
- [контракт CLI и JSON](docs/cli-contract.md);
- [приёмка MVP 0.1.0](docs/mvp-acceptance.md);
- [план развития](docs/roadmap.md).

## Лицензия

CodeSplit распространяется по лицензии MIT. Подробности приведены в файле [LICENSE](LICENSE).
