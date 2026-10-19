# Native capture cost, separated into Whole and cache-cell calls

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12), original producer
and partitioning context [#567](https://github.com/Weichwolf/outshine/issues/567).
This qualifies the [local-bound result](building_trace_bounds.md). Its large
Whole-ray count is not a measured saving for the current cache-cell path.

## The scope correction

The original global bounds contain the eye for Wien-bbd9 sources 447/448 and
Flensburg sources 2880/3042. The earlier complete Whole-grid census identified
a 36,869,177-part-call geometric avoidance opportunity for these four sources.

The new `cell_capture_membership` audit checks all five original packets,
9,409 source records and every actual accepted cell projection group. **None of
those groups contains any of these four sources.** Instead, each source has a
successful Fine mesh call with openings: bbd9 cell 32, Flensburg cell 35. They
are not missing from the cell output. The three other parents have no applicable
eye-containing projectable source in the original input.

Consequently, the 36.9 million-call mechanism contributes no saving to those
accepted cell projection groups. This does not prove that local bounds cannot
help other cell sources. It withdraws the quantitative basis for prioritizing
that candidate as an immediate cell-path fix. Keep it as a bounded Whole/future
planning reference; source-complete hierarchy before cell emission remains #12.

## Unchanged native executable, instrumented from Python

`sample_building_capture.py` launches GDB with an existing #801 normal-mode
exporter. No native code is compiled or changed. The executable's SHA-256 is
`012d9aa8c525e98f46466d99ba30f8c5273dd4040fb012b92f5733e2979a7d8d`;
its native experiment sources are `ee24b7f72`, producer basis `4be90897a`.
The relevant original capture/trace/hierarchy code matches `c453ef4dd`.
The #801 source-closure evidence remains with its native owner.

This host denies even perf task-clock events (`perf_event_paranoid=3`). GDB
16.3 child tracing works without changing that setting. The Python helper reads
inferior user/system CPU ticks, with a reproducibly jittered mean trigger near
two ticks; HZ=100 on this host. Its worker interrupts only its own inferior
through a pidfd. GDB consumes those signals. The worker never calls GDB APIs.
This is intrusive sampling, not perf, instruction counting or exact CPU shares.

Internal entry/return breakpoints label each diagnostic `Bake` wrapper call.
Its original producer calls Whole once, then each original cache cell once
(`export_building_selection.cpp` at `ee24b7f72`). Completed marker counts must
match every exported result. Samples outside the wrapper stay outside; input
decode and final report writing cannot be charged to the last cell. Every
sample also has its CPU tick and complete observed stack, capped at 64 frames.
Thread count is checked at each sample. The helper refuses unexpected stops,
unfinished phases, worker failures, output changes and incomplete profiles.

All **225 wrapper entries/returns** across four original parents match their
results. All 6,788 samples agree independently with presence/absence of the
wrapper in the stack and lie within their labeled phase's CPU interval. Maximum
observed stack depth is 34, below the cap. Normal process exit is zero throughout.

## Actual cell-path stacks

These are counts of interrupted stacks containing the named function, not
counts of calls, allocations or instructions. Capture contains source tracing
and hierarchy work; columns overlap and must not be added. Small sample counts
and intrusive scheduling limit precision; no confidence or speedup bound is
claimed.

| Original parent | Phase | Samples | CaptureFaces | BuildingSurface::Trace | PlanHierarchy::RayEntry |
| --- | --- | ---: | ---: | ---: | ---: |
| bbd9aab743 | Whole | 714 | 683 | 414 | 199 |
| bbd9aab743 | Cells | 356 | 333 | 256 | 53 |
| a169a8d0d6 | Whole | 2,675 | 2,657 | 1,529 | 826 |
| a169a8d0d6 | Cells | 982 | 965 | 659 | 222 |
| 6d41e63e73 | Whole | 1,357 | 1,345 | 971 | 256 |
| 6d41e63e73 | Cells | 611 | 596 | 455 | 97 |
| 035e2150a3 | Whole | 33 | 29 | 6 | 17 |
| 035e2150a3 | Cells | 40 | 27 | 15 | 11 |

Twenty further samples are outside Bake. The current cell capture remains the
clear CPU producer hotspot in the three larger runs, independently of the four
Whole-only sources. Cell patch construction appears in 1/6/8 samples for
bbd9/Flensburg/6d41 respectively; absence from a sample does not mean zero cost.
Samples identify source tracing broadly, not which source/part or fraction can
be eliminated. Do not multiply the earlier 37-million figure by these shares.

## Separate ordinary native baselines

Machine: Intel i5-1135G7, eight logical CPUs. Runs are serial, nice 10, with
concurrent interactive/native-analysis load (load averages about 4.4–6.7).
Ordinary runs use the same unchanged executable/environment and original inputs.

| Parent | Whole reported elapsed s | Sum of cell reported elapsed s | Full-process user+system CPU s | Full-process wall s |
| --- | ---: | ---: | ---: | ---: |
| bbd9aab743 | 12.992 | 5.490 | 18.560 | 18.574 |
| a169a8d0d6 | 41.883 | 16.596 | 58.596 | 58.710 |
| 6d41e63e73 | 27.930 | 12.417 | 40.227 | 40.600 |

Reported elapsed fields are the existing inner native bake wall clock including
observer callbacks. Profile phase markers bracket the wider diagnostic wrapper,
including its result construction. Full-process CPU includes shell/exec/loader,
input decode, both bake modes, observer/report work and output. No subtraction
of overlapping/asynchronous work is used. Instrumented elapsed/child usage also
includes debugger effects and is not substituted for an ordinary native time.

These are frozen-input CPU producer diagnostics, **not application asset-cache
hit loading, frame times or a Vulkan/Iris/A18/world budget result**. No algorithm
has been optimized here. Allocation/copy counts, resident/peak memory and GPU
time remain unmeasured. Inherited `ru_maxrss` is not used as a native peak.

## Equality, dependencies and reproduction

All original exported fields match independently, excluding **only each direct
result's `elapsed_s`**. That includes sources, nullable metadata, errors,
accepted groups, representations and geometry counters. This export does not
contain full vertex buffers or runtime pixels; equality is scoped accordingly.
Five comparison controls accept timing-only changes while rejecting a changed
source ID, built bytes, mesh acceptance and an extra nested elapsed field.

Dependencies are Python's standard library and installed GDB. On this host the
frozen native binary requires its already secured libtiff/SFCGAL environment;
without it, the loader rejects LIBTIFF_4.7.1 before main and the profile correctly
fails. Environment-script hashes, mapped library paths/hashes at main, binary,
input/reference and Python hashes are retained. No package or kernel setting
was changed.

```sh
bash -c 'source "$1"; shift; exec "$@"' bash "$native_environment_script" \
  nice -n 10 .venv/bin/python test/experiments/render/sample_building_capture.py \
  --binary "$existing_native_exporter" --input "$original_input_json" \
  --reference "$original_native_trace" --output "$fresh_system_temp_result"
```

The existing `verify_building_trace_bounds.py` additionally reports cell
membership; its original hit/part/point and complete Whole-ray checks still
pass on all five inputs. Evidence is under
`/tmp/outshine-research-capture-profile-692327c0c/`: `phase-runs.json`,
`*-phases/receipt.json`, raw `stacks.json`, `phase-summary.json`, ordinary
baselines in `runs.json`, and `source-membership/` with five terminal receipts.
Unpartitioned preliminary profiles are superseded by the phase-specific runs.

Next decision needs actual cell source/part/query counts or a bounded native
candidate comparison on these unchanged groups, plus original output and full
world image/cost checks. Source-complete demand before cache-cell emission,
movement, shadow/reflection/interaction needs and target-device gates remain
open. This audit changes priority and measurement scope; it is no product fix.

The [subsequent first-callback audit](building_capture_prefix.md) now supplies a
native-backed lower bound for part work in actual cell captures, with verified
native trees, first callbacks and returns. Its separate cell-specific local-bound
opportunity does not reuse the Whole-only count or establish a native speedup.
