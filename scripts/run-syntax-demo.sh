#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

INPUT_FILE="${1:-examples/syntax/complete.tfd}"
OUT_DIR="${2:-demo-output}"

if [ ! -x ./build/ternfold ]; then
    sh scripts/build.sh build
fi

mkdir -p "$OUT_DIR"

./build/ternfold tokens "$INPUT_FILE" \
    --classroom-table "$OUT_DIR" \
    --parser-input "$OUT_DIR/symbol-table.json" \
    > "$OUT_DIR/tokens.txt" 2> "$OUT_DIR/lex-diagnostics.txt" || {
        rc=$?
        [ "$rc" -le 1 ] || exit "$rc"
    }

./build/ternfold parse-table "$OUT_DIR/symbol-table.json" \
    > "$OUT_DIR/ast.txt" 2> "$OUT_DIR/diagnostics.txt" || true

./build/ternfold parse-table "$OUT_DIR/symbol-table.json" --json --diagnostics-json \
    > "$OUT_DIR/ast.json" 2> "$OUT_DIR/diagnostics.json"
rc=$?

printf 'Symbol Table.txt : %s/Symbol Table.txt\n' "$OUT_DIR"
printf 'Lossless Table   : %s/symbol-table.json\n' "$OUT_DIR"
printf 'AST Output       : %s/ast.json (exit %d)\n' "$OUT_DIR" "$rc"
exit "$rc"
