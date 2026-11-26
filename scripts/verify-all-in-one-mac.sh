#!/bin/sh
set -eu
test "$#" = 3 || { echo 'Usage: verify-all-in-one-mac.sh ALL_IN_ONE.zip NEW_EXTRACT_DIR SOURCE_SHA256' >&2; exit 2; }
zip_path=$1
extract_dir=$2
source_sha=$(printf '%s' "$3" | tr 'A-F' 'a-f')

test ! -e "$extract_dir"
mkdir -p "$extract_dir"
unzip -q "$zip_path" -d "$extract_dir"
cd "$extract_dir"
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

test -z "$(find . -type l -print)"
file_count=$(find . -type f | wc -l | tr -d ' ')
sum_count=$(wc -l < SHA256SUMS | tr -d ' ')
test "$file_count" -eq "$((sum_count + 1))"
! grep -Eq '  (SHA256SUMS|/|.*\.\./)' SHA256SUMS
shasum -a 256 -c SHA256SUMS

actual_src_sha=$(shasum -a 256 source-original.zip | cut -d ' ' -f 1)
test "$actual_src_sha" = "$source_sha"
mkdir -p /tmp/tf-src-cmp-$$
trap 'rm -rf /tmp/tf-src-cmp-$$' EXIT HUP INT TERM
unzip -q source-original.zip -d "/tmp/tf-src-cmp-$$"
diff -r source "/tmp/tf-src-cmp-$$"

for bin_exe in Run-Mac.command bin/macos-arm64/ternfold bin/macos-x64/ternfold; do
  test "$(stat -f %Lp "$bin_exe")" = 755
done
test -f bin/windows-x64/ternfold.exe

case "$(uname -m)" in
  arm64) target=macos-arm64; arch=arm64 ;;
  x86_64) target=macos-x64; arch=x86_64 ;;
  *) echo "Unsupported Mac architecture" >&2; exit 2 ;;
esac
file "bin/$target/ternfold" | grep -q "Mach-O 64-bit executable $arch"
otool -L "bin/$target/ternfold" | awk 'NR > 1 { if ($1 !~ /^\/usr\/lib\// && $1 !~ /^\/System\/Library\//) exit 1; n++ } END { if (!n) exit 1 }'
otool -l "bin/$target/ternfold" | awk '$1 == "minos" { if ($2 !~ /^15\.0(\.0)?$/) exit 1; seen=1 } END { if (!seen) exit 1 }'

./Run-Mac.command --batch
grep -q 'AST root=1 has_errors=false nodes=469' demo-output/ast.txt
grep -q '\[symbol-table #0\]' demo-output/token-stream-trace.txt
grep -q 'error\[E110\]' demo-output/invalid-recovery/diagnostics.txt

# Test path with spaces
cp source/examples/syntax/complete.tfd 'path with spaces.tfd'
./Run-Mac.command "$PWD/path with spaces.tfd"
grep -q 'AST root=1 has_errors=false nodes=469' demo-output/ast.txt

# Non-bypass proof: delete .tfd before parse-table
cp source/examples/syntax/complete.tfd temp_live.tfd
"./bin/$target/ternfold" tokens temp_live.tfd --parser-input temp_live.st.json
rm -f temp_live.tfd
"./bin/$target/ternfold" parse-table temp_live.st.json > temp_live.ast.txt
grep -q 'AST root=1 has_errors=false nodes=469' temp_live.ast.txt

echo "PASS: verified $target All-In-One archive cleanly under $PATH"
