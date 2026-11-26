#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "ternfold/symbol_table.h"
#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

enum { ST_MAX_FILE_BYTES = 16 * 1024 * 1024 };

/* ---- Streaming SHA-256 (aligned with source_manager.c) ---- */

typedef struct {
    uint32_t state[8];
    uint64_t total_bits;
    unsigned char buffer[64];
    size_t buffered;
    bool overflow;
} Sha256;

static uint32_t sha_rotr(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32u - n));
}

static void sha_block(uint32_t h[8], const unsigned char block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t w[64], v[8];
    size_t i;
    for (i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)block[4 * i] << 24) |
               ((uint32_t)block[4 * i + 1] << 16) |
               ((uint32_t)block[4 * i + 2] << 8) |
               (uint32_t)block[4 * i + 3];
    }
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = sha_rotr(w[i - 15], 7) ^ sha_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = sha_rotr(w[i - 2], 17) ^ sha_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    memcpy(v, h, sizeof(v));
    for (i = 0; i < 64; ++i) {
        uint32_t s1 = sha_rotr(v[4], 6) ^ sha_rotr(v[4], 11) ^ sha_rotr(v[4], 25);
        uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
        uint32_t t1 = v[7] + s1 + ch + k[i] + w[i];
        uint32_t s0 = sha_rotr(v[0], 2) ^ sha_rotr(v[0], 13) ^ sha_rotr(v[0], 22);
        uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        v[7] = v[6];
        v[6] = v[5];
        v[5] = v[4];
        v[4] = v[3] + t1;
        v[3] = v[2];
        v[2] = v[1];
        v[1] = v[0];
        v[0] = t1 + s0 + maj;
    }
    for (i = 0; i < 8; ++i) h[i] += v[i];
}

static void sha_init(Sha256 *s) {
    static const uint32_t iv[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    memcpy(s->state, iv, sizeof(iv));
    s->total_bits = 0;
    s->buffered = 0;
    s->overflow = false;
}

static void sha_update(Sha256 *s, const unsigned char *data, size_t length) {
    size_t at = 0;
    if (s->overflow || length == 0) return;
    if ((uintmax_t)length > UINT64_MAX / UINT64_C(8) ||
        s->total_bits > UINT64_MAX - (uint64_t)length * UINT64_C(8)) {
        s->overflow = true;
        return;
    }
    s->total_bits += (uint64_t)length * UINT64_C(8);
    if (s->buffered != 0) {
        size_t need = 64u - s->buffered;
        size_t copy = length < need ? length : need;
        memcpy(s->buffer + s->buffered, data, copy);
        s->buffered += copy;
        at += copy;
        if (s->buffered == 64u) {
            sha_block(s->state, s->buffer);
            s->buffered = 0;
        }
    }
    while (length - at >= 64u) {
        sha_block(s->state, data + at);
        at += 64u;
    }
    if (at < length) {
        memcpy(s->buffer, data + at, length - at);
        s->buffered = length - at;
    }
}

static bool sha_final(Sha256 *s, unsigned char out[32]) {
    unsigned char tail[128] = {0};
    size_t padded, i;
    if (s->overflow) return false;
    memcpy(tail, s->buffer, s->buffered);
    tail[s->buffered] = 0x80u;
    padded = s->buffered < 56u ? 64u : 128u;
    for (i = 0; i < 8; ++i) {
        tail[padded - 1u - i] = (unsigned char)(s->total_bits >> (8u * i));
    }
    sha_block(s->state, tail);
    if (padded == 128u) sha_block(s->state, tail + 64);
    for (i = 0; i < 32; ++i) {
        out[i] = (unsigned char)(s->state[i / 4u] >> (24u - 8u * (i % 4u)));
    }
    return true;
}

static void sha_hex(const unsigned char digest[32], char hex[65]) {
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 32; ++i) {
        hex[2 * i] = digits[(digest[i] >> 4) & 0x0fu];
        hex[2 * i + 1] = digits[digest[i] & 0x0fu];
    }
    hex[64] = '\0';
}

/* ---- Writer helpers ---- */

static bool put_str(FILE *f, const char *s) { return fputs(s, f) != EOF; }

static bool put_json_bytes(FILE *f, const unsigned char *bytes, size_t length, bool allow_raw) {
    size_t at = 0;
    if (bytes == NULL && length != 0) return false;
    if (fputc('"', f) == EOF) return false;
    while (at < length) {
        unsigned char c = bytes[at];
        uint32_t scalar;
        size_t end;
        if (c == '"' || c == '\\') {
            if (fputc('\\', f) == EOF || fputc((int)c, f) == EOF) return false;
            ++at;
        } else if (c < 0x20u || c == 0x7fu) {
            if (fprintf(f, "\\u%04x", (unsigned)c) < 0) return false;
            ++at;
        } else if (!tf_decode(bytes, length, at, &scalar, &end)) {
            if (!allow_raw) return false;
            if (fprintf(f, "\\u%04x", (unsigned)c) < 0) return false;
            ++at;
        } else {
            if (fwrite(bytes + at, 1, end - at, f) != end - at) return false;
            at = end;
        }
    }
    return fputc('"', f) != EOF;
}

static bool put_location(FILE *f, TfLocation loc) {
    return fprintf(f, "{\"scalar\":%zu,\"line\":%zu,\"column\":%zu,\"display_column\":%zu}",
                   loc.scalar, loc.line, loc.column, loc.display_column) >= 0;
}

static bool has_text_payload(TfTokenKind k) {
    return k == TF_IDENT || k == TF_INT || k == TF_FLOAT || k == TF_TEXT || k == TF_CHAR;
}

static bool put_payload(FILE *f, const TfToken *t) {
    if (has_text_payload(t->kind)) {
        return put_json_bytes(f, t->value, t->value_length, false);
    }
    if (t->kind == TF_TRUE) return put_str(f, "true");
    if (t->kind == TF_FALSE) return put_str(f, "false");
    return put_str(f, "null");
}

static bool valid_lex_result(const TfSource *source, const TfLexResult *result, size_t diag_cap) {
    size_t i, previous = 0, error_tokens = 0;
    if (!source->valid) {
        return result == NULL && source->error.span.source == source &&
               source->error.code >= TF_E001 && source->error.code <= TF_E003 &&
               source->error.span.start <= source->error.span.end &&
               source->error.span.end <= source->length;
    }
    if (result == NULL || result->source != source || result->tokens == NULL ||
        result->token_count == 0 || result->token_count > SIZE_MAX / sizeof(*result->tokens)) {
        return false;
    }
    if (result->diagnostic_count > diag_cap) return false;
    if (result->diagnostic_count != 0 && result->diagnostics == NULL) return false;
    if (!result->has_errors && (result->diagnostic_count != 0 || result->suppressed != 0)) return false;
    for (i = 0; i < result->token_count; ++i) {
        const TfToken *t = &result->tokens[i];
        TfSpan checked;
        if (t->span.source != source || t->kind < TF_IDENT || t->kind > TF_COMMENT_BLOCK ||
            !tf_source_span(source, t->span.start, t->span.end, &checked) ||
            t->span.start < previous || (t->value == NULL && t->value_length != 0)) {
            return false;
        }
        if (t->kind == TF_INVALID) {
            if (t->error < TF_E101 || t->error > TF_E105) return false;
            ++error_tokens;
        } else if (t->error != TF_E_NONE) {
            return false;
        }
        if (t->kind == TF_EOF) {
            if (i != result->token_count - 1 || t->span.start != source->length ||
                t->span.end != source->length) {
                return false;
            }
        } else if (i == result->token_count - 1 || t->span.start == t->span.end) {
            return false;
        }
        previous = t->span.end;
    }
    if (error_tokens != result->diagnostic_count + result->suppressed) return false;
    return true;
}

TfStatus tf_symbol_table_write(FILE *stream, const TfSource *source,
                               const TfLexResult *result, size_t diagnostic_cap) {
    Sha256 sha;
    unsigned char digest[32];
    char hex[65];
    size_t i, previous = 0;
    bool has_errors;
    size_t diag_count, suppressed, token_count;

    if (stream == NULL || source == NULL || source->bytes == NULL ||
        source->name == NULL || ferror(stream) ||
        !valid_lex_result(source, result, diagnostic_cap)) {
        return TF_HOST_ERROR;
    }
    sha_init(&sha);
    sha_update(&sha, source->bytes, source->length);
    if (!sha_final(&sha, digest)) return TF_HOST_ERROR;
    sha_hex(digest, hex);

    has_errors = !source->valid || result->has_errors;
    diag_count = source->valid ? result->diagnostic_count : 0u;
    suppressed = source->valid ? result->suppressed : 0u;
    token_count = source->valid ? result->token_count : 0u;

    if (!put_str(stream, "{\"format\":\"ternfold-symbol-table\",\"schema_version\":1,\"source_name\":") ||
        !put_json_bytes(stream, (const unsigned char *)source->name, strlen(source->name), true) ||
        fprintf(stream, ",\"source_valid\":%s,\"source_length\":%zu,\"source_sha256\":\"%s\"",
                source->valid ? "true" : "false", source->length, hex) < 0) {
        return TF_HOST_ERROR;
    }
    if (source->valid) {
        if (!put_str(stream, ",\"source_error\":null,\"raw_hex\":null")) return TF_HOST_ERROR;
    } else {
        if (fprintf(stream, ",\"source_error\":{\"code\":\"%s\",\"start_byte\":%zu,\"end_byte\":%zu},\"raw_hex\":\"",
                    tf_error_code(source->error.code),
                    source->error.span.start, source->error.span.end) < 0) {
            return TF_HOST_ERROR;
        }
        for (i = 0; i < source->length; ++i) {
            if (fprintf(stream, "%02x", (unsigned)source->bytes[i]) < 0) return TF_HOST_ERROR;
        }
        if (!put_str(stream, "\"")) return TF_HOST_ERROR;
    }
    if (fprintf(stream,
                ",\"has_errors\":%s,\"diagnostic_cap\":%zu,\"diagnostic_count\":%zu,"
                "\"suppressed_diagnostics\":%zu,\"token_count\":%zu,\"tokens\":[",
                has_errors ? "true" : "false", diagnostic_cap, diag_count, suppressed, token_count) < 0) {
        return TF_HOST_ERROR;
    }
    for (i = 0; source->valid && i < result->token_count; ++i) {
        const TfToken *t = &result->tokens[i];
        TfLocation start_loc, end_loc;
        size_t payload_bytes = has_text_payload(t->kind) ? t->value_length : 0u;
        if (!tf_source_location(source, t->span.start, &start_loc) ||
            !tf_source_location(source, t->span.end, &end_loc)) {
            return TF_HOST_ERROR;
        }
        if ((i != 0 && !put_str(stream, ",")) ||
            fprintf(stream, "\n  {\"index\":%zu,\"gap\":", i) < 0 ||
            !put_json_bytes(stream, source->bytes + previous, t->span.start - previous, false) ||
            fprintf(stream, ",\"kind\":\"%s\",\"reserved\":%s,\"lexeme\":",
                    tf_token_name(t->kind), t->reserved ? "true" : "false") < 0 ||
            !put_json_bytes(stream, source->bytes + t->span.start, t->span.end - t->span.start, false) ||
            !put_str(stream, ",\"payload\":") ||
            !put_payload(stream, t) ||
            fprintf(stream, ",\"payload_bytes\":%zu,\"start_byte\":%zu,\"end_byte\":%zu,\"start\":",
                    payload_bytes, t->span.start, t->span.end) < 0 ||
            !put_location(stream, start_loc) ||
            !put_str(stream, ",\"end\":") ||
            !put_location(stream, end_loc) ||
            !put_str(stream, ",\"error\":") ||
            (t->error == TF_E_NONE ? !put_str(stream, "null") : fprintf(stream, "\"%s\"", tf_error_code(t->error)) < 0) ||
            !put_str(stream, "}")) {
            return TF_HOST_ERROR;
        }
        previous = t->span.end;
    }
    if (!put_str(stream, token_count > 0 ? "\n]}\n" : "]}\n")) return TF_HOST_ERROR;
    return ferror(stream) ? TF_HOST_ERROR : TF_OK;
}

static bool same_identity(TfFileIdentity left, TfFileIdentity right) {
    return left.valid && right.valid && left.device == right.device && left.index == right.index;
}

static bool source_alias(const char *destination, const TfSource *source) {
    TfFileIdentity target, current_src;
    if (!tf_identity_path(destination, &target)) return false;
    if (same_identity(target, source->origin)) return true;
    return source->name != NULL && tf_identity_path(source->name, &current_src) &&
           same_identity(target, current_src);
}

static unsigned long current_pid(void) {
#ifdef _WIN32
    return (unsigned long)_getpid();
#else
    return (unsigned long)getpid();
#endif
}

static FILE *open_exclusive(const char *path, bool *collision) {
    *collision = false;
#ifdef _WIN32
    {
        HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        int fd;
        FILE *stream;
        if (h == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            *collision = err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS;
            return NULL;
        }
        fd = _open_osfhandle((intptr_t)h, _O_WRONLY | _O_BINARY);
        if (fd < 0) { (void)CloseHandle(h); (void)remove(path); return NULL; }
        stream = _fdopen(fd, "wb");
        if (stream == NULL) { (void)_close(fd); (void)remove(path); }
        return stream;
    }
#else
    {
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        FILE *stream;
        if (fd < 0) { *collision = errno == EEXIST; return NULL; }
        stream = fdopen(fd, "wb");
        if (stream == NULL) { (void)close(fd); (void)remove(path); }
        return stream;
    }
#endif
}

static bool replace_atomic(const char *tmp, const char *dest) {
#ifdef _WIN32
    return MoveFileExA(tmp, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmp, dest) == 0;
#endif
}

TfStatus tf_symbol_table_publish(const TfSource *source, const TfLexResult *result,
                                 size_t diagnostic_cap, const char *path) {
    TfAllocator memory;
    size_t path_len;
    char *tmp = NULL;
    unsigned attempt;
    FILE *stream = NULL;
    TfStatus status = TF_HOST_ERROR;

    if (source == NULL || path == NULL || path[0] == '\0' ||
        (result != NULL && result->source != source)) {
        return TF_HOST_ERROR;
    }
    if (!valid_lex_result(source, result, diagnostic_cap)) return TF_HOST_ERROR;
    memory = source->allocator;
    if (!tf_allocator_valid(memory)) return TF_HOST_ERROR;
    if (source_alias(path, source)) return TF_HOST_ERROR;

    path_len = strlen(path);
    if (path_len > SIZE_MAX - 64u) return TF_HOST_ERROR;
    tmp = memory.allocate(memory.context, path_len + 64u);
    if (tmp == NULL) return TF_HOST_ERROR;

    for (attempt = 0; attempt < 128u; ++attempt) {
        int count = snprintf(tmp, path_len + 64u, "%s.tmp.%lu.%u", path, current_pid(), attempt);
        bool collision;
        if (count < 0 || (size_t)count >= path_len + 64u) goto done;
        stream = open_exclusive(tmp, &collision);
        if (stream != NULL) break;
        if (!collision) goto done;
    }
    if (stream == NULL) goto done;
    status = tf_symbol_table_write(stream, source, result, diagnostic_cap);
    if (fflush(stream) != 0) status = TF_HOST_ERROR;
    if (fclose(stream) != 0) status = TF_HOST_ERROR;
    stream = NULL;
    if (status == TF_OK && (source_alias(path, source) || !replace_atomic(tmp, path))) {
        status = TF_HOST_ERROR;
    }
    if (status != TF_OK) (void)remove(tmp);
done:
    memory.deallocate(memory.context, tmp);
    return status;
}

/* ---- Sequential JSON parser and validation ---- */

struct TfSymbolTableReader {
    TfAllocator allocator;
    unsigned char *json;
    size_t json_len;
    size_t at;
    bool has_first_token;
    bool array_done;

    /* Header fields */
    char *source_name;
    bool source_valid;
    size_t source_length;
    unsigned char expected_sha256[32];
    bool has_errors;
    size_t diagnostic_cap;
    size_t expected_diag_count;
    size_t expected_suppressed;
    size_t expected_token_count;

    /* Source error info (only when source_valid == false) */
    TfError source_error_code;
    size_t source_error_start;
    size_t source_error_end;
    unsigned char *raw_bytes;
    size_t raw_length;

    /* Incremental sequential stream state */
    Sha256 sha;
    size_t tokens_read;
    bool saw_eof;
    size_t prev_end;
    size_t cur_scalar;
    size_t cur_line;
    size_t cur_col;
    size_t cur_disp;

    unsigned char *reconstructed;
    size_t reconstructed_len;
    size_t reconstructed_cap;

    TfToken *tokens;
    size_t token_cap;
    TfDiagnostic *diagnostics;
    size_t diag_count;
    size_t diag_cap;
    size_t suppressed;

    char error_message[256];
};

static void set_err(TfSymbolTableReader *r, const char *msg) {
    if (r->error_message[0] == '\0') {
        (void)snprintf(r->error_message, sizeof(r->error_message), "%s", msg);
    }
}

static int jpeek(const TfSymbolTableReader *r) {
    return r->at < r->json_len ? (unsigned char)r->json[r->at] : -1;
}

static void skip_ws(TfSymbolTableReader *r) {
    for (;;) {
        int c = jpeek(r);
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') ++r->at;
        else break;
    }
}

static bool expect_ch(TfSymbolTableReader *r, char ch, const char *msg) {
    skip_ws(r);
    if (jpeek(r) != (unsigned char)ch) {
        set_err(r, msg);
        return false;
    }
    ++r->at;
    return true;
}

static int hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool read_hex4(TfSymbolTableReader *r, uint32_t *out) {
    uint32_t v = 0;
    unsigned i;
    for (i = 0; i < 4u; ++i) {
        int h = hex_val(jpeek(r));
        if (h < 0) return false;
        v = (v << 4) | (uint32_t)h;
        ++r->at;
    }
    *out = v;
    return true;
}

static size_t encode_utf8(uint32_t u, unsigned char out[4]) {
    if (u < 0x80u) {
        out[0] = (unsigned char)u;
        return 1;
    }
    if (u < 0x800u) {
        out[0] = (unsigned char)(0xc0u | (u >> 6));
        out[1] = (unsigned char)(0x80u | (u & 0x3fu));
        return 2;
    }
    if (u < 0x10000u) {
        out[0] = (unsigned char)(0xe0u | (u >> 12));
        out[1] = (unsigned char)(0x80u | ((u >> 6) & 0x3fu));
        out[2] = (unsigned char)(0x80u | (u & 0x3fu));
        return 3;
    }
    out[0] = (unsigned char)(0xf0u | (u >> 18));
    out[1] = (unsigned char)(0x80u | ((u >> 12) & 0x3fu));
    out[2] = (unsigned char)(0x80u | ((u >> 6) & 0x3fu));
    out[3] = (unsigned char)(0x80u | (u & 0x3fu));
    return 4;
}

/* Parses a strict JSON string into an allocated UTF-8 buffer (+ NUL sentinel).
 * Returns TF_OK, TF_SOURCE_ERROR (bad JSON/UTF-8), or TF_HOST_ERROR (OOM). */
static TfStatus parse_json_string(TfSymbolTableReader *r, unsigned char **out_buf,
                                  size_t *out_len) {
    size_t cap = 64u, len = 0;
    unsigned char *buf;
    skip_ws(r);
    if (jpeek(r) != '"') {
        set_err(r, "expected JSON string");
        return TF_SOURCE_ERROR;
    }
    ++r->at;
    buf = r->allocator.allocate(r->allocator.context, cap);
    if (buf == NULL) return TF_HOST_ERROR;

    while (jpeek(r) != '"') {
        int c = jpeek(r);
        unsigned char bytes[4];
        size_t used;
        if (c < 0 || c < 0x20) {
            r->allocator.deallocate(r->allocator.context, buf);
            set_err(r, "unterminated or unescaped control in JSON string");
            return TF_SOURCE_ERROR;
        }
        if (c == '\\') {
            uint32_t u;
            ++r->at;
            c = jpeek(r);
            if (c < 0) {
                r->allocator.deallocate(r->allocator.context, buf);
                set_err(r, "truncated escape in JSON string");
                return TF_SOURCE_ERROR;
            }
            ++r->at;
            switch (c) {
                case '"': case '\\': case '/': u = (uint32_t)c; break;
                case 'b': u = 8u; break;
                case 'f': u = 12u; break;
                case 'n': u = 10u; break;
                case 'r': u = 13u; break;
                case 't': u = 9u; break;
                case 'u':
                    if (!read_hex4(r, &u)) {
                        r->allocator.deallocate(r->allocator.context, buf);
                        set_err(r, "invalid \\uXXXX escape in JSON string");
                        return TF_SOURCE_ERROR;
                    }
                    if (u >= 0xd800u && u <= 0xdbffu) {
                        uint32_t low;
                        if (jpeek(r) != '\\' || r->at + 1 >= r->json_len || r->json[r->at + 1] != 'u') {
                            r->allocator.deallocate(r->allocator.context, buf);
                            set_err(r, "unpaired high surrogate in JSON string");
                            return TF_SOURCE_ERROR;
                        }
                        r->at += 2;
                        if (!read_hex4(r, &low) || low < 0xdc00u || low > 0xdfffu) {
                            r->allocator.deallocate(r->allocator.context, buf);
                            set_err(r, "invalid low surrogate in JSON string");
                            return TF_SOURCE_ERROR;
                        }
                        u = 0x10000u + (u - 0xd800u) * 1024u + (low - 0xdc00u);
                    } else if (u >= 0xdc00u && u <= 0xdfffu) {
                        r->allocator.deallocate(r->allocator.context, buf);
                        set_err(r, "unpaired low surrogate in JSON string");
                        return TF_SOURCE_ERROR;
                    }
                    break;
                default:
                    r->allocator.deallocate(r->allocator.context, buf);
                    set_err(r, "unsupported JSON string escape");
                    return TF_SOURCE_ERROR;
            }
            used = encode_utf8(u, bytes);
        } else if (c >= 0x80) {
            uint32_t scalar;
            size_t end;
            if (!tf_decode(r->json, r->json_len, r->at, &scalar, &end)) {
                r->allocator.deallocate(r->allocator.context, buf);
                set_err(r, "invalid UTF-8 sequence in JSON string");
                return TF_SOURCE_ERROR;
            }
            used = end - r->at;
            memcpy(bytes, r->json + r->at, used);
            r->at = end;
        } else {
            bytes[0] = (unsigned char)c;
            used = 1u;
            ++r->at;
        }
        if (len > SIZE_MAX - used - 1u) {
            r->allocator.deallocate(r->allocator.context, buf);
            return TF_HOST_ERROR;
        }
        if (len + used + 1u > cap) {
            void *storage = buf;
            if (!tf_grow(&storage, &cap, len + used + 1u, 1u, r->allocator)) {
                r->allocator.deallocate(r->allocator.context, buf);
                return TF_HOST_ERROR;
            }
            buf = storage;
        }
        memcpy(buf + len, bytes, used);
        len += used;
    }
    ++r->at;
    buf[len] = '\0';
    *out_buf = buf;
    *out_len = len;
    return TF_OK;
}

static bool parse_json_uint(TfSymbolTableReader *r, size_t *out) {
    size_t val = 0;
    int c;
    skip_ws(r);
    c = jpeek(r);
    if (c == '0') {
        ++r->at;
        c = jpeek(r);
        if (c >= '0' && c <= '9') {
            set_err(r, "invalid leading zero in integer");
            return false;
        }
        *out = 0;
        return true;
    }
    if (c < '1' || c > '9') {
        set_err(r, "expected non-negative JSON integer");
        return false;
    }
    while ((c = jpeek(r)) >= '0' && c <= '9') {
        size_t d = (size_t)(c - '0');
        if (val > (SIZE_MAX - d) / 10u) {
            set_err(r, "integer overflow in symbol table");
            return false;
        }
        val = val * 10u + d;
        ++r->at;
    }
    *out = val;
    return true;
}

static bool parse_json_bool(TfSymbolTableReader *r, bool *out) {
    skip_ws(r);
    if (r->json_len - r->at >= 4u && memcmp(r->json + r->at, "true", 4u) == 0) {
        r->at += 4u;
        *out = true;
        return true;
    }
    if (r->json_len - r->at >= 5u && memcmp(r->json + r->at, "false", 5u) == 0) {
        r->at += 5u;
        *out = false;
        return true;
    }
    set_err(r, "expected JSON boolean");
    return false;
}

static bool try_consume_null(TfSymbolTableReader *r) {
    skip_ws(r);
    if (r->json_len - r->at >= 4u && memcmp(r->json + r->at, "null", 4u) == 0) {
        r->at += 4u;
        return true;
    }
    return false;
}

static bool key_equals(const unsigned char *k, size_t klen, const char *lit) {
    size_t n = strlen(lit);
    return klen == n && memcmp(k, lit, n) == 0;
}

static bool parse_location_obj(TfSymbolTableReader *r, TfLocation *out) {
    unsigned seen = 0;
    *out = (TfLocation){0};
    if (!expect_ch(r, '{', "expected coordinate object")) return false;
    skip_ws(r);
    if (jpeek(r) == '}') {
        set_err(r, "empty coordinate object");
        return false;
    }
    for (;;) {
        unsigned char *key = NULL;
        size_t klen = 0, val = 0;
        TfStatus st = parse_json_string(r, &key, &klen);
        unsigned bit = 0;
        if (st != TF_OK) return false;
        if (!expect_ch(r, ':', "expected ':' in coordinate object")) {
            r->allocator.deallocate(r->allocator.context, key);
            return false;
        }
        if (!parse_json_uint(r, &val)) {
            r->allocator.deallocate(r->allocator.context, key);
            return false;
        }
        if (key_equals(key, klen, "scalar")) { bit = 1u; out->scalar = val; }
        else if (key_equals(key, klen, "line")) { bit = 2u; out->line = val; }
        else if (key_equals(key, klen, "column")) { bit = 4u; out->column = val; }
        else if (key_equals(key, klen, "display_column")) { bit = 8u; out->display_column = val; }
        else {
            r->allocator.deallocate(r->allocator.context, key);
            set_err(r, "unexpected field in coordinate object");
            return false;
        }
        r->allocator.deallocate(r->allocator.context, key);
        if ((seen & bit) != 0u) {
            set_err(r, "duplicate field in coordinate object");
            return false;
        }
        seen |= bit;
        skip_ws(r);
        if (jpeek(r) == '}') { ++r->at; break; }
        if (jpeek(r) != ',') {
            set_err(r, "expected ',' or '}' in coordinate object");
            return false;
        }
        ++r->at;
    }
    if (seen != 0x0fu) {
        set_err(r, "missing coordinate fields");
        return false;
    }
    return true;
}

static bool lookup_token_kind(const unsigned char *s, size_t len, TfTokenKind *out) {
    int k;
    for (k = (int)TF_IDENT; k <= (int)TF_COMMENT_BLOCK; ++k) {
        const char *name = tf_token_name((TfTokenKind)k);
        size_t n = strlen(name);
        if (n == len && memcmp(s, name, n) == 0) {
            *out = (TfTokenKind)k;
            return true;
        }
    }
    return false;
}

static bool lookup_error_code(const unsigned char *s, size_t len, TfError *out, bool source_err) {
    static const TfError src_codes[] = {TF_E001, TF_E002, TF_E003};
    static const TfError lex_codes[] = {TF_E101, TF_E102, TF_E103, TF_E104, TF_E105};
    const TfError *list = source_err ? src_codes : lex_codes;
    size_t count = source_err ? 3u : 5u;
    size_t i;
    for (i = 0; i < count; ++i) {
        const char *code = tf_error_code(list[i]);
        if (len == 4u && memcmp(s, code, 4u) == 0) {
            *out = list[i];
            return true;
        }
    }
    return false;
}

/* Validates that every byte of a whitespace gap is legal Ternfold whitespace
 * (no lone CR, no non-whitespace bytes) and advances live coordinates. */
static bool step_coordinates(const unsigned char *bytes, size_t len, bool require_ws,
                             size_t *byte_pos, size_t *scalar, size_t *line,
                             size_t *col, size_t *disp) {
    size_t at = 0;
    while (at < len) {
        uint32_t u;
        size_t end;
        if (!tf_decode(bytes, len, at, &u, &end)) return false;
        if (require_ws) {
            if (u != ' ' && u != '\t' && u != '\n' && u != '\r') return false;
            if (u == '\r' && (at + 1u >= len || bytes[at + 1u] != '\n')) return false;
        } else if (u == '\r' && (at + 1u >= len || bytes[at + 1u] != '\n')) {
            return false;
        }
        *byte_pos += end - at;
        ++(*scalar);
        if (u == '\n') {
            ++(*line);
            *col = 1u;
            *disp = 1u;
        } else if (u == '\t') {
            size_t step = 4u - (*disp - 1u) % 4u;
            *col += 1u;
            *disp += step;
        } else {
            *col += 1u;
            *disp += 1u;
        }
        at = end;
    }
    return true;
}

/* Cross-checks that the serialized token classification matches its exact lexeme,
 * enforcing keyword/symbol/name DFA consistency one token at a time. */
static bool token_matches_lexeme(TfTokenKind kind, bool reserved, const unsigned char *lex,
                                 size_t len, TfError err) {
    static const char *const fixed_spellings[] = {
        /* TF_UNDERSCORE */ "_", NULL, NULL,
        /* TF_AS..TF_YIELDS */
        "as", "div", "else", "emit", "enum", "false", "fn", "for", "from", "guard",
        "if", "import", "in", "input", "let", "machine", "match", "next", "output",
        "pub", "record", "rejects", "state", "stdin", "stdout", "to", "true", "yields",
        /* TF_PLUS..TF_BLOCK_CLOSE */
        "+", "-", "*", "/", "%", "^", "<", ">", "=", "!", ".", ",", ";", ":",
        "(", ")", "[", "]", "{", "}", "::", "->", "=>", "==", "!=", "<=", ">=",
        "&&", "||", "++", "*/"
    };
    if (reserved && kind != TF_IDENT) return false;
    if (kind == TF_EOF) return len == 0u && err == TF_E_NONE;
    if (len == 0u) return false;
    if (kind == TF_INVALID) return err >= TF_E101 && err <= TF_E105;
    if (err != TF_E_NONE) return false;
    if (kind == TF_UNDERSCORE) return len == 1u && lex[0] == '_';
    if (kind >= TF_AS && kind <= TF_BLOCK_CLOSE) {
        const char *expected = fixed_spellings[(size_t)(kind - TF_AS) + 3u];
        return len == strlen(expected) && memcmp(lex, expected, len) == 0;
    }
    if (kind == TF_IDENT) {
        size_t state = 0, i;
        TfNameDfaState info;
        if (!((lex[0] >= 'a' && lex[0] <= 'z') || (lex[0] >= 'A' && lex[0] <= 'Z') || lex[0] == '_')) {
            return false;
        }
        for (i = 0; i < len; ++i) {
            unsigned char c = lex[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
                return false;
            }
            if (state != SIZE_MAX) state = tf_name_dfa_step(state, c);
        }
        if (state == SIZE_MAX) return !reserved;
        if (!tf_name_dfa_state(state, &info)) return false;
        if (info.terminal) return info.kind == TF_IDENT && info.reserved == reserved;
        return !reserved;
    }
    if (kind == TF_INT || kind == TF_FLOAT) return lex[0] >= '0' && lex[0] <= '9';
    if (kind == TF_TEXT) return len >= 2u && lex[0] == '"' && lex[len - 1u] == '"';
    if (kind == TF_CHAR) return len >= 2u && lex[0] == '\'' && lex[len - 1u] == '\'';
    if (kind == TF_COMMENT_LINE) return len >= 2u && lex[0] == '/' && lex[1] == '/';
    if (kind == TF_COMMENT_BLOCK) {
        return len >= 4u && lex[0] == '/' && lex[1] == '*' && lex[len - 2u] == '*' && lex[len - 1u] == '/';
    }
    return false;
}

static bool parse_source_error_obj(TfSymbolTableReader *r) {
    unsigned seen = 0;
    if (!expect_ch(r, '{', "expected source_error object")) return false;
    skip_ws(r);
    if (jpeek(r) == '}') {
        set_err(r, "empty source_error object");
        return false;
    }
    for (;;) {
        unsigned char *key = NULL;
        size_t klen = 0;
        if (parse_json_string(r, &key, &klen) != TF_OK) return false;
        if (!expect_ch(r, ':', "expected ':' in source_error")) {
            r->allocator.deallocate(r->allocator.context, key);
            return false;
        }
        if (key_equals(key, klen, "code")) {
            unsigned char *val = NULL;
            size_t vlen = 0;
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & 1u) != 0u) { set_err(r, "duplicate code in source_error"); return false; }
            if (parse_json_string(r, &val, &vlen) != TF_OK) return false;
            if (!lookup_error_code(val, vlen, &r->source_error_code, true)) {
                r->allocator.deallocate(r->allocator.context, val);
                set_err(r, "invalid code in source_error");
                return false;
            }
            r->allocator.deallocate(r->allocator.context, val);
            seen |= 1u;
        } else if (key_equals(key, klen, "start_byte")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & 2u) != 0u || !parse_json_uint(r, &r->source_error_start)) {
                set_err(r, "invalid start_byte in source_error");
                return false;
            }
            seen |= 2u;
        } else if (key_equals(key, klen, "end_byte")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & 4u) != 0u || !parse_json_uint(r, &r->source_error_end)) {
                set_err(r, "invalid end_byte in source_error");
                return false;
            }
            seen |= 4u;
        } else {
            r->allocator.deallocate(r->allocator.context, key);
            set_err(r, "unexpected key in source_error");
            return false;
        }
        skip_ws(r);
        if (jpeek(r) == '}') { ++r->at; break; }
        if (jpeek(r) != ',') {
            set_err(r, "expected ',' or '}' in source_error");
            return false;
        }
        ++r->at;
    }
    if (seen != 7u) {
        set_err(r, "missing field in source_error");
        return false;
    }
    return true;
}

static TfStatus parse_header(TfSymbolTableReader *r) {
    enum {
        F_FORMAT = 1u << 0,
        F_SCHEMA = 1u << 1,
        F_NAME = 1u << 2,
        F_VALID = 1u << 3,
        F_LEN = 1u << 4,
        F_SHA = 1u << 5,
        F_SRC_ERR = 1u << 6,
        F_RAW = 1u << 7,
        F_HAS_ERR = 1u << 8,
        F_DIAG_CAP = 1u << 9,
        F_DIAG_CNT = 1u << 10,
        F_SUPP = 1u << 11,
        F_TOK_CNT = 1u << 12,
        F_TOKENS = 1u << 13,
        F_ALL = (1u << 14) - 1u
    };
    unsigned seen = 0;
    bool src_err_is_null = false, raw_is_null = false;

    if (!expect_ch(r, '{', "symbol table must begin with '{'")) return TF_SOURCE_ERROR;
    for (;;) {
        unsigned char *key = NULL;
        size_t klen = 0;
        TfStatus st;
        if ((seen & F_TOKENS) != 0u) {
            set_err(r, "tokens must be last header field in sequential stream");
            return TF_SOURCE_ERROR;
        }
        st = parse_json_string(r, &key, &klen);
        if (st != TF_OK) return st;
        if (!expect_ch(r, ':', "expected ':' after header field name")) {
            r->allocator.deallocate(r->allocator.context, key);
            return TF_SOURCE_ERROR;
        }
        if (key_equals(key, klen, "format")) {
            unsigned char *val = NULL;
            size_t vlen = 0;
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_FORMAT) != 0u) { set_err(r, "duplicate format"); return TF_SOURCE_ERROR; }
            st = parse_json_string(r, &val, &vlen);
            if (st != TF_OK) return st;
            if (!key_equals(val, vlen, "ternfold-symbol-table")) {
                r->allocator.deallocate(r->allocator.context, val);
                set_err(r, "unexpected symbol table format");
                return TF_SOURCE_ERROR;
            }
            r->allocator.deallocate(r->allocator.context, val);
            seen |= F_FORMAT;
        } else if (key_equals(key, klen, "schema_version")) {
            size_t v = 0;
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_SCHEMA) != 0u || !parse_json_uint(r, &v) || v != 1u) {
                set_err(r, "unsupported or invalid schema_version");
                return TF_SOURCE_ERROR;
            }
            seen |= F_SCHEMA;
        } else if (key_equals(key, klen, "source_name")) {
            unsigned char *val = NULL;
            size_t vlen = 0;
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_NAME) != 0u) { set_err(r, "duplicate source_name"); return TF_SOURCE_ERROR; }
            st = parse_json_string(r, &val, &vlen);
            if (st != TF_OK) return st;
            if (vlen == 0u || memchr(val, '\0', vlen) != NULL) {
                r->allocator.deallocate(r->allocator.context, val);
                set_err(r, "invalid source_name");
                return TF_SOURCE_ERROR;
            }
            r->source_name = (char *)val;
            seen |= F_NAME;
        } else if (key_equals(key, klen, "source_valid")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_VALID) != 0u || !parse_json_bool(r, &r->source_valid)) {
                set_err(r, "invalid source_valid");
                return TF_SOURCE_ERROR;
            }
            seen |= F_VALID;
        } else if (key_equals(key, klen, "source_length")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_LEN) != 0u || !parse_json_uint(r, &r->source_length)) {
                set_err(r, "invalid source_length");
                return TF_SOURCE_ERROR;
            }
            seen |= F_LEN;
        } else if (key_equals(key, klen, "source_sha256")) {
            unsigned char *val = NULL;
            size_t vlen = 0, i;
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_SHA) != 0u) { set_err(r, "duplicate source_sha256"); return TF_SOURCE_ERROR; }
            st = parse_json_string(r, &val, &vlen);
            if (st != TF_OK) return st;
            if (vlen != 64u) {
                r->allocator.deallocate(r->allocator.context, val);
                set_err(r, "source_sha256 must be 64 hex chars");
                return TF_SOURCE_ERROR;
            }
            for (i = 0; i < 32u; ++i) {
                unsigned char c0 = val[2 * i], c1 = val[2 * i + 1];
                int hi = (c0 >= '0' && c0 <= '9') ? (c0 - '0') : (c0 >= 'a' && c0 <= 'f') ? (c0 - 'a' + 10) : -1;
                int lo = (c1 >= '0' && c1 <= '9') ? (c1 - '0') : (c1 >= 'a' && c1 <= 'f') ? (c1 - 'a' + 10) : -1;
                if (hi < 0 || lo < 0) {
                    r->allocator.deallocate(r->allocator.context, val);
                    set_err(r, "invalid hex character in source_sha256");
                    return TF_SOURCE_ERROR;
                }
                r->expected_sha256[i] = (unsigned char)((hi << 4) | lo);
            }
            r->allocator.deallocate(r->allocator.context, val);
            seen |= F_SHA;
        } else if (key_equals(key, klen, "source_error")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_SRC_ERR) != 0u) { set_err(r, "duplicate source_error"); return TF_SOURCE_ERROR; }
            if (try_consume_null(r)) src_err_is_null = true;
            else if (!parse_source_error_obj(r)) return TF_SOURCE_ERROR;
            seen |= F_SRC_ERR;
        } else if (key_equals(key, klen, "raw_hex")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_RAW) != 0u) { set_err(r, "duplicate raw_hex"); return TF_SOURCE_ERROR; }
            if (try_consume_null(r)) raw_is_null = true;
            else {
                unsigned char *hex = NULL;
                size_t hlen = 0, i;
                st = parse_json_string(r, &hex, &hlen);
                if (st != TF_OK) return st;
                if ((hlen % 2u) != 0u) {
                    r->allocator.deallocate(r->allocator.context, hex);
                    set_err(r, "raw_hex length must be even");
                    return TF_SOURCE_ERROR;
                }
                r->raw_length = hlen / 2u;
                r->raw_bytes = r->allocator.allocate(r->allocator.context, r->raw_length == 0u ? 1u : r->raw_length);
                if (r->raw_bytes == NULL) {
                    r->allocator.deallocate(r->allocator.context, hex);
                    return TF_HOST_ERROR;
                }
                for (i = 0; i < r->raw_length; ++i) {
                    int hi = hex_val(hex[2 * i]), lo = hex_val(hex[2 * i + 1]);
                    if (hi < 0 || lo < 0) {
                        r->allocator.deallocate(r->allocator.context, hex);
                        set_err(r, "invalid byte in raw_hex");
                        return TF_SOURCE_ERROR;
                    }
                    r->raw_bytes[i] = (unsigned char)((hi << 4) | lo);
                }
                r->allocator.deallocate(r->allocator.context, hex);
            }
            seen |= F_RAW;
        } else if (key_equals(key, klen, "has_errors")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_HAS_ERR) != 0u || !parse_json_bool(r, &r->has_errors)) {
                set_err(r, "invalid has_errors");
                return TF_SOURCE_ERROR;
            }
            seen |= F_HAS_ERR;
        } else if (key_equals(key, klen, "diagnostic_cap")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_DIAG_CAP) != 0u || !parse_json_uint(r, &r->diagnostic_cap) ||
                r->diagnostic_cap > 100u) {
                set_err(r, "invalid diagnostic_cap");
                return TF_SOURCE_ERROR;
            }
            seen |= F_DIAG_CAP;
        } else if (key_equals(key, klen, "diagnostic_count")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_DIAG_CNT) != 0u || !parse_json_uint(r, &r->expected_diag_count)) {
                set_err(r, "invalid diagnostic_count");
                return TF_SOURCE_ERROR;
            }
            seen |= F_DIAG_CNT;
        } else if (key_equals(key, klen, "suppressed_diagnostics")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_SUPP) != 0u || !parse_json_uint(r, &r->expected_suppressed)) {
                set_err(r, "invalid suppressed_diagnostics");
                return TF_SOURCE_ERROR;
            }
            seen |= F_SUPP;
        } else if (key_equals(key, klen, "token_count")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & F_TOK_CNT) != 0u || !parse_json_uint(r, &r->expected_token_count)) {
                set_err(r, "invalid token_count");
                return TF_SOURCE_ERROR;
            }
            seen |= F_TOK_CNT;
        } else if (key_equals(key, klen, "tokens")) {
            r->allocator.deallocate(r->allocator.context, key);
            if ((seen & (F_ALL ^ F_TOKENS)) != (F_ALL ^ F_TOKENS)) {
                set_err(r, "missing required symbol table header field before tokens");
                return TF_SOURCE_ERROR;
            }
            if (!expect_ch(r, '[', "tokens must be a JSON array")) return TF_SOURCE_ERROR;
            seen |= F_TOKENS;
            break;
        } else {
            r->allocator.deallocate(r->allocator.context, key);
            set_err(r, "unknown symbol table header field");
            return TF_SOURCE_ERROR;
        }
        if (!expect_ch(r, ',', "expected ',' between header fields")) return TF_SOURCE_ERROR;
    }

    if (r->expected_diag_count > r->diagnostic_cap) {
        set_err(r, "diagnostic_count exceeds diagnostic_cap");
        return TF_SOURCE_ERROR;
    }
    if (!r->has_errors && (r->expected_diag_count != 0u || r->expected_suppressed != 0u)) {
        set_err(r, "has_errors=false conflicts with diagnostics");
        return TF_SOURCE_ERROR;
    }
    if (r->source_valid) {
        if (!src_err_is_null || !raw_is_null || r->expected_token_count == 0u) {
            set_err(r, "valid source table cannot have source_error/raw_hex/zero tokens");
            return TF_SOURCE_ERROR;
        }
        r->has_first_token = true;
    } else {
        Sha256 raw_sha;
        unsigned char raw_digest[32];
        if (src_err_is_null || raw_is_null || !r->has_errors || r->expected_token_count != 0u ||
            r->raw_length != r->source_length ||
            r->source_error_start > r->source_error_end || r->source_error_end > r->source_length) {
            set_err(r, "invalid metadata on source-invalid symbol table");
            return TF_SOURCE_ERROR;
        }
        sha_init(&raw_sha);
        sha_update(&raw_sha, r->raw_bytes, r->raw_length);
        if (!sha_final(&raw_sha, raw_digest) ||
            memcmp(raw_digest, r->expected_sha256, 32u) != 0) {
            set_err(r, "SHA-256 mismatch on invalid-source raw bytes");
            return TF_SOURCE_ERROR;
        }
        skip_ws(r);
        if (jpeek(r) != ']') {
            set_err(r, "source-invalid symbol table must have empty tokens array");
            return TF_SOURCE_ERROR;
        }
        ++r->at;
        r->array_done = true;
        if (!expect_ch(r, '}', "expected closing '}' on symbol table")) return TF_SOURCE_ERROR;
        skip_ws(r);
        if (r->at != r->json_len) {
            set_err(r, "trailing content after symbol table root object");
            return TF_SOURCE_ERROR;
        }
        return TF_SOURCE_ERROR;
    }
    return TF_OK;
}

static TfStatus open_bytes_impl(TfSymbolTableReader **out, const void *bytes,
                                size_t length, const char *logical_path,
                                const TfAllocator *allocator,
                                char *error_buffer, size_t error_buffer_size) {
    TfAllocator a = tf_allocator(allocator);
    TfSymbolTableReader *r;
    TfStatus st;
    (void)logical_path;
    if (error_buffer != NULL && error_buffer_size != 0u) error_buffer[0] = '\0';
    if (out == NULL) return TF_HOST_ERROR;
    *out = NULL;
    if (!tf_allocator_valid(a) || (bytes == NULL && length != 0u) || length > ST_MAX_FILE_BYTES) {
        if (error_buffer != NULL && error_buffer_size != 0u) {
            (void)snprintf(error_buffer, error_buffer_size, "invalid buffer arguments or size limit exceeded");
        }
        return TF_HOST_ERROR;
    }
    r = a.allocate(a.context, sizeof(*r));
    if (r == NULL) return TF_HOST_ERROR;
    *r = (TfSymbolTableReader){0};
    r->allocator = a;
    r->json = a.allocate(a.context, length == 0u ? 1u : length);
    if (r->json == NULL) {
        a.deallocate(a.context, r);
        return TF_HOST_ERROR;
    }
    if (length != 0u) memcpy(r->json, bytes, length);
    r->json_len = length;
    sha_init(&r->sha);
    r->cur_line = 1u;
    r->cur_col = 1u;
    r->cur_disp = 1u;

    st = parse_header(r);
    if (st == TF_HOST_ERROR) {
        tf_symbol_table_reader_destroy(r);
        return TF_HOST_ERROR;
    }
    if (st == TF_SOURCE_ERROR) {
        if (!r->source_valid && r->array_done) {
            *out = r;
            return TF_SOURCE_ERROR;
        }
        if (error_buffer != NULL && error_buffer_size != 0u) {
            (void)snprintf(error_buffer, error_buffer_size, "%s",
                           r->error_message[0] != '\0' ? r->error_message : "malformed symbol table header");
        }
        tf_symbol_table_reader_destroy(r);
        return TF_SOURCE_ERROR;
    }
    *out = r;
    return TF_OK;
}

TfStatus tf_symbol_table_reader_open_bytes(TfSymbolTableReader **out, const void *bytes,
                                           size_t length, const char *logical_path,
                                           const TfAllocator *allocator) {
    return open_bytes_impl(out, bytes, length, logical_path, allocator, NULL, 0u);
}

static TfStatus open_file_impl(TfSymbolTableReader **out, const char *path,
                               const TfAllocator *allocator,
                               char *error_buffer, size_t error_buffer_size) {
    TfAllocator a = tf_allocator(allocator);
    FILE *f;
    void *data = NULL;
    size_t len = 0, cap = 0;
    bool read_ok = false;
    TfStatus st;

    if (error_buffer != NULL && error_buffer_size != 0u) error_buffer[0] = '\0';
    if (out == NULL) return TF_HOST_ERROR;
    *out = NULL;
    if (path == NULL || path[0] == '\0' || !tf_allocator_valid(a)) return TF_HOST_ERROR;
    f = fopen(path, "rb");
    if (f == NULL) {
        if (error_buffer != NULL && error_buffer_size != 0u) {
            (void)snprintf(error_buffer, error_buffer_size, "cannot open symbol table file");
        }
        return TF_HOST_ERROR;
    }
    for (;;) {
        size_t got;
        if (len > ST_MAX_FILE_BYTES) break;
        if (len + 4096u > cap && !tf_grow(&data, &cap, len + 4096u, 1u, a)) break;
        got = fread((unsigned char *)data + len, 1u, cap - len, f);
        len += got;
        if (len > ST_MAX_FILE_BYTES || ferror(f)) break;
        if (feof(f)) { read_ok = true; break; }
        if (got == 0u) break;
    }
    if (fclose(f) != 0) read_ok = false;
    if (!read_ok) {
        if (data != NULL) a.deallocate(a.context, data);
        if (error_buffer != NULL && error_buffer_size != 0u) {
            (void)snprintf(error_buffer, error_buffer_size, "failed reading symbol table file or size exceeds limit");
        }
        return TF_HOST_ERROR;
    }
    st = open_bytes_impl(out, data, len, path, &a, error_buffer, error_buffer_size);
    if (data != NULL) a.deallocate(a.context, data);
    return st;
}

TfStatus tf_symbol_table_reader_open(TfSymbolTableReader **out, const char *path,
                                     const TfAllocator *allocator) {
    return open_file_impl(out, path, allocator, NULL, 0u);
}

void tf_symbol_table_entry_destroy(TfSymbolTableReader *reader, TfSymbolTableEntry *entry) {
    if (reader == NULL || entry == NULL) return;
    if (entry->gap != NULL) {
        reader->allocator.deallocate(reader->allocator.context, (void *)entry->gap);
    }
    if (entry->token.value != NULL) {
        reader->allocator.deallocate(reader->allocator.context, (void *)entry->token.value);
    }
    *entry = (TfSymbolTableEntry){0};
}

TfStatus tf_symbol_table_next(TfSymbolTableReader *reader, TfSymbolTableEntry *out,
                              bool *has_token) {
    enum {
        T_INDEX = 1u << 0,
        T_GAP = 1u << 1,
        T_KIND = 1u << 2,
        T_RESERVED = 1u << 3,
        T_LEXEME = 1u << 4,
        T_PAYLOAD = 1u << 5,
        T_PAYLOAD_BYTES = 1u << 6,
        T_START_BYTE = 1u << 7,
        T_END_BYTE = 1u << 8,
        T_START = 1u << 9,
        T_END = 1u << 10,
        T_ERROR = 1u << 11,
        T_ALL = (1u << 12) - 1u
    };
    unsigned seen = 0;
    size_t idx = 0, payload_bytes = 0, start_byte = 0, end_byte = 0;
    unsigned char *gap = NULL, *lexeme = NULL, *payload_buf = NULL;
    size_t gap_len = 0, lexeme_len = 0, payload_len = 0;
    bool payload_is_null = false, payload_is_bool = false, payload_bool = false;
    bool reserved = false;
    TfTokenKind tk = TF_IDENT;
    TfError err = TF_E_NONE;
    TfLocation start_loc = {0}, end_loc = {0};
    TfStatus st = TF_SOURCE_ERROR;
    unsigned char *stored_text_val = NULL;
    unsigned char *entry_val = NULL, *entry_gap = NULL;
    void *storage;

    if (reader == NULL || out == NULL || has_token == NULL) return TF_HOST_ERROR;
    *out = (TfSymbolTableEntry){0};
    *has_token = false;
    if (!reader->source_valid) return TF_SOURCE_ERROR;
    if (reader->array_done) return TF_OK;

    skip_ws(reader);
    if (jpeek(reader) == ']') {
        ++reader->at;
        reader->array_done = true;
        return TF_OK;
    }
    if (!reader->has_first_token) {
        if (!expect_ch(reader, ',', "expected ',' or ']' after token entry")) return TF_SOURCE_ERROR;
        skip_ws(reader);
    }
    reader->has_first_token = false;

    if (!expect_ch(reader, '{', "expected token object")) return TF_SOURCE_ERROR;
    for (;;) {
        unsigned char *key = NULL;
        size_t klen = 0;
        st = parse_json_string(reader, &key, &klen);
        if (st != TF_OK) goto fail;
        if (!expect_ch(reader, ':', "expected ':' in token object")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            st = TF_SOURCE_ERROR;
            goto fail;
        }
        if (key_equals(key, klen, "index")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_INDEX) != 0u || !parse_json_uint(reader, &idx)) {
                set_err(reader, "invalid token index"); st = TF_SOURCE_ERROR; goto fail;
            }
            seen |= T_INDEX;
        } else if (key_equals(key, klen, "gap")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_GAP) != 0u) { set_err(reader, "duplicate gap"); st = TF_SOURCE_ERROR; goto fail; }
            st = parse_json_string(reader, &gap, &gap_len);
            if (st != TF_OK) goto fail;
            seen |= T_GAP;
        } else if (key_equals(key, klen, "kind")) {
            unsigned char *kstr = NULL;
            size_t kslen = 0;
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_KIND) != 0u) { set_err(reader, "duplicate kind"); st = TF_SOURCE_ERROR; goto fail; }
            st = parse_json_string(reader, &kstr, &kslen);
            if (st != TF_OK) goto fail;
            if (!lookup_token_kind(kstr, kslen, &tk)) {
                reader->allocator.deallocate(reader->allocator.context, kstr);
                set_err(reader, "unknown token kind in symbol table");
                st = TF_SOURCE_ERROR;
                goto fail;
            }
            reader->allocator.deallocate(reader->allocator.context, kstr);
            seen |= T_KIND;
        } else if (key_equals(key, klen, "reserved")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_RESERVED) != 0u || !parse_json_bool(reader, &reserved)) {
                set_err(reader, "invalid reserved flag"); st = TF_SOURCE_ERROR; goto fail;
            }
            seen |= T_RESERVED;
        } else if (key_equals(key, klen, "lexeme")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_LEXEME) != 0u) { set_err(reader, "duplicate lexeme"); st = TF_SOURCE_ERROR; goto fail; }
            st = parse_json_string(reader, &lexeme, &lexeme_len);
            if (st != TF_OK) goto fail;
            seen |= T_LEXEME;
        } else if (key_equals(key, klen, "payload")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_PAYLOAD) != 0u) { set_err(reader, "duplicate payload"); st = TF_SOURCE_ERROR; goto fail; }
            if (try_consume_null(reader)) {
                payload_is_null = true;
            } else {
                skip_ws(reader);
                if (jpeek(reader) == '"') {
                    st = parse_json_string(reader, &payload_buf, &payload_len);
                    if (st != TF_OK) goto fail;
                } else {
                    if (!parse_json_bool(reader, &payload_bool)) {
                        set_err(reader, "payload must be string, bool, or null");
                        st = TF_SOURCE_ERROR;
                        goto fail;
                    }
                    payload_is_bool = true;
                }
            }
            seen |= T_PAYLOAD;
        } else if (key_equals(key, klen, "payload_bytes")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_PAYLOAD_BYTES) != 0u || !parse_json_uint(reader, &payload_bytes)) {
                set_err(reader, "invalid payload_bytes"); st = TF_SOURCE_ERROR; goto fail;
            }
            seen |= T_PAYLOAD_BYTES;
        } else if (key_equals(key, klen, "start_byte")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_START_BYTE) != 0u || !parse_json_uint(reader, &start_byte)) {
                set_err(reader, "invalid start_byte"); st = TF_SOURCE_ERROR; goto fail;
            }
            seen |= T_START_BYTE;
        } else if (key_equals(key, klen, "end_byte")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_END_BYTE) != 0u || !parse_json_uint(reader, &end_byte)) {
                set_err(reader, "invalid end_byte"); st = TF_SOURCE_ERROR; goto fail;
            }
            seen |= T_END_BYTE;
        } else if (key_equals(key, klen, "start")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_START) != 0u || !parse_location_obj(reader, &start_loc)) {
                st = TF_SOURCE_ERROR; goto fail;
            }
            seen |= T_START;
        } else if (key_equals(key, klen, "end")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_END) != 0u || !parse_location_obj(reader, &end_loc)) {
                st = TF_SOURCE_ERROR; goto fail;
            }
            seen |= T_END;
        } else if (key_equals(key, klen, "error")) {
            reader->allocator.deallocate(reader->allocator.context, key);
            if ((seen & T_ERROR) != 0u) { set_err(reader, "duplicate error"); st = TF_SOURCE_ERROR; goto fail; }
            if (try_consume_null(reader)) {
                err = TF_E_NONE;
            } else {
                unsigned char *estr = NULL;
                size_t elen = 0;
                st = parse_json_string(reader, &estr, &elen);
                if (st != TF_OK) goto fail;
                if (!lookup_error_code(estr, elen, &err, false)) {
                    reader->allocator.deallocate(reader->allocator.context, estr);
                    set_err(reader, "invalid error code in token");
                    st = TF_SOURCE_ERROR;
                    goto fail;
                }
                reader->allocator.deallocate(reader->allocator.context, estr);
            }
            seen |= T_ERROR;
        } else {
            reader->allocator.deallocate(reader->allocator.context, key);
            set_err(reader, "unknown field in token object");
            st = TF_SOURCE_ERROR;
            goto fail;
        }
        skip_ws(reader);
        if (jpeek(reader) == '}') { ++reader->at; break; }
        if (jpeek(reader) != ',') {
            set_err(reader, "expected ',' or '}' in token object");
            st = TF_SOURCE_ERROR;
            goto fail;
        }
        ++reader->at;
    }

    if (seen != T_ALL) {
        set_err(reader, "missing required field in token object");
        st = TF_SOURCE_ERROR;
        goto fail;
    }
    if (reader->saw_eof || reader->tokens_read >= reader->expected_token_count ||
        idx != reader->tokens_read) {
        set_err(reader, "token order or count violation in symbol table");
        st = TF_SOURCE_ERROR;
        goto fail;
    }
    if (start_byte < reader->prev_end || end_byte < start_byte ||
        end_byte > reader->source_length || gap_len != start_byte - reader->prev_end ||
        lexeme_len != end_byte - start_byte) {
        set_err(reader, "token byte span/gap mismatch in symbol table");
        st = TF_SOURCE_ERROR;
        goto fail;
    }
    if (!token_matches_lexeme(tk, reserved, lexeme, lexeme_len, err)) {
        set_err(reader, "token kind/reserved/error conflicts with lexeme");
        st = TF_SOURCE_ERROR;
        goto fail;
    }
    if (tk == TF_EOF && (start_byte != reader->source_length || end_byte != reader->source_length)) {
        set_err(reader, "EOF token span does not match source_length");
        st = TF_SOURCE_ERROR;
        goto fail;
    }

    if (tk == TF_IDENT || tk == TF_INT || tk == TF_FLOAT) {
        if (payload_buf == NULL || payload_len != lexeme_len ||
            payload_bytes != lexeme_len || memcmp(payload_buf, lexeme, lexeme_len) != 0) {
            set_err(reader, "payload mismatch for identifier or numeric token");
            st = TF_SOURCE_ERROR;
            goto fail;
        }
    } else if (tk == TF_TEXT || tk == TF_CHAR) {
        if (payload_buf == NULL || payload_bytes != payload_len) {
            set_err(reader, "payload mismatch for text or char token");
            st = TF_SOURCE_ERROR;
            goto fail;
        }
    } else if (tk == TF_TRUE || tk == TF_FALSE) {
        if (!payload_is_bool || payload_bool != (tk == TF_TRUE) || payload_bytes != 0u) {
            set_err(reader, "boolean payload mismatch");
            st = TF_SOURCE_ERROR;
            goto fail;
        }
    } else if (!payload_is_null || payload_bytes != 0u) {
        set_err(reader, "unexpected non-null payload on operator/keyword/trivia token");
        st = TF_SOURCE_ERROR;
        goto fail;
    }

    /* Pre-grow output buffers and allocate caller entry BEFORE mutating reader state on OOM. */
    if (gap_len + lexeme_len > 0u) {
        size_t add = gap_len + lexeme_len;
        if (reader->reconstructed_len > SIZE_MAX - add) { st = TF_HOST_ERROR; goto fail; }
        if (reader->reconstructed_len + add > reader->reconstructed_cap) {
            storage = reader->reconstructed;
            if (!tf_grow(&storage, &reader->reconstructed_cap,
                         reader->reconstructed_len + add, 1u, reader->allocator)) {
                st = TF_HOST_ERROR;
                goto fail;
            }
            reader->reconstructed = storage;
        }
    }

    storage = reader->tokens;
    if (!tf_grow(&storage, &reader->token_cap, reader->tokens_read + 1u,
                 sizeof(*reader->tokens), reader->allocator)) {
        st = TF_HOST_ERROR;
        goto fail;
    }
    reader->tokens = storage;

    if (err != TF_E_NONE && reader->diag_count < reader->diagnostic_cap) {
        storage = reader->diagnostics;
        if (!tf_grow(&storage, &reader->diag_cap, reader->diag_count + 1u,
                     sizeof(*reader->diagnostics), reader->allocator)) {
            st = TF_HOST_ERROR;
            goto fail;
        }
        reader->diagnostics = storage;
    }

    if (gap_len > 0u) {
        entry_gap = reader->allocator.allocate(reader->allocator.context, gap_len + 1u);
        if (entry_gap == NULL) { st = TF_HOST_ERROR; goto fail; }
        memcpy(entry_gap, gap, gap_len);
        entry_gap[gap_len] = '\0';
    }
    if (tk == TF_TEXT || tk == TF_CHAR) {
        entry_val = reader->allocator.allocate(reader->allocator.context, payload_len + 1u);
        if (entry_val == NULL) { st = TF_HOST_ERROR; goto fail; }
        if (payload_len > 0u) memcpy(entry_val, payload_buf, payload_len);
        entry_val[payload_len] = '\0';
    } else if (tk == TF_IDENT || tk == TF_INT || tk == TF_FLOAT) {
        entry_val = reader->allocator.allocate(reader->allocator.context, lexeme_len + 1u);
        if (entry_val == NULL) { st = TF_HOST_ERROR; goto fail; }
        if (lexeme_len > 0u) memcpy(entry_val, lexeme, lexeme_len);
        entry_val[lexeme_len] = '\0';
    }

    {
        size_t pos = reader->prev_end, sc = reader->cur_scalar, ln = reader->cur_line;
        size_t col = reader->cur_col, disp = reader->cur_disp;
        if (!step_coordinates(gap, gap_len, true, &pos, &sc, &ln, &col, &disp) ||
            pos != start_byte || sc != start_loc.scalar || ln != start_loc.line ||
            col != start_loc.column || disp != start_loc.display_column ||
            !step_coordinates(lexeme, lexeme_len, false, &pos, &sc, &ln, &col, &disp) ||
            pos != end_byte || sc != end_loc.scalar || ln != end_loc.line ||
            col != end_loc.column || disp != end_loc.display_column) {
            set_err(reader, "token line/column/scalar coordinates inconsistent with source bytes");
            st = TF_SOURCE_ERROR;
            goto fail;
        }
        reader->prev_end = pos;
        reader->cur_scalar = sc;
        reader->cur_line = ln;
        reader->cur_col = col;
        reader->cur_disp = disp;
    }

    if (gap_len > 0u) {
        memcpy(reader->reconstructed + reader->reconstructed_len, gap, gap_len);
        reader->reconstructed_len += gap_len;
    }
    if (lexeme_len > 0u) {
        memcpy(reader->reconstructed + reader->reconstructed_len, lexeme, lexeme_len);
        reader->reconstructed_len += lexeme_len;
    }
    sha_update(&reader->sha, gap, gap_len);
    sha_update(&reader->sha, lexeme, lexeme_len);

    if (tk == TF_TEXT || tk == TF_CHAR) {
        stored_text_val = payload_buf;
        payload_buf = NULL;
    }
    if (err != TF_E_NONE) {
        if (reader->diag_count < reader->diagnostic_cap) {
            reader->diagnostics[reader->diag_count++] =
                (TfDiagnostic){err, {NULL, start_byte, end_byte}};
        } else {
            ++reader->suppressed;
        }
    }

    {
        size_t stored_val_len = (tk == TF_TEXT || tk == TF_CHAR) ? payload_len
                              : (tk == TF_IDENT || tk == TF_INT || tk == TF_FLOAT) ? lexeme_len
                              : 0u;
        reader->tokens[reader->tokens_read] = (TfToken){
            tk, {NULL, start_byte, end_byte}, err, reserved,
            stored_text_val, stored_val_len
        };
        stored_text_val = NULL;

        *out = (TfSymbolTableEntry){
            idx,
            {tk, {NULL, start_byte, end_byte}, err, reserved, entry_val, stored_val_len},
            start_loc, end_loc, entry_gap, gap_len
        };
    }

    ++reader->tokens_read;
    if (tk == TF_EOF) reader->saw_eof = true;
    *has_token = true;

    reader->allocator.deallocate(reader->allocator.context, gap);
    reader->allocator.deallocate(reader->allocator.context, lexeme);
    if (payload_buf != NULL) reader->allocator.deallocate(reader->allocator.context, payload_buf);
    return TF_OK;

fail:
    if (entry_gap != NULL) reader->allocator.deallocate(reader->allocator.context, entry_gap);
    if (entry_val != NULL) reader->allocator.deallocate(reader->allocator.context, entry_val);
    if (stored_text_val != NULL) reader->allocator.deallocate(reader->allocator.context, stored_text_val);
    if (gap != NULL) reader->allocator.deallocate(reader->allocator.context, gap);
    if (lexeme != NULL) reader->allocator.deallocate(reader->allocator.context, lexeme);
    if (payload_buf != NULL) reader->allocator.deallocate(reader->allocator.context, payload_buf);
    return st;
}

TfStatus tf_symbol_table_finish(TfSymbolTableReader *reader, TfSource *out_source,
                                TfLexResult *out_lex) {
    unsigned char digest[32];
    size_t i;
    TfStatus st;

    if (reader == NULL || out_source == NULL || out_lex == NULL) return TF_HOST_ERROR;
    *out_source = (TfSource){0};
    *out_lex = (TfLexResult){0};
    if (!reader->source_valid) return TF_SOURCE_ERROR;

    if (!reader->array_done) {
        if (!expect_ch(reader, ']', "expected closing ']' after symbol table tokens")) {
            return TF_SOURCE_ERROR;
        }
        reader->array_done = true;
    }
    if (!expect_ch(reader, '}', "expected closing '}' on symbol table")) return TF_SOURCE_ERROR;
    skip_ws(reader);
    if (reader->at != reader->json_len) {
        set_err(reader, "trailing content after symbol table root object");
        return TF_SOURCE_ERROR;
    }
    if (!reader->saw_eof || reader->tokens_read != reader->expected_token_count ||
        reader->reconstructed_len != reader->source_length ||
        reader->diag_count != reader->expected_diag_count ||
        reader->suppressed != reader->expected_suppressed) {
        set_err(reader, "symbol table count, EOF, or diagnostic summary mismatch");
        return TF_SOURCE_ERROR;
    }
    if (!sha_final(&reader->sha, digest) ||
        memcmp(digest, reader->expected_sha256, 32u) != 0) {
        set_err(reader, "symbol table SHA-256 verification failed");
        return TF_SOURCE_ERROR;
    }

    st = tf_source_init(out_source, reader->source_name,
                        reader->reconstructed_len != 0u ? reader->reconstructed : (const unsigned char *)"",
                        reader->reconstructed_len, &reader->allocator);
    if (st == TF_HOST_ERROR) return TF_HOST_ERROR;
    if (st != TF_OK || !out_source->valid) {
        tf_source_destroy(out_source);
        set_err(reader, "reconstructed source failed validation");
        return TF_SOURCE_ERROR;
    }

    out_lex->source = out_source;
    out_lex->tokens = reader->tokens;
    out_lex->token_count = reader->tokens_read;
    out_lex->token_capacity = reader->token_cap;
    out_lex->diagnostics = reader->diagnostics;
    out_lex->diagnostic_count = reader->diag_count;
    out_lex->diagnostic_capacity = reader->diag_cap;
    out_lex->suppressed = reader->suppressed;
    out_lex->has_errors = reader->has_errors;
    out_lex->allocator = reader->allocator;

    for (i = 0; i < out_lex->token_count; ++i) {
        TfToken *t = &out_lex->tokens[i];
        t->span.source = out_source;
        if (t->kind == TF_IDENT || t->kind == TF_INT || t->kind == TF_FLOAT) {
            t->value = out_source->bytes + t->span.start;
            t->value_length = t->span.end - t->span.start;
        }
    }
    for (i = 0; i < out_lex->diagnostic_count; ++i) {
        out_lex->diagnostics[i].span.source = out_source;
    }

    /* Transfer ownership of token and diagnostic arrays cleanly. */
    reader->tokens = NULL;
    reader->tokens_read = 0;
    reader->token_cap = 0;
    reader->diagnostics = NULL;
    reader->diag_count = 0;
    reader->diag_cap = 0;
    return TF_OK;
}

TfStatus tf_symbol_table_reconstruct_invalid_source(const TfSymbolTableReader *reader,
                                                    TfSource *out_source,
                                                    const TfAllocator *allocator) {
    TfAllocator a = tf_allocator(allocator);
    TfStatus st;
    if (reader == NULL || out_source == NULL || reader->source_valid ||
        reader->source_name == NULL || !tf_allocator_valid(a)) {
        return TF_HOST_ERROR;
    }
    *out_source = (TfSource){0};
    st = tf_source_init(out_source, reader->source_name,
                        reader->raw_length != 0u ? reader->raw_bytes : (const unsigned char *)"",
                        reader->raw_length, &a);
    if (st == TF_HOST_ERROR) return TF_HOST_ERROR;
    if (st != TF_SOURCE_ERROR || out_source->valid ||
        out_source->error.code != reader->source_error_code ||
        out_source->error.span.start != reader->source_error_start ||
        out_source->error.span.end != reader->source_error_end) {
        tf_source_destroy(out_source);
        return TF_HOST_ERROR;
    }
    return TF_SOURCE_ERROR;
}

static TfStatus trace_entry(FILE *trace, const TfSymbolTableEntry *entry) {
    if (trace == NULL) return TF_OK;
    if (fprintf(trace, "[symbol-table #%zu] %s reserved=%s span=[%zu,%zu) loc=%zu:%zu..%zu:%zu\n",
                entry->index, tf_token_name(entry->token.kind),
                entry->token.reserved ? "true" : "false",
                entry->token.span.start, entry->token.span.end,
                entry->start.line, entry->start.column,
                entry->end.line, entry->end.column) < 0) {
        return TF_HOST_ERROR;
    }
    return TF_OK;
}

TfStatus tf_symbol_table_load(const char *path, TfSource *out_source, TfLexResult *out_lex,
                              const TfAllocator *allocator, FILE *trace,
                              char *error_buffer, size_t error_buffer_size) {
    TfSymbolTableReader *reader = NULL;
    TfStatus st;
    bool has_token = false;

    if (out_source == NULL || out_lex == NULL) return TF_HOST_ERROR;
    *out_source = (TfSource){0};
    *out_lex = (TfLexResult){0};
    if (error_buffer != NULL && error_buffer_size != 0u) error_buffer[0] = '\0';

    st = open_file_impl(&reader, path, allocator, error_buffer, error_buffer_size);
    if (st == TF_SOURCE_ERROR && reader != NULL && tf_symbol_table_reader_is_source_invalid(reader)) {
        st = tf_symbol_table_reconstruct_invalid_source(reader, out_source, allocator);
        tf_symbol_table_reader_destroy(reader);
        return st;
    }
    if (st != TF_OK) {
        if (error_buffer != NULL && error_buffer_size != 0u && error_buffer[0] == '\0') {
            (void)snprintf(error_buffer, error_buffer_size, "%s",
                           reader != NULL && tf_symbol_table_reader_error(reader) != NULL
                               ? tf_symbol_table_reader_error(reader)
                               : "cannot open or validate symbol table header");
        }
        tf_symbol_table_reader_destroy(reader);
        return st == TF_HOST_ERROR ? TF_HOST_ERROR : TF_SOURCE_ERROR;
    }

    for (;;) {
        TfSymbolTableEntry entry = {0};
        st = tf_symbol_table_next(reader, &entry, &has_token);
        if (st != TF_OK) {
            if (error_buffer != NULL && error_buffer_size != 0u) {
                (void)snprintf(error_buffer, error_buffer_size, "%s",
                               tf_symbol_table_reader_error(reader));
            }
            tf_symbol_table_entry_destroy(reader, &entry);
            tf_symbol_table_reader_destroy(reader);
            return st;
        }
        if (!has_token) break;
        st = trace_entry(trace, &entry);
        tf_symbol_table_entry_destroy(reader, &entry);
        if (st != TF_OK) {
            tf_symbol_table_reader_destroy(reader);
            return TF_HOST_ERROR;
        }
    }

    st = tf_symbol_table_finish(reader, out_source, out_lex);
    if (st != TF_OK && error_buffer != NULL && error_buffer_size != 0u) {
        (void)snprintf(error_buffer, error_buffer_size, "%s",
                       tf_symbol_table_reader_error(reader));
    }
    tf_symbol_table_reader_destroy(reader);
    return st;
}

void tf_symbol_table_reader_destroy(TfSymbolTableReader *reader) {
    TfAllocator a;
    size_t i;
    if (reader == NULL) return;
    a = reader->allocator;
    if (reader->json != NULL) a.deallocate(a.context, reader->json);
    if (reader->source_name != NULL) a.deallocate(a.context, reader->source_name);
    if (reader->raw_bytes != NULL) a.deallocate(a.context, reader->raw_bytes);
    if (reader->reconstructed != NULL) a.deallocate(a.context, reader->reconstructed);
    for (i = 0; i < reader->tokens_read; ++i) {
        if ((reader->tokens[i].kind == TF_TEXT || reader->tokens[i].kind == TF_CHAR) &&
            reader->tokens[i].value != NULL) {
            a.deallocate(a.context, (void *)reader->tokens[i].value);
        }
    }
    if (reader->tokens != NULL) a.deallocate(a.context, reader->tokens);
    if (reader->diagnostics != NULL) a.deallocate(a.context, reader->diagnostics);
    a.deallocate(a.context, reader);
}

const char *tf_symbol_table_reader_error(const TfSymbolTableReader *reader) {
    return reader != NULL && reader->error_message[0] != '\0' ? reader->error_message : "";
}

bool tf_symbol_table_reader_is_source_invalid(const TfSymbolTableReader *reader) {
    return reader != NULL && !reader->source_valid;
}
