# Changelog

## 0.1.0 — 2026-08-13

- анализ C/C++ настоящей compilation command через Clang LibTooling;
- inventory свободных функций и out-of-line методов со стабильными USR;
- прямые call, type, global data, include и macro dependencies;
- структурированные frontend diagnostics и ограничения безопасности;
- plan, dry-run и подтверждаемое Extract Implementation свободной функции;
- сохранение исходной функции как forwarding wrapper без переписывания call sites;
- транзакционное применение, frontend/build validation и rollback;
- интеграция нового `.cpp` в доказанную CMake-цель;
- text и JSON schema 1, стабильные exit codes 0–4;
- CMake install, встроенный пример и автоматический end-to-end сценарий;
- приёмка на закреплённой версии `{fmt}` 11.2.0.

Подробные границы приведены в [`docs/mvp-acceptance.md`](docs/mvp-acceptance.md).
