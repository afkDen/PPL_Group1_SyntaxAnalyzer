#!/bin/sh
set -eu
test "$#" = 3 || { echo 'Usage: package-mac-payload.sh SOURCE.zip OUT_DIR SOURCE_SHA256' >&2; exit 2; }
scripts_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_zip=$(cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
out_dir=$2
expected_sha=$(printf '%s' "$3" | tr 'A-F' 'a-f')

case "${#expected_sha}" in
  64) ;;
  *) echo 'Expected 64-hex source SHA-256.' >&2; exit 2 ;;
esac
case "$expected_sha" in
  *[!0-9a-f]*) echo 'Expected lowercase/hex source SHA-256.' >&2; exit 2 ;;
esac

actual_sha=$(shasum -a 256 "$source_zip" | cut -d ' ' -f 1)
if [ "$actual_sha" != "$expected_sha" ]; then
  echo "Source ZIP SHA-256 mismatch: $actual_sha != $expected_sha" >&2
  exit 1
fi

test ! -e "$out_dir"
case "$(uname -sm)" in
  'Darwin arm64') target=macos-arm64; arch=arm64 ;;
  'Darwin x86_64') target=macos-x64; arch=x86_64 ;;
  *) echo 'package-mac-payload.sh must run on Apple Silicon or Intel macOS.' >&2; exit 2 ;;
esac

if [ -n "$(zipinfo -l "$source_zip" | awk '$1 ~ /^l/ {print}')" ]; then
  echo 'Source ZIP must not contain symbolic links.' >&2
  exit 1
fi

mkdir -p "$out_dir/source" "$out_dir/bin/$target"
out=$(CDPATH= cd -- "$out_dir" && pwd)
cp "$source_zip" "$out/source-original.zip"
unzip -q "$source_zip" -d "$out/source"

work_tmp=$(mktemp -d)
trap 'rm -rf "$work_tmp"' EXIT HUP INT TERM
cp -R "$out/source/." "$work_tmp/"

(
  cd "$work_tmp"
  export MACOSX_DEPLOYMENT_TARGET=15.0
  export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
  export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
  sh scripts/build.sh environment
  sh scripts/build.sh syntax-check
  sh scripts/build.sh syntax-sanitize
  sh scripts/run-syntax-demo.sh examples/syntax/complete.tfd demo-check
)

binary="$out/bin/$target/ternfold"
(
  cd "$out/source"
  export MACOSX_DEPLOYMENT_TARGET=15.0
  cc -std=c17 -O2 -Wall -Wextra -Werror -pedantic -Wconversion -Wshadow \
    -fno-fast-math -ffp-contract=off -mmacosx-version-min=15.0 -arch "$arch" -Iinclude \
    src/source.c src/lexer.c src/report.c src/source_manager.c \
    src/diagnostics.c src/token_dump.c src/symbol_table.c \
    src/ast.c src/parser.c src/ast_dump.c src/resolve.c \
    src/types.c src/type_operators.c src/typed_signatures.c \
    src/literal_value.c src/numeric.c src/checked_numeric.c \
    src/pattern_coverage.c src/typed_patterns.c src/check_constants.c \
    src/check_builtins.c src/check.c src/check_pipeline.c \
    src/machine_plan.c src/plan_dump.c \
    src/core/nodes.c src/core/primitive_rules.c src/core/lower_types.c \
    src/core/lower_constants.c src/core/lower_match.c src/core/lower.c \
    src/core/verify.c src/core/optimize.c src/core/dump.c \
    src/execution/values.c src/execution/scalars.c src/execution/operations.c \
    src/execution/builtins.c src/execution/repl_value.c src/execution/trace.c \
    src/execution/trace_value.c src/execution/trace_io.c src/execution/trace_artifact.c \
    src/execution/interpreter.c src/execution/host_io.c src/execution/driver_run.c \
    src/backend/c_nodes.c src/backend/c_metadata.c src/backend/lifetimes.c \
    src/backend/c_emit.c src/backend/native_build.c src/backend/driver_native.c \
    src/repl_input.c src/repl.c src/formatting/format.c \
    src/editor/snapshot.c src/editor/protocol.c src/compiler_main.c \
    -lm -o "$binary"
)

cp "$out/source/scripts/Run-Mac.command" "$out/Run-Mac.command"
cp "$out/source/scripts/Run-Windows.cmd" "$out/Run-Windows.cmd"
chmod 755 "$binary" "$out/Run-Mac.command"

file "$binary" | grep -q "Mach-O 64-bit executable $arch"
otool -L "$binary" > "$out/bin/$target/imports.txt"
awk 'NR > 1 { if ($1 !~ /^\/usr\/lib\// && $1 !~ /^\/System\/Library\//) exit 1; n++ } END { if (!n) exit 1 }' "$out/bin/$target/imports.txt"
{
  uname -a
  cc --version
  printf 'Deployment target: macOS 15.0 (%s). Linked libraries checked against /usr/lib and /System/Library only.\n' "$arch"
  printf 'Syntax tests verified via sh scripts/build.sh syntax-check and syntax-sanitize.\n'
  file "$binary"
  otool -L "$binary"
  codesign -dvv "$binary" 2>&1 || :
} > "$out/bin/$target/BUILD_INFO.txt"

cat > "$out/README.md" <<EOF
# Ternfold C17 Syntax Analyzer - Cross-Platform Bundle ($target staging)

Embedded source ZIP SHA-256: \`$expected_sha\`
EOF

(
  cd "$out"
  manifest_tmp=$(mktemp)
  trap 'rm -f "$manifest_tmp"' EXIT HUP INT TERM
  find . -type f | LC_ALL=C sort | while IFS= read -r path; do
    digest=$(shasum -a 256 "$path" | cut -d ' ' -f 1)
    printf '%s  %s\n' "$digest" "${path#./}"
  done > "$manifest_tmp"
  mv "$manifest_tmp" SHA256SUMS
)
echo "PASS: prepared $target payload in $out"
