#!/bin/sh
set -eu
caller_pwd=$PWD
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
case "$(uname -m)" in
  arm64) target=macos-arm64 ;;
  x86_64) target=macos-x64 ;;
  *) echo 'Supported Macs: Apple Silicon (arm64) or Intel (x86_64).' >&2; exit 2 ;;
esac
mkdir -p demo-output demo-output/invalid-recovery
bin="./bin/$target/ternfold"

if [ "$#" -eq 0 ] || [ "${1-}" = "--demo" ] || [ "${1-}" = "--batch" ]; then
  src_complete="source/examples/syntax/complete.tfd"
  src_invalid="source/examples/syntax/invalid-recovery.tfd"
  rm -f demo-output/symbol-table.json
  printf '=== Step 1: Lexical Analysis (complete) ===\n'
  "$bin" tokens "$src_complete" --classroom-table demo-output --parser-input demo-output/symbol-table.json > demo-output/tokens.txt 2> demo-output/lex-diagnostics.txt
  printf '=== Step 2: Syntax Analysis (one token at a time from symbol-table.json) ===\n'
  rc=0
  "$bin" parse-table demo-output/symbol-table.json --trace-tokens > demo-output/ast.txt 2> demo-output/token-stream-trace.txt || rc=$?
  "$bin" parse-table demo-output/symbol-table.json --json --diagnostics-json > demo-output/ast.json 2> demo-output/diagnostics.json || rc=$?
  printf '=== Step 3: Invalid Syntax Recovery & Line Numbers ===\n'
  "$bin" tokens "$src_invalid" --classroom-table demo-output/invalid-recovery --parser-input demo-output/invalid-recovery/symbol-table.json > demo-output/invalid-recovery/tokens.txt 2> demo-output/invalid-recovery/lex-diagnostics.txt || :
  "$bin" parse-table demo-output/invalid-recovery/symbol-table.json --json > demo-output/invalid-recovery/ast.json 2> demo-output/invalid-recovery/diagnostics.txt || :
  printf 'Done. Outputs written to "%s/demo-output/"\n' "$PWD"
  if [ "$#" -eq 0 ] && [ -t 0 ]; then
    printf 'Press Return to close.\n'
    read -r unused || :
  fi
  exit "$rc"
fi

case "$1" in
  /*) input_file="$1" ;;
  *) input_file="$caller_pwd/$1" ;;
esac
rm -f demo-output/symbol-table.json
printf '=== Step 1: Lexical Analysis (%s) ===\n' "$input_file"
rc_lex=0
"$bin" tokens "$input_file" --classroom-table demo-output --parser-input demo-output/symbol-table.json > demo-output/tokens.txt 2> demo-output/lex-diagnostics.txt || rc_lex=$?
if [ ! -f demo-output/symbol-table.json ]; then
  cat demo-output/lex-diagnostics.txt >&2
  exit "$rc_lex"
fi
printf '=== Step 2: Syntax Analysis (one token at a time from symbol-table.json) ===\n'
rc=0
"$bin" parse-table demo-output/symbol-table.json --trace-tokens > demo-output/ast.txt 2> demo-output/token-stream-trace.txt || rc=$?
"$bin" parse-table demo-output/symbol-table.json --json --diagnostics-json > demo-output/ast.json 2> demo-output/diagnostics.json || rc=$?
printf 'Done. Outputs written to "%s/demo-output/"\n' "$PWD"
exit "$rc"
