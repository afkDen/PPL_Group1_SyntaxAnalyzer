#ifndef TERNFOLD_SYMBOL_TABLE_H
#define TERNFOLD_SYMBOL_TABLE_H

#include "ternfold/lexer.h"

/* Lossless versioned Symbol Table serialization and sequential one-token-at-a-time
 * loader for decoupling lexical analysis from tf_parse().
 *
 * Schema v1 ("format": "ternfold-symbol-table", "schema_version": 1):
 *   Valid sources emit one ordered entry per token (comments, invalid tokens,
 *   trailing EOF inclusive). Each entry stores its preceding whitespace gap
 *   and exact source slice plus decoded UTF-8 payload for TEXT/CHAR literals,
 *   alongside line/column coordinates and byte spans. The file carries
 *   source_length and source_sha256 over the raw source bytes so the sequential
 *   reader reconstructs coordinates and verifies byte-for-byte integrity one
 *   token at a time without reopening the .tfd file or invoking tf_lex().
 *
 *   Source-invalid inputs (E001..E003) serialize source_valid=false, the
 *   diagnostic code/span, and hex-encoded raw bytes with an empty token list.
 */

typedef struct TfSymbolTableReader TfSymbolTableReader;

typedef struct {
    size_t index;
    TfToken token; /* span.source is NULL during streaming; caller/finish binds TfSource */
    TfLocation start;
    TfLocation end;
    const unsigned char *gap;
    size_t gap_length;
} TfSymbolTableEntry;

/* Writes schema v1 Symbol Table JSON to a binary FILE stream.
 * Borrowing/ownership rules match tf_token_dump_write(). */
TfStatus tf_symbol_table_write(FILE *stream, const TfSource *source,
                               const TfLexResult *result, size_t diagnostic_cap);

/* Publishes schema v1 Symbol Table JSON to path atomically via sibling temp
 * file replacement. Rejects overwriting source itself via file identity. */
TfStatus tf_symbol_table_publish(const TfSource *source, const TfLexResult *result,
                                 size_t diagnostic_cap, const char *path);

/* Opens a serialized Symbol Table file and validates its header metadata.
 * Sets *out to a reader positioned at index 0 of the token stream (or NULL on
 * allocation/I/O/schema failure). On TF_SOURCE_ERROR when source_valid=false,
 * a reader is still returned when header validation succeeds so callers can
 * inspect original source error details via tf_symbol_table_reader_source_error(). */
TfStatus tf_symbol_table_reader_open(TfSymbolTableReader **out, const char *path,
                                     const TfAllocator *allocator);

/* Same as tf_symbol_table_reader_open(), consuming an in-memory buffer. */
TfStatus tf_symbol_table_reader_open_bytes(TfSymbolTableReader **out, const void *bytes,
                                           size_t length, const char *logical_path,
                                           const TfAllocator *allocator);

/* Consumes and validates the next single token record sequentially from the
 * Symbol Table stream. Never pre-loads remaining tokens or invokes tf_lex().
 * On *has_token=true, caller owns nothing directly unless calling
 * tf_symbol_table_entry_destroy() on entry; reader retains all token storage
 * needed by tf_symbol_table_finish(). */
TfStatus tf_symbol_table_next(TfSymbolTableReader *reader, TfSymbolTableEntry *out,
                              bool *has_token);

void tf_symbol_table_entry_destroy(TfSymbolTableReader *reader, TfSymbolTableEntry *entry);

/* Verifies end-of-stream invariants (EOF seen, token_count match, source_length
 * match, SHA-256 digest match, trailing JSON closure, TfLocation parity) and
 * transfers ownership of an initialized TfSource and TfLexResult to caller.
 * Destroy *out_lex before *out_source. */
TfStatus tf_symbol_table_finish(TfSymbolTableReader *reader, TfSource *out_source,
                                TfLexResult *out_lex);

/* Convenience sequential loader: opens path, consumes every token one-by-one
 * via tf_symbol_table_next(), optionally traces each consumed token to trace,
 * and finalizes *out_source / *out_lex (including TF_SOURCE_ERROR reconstruction). */
TfStatus tf_symbol_table_load(const char *path, TfSource *out_source, TfLexResult *out_lex,
                              const TfAllocator *allocator, FILE *trace,
                              char *error_buffer, size_t error_buffer_size);

void tf_symbol_table_reader_destroy(TfSymbolTableReader *reader);

/* Human-readable validation/load error from the most recent reader failure. */
const char *tf_symbol_table_reader_error(const TfSymbolTableReader *reader);

bool tf_symbol_table_reader_is_source_invalid(const TfSymbolTableReader *reader);

/* For source_valid=false tables, writes/allocates raw invalid source via
 * tf_source_init() into *out_source (returning TF_SOURCE_ERROR on valid input
 * reconstruction). */
TfStatus tf_symbol_table_reconstruct_invalid_source(const TfSymbolTableReader *reader,
                                                    TfSource *out_source,
                                                    const TfAllocator *allocator);

#endif
