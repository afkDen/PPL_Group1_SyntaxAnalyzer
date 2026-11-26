# PPL course compliance contract — Ternfold 0.2

**Status: The lexical submission is verified separately; reference and native execution are verified on the documented Windows host.** See the acceptance ledger and verification records for exact scope. Historical Python acceptance is preserved separately. Finite automata recognize lexemes; they are not machine transition graphs.

**Authority:** user-supplied course checklist; instructor's earlier 11-criterion lexer sheet; the submitted Ternfold group proposal, `Group-1_Project_Proposal-1 (1).pdf`; reference Group-2 proposal *for examples of expected DFA illustrations only*. This file maps evidence. The language rules are in `language-spec.md`, `grammar.md`, `type-system.md`, `runtime.md` and ADRs 0009–0010.

| Requirement | Target artifact / behavior | Acceptance milestone |
|---|---|---|
| Name/background/inspiration | Ternfold triple-phase machine; Elm/Lustre/Clojure precedent without novelty overclaim | M00 |
| Sigma, English/algebraic identifiers | Explicit letters/digits/glyphs/escapes; identifiers and function `square`/`polynomial` | M03/M04 |
| Seven arithmetic `+ - / * ^ % div` | Real `/` returns Float; integer `div` floor; checked power and finite numeric domains per ADR 0010 | C01/M08/M09/M11 |
| Six relational (part of nine) | `< <= > >= == !=` with nonassociative precedence | M03/M04/M06 |
| Three logical AND/OR/NOT (nine combined) | `&& || !` short circuit | M03/M04/M09 |
| 10 keywords + 10 reserved names | 25+ keywords, 20+ reserved type/builtin names, separate explanation table | M03/M05/report |
| Finite automata | `automata.md` state diagrams + transitions/acceptance; executable DFA lexeme fixtures | M03 |
| Noise words | optional `from stdin`, `to stdout`; lex as NOISE_WORD category | M03/M04/M09 |
| Single/block comments | `//`, `/*...*/` with unterminated errors; report as trivia rows | M03 |
| Spaces & free field | SP/TAB/LF/CRLF; semicolon explicit | M01/M03 |
| Commas/semicolon/brackets | lexical and parser tests for `, ; () [] {}` | M03/M04 |
| Four constant types minimum | independent Int, Float, Bool, Text, Char tokens (plus Unit expression) | M03/C01 |
| Invalid token / wrong extension | E101 et al. plus report INVALID, `.tfd` enforced | M03 |
| Symbol Table.txt | exactly TOKEN TYPE and LEXEME columns, deterministic source-order comment/invalid rows | M03 |
| Expressions and precedence | grammar and tests for exponent, div, comparisons, logic | M04/M06 |
| Input statement | `input name: Text [from stdin];` main-only snapshot binding | M04/M06/M08/M09/M11 |
| Output statement | `output expr [to stdout];` staging, all-or-nothing output | M04/M06/M08/M09/M11 |
| Assignment statement | `let name = expr;` immutable binding; `next cell = expr;` candidate writer (not mutation) | M04/M07/M09 |
| Conditional | `if Bool { expr } else { expr }`, nested else-if; existing semantics | M04/M06/M09 |
| Single loop | pure `for x in range(...) { expr }` -> List | M04/M06/M09 |
| Nested loop | `for row ... { for col ... { expr } }` -> List[List] | M04/M06/M09 |
| Native parity | input snapshot, Char, div/power, comprehensions identical in C17 | M11/M14 |

A requirement is accepted only if its owner writes positive + negative tests (where meaningful), focused and full quality gates pass, and review confirms the rubric evidence. The native `tools/audit.c` repository audit checks static repository structure, scanner invariants, and Markdown cross-links, while `scripts/course_audit.c` verifies Syntax Analyzer fixtures and manifest presence. The complete demonstration includes source, lexical table, invalid diagnostic, DFA diagram, single loop, nested loop, both comments, and interpreter/native outputs.

## Runnable teaching demonstration

```tfd
/* A block comment is ignored during execution. */
// Course examples have both comment forms.
fn square(x: Int) -> Int { x ^ 2 }
fn polynomial(x: Int) -> Int { 2 * (x ^ 2) + 3 * x - 1 }
fn squares() -> List[Int] {
    for n in range(0, 3) { square(n) }
}
fn table() -> List[List[Int]] {
    for row in range(0, 2) {
        for col in range(0, 3) { row * 10 + col }
    }
}
pub fn main() -> List[Text] {
    input visitor: Text from stdin;
    let a = 13 / 5;
    let b = -7 div 3;
    let mark: Char = 'T';
    let ready: Bool = mark == 'T' && squares() == [0, 1, 4]
                    && table() == [[0, 1, 2], [10, 11, 12]];
    output "Hello " ++ visitor to stdout;
    [float_text(a), int_text(b), int_text(square(3)),
     if ready { int_text(polynomial(3)) } else { "unexpected" }]
}
```

With UTF-8 input `Ada` followed by LF, both executors produce the exact lines
`Hello Ada`, `2.6000000000000001e0`, `-3`, `9`, and `26`. The final
line depends on the Char and both loop result checks. See the
[maintained source](../examples/course/teaching.tfd) and
[execution manifest](../examples/manifest.json).

`scripts/course_audit.c` verifies that each required Syntax Analyzer fixture and
manifest entry is present. Classroom report tests, Symbol Table reader tests,
parser tests, and paired example runs provide behavioral verification.

## Eleven-item lexical checklist and Syntax Analyzer continuity

`/7` and `/9` are required operation counts, not grading points. Arithmetic:
`+ - / * % div ^` (seven); logical/relational: `&& || ! > < == != >= <=` (nine).
Real division `/` is mathematically distinct from integer `div`; ADR 0010 defines
finite Float semantics. Every operation is recognized and tested independently while
preserving logical short-circuiting.

The standalone [Lexical Analyzer repository](https://github.com/afkDen/PPL_Group1_LexicalAnalyzer)
covers the eleven lexical criteria (`.tfd` extension policy, identifiers, 28
keywords, 25 reserved names, 5 literal categories, `from`/`to` noise words,
`//` and `/*...*/` comments, all 7+9 operations, delimiters/brackets, `INVALID`
lexemes/recovery, and the two-column `Symbol Table.txt`). This Syntax Analyzer
repository (`COSC 303`, `BSCS 3-2`, Group 1) preserves that scanner and table
format directly while adding lossless schema-v1 Symbol Table export and
one-token-at-a-time parsing in [RUBRIC.md](../RUBRIC.md). For full C17 execution
beyond syntax analysis, see [Ternfold 0.2.1](https://github.com/afkDen/ternfold).
