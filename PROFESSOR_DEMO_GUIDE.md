# Professor demonstration guide

Use this guide during live evaluation (`COSC 303: Programming Language Design`, `BSCS 3-2`, Group 1) to walk through the one-command pipeline, sequential Symbol Table consumption, recursive-descent implementation, and invalid-input recovery.

## 1. Run the one-command pipeline

From the extracted source root (after building with `./scripts/build.ps1 build`), or by running `Run-Windows.cmd` / `Run-Demo.cmd` on Windows or `./Run-Mac.command` on macOS from the All-In-One bundle:

```powershell
./scripts/run-syntax-demo.ps1 -IncludeInvalid -TraceTokens
```

On POSIX (`macOS` / `Linux` after `sh scripts/build.sh build`):

```sh
sh scripts/run-syntax-demo.sh examples/syntax/complete.tfd demo-output
```

Show the generated artifacts in `demo-output/`:
- `demo-output/Symbol Table.txt`: two-column classroom lexical report (`TOKEN TYPE` and `LEXEME`).
- `demo-output/symbol-table.json`: lossless `ternfold-symbol-table` schema v1 consumed by the parser.
- `demo-output/token-stream-trace.txt`: sequential `[symbol-table #N]` lines confirming tokens are read one at a time.
- `demo-output/ast.txt` and `demo-output/ast.json`: 469 AST nodes for `examples/syntax/complete.tfd` (`exit 0`).
- `demo-output/invalid-recovery/diagnostics.txt`: line-numbered error messages from `invalid-recovery.tfd` (`exit 1`).

## 2. Prove non-bypass Symbol Table input live

Demonstrate that `ternfold parse-table` never reads `.tfd` source bytes and never calls `tf_lex()`:

```powershell
Copy-Item examples/syntax/complete.tfd build/live_copy.tfd
./build/ternfold.exe tokens build/live_copy.tfd --parser-input build/live.symbol-table.json
Remove-Item build/live_copy.tfd
./build/ternfold.exe parse-table build/live.symbol-table.json --trace-tokens --json > build/live.ast.json
```

Even with `build/live_copy.tfd` deleted, `ternfold parse-table` parses all 612 tokens sequentially from `build/live.symbol-table.json` and writes the exact 469-node AST.

Next, corrupt `"source_sha256"` or alter `"kind":"FN"` to `"kind":"LET"` in a copy of `build/live.symbol-table.json` and run `ternfold parse-table` on it: the loader exits `2` with an explicit integrity rejection.

## 3. Walk through recursive descent and the diagrams

- Open `src/parser.c`: show `parse_module()` (line 493), `declaration()` (line 396), `statement()` (line 320), `block()` (line 350), `primary()` (line 205), `expression()` Pratt loop (line 278), and `recover()` panic recovery (line 95).
- Cross-reference [RECURSIVE_DESCENT.md](docs/syntax-analyzer/RECURSIVE_DESCENT.md), [GRAMMAR_AND_PARSING_TABLE.md](docs/syntax-analyzer/GRAMMAR_AND_PARSING_TABLE.md), and [TRANSITION_DIAGRAM.md](docs/syntax-analyzer/TRANSITION_DIAGRAM.md).

## 4. Demonstrate error recovery and line-numbered diagnostics

Run the three invalid fixtures through the two-step modular pipeline:

```powershell
./build/ternfold.exe tokens examples/syntax/invalid-recovery.tfd --parser-input demo-output/inv-rec.json
./build/ternfold.exe parse-table demo-output/inv-rec.json --json > demo-output/inv-rec.ast.json
```

Point out:
- Line numbers (`4`, `9`, `10`, `14`, `16`) and clear descriptions (`expected missing syntax token`, `expected an expression`, `final block expression must not have a semicolon`).
- `fn surviving_helper()` at the end of `invalid-recovery.tfd` appears cleanly in `declarations` inside `demo-output/inv-rec.ast.json` because `recover()` stops at `top_boundary()`.
- Repeat with `invalid-expressions.tfd` (`E111` comparison chaining, `E112` mixed named/positional arguments) and `invalid-declarations.tfd` (`E208` I/O outside `main`, machine section order `E110`).

## 5. Anticipated professor Q&A

| Question | Direct technical answer |
|---|---|
| Does the parser re-read the `.tfd` file? | No. `parse_symbol_table_mode()` calls `tf_symbol_table_load()`, which reads only the Symbol Table JSON file via `tf_symbol_table_next()` one token record at a time and calls `tf_parse()` directly on the loaded tokens. |
| Why are `while` and `do-while` absent? | Ternfold 0.2 has pure `for ... in ... { ... }` comprehensions and recursive functions instead of imperative `while`/`do-while` loops. `RUBRIC.md` discloses `1/2` iterative support honestly rather than faking syntax. |
| Why does `if cond { 1 }` without `else` fail? | Conditionals in Ternfold return values (`if cond { a } else { b }`), so `src/parser.c` (`primary()`, lines 229-233) requires an `else` block. |
| Does `let` allow reassigning a variable like `x = x + 1;`? | No. `let` introduces an immutable binding; state changes belong inside `machine` blocks via `next cell = expr;`. `examples/syntax/complete.tfd` shows constant, left-value copy, and compound expression initializers. |
| Which error recovery method is used? | Non-interactive panic-mode synchronization (`recover()`), not user prompting (`Prompts a user` is intentionally omitted so CLI runs never block on `stdin`). |
