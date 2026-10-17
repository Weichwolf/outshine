# Externally admitted prepared terrain restoration (#678)

This report records the historical experiment at PR #689. The subsequent
[reusable library delivery](stair_prepared_surface_library.md) carries the current
contract, consumers and cost checks; the independent reference protocol below
remains a research tool rather than a native runtime architecture.

The experiment restores the immutable, independently verified terrain snapshot
without repeating its geometric validation or14historical replacements. The
normal library constructors and unknown-input path are unchanged. This is an
explicit opt-in experiment under `test/experiments/transport`, not an admitted
new `lib/` interface or native AssetCache implementation. The shared-contract
[proposal](https://github.com/Weichwolf/outshine/issues/47#issuecomment-6241901592)
required an explicit external trust root; the later optional Python promotion
is documented in the delivery above.

## Admission and representation

A hash proves unchanged bytes, not valid geometry. The cache owner must obtain
the expected digest from normally built, independently checked published data
and retain it outside the candidate. Hashing an unknown payload or trusting its
adjacent JSON does not authorize fast restoration. Deliberately rebinding the
external anchor to invalid geometry defeats this premise; structural checks do
not replace geometric validation in that case.

`stair_prepared_surface_prepare` first loads the normal validated snapshot and
writes two candidate archives. Its producer report grants no admission.
`stair_prepared_surface_verify` independently loads the original history using
**both general constructor validation and general union composition**, compares
every patch ID, XYZ/index buffer, active mask and attribute, capacity/kind, the
independent whole reference and Double/Float32 queries, then publishes a reference
digest. Only that separately pinned reference authorizes the scoped experiment.
The actual source/installed follow-up verifier keeps its strict-before and cold
readers outside this scope; only actual loading and generation use it.

Format `outshine-finite-surface/1` contains primitive numeric arrays and bounded
UTF-8 metadata in standard NPZ: vertices, faces, active masks and face attributes
for each patch, plus frame, IDs and capacities. No pickle, serialized GEOS tree,
cached valid flag, foreign code or SDK/runtime dependency is introduced. Normal
`Mesh` construction still checks finite coordinates, shapes and indices and owns
its immutable buffers. Query polygons and generic STRtrees are rebuilt from those
buffers; that remaining work is included in loading, not called zero-cost reuse.

The reader checks the external digest before ZIP parsing, format/frame/schema,
unique complete members, primitive dtypes, numeric/mask shapes and bounded counts.
It inspects NPY1.0 headers before NumPy allocation and requires declared raw sizes
to equal actual member sizes. Payload and aggregate uncompressed member limits
default to64MiB, metadata to1MiB. These limits are not a total Python/GEOS heap
budget. CLI loads have120CPU-second/3GiB-address-space limits; preparation and
independent comparison allow180CPU seconds/3GiB. No archive is extracted to disk.

Established format references are NumPy's [NPZ writer](https://numpy.org/doc/2.2/reference/generated/numpy.savez.html),
[DEFLATE alternative](https://numpy.org/doc/2.2/reference/generated/numpy.savez_compressed.html),
[public bounded NPY-header reader](https://numpy.org/doc/2.2/reference/generated/numpy.lib.format.read_array_header_1_0.html)
and Python's [ZIP member metadata](https://docs.python.org/3.13/library/zipfile.html).
Actual installed behavior is exercised, rather than inferred from current docs.
The geometric premise remains the strict validated coverage from the preceding
[boundary](stair_coverage_boundary_comparison.md) and
[composition](stair_composition_comparison.md) studies, not a new geometric theorem.
Asset storage, integrity and AssetRecord ownership remain with #10; product codecs
own their version. No parallel cache/index manager is proposed.

## Equal whole-state loading and costs

Preparation/independent-reference commit: `091200a65146e2737b503957cd99e5a9991828ae`.
Frozen measurement/replay commit: `cae9871ed4fe37cfbdc7f6f6d798af939c248d88`.
Original78-source manifest SHA256:
`4e0c339d98df06e77cf1aecbf3f5c0386819dba3c188cbd0d325419ba2a9a917`.
Independent external reference SHA256:
`ff0433accc1dd37676038f69cd44d24d4cfbce2894949dd0005925f7a6e15153`.
All methods return complete-state SHA256
`09343d36a760a2cb8e6647287cf99e0ab0cd8ee9b2048ac3b4af2c796981891d`.
This includes actual roads/profile rows,15terrain patches/31058active XYZ facets,
attributes/masks, whole supports/caps, logical headroom and source/frame bindings.

Both archives contain76NPY members/1048931uncompressed bytes. Stored NPZ occupies
1057733B, SHA256 `7078304b8da246d93a18a827793bb93b39d6823a7979f71243c1e3884fe9797b`;
DEFLATE occupies399175B, SHA256 `cad8b87c129c79ece4985df229592284abc675330ebfc0a563e85243a9bf8d9e`.
These are additional prepared terrain products; original inputs remain retained.

| Complete load | Profiled s | Positive allocations | Requested cumulative B | Tracked peak B | Process RSS B |
|---|---:|---:|---:|---:|---:|
| Current strict library |1.456714|3646871|518709230|21671878|126648320|
| Externally admitted stored |0.261642|632637|90192690|21377376|123383808|
| Externally admitted DEFLATE |0.267168|636403|103168038|21377397|123297792|

Stored avoids82.6121%of requested allocation bytes and82.6526%of positive
allocation events against the current strict path. Actual terrain constructor
calls fall15→0and replacement calls14→0. Remaining normal structural Mesh copies,
decode, query structures and unrelated road/support loading count. Input array
materialization increases152→168arrays/1894047→1933924B, including packed masks
and metadata. This is not an IO/copy-traffic counter. Full memory-bandwidth/copy
traffic remains unmeasured; cumulative allocation bytes are not resident RAM.
Tracked peak falls only294502B, about1.36%; RSS is a separate process high-water
mark. No substantial RAM saving, GPU/frame or device-budget result is claimed.

Two fresh unprofiled processes per method, in alternating orders, load in
0.655918/0.641374s(strict),0.109146/0.109758s(stored),
0.116448/0.116397s(DEFLATE). Imports, candidate preparation, independent admission
and result hashing are excluded. Lazy reference IO/hash, original binding hashes,
payload IO/hash/header/decode, all actual structures and original road/support
metadata are included. Measurement processes run at normal priority, sequentially;
shared-host load averages13.10–13.41prevent a device-speed guarantee. Python3.13.5,
NumPy2.2.4, Shapely2.2.0/GEOS3.14.1, SciPy1.18.1, Memray1.20.0, WSL2/i5-1135G7,
four exposed logical CPUs/16190028KiB RAM. No A18/RTX equivalence is inferred.

Stored is the small prototype's preferred load form: fewer allocation bytes and
slightly less CPU work. DEFLATE saves658558encoded bytes but requests12975348more
cumulative allocation bytes. This is a measured local tradeoff, not a universal
storage decision; #10's native compression remains independent.

The one-time work is measured separately, in fresh processes on the same frozen
commit. Preparation includes the current strict whole-state load, both archive
encodings, writes and hashes. Independent admission includes the older general
GEOS construction/composition and complete comparisons/queries for both variants.
Neither phase is part of the measured admitted hit.

| One-time phase | Profiled s | Positive allocations | Requested cumulative B | Tracked peak B | Process RSS B |
|---|---:|---:|---:|---:|---:|
| Prepare both candidates |2.024978|3699603|566340924|21742180|127401984|
| Independent admission |21.677416|32781251|4494659324|78932478|217104384|

These totals expose the deliberately expensive independent check; its cumulative
4.49GB is not resident memory. It must not silently return to a prepared-hit path.

## Checks, reproduction and remaining work

Twelve admission/roundtrip controls cover stored/compressed, byte order, immutable
buffers, actual new strict replacement, missing/wrong pins, changed geometry,
masks, attributes and frame, malformed/duplicate members, oversized false NPY
shapes, invalid indices/NaN/object arrays, stale original bindings and unrelated
terrain remaining strict. Rebinding the trust anchor is used only to exercise
structural guards, never claimed to validate arbitrary geometry.

125source and125installed controls pass, each with115current matching reference
imports. Both archive forms actually re-create the whole79th Radweg in each mode:
all seven road/terrain/planning/source artifacts match the original SHA256s.
Independent strict-before and cold-after readers remain outside fast restoration;
361points per precision preserve Double/Float32 index/scan results. The actual
consumer readers also use the admitted view:6648volumes,124whole supports,
128probes,65building bodies and4complete plants pass, with116matching reference
imports per mode. They restore once and reuse that same immutable view twice.
Each actual generation/reader closure has52matching reference imports.

The existing installed target is reused because no library file changes; wheel
SHA256 `db931057f4d6073cb89d83a1211c69d011c2ac85232d79ae6d5dac40120c6c05`.
Full consumer/control checks use the existing Python3.13.5/NumPy2.5.4environment
with Shapely2.2.0 and manifold3d3.5.4. An initial base-environment control attempt
lacked manifold3d and stopped; its failed log is retained. The complete125-test
run then passes in the established Buildings environment, without excluding tests
or installing a new package. All eight final gate processes and nine load
processes and both one-time phases have successful terminal statuses. Ruff and `make lint-docs` pass;
these are Python/contract checks, not native engine gates.

Run preparation on a normal verified snapshot, then the independent verifier.
Retain its successful digest separately as `PIN`; do not derive it from an
unverified adjacent report. In a clean checkout with the reference/experiment
PYTHONPATH and `TMPDIR=/tmp`, use `python -m stair_prepared_surface_load
--products SNAPSHOT --prepared-reference REFERENCE --reference-sha256 PIN
--method strict|stored|compressed --output FRESH_REPORT [--profile FRESH_TRACE]`.
Actual follow-up generation uses `stair_coverage_boundary_verify` with its normal
snapshot/reference79/fixture/output arguments plus `--prepared-reference`,
`--reference-sha256` and `--prepared-variant stored|compressed`.
Private original inputs, archives, raw traces, terminal statuses and import
closures remain under `/tmp/outshine-transport-prepared-e420edd39/`; the preceding
snapshot report locates the original source and complete79-source reference.

This prototype addresses repeated terrain validation/history. Generic query
structures and all numeric decode still cost work; no whole-world ready/cache-hit
or native integration claim follows. This experiment left #678 active pending the reusable
library path and its callers, now documented in the linked delivery. The original whole1280×720perspective
image was opened again; identical geometry is not a new visual improvement.
Sparse source coverage, contrast, coarse terrain, more transport classes/levels,
worldwide planning/LOD and actual device budgets remain open under #434/#48.
