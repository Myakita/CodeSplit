# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

CodeSplit is a C++20 CLI tool that decomposes large C/C++ source files by extracting a function's
implementation into a new `.cpp`, leaving a forwarding wrapper in place. It uses Clang LibTooling for
semantic analysis. This is a diploma project: `docs/` is a first-class deliverable, written in
Russian, and documents the engineering reasoning behind every design decision.

## Build and test

Two supported configurations. Both must pass before a change is considered done.

```bash
# Full build with LibTooling (11 tests). <llvm-install> contains include/, lib/, lib/cmake/
cmake -S . -B build-llvm -DCMAKE_PREFIX_PATH=<llvm-install>
cmake --build build-llvm --config Release
ctest --test-dir build-llvm -C Release --output-on-failure

# Fallback build without LibTooling (10 tests) — core, CLI, planning, reporting only
cmake -S . -B build -DCODESPLIT_ENABLE_CLANG=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Configure prints exactly one of `CodeSplit Clang LibTooling integration enabled` /
`... unavailable`. If you expected the full build and got `unavailable`, `CMAKE_PREFIX_PATH` is
pointing at a runtime-only LLVM install, not a development distribution.

Run a single test: `ctest --test-dir build-llvm -R codesplit_move_plan_tests --output-on-failure`,
or run the test binary directly (`build-llvm/tests/codesplit_move_plan_tests`).

**After changing any public struct in `include/`, rebuild clean** (`cmake --build <dir>
--clean-first`). Stale objects with the old layout produce ABI mismatches that crash at runtime
(`0xC0000409` on Windows) and look like logic bugs.

The e2e test (`codesplit_mvp_e2e`, LibTooling builds only) drives the real binary over
`examples/mvp`: configure → build → dry-run → apply → rebuild → assert the example's own test still
passes through the wrapper.

## Test harness

There is no test framework. Each `tests/unit/*.cpp` is a standalone `main()` with a file-local
`failure_count` and an `expect(condition, message)` helper, registered in `tests/CMakeLists.txt` as
its own executable and `add_test`. New tests follow that shape. Tests linking `codesplit_clang` must
go inside the `if(CODESPLIT_HAS_CLANG_TOOLING)` branch, which also selects the
`unavailable_*_tests.cpp` counterparts for the fallback build.

## Architecture

Ports-and-adapters around a Clang-free core.

- `codesplit_core` (`src/analysis`, `src/cli`, `src/planning`, `src/reporting`) — no LLVM/Clang
  types anywhere. Public models in `include/codesplit/` use only standard C++ types.
- `codesplit_clang` — one of two implementations of the same headers, selected at configure time:
  `src/clang/{callable_inventory,compilation_database}.cpp` (real) or
  `src/clang/unavailable_*.cpp` (returns a structured "LibTooling not available" result).
- `app/main.cpp` — wires the pipeline and owns the only shell-out (`cmake --build`, `cmake -S -B`).

**Hard boundary:** `clang::ASTContext`, `clang::Decl`, `clang::SourceManager` and friends must not
escape `src/clang`. The adapter copies facts into owned DTOs (`CallableDefinition`,
`CallableDependency`, `SourceRange`, `FrontendDiagnostic`, …) before the AST dies. Identity crosses
the boundary as a USR string in `symbol_id`, never as a pointer or a qualified name — qualified
names don't distinguish overloads.

### The four-command pipeline

Each CLI command is one more layer of guarantee over the previous one, and each has its own
blocker enum so a failure names the layer it came from:

1. `analyze` — `analyze_source_file` (bytes/lines) + `inventory_callables` (AST facts).
2. `plan_move` — `plan_callable_move`: pure function over a serializable inventory. Reads no files.
   Blockers: `MovePlanBlockerKind`.
3. `dry_run_move` — `draft_callable_move`: reads the source, produces `TextReplacement`s;
   `plan_cmake_integration` + `add_cmake_integration` append the `target_sources` edit. Writes
   nothing. Blockers: `MoveDryRunBlockerKind`.
4. `apply_move` — `apply_callable_move`: the only mutating layer, requires `--confirm`. Blockers:
   `MoveApplyBlockerKind`.

`plan` status `ready` means "the conservative checks available at this layer passed", **not**
"safe to write". Don't collapse these layers or let a later layer re-derive a policy decision the
planner owns.

### Invariants that constrain new code

Full rationale lives in `docs/architecture.md` and `docs/decisions/` (18 ADRs).

- **Never guess.** An unproven fact becomes a typed blocker or a `CallableConstraint`, not a
  plausible default. Dependency edges are only added when a specific AST node proves them
  (`getDirectCallee()`, explicit `TagTypeLoc`, …). Precision beats recall.
- **Accumulate blockers, don't short-circuit** on the first one.
- **Ranges are half-open `[begin, end)`** everywhere, matching Clang. Two ranges sharing a boundary
  don't overlap; two insertions at the same offset do conflict.
- **Preserve the source's byte format** — line endings come from the source text, never hardcoded.
- **TOCTOU guard:** every `TextReplacement` carries `expected_text`, re-verified against the file
  immediately before staging.
- **Apply is a recoverable transaction over three paths** (source, target, `CMakeLists.txt`): write
  staged files in full, keep a backup of the source, and only delete it after post-apply frontend
  re-analysis of *both* translation units and a successful build of the affected target. Rollback
  also needs a compensating `cmake -S -B` reconfigure, because the build graph is derived mutable
  state that a reverse rename does not restore.
- **The moved entity's source context matters**: `enclosing_namespaces` comes from the lexical
  `DeclContext` chain, not from parsing the qualified name, and `declaration_include` must be a
  proven direct `#include` whose resolved path matches the canonical declaration — otherwise block.

### Contracts that are versioned

- Exit codes `0..4` (`cli::ExitCode`) — `3` = safely blocked before any write, `4` = apply or
  validation failed (rolled back if possible). Documented in `docs/cli-contract.md`.
- JSON reports are hand-serialized in `src/reporting/json_report.cpp` (no JSON library) and each
  starts with `"schema_version": 1` and `"operation"`. Changing a field's meaning or removing one
  requires a new schema version.

Both are consumed programmatically. Treat them as public API.

## Conventions

- `.clang-format` (LLVM base, 4-space indent, 100 columns) — run it on touched files.
- Warnings: `/W4 /permissive-` on MSVC, `-Wall -Wextra -Wpedantic` otherwise.
- Result types are structs with a `std::string error` (or a blocker vector) plus
  `explicit operator bool()`. Follow that instead of exceptions or `std::expected`.
- Designated initializers are used throughout for these aggregates.
- Namespaces mirror directories: `codesplit::{analysis,cli,planning,reporting}`.
- Code, identifiers and comments are English; all `docs/` prose and the README are Russian.

## Workflow

The project follows a strict TDD + documentation loop, visible in the git history as paired
commits (`feat:`/`fix:` implementation, then `docs:` recording the decision):

1. Minimize the failing input, add a red regression test, confirm it fails for the right reason.
2. Fix locally, run the narrow test, then the full suite, then both build configurations.
3. Record the reasoning:
   - a new section in `docs/development-challenges.md` (context → symptom → cause → options
     considered → decision → verification → significance for the diploma);
   - a new numbered ADR in `docs/decisions/` if the decision must outlive the change;
   - tick the corresponding slice in `docs/roadmap.md`.

`docs/manual-verification.md` records manual runs against real projects (the acceptance run used
fmtlib/fmt 11.2.0); `docs/mvp-acceptance.md` pins the frozen 0.1.0 boundaries.

## Current boundaries (0.1.0)

Automated extraction handles exactly one free, non-inline, non-template, externally-linked function
with named parameters, a proven declaring header and a resolvable CMake target. Variadics, deduced
return types, rvalue-reference and unnamed parameters are blocked as `delegation_unsupported`. Any
known outgoing call/type/global/macro dependency blocks the move. Methods and classes appear in the
inventory but cannot be extracted yet. Only CMake is supported as a build system; anything else is
conservatively blocked.
