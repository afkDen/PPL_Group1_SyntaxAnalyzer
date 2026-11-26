# Error recovery and diagnostics (`src/parser.c`)

## Error recovery method: panic mode (`recover()`)

The parser uses non-interactive panic-mode error recovery. It does **not** prompt the user during syntax analysis; automated builds, tests, and batch grading runs collect up to 100 diagnostics deterministically without blocking on `stdin`. (`ternfold repl` provides an interactive loop separately, outside the file syntax analyzer.)

Recovery operates through two cooperating mechanisms in `src/parser.c`:

1. **Missing-token synthesis (`expect()` at `src/parser.c:58-65`)**: When a required delimiter or keyword (`;`, `)`, `]`, `}`, `->`, `else`, `in`, `=`) is missing, the parser logs `E110` at the insertion point, inserts a zero-width `TF_AST_MISSING` AST node recording `expected`, and returns control to the grammar rule without consuming the unexpected following token.
2. **Panic-mode synchronization (`recover()` at `src/parser.c:95-119`)**: When an unexpected token breaks a block, machine section, or declaration, `recover(p, top)` records a `TF_AST_ERROR` span over the skipped tokens until it reaches a synchronizing point:
   - `top_boundary(p)` (`pub`, `record`, `enum`, `machine`, `import`, or `fn` followed by `IDENT`) at any bracket depth. Even if malformed tokens left delimiters open, `recover()` stops before stealing the next declaration.
   - At local bracket depth `0`: `}`, `;` (consumed to clear a broken statement), or statement starters (`let`, `input`, `output`) when recovering inside a block (`!top`).
   - Open delimiters `(`, `[`, `{` inside the skipped slice push their matching closer onto a bounded stack (up to `options.nesting_limit`). On exit, unresolved closers emit zero-width `TF_AST_MISSING` nodes in LIFO order.
   - Every recovery loop checks progress (`p->at == before`) so parsing always consumes at least one token or halts on `EOF`.

## Resource and diagnostic bounds (`TfParseOptions`)

| Parameter | Default | Allowed range | Overflow behavior |
|---|---:|---:|---|
| `is_root` | `true` | `bool` | Controls whether outer `main()` permits `input`/`output` clauses (`E208`). |
| `nesting_limit` | `64` | `1..128` | Excess nesting aborts with `TF_HOST_ERROR` (`result = NULL`, clean cleanup, CLI exit `2`). |
| `diagnostic_cap` | `100` | `0..100` | Lexer + parser errors above cap increment `suppressed` without halting recovery. |

## Syntax and shared diagnostic codes

| Code | Phase | Meaning | Trigger example |
|---|---|---|---|
| `E110` | Parser | Syntax / unexpected or missing token | `let a = ;`, `if true { 1 };`, `pub import ...`, `machine` out-of-order section, final expression `;` |
| `E111` | Parser | Non-associative comparison/equality chain | `1 < 2 < 3`, `a == b != c` |
| `E112` | Parser | Mixed named and positional call arguments | `Pair(x: 1, 2)`, `Pair(1, y: 2)` |
| `E208` | Parser (scoping) | Entry-only `input`/`output` outside root zero-arg `pub fn main`, or invalid `input` binder | `fn helper() -> Int { output "x"; 0 }`, `pub fn main() -> List[Text] { input _: Text; [] }` |
| `E101`-`E105` | Lexer (retained) | Invalid char, malformed number, bad Text/Char literal, unclosed `/*` | Retained from Symbol Table without adding a redundant `E110`. |
| `E001`-`E003` | Source validation | Invalid UTF-8, UTF-8 BOM, lone CR (`\r`) | Produces raw byte diagnostic and empty AST (`root: null`). |

Every rendered diagnostic includes:
- Source file path (`"..."`), **1-based line number** and column (`line:col..line:col`), display column, and raw byte span `[start..end)`.
- Severity (`error`), code (`E###`), human-readable description, and offending source line with `^` underline.
- Structured machine-readable output via `--diagnostics-json` (schema v2).
