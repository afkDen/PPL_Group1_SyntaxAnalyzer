# Grammar and parsing table

This reference documents the Ternfold 0.2 concrete grammar implemented by `src/parser.c`, the LL(1)/LL(2) predictive parsing table for recursive-descent routines, and the Pratt operator-precedence table used inside `expression()`.

## Concrete EBNF grammar

```ebnf
program        = { import-decl }, { declaration }, EOF ;
import-decl    = "import", STRING, "as", IDENT, ";" ;
declaration    = [ "pub" ], ( function | record | enum | machine ) ;
function       = "fn", IDENT, parameters, "->", type, ( main-block | block ) ;
parameters     = "(", [ parameter, { ",", parameter }, [ "," ] ], ")" ;
parameter      = IDENT, ":", type ;
record         = "record", IDENT, "{", [ parameter, { ",", parameter }, [ "," ] ], "}" ;
enum           = "enum", IDENT, "{", variant, { ",", variant }, [ "," ], "}" ;
variant        = IDENT, [ "(", type, { ",", type }, [ "," ], ")" ] ;
machine        = "machine", IDENT, "(", parameter, ")", "yields", type, "rejects", type,
                 "{", state, { state }, writer, { writer }, { guard }, { emission }, "}" ;
state          = "state", IDENT, ":", type, "=", expression, ";" ;
writer         = "next", IDENT, "=", expression, ";" ;
guard          = "guard", expression, "else", expression, ";" ;
emission       = "emit", expression, ";" ;
type           = path, [ "[", type, { ",", type }, "]" ]
               | "fn", "(", [ type, { ",", type } ], ")", "->", type ;
path           = IDENT, { "::", IDENT } ;
block          = "{", { binding }, expression, "}" ;
main-block     = "{", { binding | input-clause | output-clause }, expression, "}" ;
input-clause   = "input", IDENT, ":", type, [ "from", "stdin" ], ";" ;
output-clause  = "output", expression, [ "to", "stdout" ], ";" ;
binding        = "let", ( IDENT | "_" ), [ ":", type ], "=", expression, ";" ;
expression     = Pratt-expression ;
primary        = INT | FLOAT | STRING | CHAR | "true" | "false" | path
               | "next", ".", IDENT
               | "(", [ expression ], ")"
               | "[", [ expression, { ",", expression }, [ "," ] ], "]"
               | block | conditional | matching | closure | for-expression ;
for-expression = "for", IDENT, "in", expression, block ;
conditional    = "if", expression, block, "else", block ;
matching       = "match", expression, "{", arm, { ",", arm }, [ "," ], "}" ;
arm            = pattern, "=>", block ;
pattern        = "_" | IDENT | "true" | "false" | [ "-" ], ( INT | FLOAT )
               | STRING | CHAR | "(", ")"
               | variant-path, [ "(", pattern, { ",", pattern }, [ "," ], ")" ] ;
closure        = "fn", parameters, "->", type, block ;
```

## Recursive-descent predictive parsing table

The table below gives the decision map `(Non-terminal, Lookahead)` used by `src/parser.c`.

| Non-terminal | Lookahead token(s) | Production selected | C handler |
|---|---|---|---|
| `program` | `import`, `pub`, `fn`, `record`, `enum`, `machine` | `declaration()` repeated until `EOF` | `parse_module` |
| `program` | `EOF` | `ε` | `parse_module` |
| `declaration` | `pub` (followed by decl keyword) | `[ "pub" ]` prefix + inner decl | `declaration` |
| `declaration` | `import` | `"import" TEXT "as" IDENT ";"` (`E110` if `pub`) | `declaration` |
| `declaration` | `fn` | `"fn" IDENT parameters "->" type block` | `declaration` |
| `declaration` | `record` | `"record" IDENT "{" fields "}"` | `declaration` |
| `declaration` | `enum` | `"enum" IDENT "{" variants "}"` | `declaration` |
| `declaration` | `machine` | `"machine" IDENT "(" parameter ")" "yields" type "rejects" type "{" sections "}"` | `declaration` |
| `declaration` | any other non-`EOF` | Error + `recover(p, true)` | `declaration` |
| `machine-section` | `state` | `"state" IDENT ":" type "=" expression ";"` | `machine_section` |
| `machine-section` | `next` | `"next" IDENT "=" expression ";"` | `machine_section` |
| `machine-section` | `guard` | `"guard" expression "else" expression ";"` | `machine_section` |
| `machine-section` | `emit` | `"emit" expression ";"` | `machine_section` |
| `machine-section` | `input`, `output` | `statement(p, false)` (diagnoses `E208`) | `declaration` |
| `type` | `fn` | `"fn" "(" types ")" "->" type` | `type` |
| `type` | `IDENT` | `segments [ "[" types "]" ]` | `type` |
| `block-item` | `let` | `"let" binder [ ":" type ] "=" expression ";"` | `statement` |
| `block-item` | `input` | `"input" binder ":" type [ "from" "stdin" ] ";"` | `statement` |
| `block-item` | `output` | `"output" expression [ "to" "stdout" ] ";"` | `statement` |
| `block-item` | `FIRST(expression)` | Final block expression (errors if followed by `;`) | `block` |
| `primary` | `INT`, `FLOAT`, `TEXT`, `CHAR`, `true`, `false` | `literal(p, false, ...)` | `primary` |
| `primary` | `IDENT` | `path = segments(p)` | `primary` |
| `primary` | `next` | `"next" "." IDENT` | `primary` |
| `primary` | `(` | `"(" ")"` (Unit) or `"(" expression ")"` (Group) | `primary` |
| `primary` | `[` | `"[" [ expression { "," expression } ] "]"` | `primary` |
| `primary` | `{` | `block(p, false)` | `primary` |
| `primary` | `if` | `"if" expression block "else" block` | `primary` |
| `primary` | `for` | `"for" IDENT "in" expression block` | `primary` |
| `primary` | `fn` | `"fn" parameters "->" type block` | `primary` |
| `primary` | `match` | `"match" expression "{" arm { "," arm } [ "," ] "}"` | `primary` |
| `argument` | `IDENT` followed by `:` (2-token lookahead) | Named argument `IDENT ":" expression` | `named_argument` |
| `argument` | other `FIRST(expression)` | Positional argument `expression` | `expression` |

## Notes on lookahead beyond LL(1)

- **Two-token lookahead for named call arguments (`named_argument()` at `src/parser.c:272-277`)**: When inside `callee(...)`, `IDENT` alone can start either a positional expression `x + 1` or a named record argument `x: 1`. The parser peeks past trivia at `p->at + 1` for `TF_COLON`. Mixing named and positional arguments triggers `E112`.
- **Two-token lookahead for top-level recovery (`top_boundary()` at `src/parser.c:81-90`)**: Because `fn` starts both a top-level function declaration (`fn name(...)`) and an anonymous closure (`fn(x: Int)`), `top_boundary()` treats `fn` as a declaration recovery boundary only when the next non-trivia token is `TF_IDENT`.
- **Single-branch `if` and loops**: Ternfold requires `else` after the then-block in `if`, and provides `for` comprehensions without `while` or `do-while` keywords.

## Pratt operator precedence table (`binding_power()` at `src/parser.c:261-271`)

Higher numbers bind tighter. Prefix unary `-` and `!` recurse with right minimum power `7`. Right-associative `^` recurses with right minimum power `7` while left-associative binary operators recurse with `power + 1`.

| Level (`power`) | Operators | Associativity | Right min power | AST kind |
|---:|---|---|---:|---|
| 1 | `\|\|` | Left | 2 | `TF_AST_BINARY` |
| 2 | `&&` | Left | 3 | `TF_AST_BINARY` |
| 3 | `==`, `!=` | Non-associative (`E111` if chained) | 4 | `TF_AST_BINARY` |
| 4 | `<`, `<=`, `>`, `>=` | Non-associative (`E111` if chained) | 5 | `TF_AST_BINARY` |
| 5 | `+`, `-`, `++` | Left | 6 | `TF_AST_BINARY` |
| 6 | `*`, `/`, `div`, `%` | Left | 7 | `TF_AST_BINARY` |
| 7 | Unary `-`, `!` | Prefix | 7 | `TF_AST_UNARY` |
| 8 | `^` | Right | 7 | `TF_AST_BINARY` |
| Postfix | `(...)`, `.IDENT` | Left-to-right | N/A (Pratt loop) | `TF_AST_CALL`, `TF_AST_PROJECT` |
