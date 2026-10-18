# Installed flat-group height fitting

#732 under #50 moves the verified numerical core from
[PR #730](https://github.com/Weichwolf/outshine/pull/730) into
`outshine_ref.transport.level_heights.fit_level_heights`. The real 90-source
world, source identities, profiles, contacts, terrain and diagnostic image remain
unchanged. This is a reusable Python component, not native/world admission.

## Contract and ownership

The additive function uses existing `Mesh`, `HeightBudget` and `HeightFit`.
Explicit disjoint vertex groups replace source/geometric assumptions inside the
solver. The caller still selects spatial groups and owns frame/source binding,
topology, physical profiles and full downstream validation. Actual position
encoding and limits are specified in [the API contract](../../../lib/level_heights.md).

One shared implementation now contains both cone fits and bounded adjacent-height
repair. Experiment adapters retain geometric group selection and the separate
negative continuous/MILP comparison; they import numerical procedures from the
library. The old numerical copies are removed. The raw comparison also preserves
budget rejection before geometric grouping. The public kernel gives an already
valid exact input back without solver work and rejects unsupported ranges before
Float32 conversion.

The existing 212 Python files and four packaged data/type files are byte-identical.
Four Python modules are added, for 216 Python/220 packaged files in this wheel.
Existing root imports, dependencies, `Plan`, reservations and native contracts are
unchanged. Fresh source and installed processes import `transport` plus the saved
source reader without importing the numeric level package. Solver imports remain
deferred until needed; no SDK/foreign implementation is copied.

Fresh wheel SHA256:
`505af9a2b7fd5be66f82494a9c4b2b026bab26d82b4252b37c724c5de12bbc8b`.
Build uses a private copy of `pyproject.toml`, README and `src`, offline/no-deps/no
build isolation. Installation uses a fresh explicit target, no dependencies and no
bytecode compilation. ZIP, installed files and source bytes are checked individually.
Build staging, environments, images, reports and original traces stay in `/tmp`.

## Tests and actual whole products

Frozen package/production code `70cd92dc81948f42b3b6ea7e6edbefb944bef9b0`:
332 source +332 installed controls pass. Both modes check 137 actual library
imports against their declared roots. Related original height/weighted-height/port
tests are included; nine new API controls cover analytical feasibility bounds,
whole real geometry, exact groups, invalid identities/frames, oversized shapes,
unsupported coordinates, budget absence and invalid claimed solver success.

The complete independent input is versioned as
`test/outshine/data/transport/flat-group-height-input.npz` (11,140 bytes), with
source/recipe/producer hashes in adjacent JSON. It has all 520 input vertices,
765 anisotropic frames/faces and four groups/237 group vertices. Numerical input
SHA256: `b34c8358955a70521373fbe7d584a317e3bf7af9aa65cec0cb02a022a8949da2`.
No private input path is required by its library test.

Both source and installed generators reproduce **all seven** PR-#730 artifacts
byte for byte, with 59 matching library imports each. Saved readers independently
pass. Complete-field reconstruction again agrees on all 33,470 XYZ facets and
attributes, and on all module/local-field arrays. Full independent control takes
33.1283 s on the shared host; this is a diagnostic, not a load/frame budget.

Since all transport products are identical, actual verified PR-#730 volumes,
buildings and plants are reused as identical downstream inputs. Fresh source and
installed module/volume/Buildings/plant readers all terminate 0: 121 matching
library imports per mode and 19 successful coverage proofs. Actual 7,669 volumes,
65 building bodies and four complete plants retain their previous meaning;
the original 6,904-volume prefix and all source metadata remain unchanged.
No existing accepted verification receipt is overwritten.

Four fresh whole 1280×720 3D before/after perspective images are rendered and
actually opened with the unchanged master renderer. `test/scripts/pixels.py`
reports **zero changed pixels in all four** against PR #730. The sparse coverage,
weak contrast and coarse terrain remain its known limitations. Software-GL Blender
diagnostics do not prove native rendering or any device budget. No public image
or gallery is added.

Only the optional cost-CLI selection and experimental raw-budget adapter changed
after the package freeze; the package/production files remain identical. The raw
adapter and numerical cases are rechecked with 20 source +20 installed controls
on `57dc2b440`. All changed Python functions remain at most 120 lines. Ruff/format,
diff checks and `make lint-docs` cover this Python/documentation delivery; doc lint
declares its AGENTS-only scope. Native gates follow native integration.

## Equal-result cost comparison

The same complete experimental `rounded_joint` call is measured in the old
prototype and new source/installed adapters. Original 520/765 geometry, targets,
frames and plans are identical. Every unprofiled/profiler result is the exact same
canonical body, vertex SHA256
`3995e08dff88fad372e8002863a242e42422b4087704da2c1b9bc3e078217267`.
Three normal-priority unprofiled fits and one separate Memray trace per mode;
no selected favorable retry. Shared WSL2 i5-1135G7, four logical CPUs, Python3.13.5,
NumPy2.2.4, SciPy1.18.1, Clarabel0.11.1, Shapely2.2.0/GEOS3.14.1, Memray1.20.0;
BLAS/OMP threads one. Other agents remain active; our verification jobs finished
before the comparison.

| Reproducible CLI cohort | Prototype | Library source | Installed library |
|---|---:|---:|---:|
| Numerical/caller revision | `5a2814d16` | `68e39b7e6` | `57dc2b440` |
| Median of three fits, s | 0.10770 | 0.12698 | 0.07162 |
| Memray allocation events | 106,163 | 103,791 | 103,791 |
| Cumulative requested bytes | 17,707,311 | 17,715,155 | 17,715,155 |
| Incremental tracked heap peak, bytes | 3,385,854 | 3,414,481 | 3,416,403 |
| Actual library imports checked | 55 | 59 | 59 |

A preparatory direct-call cohort of the same committed fit branch measured
0.07023/0.07009/0.07101 s; its complete records remain retained. Times vary and
do not establish a speed change or device budget. The source and installed
packages are byte-identical. Cumulative requested bytes rise by 7,844; the final
trace peak rises by about 29–31 KB. These bounded costs buy checked owned group/
frame snapshots before solver execution; the duplicate geometric group query is
removed. Frame payload here is 24,480 bytes, group membership 1,896 bytes.
Required validation is retained; no equal-result runtime optimization is claimed.

Timers/traces include the full fit call: group conversion/validation, matrices,
solvers, candidate checks and repair. Source/body preload, explicit solver imports,
independent observation, hashing and export are excluded. Demand-loaded numeric
package initialization is included in the first unprofiled fit; subsequent fits
and the trace use the loaded module. Older scope strings saying just “imports
excluded” refer to explicit solver imports, not this demand load. Allocation is
requested bytes, **not copy traffic**. Internal copy traffic is unmeasured;
process RSS includes preload and is not the incremental trace peak.

## Reproduction and remaining scope

API controls: `PYTHONPATH=lib/src:lib/tests python3 -m unittest test_level_heights`.
Repeat with the fresh wheel target first in `PYTHONPATH` for installed controls.
Use the original baseline chain and whole recipe/products from the linked PR-#730
report for `stair_local_generate.py`, its saved reader, full reference and display.
The explicit per-mode import manifests and complete commands/terminal logs remain
under `/tmp/outshine-transport-level-library-70cd92dc8/`.

`stair_joint_height_cost.py --method rounded_joint --repeats 3 --profile`, with
its existing resident/prior-module/fixture/products/output arguments, selects
the full relevant fit without unrelated alternatives. To repeat the old comparison,
copy that committed driver outside both checkouts, run it in the old checkout
with that checkout's `lib/src`/experiment paths first, then in the new source and
fresh installed target. The imports are resolved from the chosen checkout/target;
verify their paths and hashes. `--output` must be fresh and explicit under `/tmp`.
The default still executes all four height alternatives. The fixed public fixture
also permits isolated calls to the new API without any private world snapshot.

Geometric/precision references, original negative controls and algorithm limits
belong to the [preceding method report](stair_free_terrain_comparison.md).
Library adoption does not increase source coverage or resolve general binary64/
affine topology (#568), regional encoding (#717), full worldwide planning (#434)
or native/device admission (#48). These and the full goal remain active.
