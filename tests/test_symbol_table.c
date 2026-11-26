#include "ternfold/symbol_table.h"
#include "ternfold/parser.h"
#include "ternfold/ast_dump.h"
#include "ternfold/diagnostics.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <process.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0700)
#endif

static size_t checks;
static char work_dir[256];

static void check(bool ok, const char *expr, int line) {
    ++checks;
    if (!ok) {
        (void)fprintf(stderr, "symbol_table:%d: %s\n", line, expr);
        exit(1);
    }
}
#define CHECK(x) check((x), #x, __LINE__)

static void setup_dir(void) {
#ifdef _WIN32
    unsigned long pid = (unsigned long)_getpid();
#else
    unsigned long pid = (unsigned long)getpid();
#endif
    unsigned attempt;
    CHECK(MKDIR("build") == 0 || errno == EEXIST);
    for (attempt = 0; attempt < 256u; ++attempt) {
        CHECK(snprintf(work_dir, sizeof(work_dir), "build/st-test-%lu-%u", pid, attempt) > 0);
        if (MKDIR(work_dir) == 0) return;
        CHECK(errno == EEXIST);
    }
    CHECK(false);
}

static char *read_entire_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    CHECK(f != NULL);
    CHECK(fseek(f, 0, SEEK_END) == 0);
    n = ftell(f);
    CHECK(n >= 0);
    CHECK(fseek(f, 0, SEEK_SET) == 0);
    buf = malloc((size_t)n + 1u);
    CHECK(buf != NULL);
    CHECK(fread(buf, 1u, (size_t)n, f) == (size_t)n);
    buf[(size_t)n] = '\0';
    CHECK(fclose(f) == 0);
    *out_len = (size_t)n;
    return buf;
}

static char *capture_ast_json(const TfAst *ast, bool has_errors) {
    FILE *f = tmpfile();
    long n;
    char *s;
    CHECK(f != NULL);
    CHECK(tf_ast_dump_write(f, ast, has_errors, true) == TF_OK);
    CHECK(fflush(f) == 0);
    CHECK(fseek(f, 0, SEEK_END) == 0);
    n = ftell(f);
    CHECK(n >= 0);
    s = malloc((size_t)n + 1u);
    CHECK(s != NULL);
    CHECK(fseek(f, 0, SEEK_SET) == 0);
    CHECK(fread(s, 1u, (size_t)n, f) == (size_t)n);
    s[(size_t)n] = '\0';
    CHECK(fclose(f) == 0);
    return s;
}

static char *capture_diag_json(const TfDiagnosticBag *bag) {
    FILE *f = tmpfile();
    long n;
    char *s;
    CHECK(f != NULL);
    CHECK(tf_diagnostic_bag_write(f, bag, TF_DIAG_JSON, TF_DIAG_COLOR_NEVER) == TF_OK);
    CHECK(fflush(f) == 0);
    CHECK(fseek(f, 0, SEEK_END) == 0);
    n = ftell(f);
    CHECK(n >= 0);
    s = malloc((size_t)n + 1u);
    CHECK(s != NULL);
    CHECK(fseek(f, 0, SEEK_SET) == 0);
    CHECK(fread(s, 1u, (size_t)n, f) == (size_t)n);
    s[(size_t)n] = '\0';
    CHECK(fclose(f) == 0);
    return s;
}

static void assert_roundtrip(const char *path, const void *mem_bytes, size_t mem_len,
                             const char *logical_name, size_t cap, const char *tag) {
    TfSource src1 = {0}, src2 = {0};
    TfLexResult lex1 = {0}, lex2 = {0};
    TfParseResult *p1 = NULL, *p2 = NULL;
    char table_path[512], errbuf[256];
    TfStatus st1;
    char *ast1, *ast2, *diag1, *diag2;

    if (path != NULL) {
        st1 = tf_source_load(&src1, path, NULL);
    } else {
        st1 = tf_source_init(&src1, logical_name, mem_bytes, mem_len, NULL);
    }
    CHECK(st1 != TF_HOST_ERROR);
    if (st1 == TF_OK) {
        CHECK(tf_lex(&lex1, &src1, cap, NULL) != TF_HOST_ERROR);
    }

    CHECK(snprintf(table_path, sizeof(table_path), "%s/%s.symbol-table.json", work_dir, tag) > 0);
    CHECK(tf_symbol_table_publish(&src1, src1.valid ? &lex1 : NULL, cap, table_path) == TF_OK);

    {
        TfStatus st2 = tf_symbol_table_load(table_path, &src2, &lex2, NULL, NULL, errbuf, sizeof(errbuf));
        if (!src1.valid) {
            TfDiagnosticBag *b1 = NULL, *b2 = NULL;
            CHECK(st2 == TF_SOURCE_ERROR && !src2.valid);
            CHECK(tf_diagnostic_bag_create(&b1, 100u, NULL) == TF_OK);
            CHECK(tf_diagnostic_bag_create(&b2, 100u, NULL) == TF_OK);
            CHECK(tf_diagnostic_bag_source(b1, &src1) == TF_OK);
            CHECK(tf_diagnostic_bag_source(b2, &src2) == TF_OK);
            diag1 = capture_diag_json(b1);
            diag2 = capture_diag_json(b2);
            CHECK(strcmp(diag1, diag2) == 0);
            free(diag1);
            free(diag2);
            tf_diagnostic_bag_destroy(b1);
            tf_diagnostic_bag_destroy(b2);
            tf_source_destroy(&src1);
            return;
        }
        if (st2 != TF_OK) (void)fprintf(stderr, "load failure (%s): %s\n", tag, errbuf);
        CHECK(st2 == TF_OK && src2.valid);
    }

    CHECK(lex1.token_count == lex2.token_count);
    CHECK(lex1.diagnostic_count == lex2.diagnostic_count);
    CHECK(lex1.suppressed == lex2.suppressed);
    CHECK(lex1.has_errors == lex2.has_errors);

    {
        TfParseOptions opts = {true, 64u, cap <= 100u ? cap : 100u};
        CHECK(tf_parse(&p1, &lex1, &opts, NULL) == TF_OK);
        CHECK(tf_parse(&p2, &lex2, &opts, NULL) == TF_OK);
    }

    ast1 = capture_ast_json(tf_parse_ast(p1), tf_diagnostic_bag_has_errors(tf_parse_diagnostics(p1)));
    ast2 = capture_ast_json(tf_parse_ast(p2), tf_diagnostic_bag_has_errors(tf_parse_diagnostics(p2)));
    CHECK(strcmp(ast1, ast2) == 0);

    diag1 = capture_diag_json(tf_parse_diagnostics(p1));
    diag2 = capture_diag_json(tf_parse_diagnostics(p2));
    CHECK(strcmp(diag1, diag2) == 0);

    free(ast1);
    free(ast2);
    free(diag1);
    free(diag2);
    tf_parse_destroy(p1);
    tf_parse_destroy(p2);
    tf_lex_destroy(&lex1);
    tf_lex_destroy(&lex2);
    tf_source_destroy(&src1);
    tf_source_destroy(&src2);
}

static void test_roundtrips(void) {
    static const char nul_src[] = "pub fn main() -> List[Text] {\n\tlet x = \"a\\u{0}b\";\n\tlet c = '\\u{0}';\n\t[x]\n}\n";
    static const char empty_src[] = "";
    static const char utf8_bad[] = "let x = \xff;";
    static const char bom_src[] = "\xef\xbb\xbflet x = 1;";
    static const char cr_src[] = "let x = 1;\r";
    char many_errors[1024];

    assert_roundtrip("examples/course/teaching.tfd", NULL, 0, NULL, 100u, "teaching");
    assert_roundtrip("examples/course/precedence_bindings.tfd", NULL, 0, NULL, 100u, "prec");
    assert_roundtrip("examples/lexical/complete.tfd", NULL, 0, NULL, 100u, "complete");
    assert_roundtrip("examples/lexical/invalid.tfd", NULL, 0, NULL, 100u, "invalid");
    assert_roundtrip("tests/fixtures/parser/surface.tfd", NULL, 0, NULL, 100u, "surface");
    assert_roundtrip(NULL, nul_src, sizeof(nul_src) - 1u, "nul.tfd", 100u, "nul");
    assert_roundtrip(NULL, empty_src, 0u, "empty.tfd", 100u, "empty");
    assert_roundtrip(NULL, utf8_bad, sizeof(utf8_bad) - 1u, "e001.tfd", 100u, "e001");
    assert_roundtrip(NULL, bom_src, sizeof(bom_src) - 1u, "e002.tfd", 100u, "e002");
    assert_roundtrip(NULL, cr_src, sizeof(cr_src) - 1u, "e003.tfd", 100u, "e003");

    memset(many_errors, '@', 150u);
    many_errors[150] = '\0';
    assert_roundtrip(NULL, many_errors, 150u, "many.tfd", 100u, "many100");
    assert_roundtrip(NULL, many_errors, 150u, "many0.tfd", 0u, "many0");
}

static void test_streaming_and_non_bypass(void) {
    char tfd_path[512], st_path[512], errbuf[256];
    static const char code[] = "fn add(a: Int, b: Int) -> Int {\n  a + b\n}\n";
    TfSource src = {0}, loaded_src = {0};
    TfLexResult lex = {0}, loaded_lex = {0};
    TfSymbolTableReader *reader = NULL;
    TfParseResult *parsed = NULL;
    FILE *f;
    size_t count = 0;
    bool has_token = false;

    CHECK(snprintf(tfd_path, sizeof(tfd_path), "%s/ephemeral.tfd", work_dir) > 0);
    CHECK(snprintf(st_path, sizeof(st_path), "%s/ephemeral.symbol-table.json", work_dir) > 0);
    f = fopen(tfd_path, "wb");
    CHECK(f != NULL);
    CHECK(fwrite(code, 1u, sizeof(code) - 1u, f) == sizeof(code) - 1u);
    CHECK(fclose(f) == 0);

    CHECK(tf_source_load(&src, tfd_path, NULL) == TF_OK);
    CHECK(tf_lex(&lex, &src, 100u, NULL) == TF_OK);
    CHECK(tf_symbol_table_publish(&src, &lex, 100u, st_path) == TF_OK);
    CHECK(tf_symbol_table_publish(&src, &lex, 100u, tfd_path) == TF_HOST_ERROR);
    tf_lex_destroy(&lex);
    tf_source_destroy(&src);

    /* Delete original .tfd file to prove symbol table loader never falls back to raw source. */
    CHECK(remove(tfd_path) == 0);

    CHECK(tf_symbol_table_reader_open(&reader, st_path, NULL) == TF_OK);
    for (;;) {
        TfSymbolTableEntry entry = {0};
        CHECK(tf_symbol_table_next(reader, &entry, &has_token) == TF_OK);
        if (!has_token) break;
        CHECK(entry.index == count);
        ++count;
        tf_symbol_table_entry_destroy(reader, &entry);
    }
    CHECK(count > 5u);
    CHECK(tf_symbol_table_finish(reader, &loaded_src, &loaded_lex) == TF_OK);
    tf_symbol_table_reader_destroy(reader);
    CHECK(loaded_lex.token_count == count);
    CHECK(tf_parse(&parsed, &loaded_lex, NULL, NULL) == TF_OK);
    CHECK(tf_diagnostic_phase_kind(tf_parse_phase(parsed)) == TF_DIAG_PHASE_VALID);
    tf_parse_destroy(parsed);
    tf_lex_destroy(&loaded_lex);
    tf_source_destroy(&loaded_src);

    CHECK(tf_symbol_table_load(st_path, &loaded_src, &loaded_lex, NULL, NULL, errbuf, sizeof(errbuf)) == TF_OK);
    tf_lex_destroy(&loaded_lex);
    tf_source_destroy(&loaded_src);
}

static void expect_corrupt(const char *json, size_t len) {
    TfSymbolTableReader *reader = NULL;
    TfStatus st = tf_symbol_table_reader_open_bytes(&reader, json, len, "corrupt", NULL);
    if (st == TF_OK) {
        bool has_token = false;
        for (;;) {
            TfSymbolTableEntry entry = {0};
            st = tf_symbol_table_next(reader, &entry, &has_token);
            tf_symbol_table_entry_destroy(reader, &entry);
            if (st != TF_OK || !has_token) break;
        }
        if (st == TF_OK) {
            TfSource src = {0};
            TfLexResult lex = {0};
            st = tf_symbol_table_finish(reader, &src, &lex);
            tf_lex_destroy(&lex);
            tf_source_destroy(&src);
        }
    }
    tf_symbol_table_reader_destroy(reader);
    CHECK(st != TF_OK);
}

static char *replace_once(const char *haystack, const char *needle, const char *repl) {
    const char *pos = strstr(haystack, needle);
    size_t pre, nlen, rlen, tail;
    char *out;
    CHECK(pos != NULL);
    pre = (size_t)(pos - haystack);
    nlen = strlen(needle);
    rlen = strlen(repl);
    tail = strlen(pos + nlen);
    out = malloc(pre + rlen + tail + 1u);
    CHECK(out != NULL);
    memcpy(out, haystack, pre);
    memcpy(out + pre, repl, rlen);
    memcpy(out + pre + rlen, pos + nlen, tail + 1u);
    return out;
}

static void test_malformed_tables(void) {
    char path[512];
    char *valid_json;
    size_t len = 0;

    CHECK(snprintf(path, sizeof(path), "%s/ephemeral.symbol-table.json", work_dir) > 0);
    valid_json = read_entire_file(path, &len);

    expect_corrupt("", 0u);
    expect_corrupt("{}", 2u);
    expect_corrupt("[]", 2u);
    expect_corrupt(valid_json, len - 4u);

    {
        static const struct { const char *find, *repl; } muts[] = {
            {"\"format\":\"ternfold-symbol-table\"", "\"format\":\"wrong-format\""},
            {"\"schema_version\":1", "\"schema_version\":2"},
            {"\"source_sha256\":\"", "\"source_sha256\":\"0000000000000000000000000000000000000000000000000000000000000000"},
            {"\"index\":1", "\"index\":9"},
            {"\"kind\":\"FN\"", "\"kind\":\"LET\""},
            {"\"kind\":\"PLUS\"", "\"kind\":\"MINUS\""},
            {"\"reserved\":false", "\"reserved\":true"},
            {"\"gap\":\"\"", "\"gap\":\"x\""},
            {"\"line\":1,\"column\":1,\"display_column\":1", "\"line\":9,\"column\":1,\"display_column\":1"},
            {"\"payload\":\"add\"", "\"payload\":\"bad\""},
            {"\"tokens\":[", "\"unknown\":1,\"tokens\":["}
        };
        size_t i;
        for (i = 0; i < sizeof(muts) / sizeof(muts[0]); ++i) {
            if (strstr(valid_json, muts[i].find) != NULL) {
                char *bad = replace_once(valid_json, muts[i].find, muts[i].repl);
                expect_corrupt(bad, strlen(bad));
                free(bad);
            }
        }
    }
    /* Non-whitespace trailing content and swapped sha check on sha itself if first digits happened to match */
    {
        char *trail = malloc(len + 16u);
        CHECK(trail != NULL);
        memcpy(trail, valid_json, len);
        memcpy(trail + len, "junk", 5u);
        expect_corrupt(trail, len + 4u);
        free(trail);
    }
    free(valid_json);
}

typedef struct { size_t calls, fail_at, live; } MemTracker;

static void *test_alloc(void *ctx, size_t size) {
    MemTracker *m = ctx;
    void *p;
    ++m->calls;
    if (m->calls == m->fail_at) return NULL;
    p = malloc(size);
    if (p != NULL) ++m->live;
    return p;
}

static void test_free(void *ctx, void *p) {
    MemTracker *m = ctx;
    if (p == NULL) return;
    CHECK(m->live != 0u);
    --m->live;
    free(p);
}

static void test_oom_sweep(void) {
    char path[512];
    char *json;
    size_t len = 0, fail_at;
    bool succeeded = false;

    CHECK(snprintf(path, sizeof(path), "%s/nul.symbol-table.json", work_dir) > 0);
    json = read_entire_file(path, &len);

    for (fail_at = 1u; fail_at < 800u; ++fail_at) {
        MemTracker m = {0, fail_at, 0};
        TfAllocator alloc = {&m, test_alloc, test_free};
        TfSymbolTableReader *reader = NULL;
        TfSource src = {0};
        TfLexResult lex = {0};
        TfStatus st = tf_symbol_table_reader_open_bytes(&reader, json, len, "nul.tfd", &alloc);
        if (st == TF_OK) {
            bool has_tok = false;
            for (;;) {
                TfSymbolTableEntry entry = {0};
                st = tf_symbol_table_next(reader, &entry, &has_tok);
                tf_symbol_table_entry_destroy(reader, &entry);
                if (st != TF_OK || !has_tok) break;
            }
            if (st == TF_OK) {
                st = tf_symbol_table_finish(reader, &src, &lex);
            }
        }
        tf_symbol_table_reader_destroy(reader);
        tf_lex_destroy(&lex);
        tf_source_destroy(&src);
        CHECK(m.live == 0u);
        if (st == TF_OK) {
            succeeded = true;
            break;
        }
    }
    CHECK(succeeded);
    free(json);
}

int main(void) {
    setup_dir();
    test_roundtrips();
    test_streaming_and_non_bypass();
    test_malformed_tables();
    test_oom_sweep();
    (void)printf("symbol_table: %zu checks passed\n", checks);
    return 0;
}
