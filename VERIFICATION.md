# Verification and reproduction

This document records verified build, test, sanitizer, demonstration, and archive checks executed directly on this repository (`COSC 303: Programming Language Design`, `BSCS 3-2`, Group 1) under the documented Windows x64 toolchain.

## Verified environment and compiler flags

- **Windows x64 (Full Compiler + Syntax)**: `windows-2025`, LLVM-MinGW Clang `23.1.4` (`-std=c17 -Wall -Wextra -Werror -pedantic -Wconversion -Wshadow`)
- **macOS 15 ARM64 & Intel x64 (Syntax Analyzer + Mach-O Executables)**: `macos-15` (`arm64`) and `macos-15-intel` (`x86_64`), Apple Clang, `MACOSX_DEPLOYMENT_TARGET=15.0` (`syntax-check`, `syntax-sanitize`, and All-In-One native verification)
- **Linux Ubuntu 24.04 x64 (Syntax Analyzer Source)**: `ubuntu-24.04` (`cc` / GCC, `syntax-check`, `syntax-sanitize`, and `run-syntax-demo.sh`)
- **Sanitizers**: `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer` (with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; LeakSanitizer leak detection is disabled on Windows MinGW / CI runners)
- **Scope note**: Downstream full-compiler `Float` execution helpers require x86_64 Clang (`windows-2025` is verified end-to-end across all M01-M17 suites); POSIX CI runs `syntax-check` and `syntax-sanitize` covering the complete lexical, Symbol Table, parser, AST, diagnostics, resolution, and audit suites plus native Mach-O All-In-One tests.

## Reproduction commands

```powershell
./scripts/build.ps1 environment -Compiler PATH_TO_CLANG
./scripts/build.ps1 check -Compiler PATH_TO_CLANG
./scripts/build.ps1 sanitize -Compiler PATH_TO_CLANG
./scripts/run-syntax-demo.ps1 -Compiler PATH_TO_CLANG -IncludeInvalid -TraceTokens
```

## Observed suite results (both `check` and `sanitize` passed with exit `0`)

| Suite | Result (`check` and `sanitize`) | Notes |
|---|---|---|
| `test_symbol_table.c` | `219,429` checks passed (normal and ASan/UBSan) | Sequential `tf_symbol_table_next()`, SHA-256 integrity, `.tfd`-deletion non-bypass, malformed table rejection, `E001`-`E003`, OOM fault injection (`0` leaks) |
| `test_parser.c` | `143,722` checks passed | Recursive descent, Pratt precedence/associativity, `recover()`, `TF_AST_MISSING`, nesting limit (`1..128`), OOM sweep |
| `test_tokens_cli.c` | `9,314` checks passed (`58` token invocations, `28` compiler examples) | Includes `test_parse_table_cli()` (`tokens --classroom-table --parser-input`, `.tfd` removal, `parse-table`, `ast --from-symbol-table`, `--trace-tokens`, exit `1` and `2` checks) |
| `test_frontend_fuzz.c` | `768` cases passed (`seed=0x4d313446`) | Random/malformed frontend stress |
| `test_ast_dump.c` | `139` checks passed | Deterministic AST schema v1 JSON and text |
| `test_lexer.c` / `test_dfa.c` / `test_trace.c` / `test_classroom.c` / `test_demo.c` | All passed (`117,463`, `58,087`, `994`, `684`, `78` checks) | Handwritten C17 scanner, 215-state trie, classroom `Symbol Table.txt` parity |
| `test_integration.c` | `27` cases, `7,810` checks (`check`), `7,814` checks (`sanitize`), `1` capability skip | Unprivileged Windows skips symbolic-link alias creation test (`SKIP`) |
| `test_diagnostics.c` / `test_diagnostic_json.c` / `test_source_manager.c` | All passed (`5,228`, `740,770`, `235,295` checks) | `1` unprivileged symlink skip in `test_resolution.c` (`28,549` checks in `18` cases) |
| `test_examples.c` | `39/39` manifest cases executed, `39` sources inventoried, `0` coverage failures | Covers all `examples/syntax/*.tfd` valid runtime/check cases and invalid lexical tokenization |
| Downstream type, Core, interpreter, native backend, REPL, formatter, editor, and differential suites | All passed under `check` and `sanitize` | Includes `m14_generated`, `plan_properties`, `test_differential` (`44` cases), `test_execution_trace` (`43` pairs), `test_editor` (`3,310,165` checks) |
| `scripts/course_audit.c` | `10` syntax fixtures, `21` coursework files verified | Verifies manifest schema and required files |
| `tools/audit.c` | `342` checks, `0` failures | Confirms `src/lexer.c` has no word comparison calls, `src/symbol_table.c` does not call `tf_lex(`, and all active Markdown links resolve |

## One-command coursework demonstration

`./scripts/run-syntax-demo.ps1 -IncludeInvalid -TraceTokens` ran cleanly against `examples/syntax/complete.tfd` and `examples/syntax/invalid-recovery.tfd`:
- `demo-output/Symbol Table.txt`: two-column classroom lexical report (`612` rows including header).
- `demo-output/symbol-table.json`: lossless `ternfold-symbol-table` v1 (`612` tokens including `EOF`, SHA-256 `aa81dafdf6384d4af033c33e8dcb67e1df179c5e4dded1b3f10243dcea564e86`).
- `demo-output/ast.txt` and `demo-output/ast.json`: `AST root=1 has_errors=false nodes=469` (`exit 0`).
- `demo-output/invalid-recovery/`: `AST root=1 has_errors=true nodes=52` (`exit 1`) recovering `fn surviving_helper()` cleanly and printing 1-based line numbers (`4`, `9`, `10`, `14`, `16`).

## Project continuity

- **Lexical compatibility**: Preserves the exact C17 scanner, 215-state trie DFA, and two-column `Symbol Table.txt` contract verified in [afkDen/PPL_Group1_LexicalAnalyzer](https://github.com/afkDen/PPL_Group1_LexicalAnalyzer) (`lexical-submission-clean-2026-10-30`, commit `d44447034f3c0ef7de404a424dedc929843b4650`).
- **Full compiler compatibility**: Extracted cleanly from [afkDen/ternfold](https://github.com/afkDen/ternfold) (`v0.2.1`, `27a7b91300787704860c2caa653815ca450f81c9`) and the local `Ternfold_Repo` (`c17-foundation`, `fc54ce151b7ce31c713c0279a77639d4810aaa40`), where milestones M01-M17 are implemented and verified on Windows x64.

## Distribution packaging and fresh extraction verification

Release packages built and verified in GitHub Actions (`windows-2025`, `macos-15`, `macos-15-intel`, and `ubuntu-24.04`) and locally outside the repository checkout:
- `Ternfold_Syntax_Analyzer_All_In_One_2026-11-27.zip`: assembled on `macos-15` combining verified `windows-x64`, `macos-arm64`, and `macos-x64` native binaries with identical `source/` trees, verified on fresh extractions across Windows x64, Apple Silicon macOS 15, and Intel macOS 15 using `Run-Windows.cmd --batch`, `Run-Mac.command --batch`, `SHA256SUMS` / `shasum`, `otool -L`, `llvm-readobj`, filename-with-spaces, and `.tfd`-removal non-bypass parsing.
- `Ternfold_Syntax_Analyzer_Source_2026-11-27.zip`: SHA-256 verified against `.sha256`, extracted to a new directory, passes `environment`, `check`, `sanitize`, and `run-syntax-demo.ps1 -IncludeInvalid -TraceTokens` on Windows, plus `syntax-check` and `syntax-sanitize` on macOS and Ubuntu.
- `Ternfold_Syntax_Analyzer_Windows_x64_2026-11-27.zip`: every file verified against `SHA256SUMS`, `source-original.zip` matches the source archive hash, statically linked `bin/windows-x64/ternfold.exe` verified via `llvm-readobj` for UCRT/Windows-system-only DLL imports, executes `Run-Demo.cmd --batch` and `.tfd`-deletion non-bypass proof under a clean `PATH` containing only `System32` and `Windows`.

Exact SHA-256 digests and file manifests accompany the GitHub Release assets in `*.sha256` and `*.manifest.json`.
