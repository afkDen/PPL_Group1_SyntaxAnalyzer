param(
    [string]$InputFile = 'examples/syntax/complete.tfd',
    [string]$OutDir = 'demo-output',
    [string]$Compiler = $env:CC,
    [switch]$IncludeInvalid,
    [switch]$TraceTokens
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

function Format-WinArg([string]$Arg) {
    if ($Arg.Length -gt 0 -and $Arg -notmatch '[\s"]') { return $Arg }
    $sb = [Text.StringBuilder]::new('"')
    $i = 0
    while ($i -lt $Arg.Length) {
        $slashes = 0
        while ($i -lt $Arg.Length -and $Arg[$i] -eq '\') { $slashes++; $i++ }
        if ($i -eq $Arg.Length -or $Arg[$i] -eq '"') { $slashes *= 2 }
        if ($slashes -gt 0) { [void]$sb.Append('\', $slashes) }
        if ($i -lt $Arg.Length) {
            if ($Arg[$i] -eq '"') { [void]$sb.Append('\"') } else { [void]$sb.Append($Arg[$i]) }
            $i++
        }
    }
    [void]$sb.Append('"')
    return $sb.ToString()
}

function Invoke-NativeCapture([string]$ExePath, [string[]]$ArgList, [string]$OutFile, [string]$ErrFile) {
    $psi = [Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $ExePath
    $psi.Arguments = ($ArgList | ForEach-Object { Format-WinArg $_ }) -join ' '
    $psi.WorkingDirectory = $projectRoot
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    $proc = [Diagnostics.Process]::new()
    $proc.StartInfo = $psi
    try {
        if (-not $proc.Start()) { throw "Failed to launch $ExePath" }
        $msOut = [IO.MemoryStream]::new()
        $msErr = [IO.MemoryStream]::new()
        $tOut = $proc.StandardOutput.BaseStream.CopyToAsync($msOut)
        $tErr = $proc.StandardError.BaseStream.CopyToAsync($msErr)
        $proc.WaitForExit()
        [void]$tOut.GetAwaiter().GetResult()
        [void]$tErr.GetAwaiter().GetResult()
        if ($OutFile) { [IO.File]::WriteAllBytes($OutFile, $msOut.ToArray()) }
        if ($ErrFile) { [IO.File]::WriteAllBytes($ErrFile, $msErr.ToArray()) }
        return $proc.ExitCode
    } finally {
        $proc.Dispose()
    }
}

Push-Location $projectRoot
try {
    $resolvedInput = (Resolve-Path -LiteralPath $InputFile -ErrorAction Stop).Path
    $exe = Join-Path $projectRoot 'build/ternfold.exe'
    if (-not (Test-Path -LiteralPath $exe)) {
        if ($Compiler) {
            & (Join-Path $PSScriptRoot 'build.ps1') build -Compiler $Compiler
        } else {
            & (Join-Path $PSScriptRoot 'build.ps1') build
        }
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    }

    $outPath = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $projectRoot $OutDir }
    New-Item -ItemType Directory -Force -Path $outPath | Out-Null

    $tableJson = Join-Path $outPath 'symbol-table.json'
    $tokensText = Join-Path $outPath 'tokens.txt'
    $astJson = Join-Path $outPath 'ast.json'
    $astText = Join-Path $outPath 'ast.txt'
    $diagText = Join-Path $outPath 'diagnostics.txt'
    $diagJson = Join-Path $outPath 'diagnostics.json'
    $traceFile = Join-Path $outPath 'token-stream-trace.txt'
    $lexDiag = Join-Path $outPath 'lex-diagnostics.txt'

    Write-Output "=== Step 1: Lexical Analysis -> Symbol Table.txt & lossless symbol-table.json ==="
    $tokExit = Invoke-NativeCapture $exe @('tokens', $resolvedInput, '--classroom-table', $outPath, '--parser-input', $tableJson) $tokensText $lexDiag
    if ($tokExit -gt 1) {
        $errMsg = [IO.File]::ReadAllText($lexDiag, [Text.UTF8Encoding]::new($false))
        throw "Lexer exited unexpectedly with ${tokExit}: $errMsg"
    }
    Write-Output "Lexer exit: $tokExit"
    Write-Output "Classroom report: $(Join-Path $outPath 'Symbol Table.txt')"
    Write-Output "Parser-input table: $tableJson"

    Write-Output "=== Step 2: Syntax Analysis (reading sequential tokens from $tableJson only) ==="
    $textArgs = @('parse-table', $tableJson)
    if ($TraceTokens) { $textArgs += '--trace-tokens' }
    $textExit = Invoke-NativeCapture $exe $textArgs $astText $(if ($TraceTokens) { $traceFile } else { $diagText })
    $jsonExit = Invoke-NativeCapture $exe @('parse-table', $tableJson, '--json', '--diagnostics-json') $astJson $diagJson

    Write-Output "Parser text exit: $textExit | AST text: $astText"
    Write-Output "Parser JSON exit: $jsonExit | AST JSON: $astJson"
    Get-Content -LiteralPath $astText -TotalCount 5 | ForEach-Object { Write-Output "  $_" }

    if ($IncludeInvalid) {
        $invDir = Join-Path $outPath 'invalid-recovery'
        New-Item -ItemType Directory -Force -Path $invDir | Out-Null
        $invSrc = Join-Path $projectRoot 'examples/syntax/invalid-recovery.tfd'
        $invTable = Join-Path $invDir 'symbol-table.json'
        $invAst = Join-Path $invDir 'ast.json'
        $invDiag = Join-Path $invDir 'diagnostics.txt'
        $invTokExit = Invoke-NativeCapture $exe @('tokens', $invSrc, '--classroom-table', $invDir, '--parser-input', $invTable) (Join-Path $invDir 'tokens.txt') (Join-Path $invDir 'lex-diagnostics.txt')
        if ($invTokExit -gt 1) { throw "Invalid fixture lexer exited with $invTokExit" }
        $invExit = Invoke-NativeCapture $exe @('parse-table', $invTable, '--json') $invAst $invDiag

        Write-Output "=== Step 3: Invalid Syntax Recovery & Line-Numbered Diagnostics ==="
        Write-Output "Invalid recovery parser exit: $invExit (expected 1)"
        Write-Output "Recovered AST & diagnostics written to $invDir"
        Get-Content -LiteralPath $invDiag | Select-Object -First 24 | ForEach-Object { Write-Output "  $_" }
    }

    if ($jsonExit -gt 1) { exit $jsonExit }
    exit $jsonExit
} finally { Pop-Location }
