/* Checks Syntax Analyzer coursework manifest and syntax example presence. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *data; size_t length; } Bytes;
static const char *context = "syntax coursework mapping";
#define CHECK(ok) do { if (!(ok)) { \
    fprintf(stderr, "course audit: %s:%d: %s\n", context, __LINE__, #ok); \
    exit(EXIT_FAILURE); \
} } while (0)
#include "../tests/fixture_json.h"

static void require_file(const char *path) {
    FILE *f = fopen(path, "rb");
    context = path;
    CHECK(f != NULL);
    CHECK(fclose(f) == 0);
}

static Bytes read_document(const char *path, Json *json, size_t schema) {
    context = path;
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    Bytes bytes = {malloc(JSON_BYTES + 1u), 0};
    CHECK(bytes.data != NULL);
    bytes.length = fread(bytes.data, 1, JSON_BYTES + 1u, file);
    CHECK(bytes.length <= JSON_BYTES && feof(file) && !ferror(file));
    CHECK(fclose(file) == 0);
    json->t = calloc(JSON_TOKENS, sizeof *json->t);
    CHECK(json->t != NULL && jparse(json, bytes.data, bytes.length));
    CHECK(json->t[0].type == '{');
    if (schema) CHECK(jnumber(json, jfield(json, 0, "schema")) == schema);
    return bytes;
}

int main(void) {
    static const char *const syntax_fixtures[] = {
        "examples/syntax/complete.tfd",
        "examples/syntax/input-output.tfd",
        "examples/syntax/declarations.tfd",
        "examples/syntax/expressions.tfd",
        "examples/syntax/conditions.tfd",
        "examples/syntax/loops.tfd",
        "examples/syntax/invalid-recovery.tfd",
        "examples/syntax/invalid-expressions.tfd",
        "examples/syntax/invalid-declarations.tfd",
        "examples/syntax/README.md"
    };
    static const char *const required_sources[] = {
        "include/ternfold/symbol_table.h",
        "src/symbol_table.c",
        "src/parser.c",
        "src/ast.c",
        "src/ast_dump.c",
        "src/token_dump.c",
        "src/compiler_main.c",
        "scripts/run-syntax-demo.ps1",
        "scripts/run-syntax-demo.sh",
        "RUBRIC.md",
        "PROFESSOR_DEMO_GUIDE.md",
        "SOURCE_PROVENANCE.md",
        "START_HERE.md",
        "VERIFICATION.md",
        "docs/syntax-analyzer/SYMBOL_TABLE_INPUT.md",
        "docs/syntax-analyzer/RECURSIVE_DESCENT.md",
        "docs/syntax-analyzer/GRAMMAR_AND_PARSING_TABLE.md",
        "docs/syntax-analyzer/TRANSITION_DIAGRAM.md",
        "docs/syntax-analyzer/ERROR_RECOVERY.md",
        "docs/syntax-analyzer/DEMO_ROWS.md",
        "docs/syntax-analyzer/INVALID_CASES.md"
    };
    Json manifest = {0};
    Bytes manifest_bytes = read_document("examples/manifest.json", &manifest, 1);
    const size_t cases = jfield(&manifest, 0, "cases");
    CHECK(cases < manifest.count && manifest.t[cases].type == '[');
    for (size_t i = 0; i < sizeof(syntax_fixtures) / sizeof(syntax_fixtures[0]); ++i) {
        require_file(syntax_fixtures[i]);
    }
    for (size_t i = 0; i < sizeof(required_sources) / sizeof(required_sources[0]); ++i) {
        require_file(required_sources[i]);
    }
    free(manifest.t);
    free(manifest_bytes.data);
    printf("Syntax coursework audit: %zu fixtures, %zu core files verified\n",
           sizeof(syntax_fixtures) / sizeof(syntax_fixtures[0]),
           sizeof(required_sources) / sizeof(required_sources[0]));
    return EXIT_SUCCESS;
}
