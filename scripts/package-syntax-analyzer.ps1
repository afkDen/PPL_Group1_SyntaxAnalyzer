param(
    [string]$Tag = 'syntax-submission-2026-11-27',
    [string]$Compiler = $env:CC,
    [string]$OutDir = 'dist'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    if (-not $Compiler) {
        foreach ($candidate in @('clang', 'gcc', 'cc')) {
            $found = Get-Command $candidate -ErrorAction SilentlyContinue
            if ($found) { $Compiler = $found.Source; break }
        }
    }
    if (-not $Compiler) { throw 'Pass -Compiler pointing to verified LLVM-MinGW Clang 23.1.4 x64.' }
    $cc = (Get-Command $Compiler -ErrorAction Stop).Source
    $toolchain = Split-Path -Parent (Split-Path -Parent $cc)
    $ccVer = (& $cc --version | Out-String)
    if ($LASTEXITCODE -ne 0 -or $ccVer -notmatch 'x86_64-w64-windows-gnu') {
        throw "Windows x64 package requires LLVM-MinGW x86_64 Clang toolchain; got: $ccVer"
    }

    $outBase = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $projectRoot $OutDir }
    $distRoot = Join-Path $outBase $Tag
    if (Test-Path -LiteralPath $distRoot) { Remove-Item -Recurse -Force -LiteralPath $distRoot }
    New-Item -ItemType Directory -Force -Path $distRoot | Out-Null

    $sourceZipName = 'Ternfold_Syntax_Analyzer_Source_2026-11-27.zip'
    $winZipName = 'Ternfold_Syntax_Analyzer_Windows_x64_2026-11-27.zip'
    $sourceZipPath = Join-Path $distRoot $sourceZipName
    $winZipPath = Join-Path $distRoot $winZipName

    # Build source archive strictly from tracked non-temporary files (or exact allowlisted dirs/files).
    $tracked = @(git -C $projectRoot -c core.quotepath=false ls-files)
    if ($LASTEXITCODE -ne 0 -or -not $tracked.Count) {
        throw 'Git file listing failed; commit files cleanly before packaging.'
    }
    $sourceFiles = @($tracked | Where-Object {
        $_ -notmatch '^(build|dist|demo-output|output|tmp|\.uv-cache|\.venv)/' -and
        $_ -notmatch '\.(exe|o|obj|out|pdb|tmp)$'
    } | Sort-Object)

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $srcArchive = [IO.Compression.ZipFile]::Open($sourceZipPath, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($rel in $sourceFiles) {
            $full = Join-Path $projectRoot $rel
            if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Missing tracked file: $rel" }
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($srcArchive, $full, $rel, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally {
        $srcArchive.Dispose()
    }
    $sourceSha = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceZipPath).Hash.ToLowerInvariant()
    $utf8NoBom = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText("$sourceZipPath.sha256", "$sourceSha  $sourceZipName`n", $utf8NoBom)

    $srcManifest = [ordered]@{
        format = 'ternfold-syntax-archive-manifest'
        schema_version = 1
        package = $sourceZipName
        sha256 = $sourceSha
        file_count = $sourceFiles.Count
        files = @($sourceFiles | ForEach-Object {
            $p = Join-Path $projectRoot $_
            [ordered]@{
                path = $_
                bytes = (Get-Item -LiteralPath $p).Length
                sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLowerInvariant()
            }
        })
    } | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText((Join-Path $distRoot ($sourceZipName + '.manifest.json')), $srcManifest + "`n", $utf8NoBom)

    # Assemble Windows x64 runnable bundle
    $stage = Join-Path $distRoot '_stage_win64'
    $binDir = Join-Path $stage 'bin/windows-x64'
    $licDir = Join-Path $stage 'licenses/windows-x64'
    $bundleSrcDir = Join-Path $stage 'source'
    New-Item -ItemType Directory -Force -Path $binDir, $licDir, $bundleSrcDir | Out-Null

    $compilerSources = @(
        'src/source.c', 'src/lexer.c', 'src/report.c', 'src/source_manager.c',
        'src/diagnostics.c', 'src/token_dump.c', 'src/symbol_table.c',
        'src/ast.c', 'src/parser.c', 'src/ast_dump.c', 'src/resolve.c',
        'src/types.c', 'src/type_operators.c', 'src/typed_signatures.c',
        'src/literal_value.c', 'src/numeric.c', 'src/checked_numeric.c',
        'src/pattern_coverage.c', 'src/typed_patterns.c', 'src/check_constants.c',
        'src/check_builtins.c', 'src/check.c', 'src/check_pipeline.c',
        'src/machine_plan.c', 'src/plan_dump.c',
        'src/core/nodes.c', 'src/core/primitive_rules.c', 'src/core/lower_types.c',
        'src/core/lower_constants.c', 'src/core/lower_match.c', 'src/core/lower.c',
        'src/core/verify.c', 'src/core/optimize.c', 'src/core/dump.c',
        'src/execution/values.c', 'src/execution/scalars.c', 'src/execution/operations.c',
        'src/execution/builtins.c', 'src/execution/repl_value.c', 'src/execution/trace.c',
        'src/execution/trace_value.c', 'src/execution/trace_io.c', 'src/execution/trace_artifact.c',
        'src/execution/interpreter.c', 'src/execution/host_io.c', 'src/execution/driver_run.c',
        'src/backend/c_nodes.c', 'src/backend/c_metadata.c', 'src/backend/lifetimes.c',
        'src/backend/c_emit.c', 'src/backend/native_build.c', 'src/backend/driver_native.c',
        'src/repl_input.c', 'src/repl.c', 'src/formatting/format.c',
        'src/editor/snapshot.c', 'src/editor/protocol.c', 'src/compiler_main.c'
    )
    $exeOut = Join-Path $binDir 'ternfold.exe'
    $flags = @('-std=c17', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic', '-Wconversion', '-Wshadow',
               '-fno-fast-math', '-ffp-contract=off', '-static', '-Iinclude')
    & $cc @flags @compilerSources -o $exeOut
    if ($LASTEXITCODE -ne 0) { throw 'Static Windows binary build failed.' }

    $inspector = Join-Path (Split-Path -Parent $cc) 'llvm-readobj.exe'
    $imports = (& $inspector --file-headers --coff-imports $exeOut | Out-String)
    if ($LASTEXITCODE -ne 0 -or $imports -notmatch 'IMAGE_FILE_MACHINE_AMD64') {
        throw 'Built binary is not a valid Windows x64 PE executable.'
    }
    $dlls = @([regex]::Matches($imports, '(?m)^\s+Name: (\S+\.dll)\s*$') | ForEach-Object { $_.Groups[1].Value })
    foreach ($dll in $dlls) {
        if ($dll -notmatch '^(?i:KERNEL32\.dll|ADVAPI32\.dll|SHELL32\.dll|USER32\.dll|api-ms-win-crt-[a-z0-9-]+\.dll|ucrtbase\.dll)$') {
            throw "Unexpected external DLL dependency: $dll"
        }
    }

    $sanVer = $ccVer.Replace($toolchain.Replace('\', '/'), '<toolchain>').Replace($toolchain, '<toolchain>')
    $sanImports = $imports.Replace($exeOut, 'bin/windows-x64/ternfold.exe')
    [IO.File]::WriteAllText((Join-Path $binDir 'BUILD_INFO.txt'),
        "Compiler:`n$sanVer`nFlags: $($flags -join ' ')`n$sanImports", $utf8NoBom)

    if (Test-Path -LiteralPath (Join-Path $toolchain 'LICENSE.TXT')) {
        Copy-Item -LiteralPath (Join-Path $toolchain 'LICENSE.TXT') -Destination (Join-Path $licDir 'LLVM-LICENSE.txt')
    }
    foreach ($licName in @('COPYING', 'COPYING.MinGW-w64.txt', 'COPYING.MinGW-w64-runtime.txt', 'COPYING.winpthreads.txt', 'COPYING.winstorecompat.txt')) {
        $licPath = Join-Path $toolchain "x86_64-w64-mingw32/share/mingw32/$licName"
        if (Test-Path -LiteralPath $licPath) { Copy-Item -LiteralPath $licPath -Destination $licDir }
    }

    foreach ($rel in $sourceFiles) {
        $dest = Join-Path $bundleSrcDir $rel
        $parent = Split-Path -Parent $dest
        if (-not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        Copy-Item -LiteralPath (Join-Path $projectRoot $rel) -Destination $dest
    }
    Copy-Item -LiteralPath $sourceZipPath -Destination (Join-Path $stage 'source-original.zip')

    $runDemoCmd = @"
@echo off
setlocal DisableDelayedExpansion
cd /d "%~dp0"
if not exist demo-output mkdir demo-output
if not exist demo-output\invalid-recovery mkdir demo-output\invalid-recovery

set "SRC_COMPLETE=%~dp0source\examples\syntax\complete.tfd"
set "SRC_INVALID=%~dp0source\examples\syntax\invalid-recovery.tfd"
if not "%~1"=="" if not "%~1"=="--batch" set "SRC_COMPLETE=%~f1"

echo === Step 1: Lexical Analysis (complete) ===
"%~dp0bin\windows-x64\ternfold.exe" tokens "%SRC_COMPLETE%" --classroom-table demo-output --parser-input demo-output\symbol-table.json > demo-output\tokens.txt 2> demo-output\lex-diagnostics.txt
if errorlevel 2 exit /b 2

echo === Step 2: Syntax Analysis (one token at a time from symbol-table.json) ===
"%~dp0bin\windows-x64\ternfold.exe" parse-table demo-output\symbol-table.json --trace-tokens > demo-output\ast.txt 2> demo-output\token-stream-trace.txt
set "rc=%errorlevel%"
"%~dp0bin\windows-x64\ternfold.exe" parse-table demo-output\symbol-table.json --json --diagnostics-json > demo-output\ast.json 2> demo-output\diagnostics.json

echo === Step 3: Invalid Syntax Recovery ^& Line Numbers ===
"%~dp0bin\windows-x64\ternfold.exe" tokens "%SRC_INVALID%" --classroom-table demo-output\invalid-recovery --parser-input demo-output\invalid-recovery\symbol-table.json > demo-output\invalid-recovery\tokens.txt 2> demo-output\invalid-recovery\lex-diagnostics.txt
"%~dp0bin\windows-x64\ternfold.exe" parse-table demo-output\invalid-recovery\symbol-table.json --json > demo-output\invalid-recovery\ast.json 2> demo-output\invalid-recovery\diagnostics.txt

echo Done. Outputs written to "%~dp0demo-output\"
if not "%~1"=="--batch" if not "%~2"=="--batch" pause
exit /b %rc%
"@
    [IO.File]::WriteAllText((Join-Path $stage 'Run-Demo.cmd'), $runDemoCmd.Replace("`r`n", "`n").Replace("`n", "`r`n"), $utf8NoBom)

    $bundleReadme = @"
# Ternfold C17 Syntax Analyzer - Windows x64 Runnable Bundle

This self-contained archive packages the Ternfold C17 Syntax Analyzer (`COSC 303: Programming Language Design`, `BSCS 3-2`, Group 1). It includes `bin\windows-x64\ternfold.exe` (statically linked against Windows UCRT with no external DLL dependencies), the exact verified source snapshot under `source\`, `source-original.zip`, toolchain license notices, and `Run-Demo.cmd`.

## Run without compiling

Double-click `Run-Demo.cmd`, or run in Command Prompt / PowerShell:

```bat
Run-Demo.cmd --batch
```

Generated artifacts appear in `demo-output\`:
- `demo-output\Symbol Table.txt`: two-column classroom lexical report
- `demo-output\symbol-table.json`: lossless `ternfold-symbol-table` v1 consumed sequentially
- `demo-output\token-stream-trace.txt`: token-by-token read log from `tf_symbol_table_next()`
- `demo-output\ast.txt` and `demo-output\ast.json`: AST output (`469` nodes on `complete.tfd`)
- `demo-output\invalid-recovery\diagnostics.txt`: line-numbered syntax recovery report
"@
    [IO.File]::WriteAllText((Join-Path $stage 'README.md'), $bundleReadme, $utf8NoBom)

    $payloadFiles = @(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName)
    $shaRows = @($payloadFiles | ForEach-Object {
        $rel = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
        $h = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
        "$h  $rel"
    } | Sort-Object)
    [IO.File]::WriteAllText((Join-Path $stage 'SHA256SUMS'), ($shaRows -join "`n") + "`n", $utf8NoBom)

    $allStage = @(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName)
    $winArchive = [IO.Compression.ZipFile]::Open($winZipPath, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($item in $allStage) {
            $rel = $item.FullName.Substring($stage.Length + 1).Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($winArchive, $item.FullName, $rel, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally {
        $winArchive.Dispose()
    }
    $winSha = (Get-FileHash -Algorithm SHA256 -LiteralPath $winZipPath).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText("$winZipPath.sha256", "$winSha  $winZipName`n", $utf8NoBom)

    $winManifest = [ordered]@{
        format = 'ternfold-syntax-archive-manifest'
        schema_version = 1
        package = $winZipName
        sha256 = $winSha
        source_package = $sourceZipName
        source_sha256 = $sourceSha
        file_count = $allStage.Count
        files = @($allStage | ForEach-Object {
            $rel = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
            [ordered]@{
                path = $rel
                bytes = $_.Length
                sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
            }
        })
    } | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText((Join-Path $distRoot ($winZipName + '.manifest.json')), $winManifest + "`n", $utf8NoBom)

    Remove-Item -Recurse -Force -LiteralPath $stage

    Write-Output "PASS: packaged $sourceZipName ($sourceSha)"
    Write-Output "PASS: packaged $winZipName ($winSha)"
    Write-Output "Artifacts in: $distRoot"
} finally { Pop-Location }
