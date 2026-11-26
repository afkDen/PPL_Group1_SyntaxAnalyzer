# C17 testing and acceptance

Run `./scripts/build.ps1 check` and `sanitize` (Windows), or intended
`sh scripts/build.sh check`/`sanitize` (POSIX). Environment check separately
compiles/runs a C17 host probe. check builds production C, native unit/integration
tests and repository audit with warnings as errors. Sanitizers fail on errors;
never treat unavailable checks or skipped cases as successful verification.

For lexing cover all keywords/reserved words/noise words, maximal munch, prefix
suffix boundaries, ASCII identifiers, five literal categories, Unicode scalar
validation, BOM/NUL/CRLF/source coordinates, all seven arithmetic and nine Boolean
operators, delimiters/comments/invalid recovery, exact report bytes and ordering.
Exercise allocation failures and deterministic randomized byte/scalar inputs.
CLI integration covers exact extension policy, missing files/output directories,
spaces in paths, stable reruns, invalid source/header output, source/report aliases
and failed publication without source mutation. Review actual assertions.

Keep fixture/golden bytes under tests and runnable examples under examples.
No numeric conversion belongs in the scanner; range/arithmetic checks are later
phase tests. Keep lexical evidence separate from actual M09/M11 executor parity.
Machine tests must eventually cover old/next reads, one writer, lexical deps,
cycles, ASCII ready order, candidates-before-guards, first failed guard, typed
rejection, runtime faults and no leaked emissions. Numeric tests preserve ADR0010.

Independent reviewers remain read-only. Native safety review challenges lifetime,
allocation failure, size arithmetic, aliasing, atomic publication and UB.
Record exact commands/results and hash frozen source. Every task retains original
acceptance intent; lexical subsets do not silently complete broad phase tasks.
Fresh ZIP extraction must rebuild/test without the checkout, Python or AI access.
Course ledger tracks lexical recognition separately from future semantic execution.

M01 adds a required native SourceManager gate to check/release/sanitize: cached
valid and invalid snapshots, canonical aliases, no reread after edits/deletion,
stable IDs/addresses, digest vectors, allocation failure, scalar coordinates and
forged span rejection. It is excluded only by the standalone package marker.
Original coursework/foundation ZIPs are preserved. Later milestone ZIPs come from
a clean local commit and must pass verify-checkpoint.ps1 outside the checkout.

M02 adds required native diagnostic bag/result/text tests and independently decoded
JSONschema/roundtrip tests. Include multiline/crossfile/EOF/line-number gutters,
NUL/Unicode/escape/raw-source values, caps, revision invalidation and OOM rollback.
Goldens, grammar recognition and decoded schema checks are separate evidence.

M04 adds required parser and ASTdump gates plus ast CLI coverage. Parser tests
assert precedence/associativity, full surface schema,23maintained declaration
examples +surface fixture,negative patterns,mainIOscopeE208,machine ordering,
recovery preserving following declarations,diagnosticcaps,allallocationfailure
positions and a bounded deterministic malformed corpus. ASTdump tests assert
empty-program exactgoldens,stableIDs/spans,UTF8/NULpayloads,recoverytree,repeat
identity andwritefailures. CLI tests validate separateAST/diagnosticsJSONstreams,
invalidsourceemptyAST,noimplicitreport,rejectedunsupportedoptions andbrokenpipes.
These are parsing/inspection gates,not typechecking or executableconformance.

The compiler now uses the fixed name DFA from the verified coursework source.
The private gate runs the table and trace suites in addition to the existing
lexer, parser and CLI regressions. The standalone release is unchanged.

M05 adds a native resolution suite and exercises the `check` command in the
compiler CLI suite. It tests snapshots, alias identity, canonical roots, cycles,
separate namespaces, cross-module visibility, captures, machine reference
kinds, host-entry-only E208, diagnostics and allocation failures. A library
may pass resolution without a main. The integrated M06 gate adds expression,
prelude, initializer and match tests with source spans, allocation injection
and the 24-program CLI corpus. M07 machine scheduling diagnostics remain
outside the M06 gate.

On Windows, use `./scripts/build.ps1 sanitize` to run sanitizer executables
with the bundled toolchain's DLLs on the child process PATH. For a manually
built sanitizer executable, prepend the directory containing that build's
`clang.exe` to PATH in the shell that launches it. A Windows missing-DLL
dialog (for example, for `libc++.dll`) is a loader failure before tests run.
It does not require a change to the system PATH.

M06 host limits include 160 checker expression frames, 2 Mi speculative node
visits per program, and 8 Mi coverage work units per match. The matrix caps
recursive specializations at 192. Hitting these limits returns a host failure;
it never certifies a program or fabricates a language type error. The synthetic
3,000-binding/28-wrapper probe exceeds the checker work limit on the verified
Windows build. Keep that capacity limitation distinct from the repaired
exponential failure on small invalid programs.

C01 adds the private numerical conformance and classroom report gates.
Regenerate the report by running the standalone analyzer on
`examples/course/operator_inventory.tfd` with `--classroom --output-dir DIR`
where DIR already exists. The test verifies the maintained fixture, original
span bytes, column count and every required operator's spelling and category.
That frontend gate alone does not establish general execution or native parity.

M08 adds source-to-verified-Core tests, hand-built malformed IR, allocation
failures, lexical/tag-proof checks, stable dumps and the `core` CLI corpus.
M09 adds exact `run` outputs and runtime diagnostics to the CLI suite, including
NUL, input staging, recursion limits, ordered candidate/guard/emission faults,
and all prelude operations. M10's native harnesses are separate from language
execution and do not establish interpreter/backend parity before M11.
The native runtime harnesses launch directly with argv and a 30-second limit.
The private `sanitize` gate first builds and launches a short ASan/UBSan probe;
an unavailable compiler or loader fails that gate explicitly.

M11 registers `test_c_emit`, `test_c_metadata`, `test_native_build` and
`test_differential`. The latter runs real native builds for the maintained
reference-execution corpus, compares byte output against hand-derived
expectations, checks exit status and original primary fault spans, and tests
source/output alias protection plus preservation after downstream failures.
Run both canonical `check` and `sanitize`; only the latter asks the native
builder to instrument the generated programs. Resource-limit allocation
counts and implementation-only stack frames are not parity observations.

M12 adds manifest-driven checks for every example, including lexical-only
streams, library roots, accepted/rejected source, and exact execution outputs.
Source-library integration copies the distributed `stdlib/` files to a fresh
test project and exercises explicit imports, Unicode/NUL, checked Int
boundaries, static privacy/type errors, and fault origins in both engines.

M13 adds redirected-stream REPL transcripts, lexer/parser prefix probes,
virtual resolver and cache rollback tests, and verified evaluator tests.
The transcripts assert multiline grammar completion, type-query purity,
scope persistence, failed unit/load rollback, E208, machine Reject as a
value, reset and EOF. Test streams contain no interactive prompts.

M14 adds bounded C17 campaigns to strict, sanitizer and optimized gates:

- Frontend fuzz: 768 cases at seed `0x4d313446`; replay with
  `test-frontend-fuzz --case SEED INDEX`. The lanes check raw bytes, malformed
  tokens, parser recovery, bounded nesting and deterministic diagnostic spans.
- Machine graphs: 48 cases at each of seeds `0x5445524e` and `0xc0ffee17`,
  with both declaration/writer layouts. A small independent oracle verifies
  the ASCII schedule and cyclic components, including lexical reads in dead
  branches. The replay arguments are decimal seed and optional case.
- [Generated execution](../tests/property/README.md): nine value cases and
  nine fault cases at seed 141592653. Both engines run real source files and
  are checked against an independent oracle. Failure shrinking uses at most
  24 probes and preserves the observed mismatch class.
- The fixed differential harness also checks five diagnostic stage boundaries
  through later commands, including primary spans and preservation of existing
  output files. Separate child processes must produce byte-identical
  token, AST, plan, Core and emitted-C results across two process environments,
  and independent native builds must run identically. The compiler has no
  externally configurable hash seed. Source-generation seeds vary the graph
  and writer permutations separately. Native executable byte identity across
  downstream compilers or debug build directories is not promised.
- The course audit checks all 25 rubric mappings and the C01 amendment.
  Classroom report tests and the interpreter/native example harness separately
  verify executed behavior, including the expanded teaching transcript.

The same gates register the optional trace, optimizer, formatter, editor and
bounded composition suites. The fixed differential harness compares trace
records, source maps and replay against both executors. Fault injection covers
capture-store allocation, optimization and output publication. The editor
tests cover both formatter equivalence and the protocol limits, including
Unicode positions, stale revisions, cancellation and depth-clipped hover.

The advanced archive gate also runs `benchmarks/three-way.ps1` and
`experiments/composition/run.ps1` in strict and sanitizer modes from its
fresh extraction. Both invoke the real compiler and native executables.
