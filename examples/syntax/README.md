# Syntax Analyzer examples

Programs in this folder demonstrate each syntax rule tested by the course rubric. Valid examples exit `0` from `ternfold parse-table`; invalid examples exit `1` while emitting recovered AST nodes and line-numbered diagnostics.

| File | Purpose | Expected exit |
|---|---|---:|
| `complete.tfd` | Full demonstration covering input, output (string, variable, concatenation), immutable/candidate bindings, conditionals, `match`, single and nested `for` loops, declarations, comments, and operator precedence. | 0 |
| `input-output.tfd` | Focused Criterion 5 (`input ... from stdin`) and Criterion 6 (`output` string, variable, concatenation). | 0 |
| `declarations.tfd` | Focused Criterion 10 declarations (`import`, `record`, `enum`, `machine`, `fn`, `let`). | 0 |
| `expressions.tfd` | Criterion 7 bindings (constant, variable copy, compound expression) plus arithmetic, relational, and logical operator precedence. | 0 |
| `conditions.tfd` | Criterion 8 (`if`/`else`, nested `if`/`else`, and `match` case expressions). | 0 |
| `loops.tfd` | Criterion 9 single `for` and nested `for` comprehensions (notes absence of `while`/`do-while`). | 0 |
| `invalid-recovery.tfd` | Multi-error panic-mode recovery inserting missing tokens (`TF_AST_MISSING`) and synchronizing to `fn surviving_helper()`. | 1 |
| `invalid-expressions.tfd` | Non-associative comparison chaining (`E111`), mixed named/positional arguments (`E112`), and missing operand (`E110`). | 1 |
| `invalid-declarations.tfd` | Invalid `pub import`, empty `enum`, malformed parameter list, out-of-order `machine` section, empty block (`E110`), and non-main I/O (`E208`). | 1 |
