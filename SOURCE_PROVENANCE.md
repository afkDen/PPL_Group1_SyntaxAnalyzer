# Source provenance and project continuity

## Related Ternfold repositories

This repository packages the standalone ISO C17 syntax analyzer deliverable (`COSC 303: Programming Language Design`, `BSCS 3-2`, Group 1, [afkDen/PPL_Group1_SyntaxAnalyzer](https://github.com/afkDen/PPL_Group1_SyntaxAnalyzer)) so it builds, runs, and packages independently while staying cohesive with both sibling Ternfold repositories:

| Repository | Role | Reference checkpoint |
|---|---|---|
| [afkDen/PPL_Group1_LexicalAnalyzer](https://github.com/afkDen/PPL_Group1_LexicalAnalyzer) | Standalone Lexical Analyzer coursework (`Symbol Table.txt`, 28 keywords, 25 reserved names, 7+9 operators, 215-state trie DFA) | Release `lexical-submission-clean-2026-10-30`, commit `d44447034f3c0ef7de404a424dedc929843b4650` |
| [afkDen/PPL_Group1_SyntaxAnalyzer](https://github.com/afkDen/PPL_Group1_SyntaxAnalyzer) | Standalone Syntax Analyzer coursework (lossless `ternfold-symbol-table` v1 sequential token reader + recursive-descent/Pratt parser) | This repository (`COSC 303`, `BSCS 3-2`, Group 1) |
| [afkDen/ternfold](https://github.com/afkDen/ternfold) | Complete Ternfold 0.2.1 ISO C17 language, compiler, verified Core interpreter, runtime, and native C17 backend (all milestones M01-M17 implemented and verified on Windows x64) | Public release `v0.2.1` (`27a7b91300787704860c2caa653815ca450f81c9`), local `c17-foundation` (`fc54ce151b7ce31c713c0279a77639d4810aaa40`) |

Both upstream repositories (`afkDen/ternfold` and `afkDen/PPL_Group1_LexicalAnalyzer`) remain unchanged by coursework packaging here.

## Unmodified C17 core syntax and lexical files (SHA-256 verified)

The recursive-descent parser, AST arena, AST dumper, handwritten scanner, and name DFA retain their exact upstream C17 implementation bytes:

| File | Role | SHA-256 |
|---|---|---|
| `src/parser.c` | Recursive-descent and Pratt parser + panic recovery | `57448a0878d9e929eb2e538bb95394deee688f980e305660970da5bcc422982e` |
| `src/ast.c` | Arena allocator and owner-checked AST nodes | `3f2387a464590d353130b0ac998d3bdb7953a0504d4891f9d93b3a5a26a83653` |
| `src/ast_dump.c` | Deterministic schema v1 AST JSON and text writer | `1d6bbf8db7cfc7de399bb2134dc9c1d77e45175687f76b297034271fb56332f6` |
| `src/lexer.c` | Handwritten C17 lexical scanner | `56f95b1afba7ac2af47a03c27d74e6d53c932f59989dd22e79f7b34aeea07918` |
| `src/name_dfa.inc` | Fixed 215-state deterministic keyword/reserved trie | `62e5666caa33905f7d7fd63820bd419956bcfe43c0b3c6e9736f97494f882c41` |
| `src/source.c` | UTF-8 validation, line/scalar index, and allocator helpers | `a5762b2bfc8979094ab01f00f2f38c6adfa6fcfb2bac81d9703e379bb47b9c73` |
| `src/report.c` | Classroom and lossless two-column `Symbol Table.txt` writer | `aafae40099f67acdc2d454e5d579ca49db5dcd2eb5bf1ec4dd1e810d2aa8adfc` |
| `src/token_dump.c` | Schema v1 token stream inspection serializer | `2524aea27765d7e711cf71ff8ee9a51a5602bb403c1c927d33e333152127404a` |
| `src/diagnostics.c` | Diagnostic bag and schema v2 JSON/text renderer | `4fb746f48db50785034e8c9c661eb520cc2e0a332195c7138a515b22c05a7242` |
| `src/source_manager.c` | Source caching and canonical file identity manager | `02c6f78d27265341b1b0193e42f60a3d9e03cbb40fc8d412e7f978fc18861511` |

## Coursework additions and repository adaptations

- Added `include/ternfold/symbol_table.h` and `src/symbol_table.c`: lossless `ternfold-symbol-table` writer/atomic publisher and sequential one-token-at-a-time reader (`tf_symbol_table_next()`) with SHA-256 verification.
- Updated `src/compiler_main.c`: added `tokens --classroom-table DIR --parser-input FILE`, `parse-table FILE`, and `ast --from-symbol-table FILE` with optional `--trace-tokens`.
- Added `tests/test_symbol_table.c`, extended `tests/test_tokens_cli.c`, and updated `tools/tfdev.c`, `tools/audit.c`, and `scripts/course_audit.c` for standalone coursework checks.
- Added `examples/syntax/*`, `scripts/run-syntax-demo.ps1`, `scripts/run-syntax-demo.sh`, `scripts/Run-Windows.cmd`, `scripts/Run-Mac.command`, cross-platform All-In-One packaging scripts (`package-syntax-analyzer.ps1`, `package-mac-payload.sh`, `assemble-all-in-one.sh`, `verify-*-*`), CI workflows, and `docs/syntax-analyzer/*`.
- Removed internal development logs, historical Python host (`src/ternfold/`), and proposal PDFs from the standalone submission tree while keeping compiler documentation links self-contained.
