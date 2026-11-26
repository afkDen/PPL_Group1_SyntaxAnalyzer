#!/bin/sh
set -eu
test "$#" = 5 || { echo 'Usage: assemble-all-in-one.sh WINDOWS.zip MAC_ARM64.zip MAC_X64.zip OUT_ALL_IN_ONE.zip SOURCE_SHA256' >&2; exit 2; }

win_zip=$1
arm_zip=$2
x64_zip=$3
out_zip_dir=$(cd -- "$(dirname -- "$4")" && pwd)
out_zip="$out_zip_dir/$(basename -- "$4")"
expected_sha=$(printf '%s' "$5" | tr 'A-F' 'a-f')

test ! -e "$out_zip"
test ! -e "$out_zip.sha256"
test ! -e "$out_zip.manifest.json"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/win" "$work/arm" "$work/x64" "$work/stage"
unzip -q "$win_zip" -d "$work/win"
unzip -q "$arm_zip" -d "$work/arm"
unzip -q "$x64_zip" -d "$work/x64"

for d in "$work/win" "$work/arm" "$work/x64"; do
  (cd "$d" && shasum -a 256 -c SHA256SUMS >/dev/null)
  actual=$(shasum -a 256 "$d/source-original.zip" | cut -d ' ' -f 1)
  if [ "$actual" != "$expected_sha" ]; then
    echo "source-original.zip mismatch in $d: $actual != $expected_sha" >&2
    exit 1
  fi
done

diff -r "$work/win/source" "$work/arm/source"
diff -r "$work/win/source" "$work/x64/source"
cmp "$work/arm/Run-Mac.command" "$work/x64/Run-Mac.command"

cp -pR "$work/win/bin" "$work/stage/bin"
cp -pR "$work/arm/bin/macos-arm64" "$work/stage/bin/macos-arm64"
cp -pR "$work/x64/bin/macos-x64" "$work/stage/bin/macos-x64"
cp -pR "$work/win/licenses" "$work/stage/licenses"
cp -pR "$work/win/source" "$work/stage/source"
cp -p "$work/win/source-original.zip" "$work/stage/source-original.zip"
cp -p "$work/arm/Run-Mac.command" "$work/stage/Run-Mac.command"
cp -p "$work/stage/source/scripts/Run-Windows.cmd" "$work/stage/Run-Windows.cmd"
chmod 755 "$work/stage/Run-Mac.command" \
  "$work/stage/bin/macos-arm64/ternfold" \
  "$work/stage/bin/macos-x64/ternfold"

{
  printf '# Ternfold C17 Syntax Analyzer - All-In-One Release (`COSC 303`, `BSCS 3-2`, Group 1)\n\n'
  cat <<'EOF'
This self-contained cross-platform archive packages prebuilt CLI binaries for **Windows x64**, **macOS Apple Silicon (`arm64`)**, and **macOS Intel (`x86_64`)**, the exact verified source snapshot under `source/`, `source-original.zip`, runtime license notices under `licenses/`, launchers, and `SHA256SUMS`.

EOF
  printf 'Embedded source ZIP SHA-256: `%s`\n' "$expected_sha"
  cat <<'EOF'

## Quick run (no compiler required)

### Windows x64
Double-click `Run-Windows.cmd`, or run in PowerShell / Command Prompt:
```bat
Run-Windows.cmd --batch
```

### macOS 15+ (Apple Silicon or Intel)
Double-click `Run-Mac.command`, or run in Terminal:
```sh
./Run-Mac.command --batch
```
Note: macOS executables are unsigned/unnotarized; if Gatekeeper blocks first launch via Finder, run `./Run-Mac.command` in Terminal or inspect the binary architecture/imports in `bin/macos-*/BUILD_INFO.txt`.

### Custom `.tfd` input
Pass any `.tfd` file path to either launcher:
```sh
./Run-Mac.command path/to/program.tfd
```
```bat
Run-Windows.cmd path\to\program.tfd
```

Generated outputs in `demo-output/`:
- `demo-output/Symbol Table.txt`: classroom two-column lexical report
- `demo-output/symbol-table.json`: lossless `ternfold-symbol-table` v1 parsed sequentially one token at a time
- `demo-output/token-stream-trace.txt`: token-by-token read trace (`[symbol-table #N]`)
- `demo-output/ast.txt` and `demo-output/ast.json`: parsed AST (`469` nodes on `examples/syntax/complete.tfd`)
- `demo-output/invalid-recovery/diagnostics.txt`: line-numbered syntax recovery report (`E110`)

## Integrity and platform scope

- Verify every bundled file on macOS/Linux: `shasum -a 256 -c SHA256SUMS`
- Full compiler (`check`, `sanitize`, `run`, `emit-c`, `build`) is verified on Windows x64 (`LLVM-MinGW Clang 23.1.4`).
- Syntax analyzer compilation, frontend/symbol-table/parser/AST/audit tests (`syntax-check`, `syntax-sanitize`), and Mach-O CLI binaries are verified on macOS 15 ARM64 and Intel (deployment target macOS 15.0; links Apple system libraries only), plus source checks on Ubuntu 24.04 x64.
EOF
} > "$work/stage/README.md"

(
  cd "$work/stage"
  for exe in Run-Mac.command bin/macos-arm64/ternfold bin/macos-x64/ternfold; do
    test "$(stat -f %Lp "$exe")" = 755
  done
  test -f bin/windows-x64/ternfold.exe
  find . -type f | LC_ALL=C sort | while IFS= read -r path; do
    digest=$(shasum -a 256 "$path" | cut -d ' ' -f 1)
    printf '%s  %s\n' "$digest" "${path#./}"
  done > "$work/SHA256SUMS.tmp"
  mv "$work/SHA256SUMS.tmp" SHA256SUMS
  zip -q -r "$out_zip" .
)

zip_sha=$(shasum -a 256 "$out_zip" | cut -d ' ' -f 1)
zip_name=$(basename -- "$out_zip")
printf '%s  %s\n' "$zip_sha" "$zip_name" > "$out_zip.sha256"

(
  cd "$work/stage"
  file_count=$(find . -type f | wc -l | tr -d ' ')
  manifest="$out_zip.manifest.json"
  {
    printf '{\n'
    printf '  "format": "ternfold-syntax-archive-manifest",\n'
    printf '  "schema_version": 1,\n'
    printf '  "package": "%s",\n' "$zip_name"
    printf '  "sha256": "%s",\n' "$zip_sha"
    printf '  "source_sha256": "%s",\n' "$expected_sha"
    printf '  "file_count": %s,\n' "$file_count"
    printf '  "files": [\n'
    first=1
    find . -type f | LC_ALL=C sort | while IFS= read -r rel; do
      rel=${rel#./}
      bytes=$(wc -c < "$rel" | tr -d ' ')
      fsha=$(shasum -a 256 "$rel" | cut -d ' ' -f 1)
      if [ "$first" -eq 0 ]; then printf ',\n'; fi
      first=0
      esc=$(printf '%s' "$rel" | sed 's/\\/\\\\/g; s/"/\\"/g')
      printf '    {"path": "%s", "bytes": %s, "sha256": "%s"}' "$esc" "$bytes" "$fsha"
    done
    printf '\n  ]\n}\n'
  } > "$manifest"
)

echo "PASS: assembled $zip_name ($zip_sha)"
