# CodeSplit

CodeSplit — кроссплатформенная консольная утилита для автоматизированной декомпозиции крупных файлов исходного кода.

Проект ориентирован на безопасное разделение файлов C и C++ размером более 100 КБ на
логически связанные файлы меньшего размера.

## Статус проекта

CodeSplit находится на ранней стадии разработки. Текущий интерфейс закладывает основу для
первой команды анализа:

```text
codesplit analyze <file> [--build-path <directory>] [--max-size-kb <number>]
                  [--format <text|json>]
```

Команда выводит путь, размер файла в байтах, количество строк, признак превышения заданного
порога и доступность команды компиляции. По умолчанию используется 100 КиБ.

Если сборка CodeSplit содержит Clang LibTooling, команда загружается из
`<build-path>/compile_commands.json`. Отсутствие базы или записи для файла не прерывает базовый
анализ, но явно отражается в отчёте. Будущие преобразования исходного кода будут требовать
доступной команды компиляции.

Для автоматической обработки результат можно вывести в JSON:

```text
codesplit analyze src/large.cpp --format json
```

## Требования

- компилятор с поддержкой C++20;
- CMake 3.25 или новее.

Для загрузки команд компиляции дополнительно требуются LLVM и Clang LibTooling с CMake-пакетами
`LLVMConfig.cmake` и `ClangConfig.cmake`. Независимое ядро и базовый анализ можно собрать без них.

## Сборка и тестирование

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Для сборки с LibTooling укажите корень development-дистрибутива LLVM и включите создание базы
команд компиляции:

```text
cmake -S . -B build -DCMAKE_PREFIX_PATH=<llvm-install> -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Обычный Windows-инсталлятор LLVM может не содержать заголовки и библиотеки LibTooling. В таком
случае нужен полный development-архив `clang+llvm`.

Интеграцию можно явно отключить независимо от установленных пакетов:

```text
cmake -S . -B build -DCODESPLIT_ENABLE_CLANG=OFF
```

Параметр `--build-path` указывает каталог, в котором CodeSplit ищет `compile_commands.json`:

```text
codesplit analyze src/large.cpp --build-path build
```

## Лицензия

CodeSplit распространяется по лицензии MIT. Подробности приведены в файле [LICENSE](LICENSE).
