# Recursive descent architecture (`src/parser.c`)

The Ternfold syntax analyzer uses handwritten ISO C17 recursive descent in `src/parser.c`, combined with a Pratt precedence loop for unary, binary, call, and projection expressions. It consumes an immutable sequence of `TfToken` records (supplied either by `tf_lex()` or reconstructed one token at a time from disk by `tf_symbol_table_load()`), validates input invariants in `input_valid()`, and constructs an arena-allocated `TfAst`.

## Sequential cursor and trivia filtering

The parser tracks its position through the token array with `Parser` (`src/parser.c:13-20`):

- `peek(p)` (`src/parser.c:23-26`) skips `TF_COMMENT_LINE` and `TF_COMMENT_BLOCK` trivia tokens before returning `&p->lex->tokens[p->at]`. Comment tokens stay in the Symbol Table for inspection while grammar routines read only non-trivia tokens.
- `kind(p)` (`src/parser.c:27`) returns `TF_EOF` if host allocation or nesting limits caused failure (`p->failed`), preventing runaway recursion.
- `take(p)` (`src/parser.c:28-32`) advances `p->at` and updates `p->end` so every AST node records its tight `[start_byte, end_byte)` span through `finish(p, n)`.
- `eat(p, expected)` (`src/parser.c:33`) consumes one matching token.
- `expect(p, expected)` (`src/parser.c:58-65`) consumes a matching token or synthesizes a zero-width `TF_AST_MISSING` node and logs `E110`.

## Top-down parser routines

Every non-terminal maps to a dedicated C routine in `src/parser.c`:

| Routine | Lines | Grammar non-terminals handled |
|---|---|---|
| `parse_module()` | `493-507` | `program`, enforces imports before other declarations (`E110`). |
| `declaration()` | `396-469` | Optional `pub`, `import-decl`, `function`, `record`, `enum`, `machine`, or top-level error recovery. |
| `machine_section()` | `382-395` | `state`, `next` writer, `guard ... else ... ;`, and `emit ... ;`. Order `state -> next -> guard -> emit` checked in `declaration()`. |
| `parameters()` | `133-145` | Parameter lists `( name: type, ... )` with optional trailing comma. |
| `type()` | `146-167` | Path types with optional `[arg, ...]` and function types `fn(T...) -> R`. |
| `block()` | `350-381` | Zero or more `statement()` nodes followed by a mandatory final expression before `}`. Enforces no semicolon on the final expression. |
| `statement()` | `320-349` | `let [mutability disabled] binder [: type] = expr;`, `input id: type [from stdin];`, `output expr [to stdout];`. Checks main-only I/O scope (`E208`). |
| `pattern()` | `175-204` | `_`, identifiers (binders), literals (including signed numeric pattern `- INT`/`- FLOAT` keeping `minus` span), `()`, and `path::Variant(...)`. |
| `primary()` | `205-260` | Literals, paths, `next.cell`, `()` unit, `(expr)` group, `[...]` lists, `{...}` blocks, `if/else`, `for`, `fn` closure, `match` arms. |
| `expression()` | `278-319` | Unary `-`/`!`, postfix call `(...)` (with `named_argument()` 2-token lookahead), postfix `.field`, and infix binary operators driven by `binding_power()`. |

## AST arena memory and immutability

- All nodes live in `TfAst` (`src/ast.c:8-15`), allocated in aligned blocks chained inside the arena.
- Decoded `Text`/`Char` byte strings and identifier names are copied into the AST arena via `tf_ast_copy_text()` (`src/ast.c:62-68`), so the caller can call `tf_lex_destroy()` immediately after `tf_parse()`.
- Each node receives an owner-scoped `TfNodeId { const TfAst *owner; size_t index; }` (`0` means absent). `tf_ast_get()` (`src/ast.c:69-71`) returns `NULL` for out-of-range or foreign IDs.
- Serialization in `src/ast_dump.c` outputs flat deterministic JSON (`schema_version: 1`) or line-per-node text sorted by stable node ID.
