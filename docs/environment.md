# C17 development environment

Building the compiler or generating a native executable requires a C17 compiler
and the platform shell adapter. Running the built compiler or a normal generated
program does not require a compiler, Python, Node, CMake, AI access or a network
connection. Sanitizer binaries additionally need their matching runtime
libraries. The native tools are C programs. GCC/Clang-compatible flags are used;
MSVC is not currently supported.
CMake/CTest were unavailable locally; the small native tfdev driver is the
verified equivalent build/test path rather than an additional build dependency.

## Windows (verified)

`./scripts/build.ps1 environment`, `./scripts/build.ps1 build`,
`./scripts/build.ps1 test`, `./scripts/build.ps1 audit`,
`./scripts/build.ps1 check`, `./scripts/build.ps1 sanitize`.
`./scripts/build.ps1 release` builds with -O2 and runs the full native gate.
Clean builds use a fresh archive extraction; no generated binaries are shipped.
Pass `-Compiler PATH` or set CC for a compiler path (not shell flags).
`./build.bat check` is the convenience wrapper.
The wrapper detects Clang/GCC or the locally cached LLVM-MinGW toolchain and
adds its binary directory for runtime DLL discovery. Paths with spaces work.
Use the wrapper to run sanitizer gates. A scratch `*-san.exe` launched directly
may fail before running any tests if its LLVM-MinGW runtime DLL directory is
absent from that process's PATH. For manual probes, set PATH in the launching
PowerShell session. The native driver reports Windows child launch failures
as console errors; a loader failure is not a sanitizer result.

Verified: official LLVM-MinGW 20261020 UCRT x86_64, Clang 23.1.4, Windows x64.
Archive SHA256 fd5462b7f5d93b2ff981fcd3f9c7e427e47f3203c64ea91c27bcc444eebd74cd.
Optional `./scripts/bootstrap.ps1` downloads this pinned official compiler only
if needed; it is not a normal build prerequisite. Installed compiler use is offline.

## POSIX

`CC=clang sh scripts/build.sh environment` and `sh scripts/build.sh check`.
The standalone lexical submission passed actual macOS15 ARM/Intel AppleClang17
and Ubuntu24.04 GCC13.3 source builds and lexical tests. See lexical release
verification records. Later full-compiler SourceManager/parser phases have not
run on POSIX. Cross compilation alone does not verify target execution.

## Gates and limitations

Strict C17 flags: -Wall -Wextra -Werror -pedantic -Wconversion -Wshadow.
ASan/UBSan are verified in the Windows toolchain; LeakSanitizer is unsupported.
The native audit checks maintained files/configuration; independent code review
and allocation-failure counters supplement sanitizer coverage. Use clang --analyze
for static analysis; native debug symbols are retained in build outputs.
`build` builds the standalone lexer and the compiler driver. The compiler
can inspect tokens, the AST, machine plans, verified Core, execute via the
reference interpreter (`run`), and emit/compile C17 (`emit-c` and `build`).

## Packaging and coursework verification

This standalone syntax analyzer repository packages clean release archives via
`./scripts/package-syntax-analyzer.ps1` and verifies fresh extractions outside
the repository checkout via `./scripts/verify-syntax-packages.ps1`:

- `Ternfold_Syntax_Analyzer_Source_2026-11-27.zip`: complete source tree,
  headers, coursework examples, docs, and tests.
- `Ternfold_Syntax_Analyzer_Windows_x64_2026-11-27.zip`: statically linked
  Windows x64 `ternfold.exe`, `Run-Demo.cmd`, MinGW/LLVM license notices,
  bundled source tree, and checksums.

See [VERIFICATION.md](../VERIFICATION.md), [README.md](../README.md), and the
companion repositories [PPL_Group1_LexicalAnalyzer](https://github.com/afkDen/PPL_Group1_LexicalAnalyzer)
and [Ternfold 0.2.1](https://github.com/afkDen/ternfold).
