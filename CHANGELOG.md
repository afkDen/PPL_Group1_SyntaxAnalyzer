# Changelog

## Coursework Release 2026-11-27 (COSC 303, BSCS 3-2, Group 1 - Lexical Checking 2 / Syntax Checking 1)

- Extracted the ISO C17 parser (`src/parser.c`), AST (`src/ast.c`), AST dump (`src/ast_dump.c`), diagnostics (`src/diagnostics.c`), handwritten scanner (`src/lexer.c`, `src/name_dfa.inc`), and compiler harness from Ternfold 0.2.1 ([afkDen/ternfold](https://github.com/afkDen/ternfold)) while keeping full compatibility with the standalone lexical analyzer ([afkDen/PPL_Group1_LexicalAnalyzer](https://github.com/afkDen/PPL_Group1_LexicalAnalyzer)).
- Added lossless versioned Symbol Table serialization (`include/ternfold/symbol_table.h`, `src/symbol_table.c`) with streaming one-token-at-a-time consumption (`tf_symbol_table_next()`), live coordinate checks, DFA classification cross-checks, and SHA-256 verification.
- Added CLI subcommands `ternfold parse-table FILE`, `ternfold ast --from-symbol-table FILE`, `--trace-tokens`, and `ternfold tokens ... --classroom-table DIR --parser-input FILE`.
- Added valid and invalid coursework syntax fixtures under `examples/syntax/` plus one-command demo runners (`scripts/run-syntax-demo.ps1`, `scripts/run-syntax-demo.sh`).
- Added unit, negative, non-bypass, OOM, and CLI integration tests in `tests/test_symbol_table.c` and `tests/test_tokens_cli.c`.
- Added coursework documentation mapping every item on the professor checklist and grading sheet, transition diagrams, predictive parsing and Pratt precedence tables, packaging scripts, and GitHub Actions CI.
