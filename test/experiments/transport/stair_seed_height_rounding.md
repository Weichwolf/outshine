# Repair stored heights of free ground seeds

Issue #1731 fixes an actual false rejection from the shared-service reference.
The complete curved source part `9609826400:1` is valid in Double but violates
the unchanged profile limit after its heights are stored as Float32. The existing
finite height contraction now runs automatically for that failure in one free
open seed. Already valid geometry returns unchanged, without a new witness.

## Cause and method

The original 36-vertex/47-face fit has maximum normalized grade0.993340.
Float32 storage makes two faces invalid, maximum1.004621. One boundary stencil
contains a0.5mm edge and has area0.0003624m²: small independent height rounding
errors are amplified by the thin triangle. Source XY is already Float32-exact.
The stored-position/profile audit is correct and remains strict.

Reuse the existing seven contractions from the closed-seed procedure. They
scale free heights about one rounded level, preserving source XY, topology,
width and profile limits. Each candidate must stay within5mm of the original
valid continuous fit and pass independent Double AND actual Float32 grade checks.
Choose the first valid candidate; no convergence or global optimality claim.
The generic numerical helper moves to `stair_height_contraction.py`; every
versioned caller is migrated. Closed-seed validation stays with its own witness.

Only a single open source with no fixed heights or junction planes enters the
new fallback. Valid stored heights keep the original object and all data values.
For an invalid stored result, exact Float32 XY and a valid Double fit are required.
Pins, multiple sources and closed seeds retain their existing procedures.
The initial fit and correction share the existing8s height-fit budget; at most
seven fixed candidates are tested. Time is checked between bounded candidates,
not a hard frame deadline. No additional solver, MILP fallback or runtime job.

The new product stores the original Double heights as an explicit witness.
Its independent saved reader verifies original validity/actual storage failure,
free-source ownership,5mm budget and the complete corrected geometry in both
precisions. Normal source coverage, complete outer profiles, closed body,
terrain contacts, support clearance and source/unknown guards still apply.
The raw continuous objective/report remains the original fit's report; the
repaired surface is not claimed to preserve its optimum.

SIGGRAPH-first background:
[Deering, Geometry Compression,1995](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
PDF page2 visually inspected: reduced representation precision needs numerical
analysis. No quantization constants or compression algorithm are imported.
The actual method is the existing [closed-seed contraction](stair_closed_ground_comparison.md);
the independent alternative is the existing [finite level-height repair](../../../lib/level_heights.md).

## Whole-source result and preserved controls

The original fixture `stair-service-shared-profile-refusal-vienna.json` is unchanged.
It remains the historical failure input; current default construction now repairs
its height-storage failure. No separate recipe mode or altered limit is needed.
Its original MVT/DEM/source IDs and tags remain bound to the same data.

The earlier complete106-source state, including the straight shared-service asset
from #1727, becomes107 sources. All26 old assets/179 arrays/950,908B are unchanged.
The complete new curved body has36 surface vertices/47 triangles and140 closed-body
triangles, volume97.278217/97.278069m³ in Double/Float32. Eight actual curved body
checks cover both centered directions and both precisions. Four whole straight
body checks and the full original outer profiles also pass.

Scale0.99 changes heights by at most0.001162999824m. Corrected maximum normalized
grades are0.983406/0.995098. Source XY and topology match the original continuous
mesh exactly. A fresh independent continuous solve reconstructs all original
Double witness heights bit-for-bit on this host; a fresh contraction reproduces
the complete delivered vertices. No cross-platform solver-bit-identity claim.

Terrain work selects134 old faces and produces151 points/258 faces, with42
boundary vertices and109 finite pins. Existing full terrain/support/body/contact
checks remain required. Native admitted-source IDs stay empty; this is not a
new source-closure, endpoint, routing or dynamic-traffic qualification.

Fresh straight shared-service and old one-way production retains all23 array
fields per case (12,750/25,696B), plus each case's three planning/catalog/key
documents. Seventy focused checks cover the new real-face witness, time/repair
budgets, overflow, unchanged XY, invalid continuous fits, malformed witnesses,
pins/junctions/rings and existing seed/one-way/curve/level paths.

## Alternatives and costs

Both existing methods receive the same complete raw fit, unchanged physical
limits and5mm correction budget. The finite adjacent-code method finds no
improving candidate after54 proposals/432 incident-face tests. This is failure
within its bounded candidate space, not proof that no quantized solution exists.
The contraction produces a valid whole product after two trials. These outcomes
are different; their timings are not an equal-result speed comparison.

Frozen bce3ff516, Linux/Python3.13.5/NumPy2.5.4/Memray1.20, BLAS/OMP1, host load
16.21/15.66/13.98: nine rotating normal-priority CPU/wall rounds and separate raw
allocation traces. Contraction/adjacent CPU medians1.305/2.489ms; allocation
events2,547/4,760; requested bytes365,874/596,095; scoped high watermark
18,920/48,054B. Only contraction succeeds here. No portable/native winner claimed.

The contraction wrapper performs two prechecks plus four candidate grade passes:
282 triangle-gradient evaluations on47 faces. The successful result copies two
864B candidate vertex matrices; three Float32/Double XYZ roundtrips copy at least
3,888B. Other arrays/copies and physical traffic are unmeasured. The saved raw
height witness adds288B. These warm Python scopes exclude IO, continuous fitting,
held inputs/expected results, total process/UMA peaks and native frame/device time.
Full local production took3.268s and independent reading25.755s under development
load; neither is a native loading/frame metric.

## Images and reproduction

Matched actual whole transport/terrain/support mesh diagnostics at1280×720,
16-sample Eevee were rendered and opened. The complete curved paving is added;
the existing road remains. Coarse terrain and independent endpoints remain
visible limitations. Pixel changes182,629, of which54,784 exceed1/255, include
terrain/shading and are not solely attributed to the new cap. Blender5.2.2/
llvmpipe is a geometry diagnostic, not the native Places/device gate. Images
remain in system temp; no README/doc image publication.

Use `.venv`, one BLAS/OMP thread and PYTHONPATH containing `lib/src` plus transport,
assets and render experiment directories. The cost driver also uses the existing
buildings `allocation_measure` helper. No new packages needed.

Existing `stair_local_generate.py`/fresh `stair_local_verify.py` take the unchanged
fixture, resident102, stored105 prior and the accepted shared-service prior.
`stair_seed_height_study.py` takes those `--resident`, repeatable `--prior-module`,
`--fixture`, the saved `--local` and fresh explicit system-temp `--output`.
It reconstructs the continuous opponent and exports actual scenes for the existing
`render_transport_research.py`; compare with `test/scripts/pixels.py`.
`stair_seed_height_cost.py --local ... --fixture ... --output /tmp/outshine-...`
records both outcomes and raw allocation traces on a frozen source tree.

Focused unittest modules: `stair_seed_height_checks`, `stair_service_checks`,
`stair_shared_service_checks`, `stair_seed_checks`, `stair_closed_seed_checks`,
`stair_southwest_checks`, `stair_road_curve_checks`, `stair_frontier_checks`,
`stair_joint_rounding_checks`, `stair_quantized_height_checks`:70/0. The fixed
actual face is versioned under `test/outshine/data/transport/`; unit tests require
no private outputs. Original full MVT/DEM fixtures are unchanged.

Evidence: `/tmp/outshine-research-service-gradients-d5fcb33d0-evidence/`.
Source/endpoint closure, geographic coverage, building/plant interaction, native
assets/World, routing/dynamics and both devices' p99/8-GB-system budgets remain
separate. This completes one bounded reference improvement, not the full goal.
