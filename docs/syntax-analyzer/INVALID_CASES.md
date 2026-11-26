# Invalid syntax fixtures, recovery, and negative Symbol Table cases

All syntax-error fixtures return exit code `1` from `ternfold parse-table`, write a recovered AST (preserving nodes parsed around skipped spans), and print 1-based line numbers with error descriptions on `stderr`. Malformed Symbol Table files return exit code `2` before reaching `tf_parse()`.

## 1. Panic-mode recovery (`examples/syntax/invalid-recovery.tfd`)

Command:

```powershell
./build/ternfold.exe tokens examples/syntax/invalid-recovery.tfd --parser-input build/inv-rec.symbol-table.json
./build/ternfold.exe parse-table build/inv-rec.symbol-table.json --json
```

Observed result: `exit=1`, `AST root=1 has_errors=true nodes=52`, top-level `declarations=[2, 14, 33, 48]`. Function `surviving_helper` (`id=48`) parses completely after three broken functions.

Observed diagnostics (line number and description):
- Line `4`: `error[E110]: expected missing syntax token` (`fn missing_rparen(a: Int -> Int {`, missing `)`)
- Line `9`: `error[E110]: expected an expression` (`let first = ;`)
- Line `10`: `error[E110]: expected missing syntax token` and `final block expression must not have a semicolon` (`let second = if true { 1 };` missing mandatory `else` block)
- Line `14` and `16`: missing closing `)` and block final expression recovered cleanly before `surviving_helper` at declaration index `3`

## 2. Expression diagnostics (`examples/syntax/invalid-expressions.tfd`)

Observed result: `exit=1`, `AST root=1 has_errors=true nodes=38`:
- Line `8`: `error[E111]: comparison/equality operators do not chain` (`1 < 2 < 3`)
- Line `9`: `error[E111]: comparison/equality operators do not chain` (`1 == 2 != 3`)
- Line `10`: `error[E112]: mixed named and positional arguments` (`Pair(x: 1, 2)`)
- Line `11`: `error[E110]: expected an expression` (`2 ^ ;`)

## 3. Declaration and scoping diagnostics (`examples/syntax/invalid-declarations.tfd`)

Observed result: `exit=1`, `AST root=1 has_errors=true nodes=44`, top-level `declarations=[2, 3, 4, 16, 33, 40]`:
- Line `2`: `error[E110]: imports cannot be public` (`pub import "stdlib/text.tfd" as txt;`)
- Line `4`: `error[E110]: enum requires a variant` (`enum EmptyEnum {}`)
- Line `6`: `error[E110]` on trailing comma in single-parameter `machine BadTrailingComma(delta: Int,)`
- Line `11` and `13`: `error[E110]: machine requires state and next sections` and `machine sections must be state, next, guard, emit in order` (`next` written before `state`)
- Line `17`: `error[E208]: output is restricted to root public zero-argument main outer body`
- Line `21`: `error[E110]: block requires a final expression` (`fn empty_body() -> Int {}`)

## 4. Malformed Symbol Table rejections (exit code `2`)

Tested by `tests/test_symbol_table.c` and `test_parse_table_cli()` in `tests/test_tokens_cli.c`:
- Empty input, truncated JSON, trailing non-whitespace bytes after root `}`
- Wrong `"format"`, wrong `"schema_version"`, missing or unknown header keys
- Modified `"source_sha256"` (caught on `tf_symbol_table_finish()`)
- Spoofed token `"index"`, `"kind"` vs `"lexeme"` mismatch (for example, `"PLUS"` paired with `"-"`, or `"LET"` paired with identifier), or flipped `"reserved"` flag on keywords/identifiers
- Non-whitespace byte in `"gap"`, lone `\r` in `"gap"`, mismatched `"line"`/`"column"`/`"display_column"`, or `"payload"` mismatch
- Missing `TF_EOF` or extra token after `TF_EOF`

## 5. Unsupported constructs and non-applicable cases

- `while` and `do-while` loops are outside the Ternfold 0.2 grammar; using `while` parses as an ordinary identifier instead of a loop header, producing `E110` around the following block/condition tokens.
- Bare `if cond { ... }` without `else` always triggers `E110` because `if` is a total expression.
- Mutable variable reassignment (`x = 5;` without `let` or `next` inside a machine) fails with `E110` inside a block.
