# Parser transition diagrams

The diagrams below map the actual control transitions in `src/symbol_table.c` and `src/parser.c`. Comments (`TF_COMMENT_LINE`, `TF_COMMENT_BLOCK`) are skipped automatically by `peek()` between any two parser states.

## 1. End-to-end pipeline (Symbol Table input to AST)

```mermaid
flowchart LR
    File["Saved Symbol Table File"] --> Open["tf_symbol_table_reader_open()"]
    Open --> Next["tf_symbol_table_next() (one token at a time)"]
    Next -->|has_token = true| Next
    Next -->|has_token = false| Finish["tf_symbol_table_finish() (SHA-256 + EOF check)"]
    Finish -->|valid or lex-error stream| Parse["tf_parse()"]
    Finish -->|source_valid = false| SrcErr["E001-E003 Diagnostic + null root"]
    Next -->|schema/digest/order error| Corrupt["Exit 2: Rejected"]
    Parse --> AstOut["TfAst + Diagnostics"]
```

## 2. Module and top-level declaration dispatch (`parse_module`, `declaration`)

```mermaid
stateDiagram-v2
    [*] --> ModuleStart
    ModuleStart --> ImportDecl : "import"
    ModuleStart --> DeclPub : "pub"
    ModuleStart --> DeclFn : "fn"
    ModuleStart --> DeclRecord : "record"
    ModuleStart --> DeclEnum : "enum"
    ModuleStart --> DeclMachine : "machine"
    ModuleStart --> TopRecover : "other token (E110)"
    ModuleStart --> [*] : "EOF"

    ImportDecl --> ModuleStart : "STRING as IDENT ;"
    DeclPub --> DeclFn : "fn"
    DeclPub --> DeclRecord : "record"
    DeclPub --> DeclEnum : "enum"
    DeclPub --> DeclMachine : "machine"
    DeclPub --> ImportDecl : "import (E110)"
    DeclFn --> ModuleStart : "IDENT params -> type block"
    DeclRecord --> ModuleStart : "IDENT { fields }"
    DeclEnum --> ModuleStart : "IDENT { variants }"
    DeclMachine --> ModuleStart : "IDENT ( param ) yields type rejects type { sections }"
    TopRecover --> ModuleStart : "recover(true) at boundary"
```

## 3. Block statements, conditionals, loops, and match (`block`, `statement`, `primary`)

```mermaid
flowchart TD
    BStart["Block Start '{'"] --> CheckTok{"peek(p)"}
    CheckTok -->|"let"| LetStmt["statement(): let (IDENT|'_') [: type] = expr ;"] --> CheckTok
    CheckTok -->|"input / output"| IoStmt["statement(io): check main-only E208; parse clause ;"] --> CheckTok
    CheckTok -->|"RBRACE (no final expr)"| MissingFinal["Diagnose E110 -> TF_AST_ERROR -> expect '}'"]
    CheckTok -->|"FIRST(expression)"| Expr["expression(0)"]
    Expr --> AfterExpr{"peek(p)"}
    AfterExpr -->|"';'"| SemiErr["Diagnose E110 (semicolon after final expr)"] --> CheckTok
    AfterExpr -->|"'}'"| BEnd["consume '}' -> Block Done"]
    AfterExpr -->|"other token"| TrailErr["Diagnose E110 -> recover(false)"] --> CheckTok

    Expr -->|"if"| IfCond["expression(0) -> block -> expect else -> block"]
    Expr -->|"for"| ForLoop["IDENT -> expect in -> expression(0) -> block"]
    Expr -->|"match"| MatchArms["expression(0) -> '{' -> pattern => block { , arm } -> '}'"]
```

## 4. Pratt expression and postfix loop (`expression`, `binding_power`)

```mermaid
stateDiagram-v2
    [*] --> Prefix
    Prefix --> Primary : "literal | path | next.id | () | (e) | [...] | {...} | if | for | fn | match"
    Prefix --> Unary : "'-' or '!' -> recurse expression(7)"
    Primary --> PostfixLoop
    Unary --> PostfixLoop

    state PostfixLoop {
        [*] --> CheckOp
        CheckOp --> Call : "'(' -> args (2-token lookahead for IDENT :)"
        Call --> CheckOp : "')'"
        CheckOp --> Project : "'.' -> IDENT"
        Project --> CheckOp
        CheckOp --> Binary : "power >= min_bp"
        Binary --> CheckOp : "take op -> check nonassoc -> recurse expression(next_min_bp)"
        CheckOp --> Done : "power == 0 or power < min_bp"
    }
```
