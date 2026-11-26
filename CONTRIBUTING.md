# Contributing

All changes in this repository must keep the ISO C17 syntax analyzer standalone, warning-free, and aligned with the course specification and the full Ternfold compiler.

## Development checks

Run these gates from the repository root before committing (pass `-Compiler` if Clang is not on `PATH`):

```powershell
./scripts/build.ps1 environment
./scripts/build.ps1 check
./scripts/build.ps1 sanitize
./scripts/run-syntax-demo.ps1 -IncludeInvalid -TraceTokens
```

## Rules for changes

- Keep `src/parser.c`, `src/lexer.c`, `src/ast.c`, and `src/symbol_table.c` warning-clean under `-std=c17 -Wall -Wextra -Werror -pedantic -Wconversion -Wshadow`.
- Keep `src/lexer.c` free of string-comparison functions (`strcmp`, `strncmp`, `strstr`), and keep `src/symbol_table.c` free of calls to `tf_lex()`. Both invariants are enforced by `tools/audit.c`.
- Add new source or test files to `tools/tfdev.c` so `check` and `sanitize` execute them automatically.
- Do not commit build outputs (`build/`, `demo-output/`, `dist/`, `tmp/`) or cached toolchains.
- When editing explanatory Markdown files, update cross-links so `build/audit.exe` passes with zero broken links.

## Project alignment

- Standalone Lexical Analyzer coursework: [afkDen/PPL_Group1_LexicalAnalyzer](https://github.com/afkDen/PPL_Group1_LexicalAnalyzer)
- Full Ternfold 0.2.1 C17 compiler and language: [afkDen/ternfold](https://github.com/afkDen/ternfold)
