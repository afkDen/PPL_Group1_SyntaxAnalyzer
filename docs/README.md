# Syntax Analyzer documentation index

Coursework reference for `COSC 303: Programming Language Design` (`BSCS 3-2`, Group 1).

## Coursework deliverables

- [Symbol Table input contract](syntax-analyzer/SYMBOL_TABLE_INPUT.md): Versioned schema v1 (`ternfold-symbol-table`), sequential token reader API (`tf_symbol_table_next`), SHA-256 verification, and classroom report relationship.
- [Recursive descent architecture](syntax-analyzer/RECURSIVE_DESCENT.md): Walkthrough of `src/parser.c` routines, Pratt operator climbing, and AST arena ownership.
- [Grammar and parsing table](syntax-analyzer/GRAMMAR_AND_PARSING_TABLE.md): Normative EBNF, predictive parsing table, lookahead rules, and expression precedence table.
- [Transition diagram](syntax-analyzer/TRANSITION_DIAGRAM.md): Parser state-flow diagrams covering module parsing, declarations, statements, conditionals, loops, and expression precedence.
- [Error recovery](syntax-analyzer/ERROR_RECOVERY.md): Panic-mode synchronization in `recover()`, zero-width `MISSING` token synthesis, recovery bounds, and error diagnostics (`E110`, `E111`, `E112`, `E208`).
- [Demo row mapping](syntax-analyzer/DEMO_ROWS.md): Exact source lines, Symbol Table token indices, and AST node kinds for `examples/syntax/complete.tfd`.
- [Invalid cases and negative tests](syntax-analyzer/INVALID_CASES.md): Invalid syntax inputs, line-numbered diagnostic outputs, malformed Symbol Table rejections, and unsupported constructs.

## Core language and compiler reference

- [Language specification](language-spec.md)
- [Concrete grammar](grammar.md)
- [Parser design overview](parser.md)
- [Lexical finite automata](automata.md)
- [Compiler pipeline](compiler-pipeline.md)
- [Diagnostics specification](diagnostics.md)
- [Source management](source-management.md)
- [Architecture notes](architecture.md)
- [Testing guide](testing.md)
- [Architectural decision records](decisions/README.md)
