# Engine tests

`src/<component>/` mirrors production ownership. Mixed contracts belong under
`integration/<area>/`; shared helpers stay under `test/harness/`. No SDK or claims layer.
CMake targets own source/header dependencies, compilation and linking. CTest owns
selection, timeouts, output and results. Missing input or a skipped case is not success.

```sh
cmake --preset native
cmake --build --preset native --target outshine
make suite SUITE=outshine/src/base/curve
make suite SUITE=outshine/integration/content/AssetGeneration
make suite SUITE=tools
make suite SUITE=render/shader-artifacts
ctest --test-dir build --show-only
```

`make suite` builds only the selected tests and their module dependencies, using all
cores. Each test source becomes `test_<path with slashes replaced by underscores>`.
For example, `test_outshine_src_base_curve` builds that subtree; CTest names preserve
slashes. `OUTSHINE_BUILD_DIR` selects a separate build for `test/run.sh`;
`BUILD_DIR` does the same for Make. Product and test objects share the configured
compiler/options; CMake maintains distinct builds for instrumentation. Native tests
use the runtime's exception-free model. The intentional process-boundary exception
test explicitly enables exceptions; its failure and unwinding assertions stay intact.

| Command | Scope |
|---|---|
| `make test` | World contracts in `test/world-gate.txt`, including offline Places |
| `make test-all` | All native tests and pinned HTML/CSS/JS/geodesic corpora |
| `make suite SUITE=<path>` | Named test or subtree |
| `make test-sanitizers SUITE=<path>` | ASan/UBSan, checked standard library |
| `make test-races SUITE=<path>` | TSan in a separate build |
| `make lint` | Incremental CMake-integrated clang-tidy and formatting |
| `make lint-all` | Clean clang-tidy build for the complete integration |

Prepared corpus inputs live in the system temp directory under `outshine-prepared/`;
`OUTSHINE_PREPARED_DIR` is a CMake cache setting. Prepare pinned manifests explicitly
with `make corpus-prepare MANIFEST=test/.../manifest.json`. Normal tests never fetch
new oracles or change pins. Functional GPU tests and release performance measurements
remain separate; instrumented results do not establish release frame budgets.

Places are diagnostic profiles, not worldwide acceptance. Target conditions remain
1280×720/60 including p99; lower-rate profiles are explicitly separate comparisons.
Image correctness, shader layouts, persistence and device timing need their own
checks. Tests succeeding at one boundary do not establish the others.

Built shaders resolve through their owning CMake build, independently of the working
directory. Tests invoke that build's client through `OUTSHINE_CLIENT`. The optional
`OUTSHINE_CACHE_DIR` selects prepared source/asset storage for native Place checks.
The profile test requires a prepared Place asset cache; CTest supplies its offline
preparation fixture before the unchanged warm-load and 25/30/60-Hz checks.
