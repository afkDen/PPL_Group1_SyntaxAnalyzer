# Symbol Table file input and sequential token reader

The syntax analyzer does not read raw `.tfd` source files or call `tf_lex()` when invoked through `ternfold parse-table` or `ternfold ast --from-symbol-table`. Instead, the lexer publishes a lossless, versioned Symbol Table file to disk, and the parser stage consumes that file one token record at a time through `TfSymbolTableReader` (`include/ternfold/symbol_table.h`, `src/symbol_table.c`).

## Two companion outputs from the lexer

Running `ternfold tokens` supports publishing both artifacts from a single lexical pass:

```text
source.tfd
  |
  +---> tf_lex()
          |
          +--> --classroom-table DIR   ---> DIR/Symbol Table.txt (2-column TOKEN TYPE / LEXEME)
          +--> --parser-input FILE     ---> FILE (ternfold-symbol-table schema v1)
```

`Symbol Table.txt` preserves the classroom two-column format (`TOKEN TYPE` and `LEXEME`) used in the Lexical Analyzer submission. It omits whitespace gaps, byte spans, line numbers, decoded embedded NUL payloads, and `EOF`. Because the syntax analyzer needs exact byte spans, decoded `Text`/`Char` payloads, diagnostic records, and verified line coordinates, `--parser-input` writes a lossless structured JSON stream alongside the classroom table.

## Lossless schema v1 (`ternfold-symbol-table`)

Header fields appear in order before the `tokens` array:

| Field | Type | Validation rule |
|---|---|---|
| `format` | String | Must equal `"ternfold-symbol-table"`. |
| `schema_version` | Integer | Must equal `1`. |
| `source_name` | String | Non-empty original source path, no embedded NUL. |
| `source_valid` | Boolean | `true` for tokenized files; `false` when `E001`-`E003` prevented scanning. |
| `source_length` | Integer | Exact raw byte length of the original `.tfd` input. |
| `source_sha256` | String | 64 lowercase hexadecimal digits over the raw source bytes. |
| `source_error` | Object or `null` | `null` when valid; `{code, start_byte, end_byte}` for `E001`-`E003`. |
| `raw_hex` | String or `null` | `null` when valid; hex raw bytes only for source-invalid inputs (`E001`-`E003`). |
| `has_errors` | Boolean | `true` if `source_valid=false` or any `TF_INVALID` token occurred. |
| `diagnostic_cap` | Integer | Diagnostic cap in `0..100`. |
| `diagnostic_count` | Integer | Count of retained lexical diagnostics (`<= diagnostic_cap`). |
| `suppressed_diagnostics` | Integer | Count of overflow diagnostics above `diagnostic_cap`. |
| `token_count` | Integer | Must be `>= 1` when `source_valid=true`, or `0` when `false`. |
| `tokens` | Array | Last header key; consumed one token object at a time by the reader. |

Each object inside `tokens` carries:

```json
{
  "index": 0,
  "gap": "",
  "kind": "FN",
  "reserved": false,
  "lexeme": "fn",
  "payload": null,
  "payload_bytes": 0,
  "start_byte": 0,
  "end_byte": 2,
  "start": {"scalar": 0, "line": 1, "column": 1, "display_column": 1},
  "end": {"scalar": 2, "line": 1, "column": 3, "display_column": 3},
  "error": null
}
```

Every byte of a valid source is partitioned across `(token[i].gap, token[i].lexeme)` pairs ending with `TF_EOF` at `[source_length, source_length)`. The file contains no separate full-source copy when `source_valid=true`.

## Sequential one-token-at-a-time consumption

The syntax loader uses a streaming state machine across three public entry points in `include/ternfold/symbol_table.h`:

1. `tf_symbol_table_reader_open()` opens the JSON file, checks all header keys up to `tokens:[`, and stops without pre-parsing any token element.
2. `tf_symbol_table_next()` parses and validates one `{...}` token record per call:
   - Checks monotonic `index`, exact field completeness (no unknown or duplicate keys), and non-overlapping `start_byte >= prev_end`.
   - Verifies that `gap` contains only legal whitespace (` `, `\t`, `\n`, or `\r\n`, rejecting lone `\r`) and matches `start_byte - prev_end`.
   - Validates token classification consistency (`token_matches_lexeme()`), including checking `TF_IDENT` and keyword/reserved classifications through `tf_name_dfa_step()` and `tf_name_dfa_state()`.
   - Verifies `start` and `end` `(scalar, line, column, display_column)` against a live UTF-8 scalar and tab-stop tracker.
   - Feeds `gap` and `lexeme` bytes into a streaming SHA-256 accumulator and appends the validated `TfToken` (with decoded `Text`/`Char` payload) to the growing token stream.
3. `tf_symbol_table_finish()` verifies closing `]}` JSON syntax, `TF_EOF` presence, exact `token_count` and `source_length`, diagnostic counts, and final SHA-256 digest equality before calling `tf_source_init()` to construct line/scalar lookup tables for `tf_parse()` and diagnostic reporting.

## Preventing re-lex bypass and spoofing

- Neither `src/symbol_table.c` nor `parse_symbol_table_mode()` in `src/compiler_main.c` calls `tf_lex()`. `tools/audit.c` verifies that `src/symbol_table.c` contains no `tf_lex(` call.
- `tests/test_symbol_table.c` and `test_parse_table_cli()` in `tests/test_tokens_cli.c` delete the `.tfd` source file after generating the Symbol Table file and prove `ternfold parse-table` and `ternfold ast --from-symbol-table` succeed from the Symbol Table file alone.
- Tampering with `format`, `schema_version`, `source_sha256`, `index`, `kind`, `reserved`, `gap`, `lexeme`, `payload`, line/column numbers, or trailing bytes causes immediate rejection with a descriptive error and exit status `2`.
