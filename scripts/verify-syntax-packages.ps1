param(
    [Parameter(Mandatory=$true)][string]$ReleaseDir,
    [Parameter(Mandatory=$true)][string]$Compiler,
    [string]$TempRoot = ''
)
$ErrorActionPreference = 'Stop'
$dir = (Resolve-Path -LiteralPath $ReleaseDir -ErrorAction Stop).Path
$cc = (Get-Command $Compiler -ErrorAction Stop).Source

$sourceZip = Join-Path $dir 'Ternfold_Syntax_Analyzer_Source_2026-11-27.zip'
$winZip = Join-Path $dir 'Ternfold_Syntax_Analyzer_Windows_x64_2026-11-27.zip'
foreach ($f in @($sourceZip, "$sourceZip.sha256", $winZip, "$winZip.sha256")) {
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { throw "Missing package asset: $f" }
}

$expectedSrcHash = ((Get-Content -LiteralPath "$sourceZip.sha256").Trim() -split '\s+')[0].ToLowerInvariant()
$actualSrcHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceZip).Hash.ToLowerInvariant()
if ($expectedSrcHash -cne $actualSrcHash) { throw "Source ZIP SHA-256 mismatch: $actualSrcHash != $expectedSrcHash" }

$expectedWinHash = ((Get-Content -LiteralPath "$winZip.sha256").Trim() -split '\s+')[0].ToLowerInvariant()
$actualWinHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $winZip).Hash.ToLowerInvariant()
if ($expectedWinHash -cne $actualWinHash) { throw "Windows ZIP SHA-256 mismatch: $actualWinHash != $expectedWinHash" }

if (-not $TempRoot) {
    $TempRoot = Join-Path ([IO.Path]::GetTempPath()) ("tf-syntax-verify-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
}
$srcExtract = Join-Path $TempRoot 'source-extract'
$winExtract = Join-Path $TempRoot 'win64-extract'
New-Item -ItemType Directory -Force -Path $srcExtract, $winExtract | Out-Null

Add-Type -AssemblyName System.IO.Compression.FileSystem
Write-Output "=== Verifying fresh extraction of Source ZIP into $srcExtract ==="
[IO.Compression.ZipFile]::ExtractToDirectory($sourceZip, $srcExtract)
Push-Location $srcExtract
try {
    if (Get-Command pwsh -ErrorAction SilentlyContinue) {
        & pwsh -NoProfile -File (Join-Path $srcExtract 'scripts/build.ps1') environment -Compiler $cc
    } else {
        & (Join-Path $srcExtract 'scripts/build.ps1') environment -Compiler $cc
    }
    if ($LASTEXITCODE -ne 0) { throw 'Fresh source environment check failed.' }
    & (Join-Path $srcExtract 'scripts/build.ps1') check -Compiler $cc
    if ($LASTEXITCODE -ne 0) { throw 'Fresh source check gate failed.' }
    & (Join-Path $srcExtract 'scripts/build.ps1') sanitize -Compiler $cc
    if ($LASTEXITCODE -ne 0) { throw 'Fresh source sanitize gate failed.' }
    & (Join-Path $srcExtract 'scripts/run-syntax-demo.ps1') -Compiler $cc -IncludeInvalid -TraceTokens
    if ($LASTEXITCODE -ne 0) { throw 'Fresh source demo script failed.' }
} finally { Pop-Location }

Write-Output "=== Verifying fresh extraction of Windows x64 ZIP into $winExtract ==="
[IO.Compression.ZipFile]::ExtractToDirectory($winZip, $winExtract)
$rows = @(Get-Content -LiteralPath (Join-Path $winExtract 'SHA256SUMS'))
foreach ($row in $rows) {
    if ($row -notmatch '^([a-f0-9]{64})  (.+)$') { throw "Malformed SHA256SUMS line: $row" }
    $path = Join-Path $winExtract $Matches[2]
    $h = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    if ($h -cne $Matches[1]) { throw "Bundle checksum mismatch: $($Matches[2])" }
}
$bundledOrigHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $winExtract 'source-original.zip')).Hash.ToLowerInvariant()
if ($bundledOrigHash -cne $expectedSrcHash) { throw 'Bundled source-original.zip differs from source archive.' }

$savedPath = $env:PATH
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
Push-Location $winExtract
try {
    cmd /d /c Run-Demo.cmd --batch
    if ($LASTEXITCODE -ne 0) { throw "Run-Demo.cmd --batch failed with exit $LASTEXITCODE under minimal PATH" }
    foreach ($req in @(
        'demo-output/Symbol Table.txt',
        'demo-output/symbol-table.json',
        'demo-output/token-stream-trace.txt',
        'demo-output/ast.json',
        'demo-output/invalid-recovery/diagnostics.txt'
    )) {
        if (-not (Test-Path -LiteralPath $req -PathType Leaf)) { throw "Missing demo artifact: $req" }
    }
    $astText = [IO.File]::ReadAllText((Join-Path $winExtract 'demo-output/ast.txt'))
    if ($astText -notmatch 'AST root=1 has_errors=false nodes=469') {
        throw "Unexpected AST summary: $astText"
    }
    $invDiag = [IO.File]::ReadAllText((Join-Path $winExtract 'demo-output/invalid-recovery/diagnostics.txt'))
    if ($invDiag -notmatch 'error\[E110\]') {
        throw 'Missing error[E110] in invalid-recovery diagnostics.'
    }
    # Non-bypass proof directly on bundled ternfold.exe
    Copy-Item -LiteralPath 'source/examples/syntax/complete.tfd' -Destination 'temp_live.tfd'
    & ./bin/windows-x64/ternfold.exe tokens temp_live.tfd --parser-input temp_live.st.json
    if ($LASTEXITCODE -ne 0) { throw 'Bundled tokens export failed.' }
    Remove-Item -LiteralPath 'temp_live.tfd'
    & ./bin/windows-x64/ternfold.exe parse-table temp_live.st.json --json > temp_live.ast.json
    if ($LASTEXITCODE -ne 0) { throw 'Bundled parse-table failed after .tfd removal.' }
} finally {
    Pop-Location
    $env:PATH = $savedPath
}

Write-Output "PASS: Both source and Windows x64 archives verified cleanly at $TempRoot"
