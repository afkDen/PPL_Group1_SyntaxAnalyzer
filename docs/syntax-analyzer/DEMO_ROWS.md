# Demonstration row and AST mapping (`examples/syntax/complete.tfd`)

Running `./scripts/run-syntax-demo.ps1 -IncludeInvalid -TraceTokens` processes `examples/syntax/complete.tfd` (4,110 bytes, SHA-256 `aa81dafdf6384d4af033c33e8dcb67e1df179c5e4dded1b3f10243dcea564e86`), writes `demo-output/Symbol Table.txt` (612 lines including header, `EOF` omitted) and `demo-output/symbol-table.json` (`612` token objects including `EOF`), and parses the token stream sequentially into `469` AST nodes (`root=1`, `has_errors=false`).

## Checklist evidence index

| Rubric item | Line(s) in `complete.tfd` | First Symbol Table index | AST node kind(s) and count |
|---|---|---|---|
| 1. Token-by-token input | All (`1..124`) | `#0`..`#611` (`EOF`) | `PROGRAM` (`id=1`), consumed via `tf_symbol_table_next()` |
| 2. Recursive descent + Pratt parser | `10..123` | `#2`..`#610` | `FUNCTION` (5), `RECORD` (1), `ENUM` (1), `MACHINE` (1), `BLOCK` (17), `BINARY` (39) |
| 5. Input statement (`/1`) | `62` (`input visitor: Text from stdin;`) | `#247` (`INPUT`) | `INPUT` (`count=1`, `first_id=184`, `from_stdin=true`) |
| 6a. Output string (`/3`) | `111` (`output "=== Ternfold Syntax Analyzer ===" to stdout;`) | `#550` (`OUTPUT`) | `OUTPUT` (`count=3`, `first_id=410`, `to_stdout=true`) |
| 6b. Output variable (`/3`) | `112` (`output visitor to stdout;`) | `#555` (`OUTPUT`) | `OUTPUT` (`PATH` child) |
| 6c. Output concatenation (`/3`) | `113` (`output "Hello, " ++ visitor ++ ... to stdout;`) | `#560` (`OUTPUT`) | `OUTPUT` (`BINARY` `PLUS_PLUS` tree) |
| 7a. Constant assigned to variable (`/3`) | `65` (`let constant_seed: Int = 10;`) | `#255` (`LET`) | `LET` (`count=16`, `first_id=187`) |
| 7b. Left value assigned to variable (`/3`) | `66` (`let lvalue_copy = constant_seed;`), `36` (`next doubled = ...`) | `#262` (`LET`), `#106` (`NEXT`) | `LET` (`PATH` value), `WRITER` (`count=2`, `first_id=65`) |
| 7c. Expression assigned to variable (`/3`) | `67..72` (`computed_value`, `13 / 5`, `-7 div 3`, `20 % 3`, `2 ^ 3 ^ 2`) | `#267`, `#282`, `#291`, `#301`, `#310` | `LET`, `BINARY` (`STAR`, `PLUS`, `MINUS`, `SLASH`, `DIV`, `PERCENT`, `CARET`), `UNARY` (`id=216`) |
| 8a. If / If-else statement (`/3`) | `75..79` (`if computed_value % 2 == 0 { "even" } else { "odd" }`) | `#334` (`IF`), `#343` (`ELSE`) | `IF` (`count=3`, `first_id=241`) |
| 8b. Nested if-else statement (`/3`) | `80..88` (`if computed_value < 0 { ... } else { if ... else ... }`) | `#353` (`IF`), `#362` (`IF`), `#369` (`ELSE`) | Nested `IF` inside `else` `BLOCK` |
| 8c. Case statement (`/3`) | `97..108` (`match step(meter0, 5) { ... }`) | `#451` (`MATCH`) | `MATCH` (`id=325`), `ARM` (`count=3`, `first_id=334`), `PATTERN_VARIANT` (5), `PATTERN_WILDCARD` (2) |
| 9. Iterative for and nested for (`/2`) | `44..48` (`first_squares`), `51..57` (`multiplication_grid`) | `#169` (`FOR`), `#206` (`FOR`), `#216` (`FOR`) | `FOR` (`count=3`, `first_id=121`) |
| 10. Declaration statements | `10` (`fn`), `20` (`record`), `26` (`enum`), `32` (`machine`), `60` (`pub fn main`) | `#2`, `#43`, `#60`, `#73`, `#236` | `FUNCTION`, `RECORD`, `ENUM`, `MACHINE`, `STATE` (2), `WRITER` (2), `GUARD` (2), `EMIT` (2), `PARAMETER` (9), `VARIANT` (2) |

## Reproducing these counts

```powershell
./scripts/run-syntax-demo.ps1 -TraceTokens
Get-Content demo-output/ast.txt -TotalCount 6
Get-Content demo-output/token-stream-trace.txt -TotalCount 20
```
