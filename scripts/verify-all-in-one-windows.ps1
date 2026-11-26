param(
    [Parameter(Mandatory=$true)][string]$Zip,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$SourceSha256,
    [string]$TempRoot = ''
)
$ErrorActionPreference = 'Stop'
$zipFull = (Resolve-Path -LiteralPath $Zip -ErrorAction Stop).Path
$expectedSrcSha = $SourceSha256.ToLowerInvariant()

if (Test-Path -LiteralPath "$zipFull.sha256" -PathType Leaf) {
    $expectedZipHash = ((Get-Content -LiteralPath "$zipFull.sha256").Trim() -split '\s+')[0].ToLowerInvariant()
    $actualZipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipFull).Hash.ToLowerInvariant()
    if ($expectedZipHash -cne $actualZipHash) { throw "All-In-One ZIP SHA-256 mismatch: $actualZipHash != $expectedZipHash" }
}

if (-not $TempRoot) {
    $TempRoot = Join-Path ([IO.Path]::GetTempPath()) ("tf-allinone-win-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
}
if (Test-Path -LiteralPath $TempRoot) { Remove-Item -Recurse -Force -LiteralPath $TempRoot }
New-Item -ItemType Directory -Force -Path $TempRoot | Out-Null

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($zipFull, $TempRoot)

$rows = @(Get-Content -LiteralPath (Join-Path $TempRoot 'SHA256SUMS'))
foreach ($row in $rows) {
    if ($row -notmatch '^([a-f0-9]{64})  (.+)$') { throw "Malformed SHA256SUMS line: $row" }
    $p = Join-Path $TempRoot $Matches[2]
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "Missing bundled file: $($Matches[2])" }
    $h = (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLowerInvariant()
    if ($h -cne $Matches[1]) { throw "Bundle checksum mismatch: $($Matches[2])" }
}
$origHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $TempRoot 'source-original.zip')).Hash.ToLowerInvariant()
if ($origHash -cne $expectedSrcSha) { throw "Bundled source-original.zip hash mismatch" }

foreach ($reqBin in @('bin/windows-x64/ternfold.exe', 'bin/macos-arm64/ternfold', 'bin/macos-x64/ternfold', 'Run-Mac.command', 'Run-Windows.cmd')) {
    if (-not (Test-Path -LiteralPath (Join-Path $TempRoot $reqBin) -PathType Leaf)) {
        throw "Missing required cross-platform entry: $reqBin"
    }
}

$savedPath = $env:PATH
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
Push-Location $TempRoot
try {
    cmd /d /c Run-Windows.cmd --batch
    if ($LASTEXITCODE -ne 0) { throw "Run-Windows.cmd --batch failed with exit $LASTEXITCODE" }
    $astText = [IO.File]::ReadAllText((Join-Path $TempRoot 'demo-output/ast.txt'))
    if ($astText -notmatch 'AST root=1 has_errors=false nodes=469') { throw "Unexpected AST output: $astText" }
    $traceText = [IO.File]::ReadAllText((Join-Path $TempRoot 'demo-output/token-stream-trace.txt'))
    if ($traceText -notmatch '\[symbol-table #0\]') { throw 'Missing token-stream trace.' }
    $invDiag = [IO.File]::ReadAllText((Join-Path $TempRoot 'demo-output/invalid-recovery/diagnostics.txt'))
    if ($invDiag -notmatch 'error\[E110\]') { throw 'Missing E110 in invalid recovery diagnostics.' }

    # Path with spaces
    Copy-Item -LiteralPath 'source/examples/syntax/complete.tfd' -Destination 'path with spaces.tfd'
    cmd /d /c Run-Windows.cmd "path with spaces.tfd"
    if ($LASTEXITCODE -ne 0) { throw 'Run-Windows.cmd failed on filename with spaces.' }

    # Non-bypass proof directly on bundled Windows CLI
    Copy-Item -LiteralPath 'source/examples/syntax/complete.tfd' -Destination 'temp_live.tfd'
    & ./bin/windows-x64/ternfold.exe tokens temp_live.tfd --parser-input temp_live.st.json
    if ($LASTEXITCODE -ne 0) { throw 'tokens export failed.' }
    Remove-Item -LiteralPath 'temp_live.tfd'
    $nbOut = (& ./bin/windows-x64/ternfold.exe parse-table temp_live.st.json | Out-String)
    if ($LASTEXITCODE -ne 0 -or $nbOut -notmatch 'AST root=1 has_errors=false nodes=469') { throw 'Non-bypass parse failed.' }

    Write-Output "PASS: Windows verification of All-In-One ZIP succeeded at $TempRoot"
} finally {
    Pop-Location
    $env:PATH = $savedPath
}
