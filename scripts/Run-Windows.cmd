@echo off
setlocal DisableDelayedExpansion
set "CUSTOM_INPUT="
if not "%~1"=="" if /i not "%~1"=="--batch" if /i not "%~1"=="--demo" set "CUSTOM_INPUT=%~f1"

cd /d "%~dp0"
if not exist demo-output mkdir demo-output
if not exist demo-output\invalid-recovery mkdir demo-output\invalid-recovery

if not defined CUSTOM_INPUT goto :demo

if exist demo-output\symbol-table.json del /f /q demo-output\symbol-table.json
echo === Step 1: Lexical Analysis: "%CUSTOM_INPUT%" ===
"%~dp0bin\windows-x64\ternfold.exe" tokens "%CUSTOM_INPUT%" --classroom-table demo-output --parser-input demo-output\symbol-table.json > demo-output\tokens.txt 2> demo-output\lex-diagnostics.txt
set "rc=%errorlevel%"
if not exist demo-output\symbol-table.json goto :lex_failed

echo === Step 2: Syntax Analysis (one token at a time from symbol-table.json) ===
"%~dp0bin\windows-x64\ternfold.exe" parse-table demo-output\symbol-table.json --trace-tokens > demo-output\ast.txt 2> demo-output\token-stream-trace.txt
set "rc=%errorlevel%"
"%~dp0bin\windows-x64\ternfold.exe" parse-table demo-output\symbol-table.json --json --diagnostics-json > demo-output\ast.json 2> demo-output\diagnostics.json
echo Done. Outputs written to "%~dp0demo-output\"
exit /b %rc%

:lex_failed
type demo-output\lex-diagnostics.txt 1>&2
exit /b %rc%

:demo
if exist demo-output\symbol-table.json del /f /q demo-output\symbol-table.json
set "SRC_COMPLETE=%~dp0source\examples\syntax\complete.tfd"
set "SRC_INVALID=%~dp0source\examples\syntax\invalid-recovery.tfd"

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
if "%~1"=="" pause
exit /b %rc%
