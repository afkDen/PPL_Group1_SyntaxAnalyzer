# Ternfold C17 Syntax Analyzer (`COSC 303: Programming Language Design`)

This standalone ISO C17 repository packages the Ternfold 0.2 syntax analyzer for course evaluation (`COSC 303: Programming Language Design`, `BSCS 3-2`, Group 1). It pairs the handwritten lexical scanner with a separate symbol-table file interface (`ternfold-symbol-table` v1) so the recursive-descent and Pratt parser consumes tokens sequentially from disk, one token at a time, without re-opening the `.tfd` source or calling `tf_lex()`.

## Quick start

### Option A: Run without compiling (All-In-One or Windows x64 ZIP)

- **Cross-platform All-In-One (`Ternfold_Syntax_Analyzer_All_In_One_2026-11-27.zip`)**: Download from [GitHub Releases](https://github.com/afkDen/PPL_Group1_SyntaxAnalyzer/releases), extract the archive, and run `Run-Windows.cmd` on Windows x64 or `./Run-Mac.command` on macOS 15+ (Apple Silicon `arm64` or Intel `x86_64`). Both execute the two-step lexical-to-syntax pipeline against `examples/syntax/complete.tfd` and `invalid-recovery.tfd`, writing outputs into `demo-output/`.
- **Windows x64 (`Ternfold_Syntax_Analyzer_Windows_x64_2026-11-27.zip`)**: Prebuilt Windows x64 executable (`Run-Demo.cmd` / `Run-Windows.cmd`) and exact bundled source.

### Option B: Compile from source

```powershell
# Windows PowerShell (pass -Compiler PATH if clang/gcc is not on PATH)
./scripts/build.ps1 build
./scripts/run-syntax-demo.ps1 -IncludeInvalid -TraceTokens
```

On macOS 15+ or Linux (`Clang` / `GCC`):

```sh
sh scripts/build.sh syntax-check
sh scripts/build.sh syntax-sanitize
sh scripts/run-syntax-demo.sh examples/syntax/complete.tfd demo-output
```

Run the full Windows compiler, runtime, backend, and sanitizer suites:

```powershell
./scripts/build.ps1 environment
./scripts/build.ps1 check
./scripts/build.ps1 sanitize
```

## Two-step Symbol Table architecture

1. Lexical step (`ternfold tokens source.tfd --classroom-table OUT_DIR --parser-input FILE`) produces both the classroom two-column `Symbol Table.txt` and the lossless `ternfold-symbol-table` v1 file.
2. Syntax step (`ternfold parse-table FILE [--json] [--diagnostics-json] [--trace-tokens]`, or `ternfold ast --from-symbol-table FILE`) opens only `FILE`, verifies the header and SHA-256 digest while reading token entries one by one through `tf_symbol_table_next()`, and feeds the reconstructed token stream into `tf_parse()`.

See [SYMBOL_TABLE_INPUT.md](docs/syntax-analyzer/SYMBOL_TABLE_INPUT.md) for the schema specification and non-bypass guarantees.

## Professor checklist and honest scope

Full rubric cross-references live in [RUBRIC.md](RUBRIC.md) and [PROFESSOR_DEMO_GUIDE.md](PROFESSOR_DEMO_GUIDE.md). Supported constructs by rubric area:

- Input (`/1`) and Output (`/3`): `input name: Text from stdin;` and all three `output` variants (string literal, variable, `++` concatenation) in root zero-arg `pub fn main()`.
- Assignment (`/3`): constant (`let x: Int = 10;`), left-value copy (`let y = x;`, machine `next cell = expr;`), and compound expression bindings. Ternfold uses immutable `let` and machine `next` writers rather than mutable reassignment.
- Conditions (`/3`): `if ... else ...`, nested `if ... else`, and `match` case expressions. Ternfold `if` is value-returning and requires `else`; single-branch `if` without `else` diagnoses `E110`.
- Iterative (`/2`): `for` and nested `for` list comprehensions (`1/2`); `while` and `do-while` loops do not exist in Ternfold 0.2.
- Declarations, recovery, and messages: `import`, `record`, `enum`, `machine` (`state`/`next`/`guard`/`emit`), `fn`, `let`; panic-mode recovery in `recover()` (batch CLI, no interactive user prompt); 1-based line numbers and descriptions (`E110`, `E111`, `E112`, `E208`).

## Related repositories

This submission forms the Syntax Analyzer stage of the Ternfold coursework and compiler family:

- **Lexical Analyzer (`PPL_Group1_LexicalAnalyzer`)**: [afkDen/PPL_Group1_LexicalAnalyzer](https://github.com/afkDen/PPL_Group1_LexicalAnalyzer) (release `lexical-submission-clean-2026-10-30`) defines the two-column `Symbol Table.txt`, 215-state trie DFA, 28 keywords, 25 reserved names, and 7+9 operator scanner preserved in `src/lexer.c`.
- **Full Language and Compiler (`ternfold` v0.2.1)**: [afkDen/ternfold](https://github.com/afkDen/ternfold) contains the complete Ternfold ISO C17 compiler, verified Core interpreter, runtime, and native C17 backend (all milestones M01-M17 implemented and verified on Windows x64), from which this repository extracts its parser, AST, diagnostics, and downstream test harness without modification.

## Documentation and release artifacts

- [START_HERE.md](START_HERE.md): Windows bundle and source build walkthrough
- [RUBRIC.md](RUBRIC.md): Exact 12-item grading sheet and guidelines cross-reference
- [PROFESSOR_DEMO_GUIDE.md](PROFESSOR_DEMO_GUIDE.md): Step-by-step live demo and Q&A
- [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md): Upstream commit attribution and SHA-256 hashes
- [VERIFICATION.md](VERIFICATION.md): Exact observed compiler, sanitizer, demo, and fresh-extraction test records
- [docs/README.md](docs/README.md): Grammar, parsing table, recursive descent, transition diagrams, error recovery, and row/invalid indices

Release packages on [Releases](https://github.com/afkDen/PPL_Group1_SyntaxAnalyzer/releases):
- `Ternfold_Syntax_Analyzer_All_In_One_2026-11-27.zip`: Prebuilt Windows x64, macOS Apple Silicon (`arm64`), and macOS Intel (`x86_64`) executables, launchers (`Run-Windows.cmd`, `Run-Mac.command`), MinGW/LLVM notices, bundled source snapshot (`source/` + `source-original.zip`), `SHA256SUMS`, and sidecars.
- `Ternfold_Syntax_Analyzer_Windows_x64_2026-11-27.zip`: Windows x64 executable, launchers (`Run-Demo.cmd`, `Run-Windows.cmd`), MinGW/LLVM notices, bundled source snapshot, and SHA-256 sidecar/manifest.
- `Ternfold_Syntax_Analyzer_Source_2026-11-27.zip`: Source, headers, coursework examples, docs, scripts, tests, and SHA-256 sidecar/manifest.
