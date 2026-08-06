# CodeSplit

CodeSplit — кроссплатформенная консольная утилита для автоматизированной декомпозиции крупных файлов исходного кода.

Проект ориентирован на безопасное разделение файлов C и C++ размером более 100 КБ на
логически связанные файлы меньшего размера.

## Статус проекта

CodeSplit находится на ранней стадии разработки. Доступна первая диагностическая команда:

```text
codesplit analyze <file> [--build-path <directory>] [--max-size-kb <number>]
                  [--format <text|json>]
```

Команда выводит путь, размер файла в байтах, количество строк, признак превышения заданного
порога и доступность команды компиляции. Если доступен Clang LibTooling, отчёт также содержит
найденные определения свободных функций и out-of-line методов, их строки, байтовые диапазоны,
размер и ограничения безопасности. По умолчанию используется 100 КиБ.

Если сборка CodeSplit содержит Clang LibTooling, команда загружается из
`<build-path>/compile_commands.json`. Отсутствие базы или записи для файла не прерывает базовый
анализ, но явно отражается в отчёте. Будущие преобразования исходного кода будут требовать
доступной команды компиляции.

Сейчас `analyze` не изменяет исходники. Для автоматической обработки результат можно вывести в
JSON:

```text
codesplit analyze src/large.cpp --format json
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

## План распространения

До первого пользовательского релиза планируется убрать необходимость вручную собирать и
настраивать LLVM: подготовить готовые архивы или установочные пакеты CodeSplit, автоматическое
обнаружение зависимостей, понятную диагностику окружения и короткую проверку установки. Текущие
инструкции описывают окружение разработчика и будут уточняться по мере появления поддерживаемых
релизных пакетов.

## Лицензия

CodeSplit распространяется по лицензии MIT. Подробности приведены в файле [LICENSE](LICENSE).
