# Start here

Quick setup and run guide for `COSC 303: Programming Language Design` (`BSCS 3-2`, Group 1).

## Option A: Run the prebuilt Windows x64 bundle (no compiler needed)

1. Download `Ternfold_Syntax_Analyzer_Windows_x64_2026-11-27.zip` from the [GitHub releases page](https://github.com/afkDen/PPL_Group1_SyntaxAnalyzer/releases).
2. Extract the full archive to a new directory.
3. Run `Run-Demo.cmd` (or `./Run-Demo.cmd` in PowerShell).
4. Inspect the generated outputs inside `demo-output/`:
   - `demo-output/Symbol Table.txt` (classroom two-column table)
   - `demo-output/symbol-table.json` (lossless token stream parsed one token at a time)
   - `demo-output/ast.txt` and `demo-output/ast.json`
   - `demo-output/invalid-recovery/diagnostics.txt` (line-numbered error recovery)

## Option B: Build and test from source

Extract `Ternfold_Syntax_Analyzer_Source_2026-11-27.zip` (or clone this repository), ensure Clang/GCC C17 is available, and run:

### Windows PowerShell

```powershell
# Optional if Clang/GCC is not on PATH: pass -Compiler 'C:\path\to\clang.exe'
./scripts/build.ps1 environment
./scripts/build.ps1 check
./scripts/build.ps1 sanitize
./scripts/run-syntax-demo.ps1 -IncludeInvalid -TraceTokens
```

### POSIX (macOS / Linux)

Note: full compiler targets using `Float` helpers require `x86_64` Clang without `-ffast-math`. On Windows x64 LLVM-MinGW Clang 23.1.4, all suites including sanitizers are verified.

```sh
sh scripts/build.sh environment
sh scripts/build.sh build
sh scripts/run-syntax-demo.sh examples/syntax/complete.tfd demo-output
```

## Two-step modular execution

```powershell
./build/ternfold.exe tokens examples/syntax/complete.tfd --classroom-table demo-output --parser-input demo-output/symbol-table.json
./build/ternfold.exe parse-table demo-output/symbol-table.json --trace-tokens --json
```

Next reading: [RUBRIC.md](RUBRIC.md), [PROFESSOR_DEMO_GUIDE.md](PROFESSOR_DEMO_GUIDE.md), and [docs/README.md](docs/README.md).
