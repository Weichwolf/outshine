# Whole-source XY clip exclusion before fine view demand

Owner: outshine_research, #1873 / #1797. Native whole/clear controls: master #1875.
`source_clip_demand.query` is a new explicit view-only reference entry point.
The existing selector/hierarchy remains its fallback and retains its old contract.
No runtime default, GPU ABI, AssetCache format or shader changes.

## Contract and bound

The previous entry point returns the whole original if any source vertex crosses
its supported clip interval. The new entry point may instead return empty when
one strict root bound excludes the ENTIRE immutable source from an XY halfspace:
`x+w < 0`, `w-x < 0`, `y+w < 0` or `w-y < 0`. These are homogeneous clip values,
not projected pixel coordinates. No division or positive-w approximation is needed.
Every triangle's clip coordinates are convex combinations of its vertex outputs;
if an upper bound is negative for all source vertices, no part enters that plane.

All original positions, including unreferenced vertices, determine the root box.
Preparation already verifies/fixes the complete source and records whether every
position is finite normal-or-zero Float32. Actual static placement, shift and view
upload words use the existing layouts. This is the existing static litVertex order:
`stable=M*(p,1); world=stable+(shift,0); clip=V*world`. No position deformation,
other vertex program, invented SourceID or unknown generation is admitted.

`clip_envelope` is extracted from the existing coarse-margin implementation, with
unchanged arithmetic/overflow decisions. Root center/extents enclose the exact
binary64 affine result, expanded by64epsilon64 norm guards and directed rounding.
Positive norms bound the complete source, with gamma7=7epsilon32/(1-7epsilon32)
for four multiplies/three additions, propagated through both matrix stages and
shift. Additive minimum-normal terms cover the existing bounded underflow model;
subnormal source/upload operands remain unsupported. The coarse factor two and
upward norm guards remain. Overflow or nonfinite bounds yield no proof.

The final XY plane sums are evaluated outward with64epsilon64 scale slack. Only
a STRICT negative upper bound admits empty. Equality and a one-ULP outside input
may remain unknown; partial cuts and uncertain bounds call the old hierarchy.
No near/far exclusion is inferred without a bound pipeline depth-clip/clamp contract.
The existing viewport-covering raster-guard Whole policy remains unchanged.
`complete_model=true` describes this conditional arithmetic model;
`qualified_gpu_clip=false` and `qualified_gpu_raster=false` remain explicit.

The new empty result has reason `source_outside_xy_clip`, no original ordinals,
no leaf traversal and no fine position/index access. Source bytes remain resident
in this experiment; no physical residency/IO gain follows. The old hierarchy already
avoids fine projection for these seven Whole cases: the new gain is avoiding the
whole-original VIEW demand and its ordinal output, not seven full vertex scans.
View emptiness does not discard off-camera shadows, simulation, collision or state.
World admission and union of those purposes remain with #1797/#1853.

## Finite input and cost

Whole existing19-operand/1558-source input:78374 vertices,38476 triangles. All four
bound native pose/guard candidates remain unchanged. Twelve synthetic rotations
use the existing fixed near-direction upload packet and `changed_frame` at0…330°
in30° steps. The five previously supported cases stay unchanged. The seven old
Whole cases90,120,150,180,210,240,270° have strict Bottom exclusion;90…150° also
Left,210…270° also Right. At180°, Bottom upper is-63416.37570351314 in homogeneous
coordinates.63.5° is a partial/unknown Whole control and is not discarded.

Frozen measured source `b55640225087de4e180b705f68bf6094861010e7`, including Master's
#1874/76aacf63c fixed four-term projection order. Linux i5-1135G7, NumPy2.5.4,
Memray1.20.0, BLAS/OMP1, concurrent host work, load3.92/5.10/5.81. Seven timing
rounds and a separate allocation trace; no isolated/native/device speedup claim.
Held hierarchy binary payload2287276B is separate from scoped query peaks.

| Twelve-query turn | Old hierarchy | Clip entry point |
| --- | ---: | ---: |
| Whole fallback requests | 7 | 0 |
| Sum requested original view triangles | 273844 | 4512 |
| Sum output ordinal bytes | 1095376 | 18048 |
| Wall ms | 487.433 | 494.422 |
| Allocation events | 118769 | 126207 |
| Requested allocation B | 845280134 | 838833145 |
| Scoped peak B | 9666674 | 9514710 |

No CPU speedup: the root proof adds metadata work. Output ordinals fall1077328B
across the sequence; this is not simultaneously resident memory or physical traffic.
The roughly152kB peak difference is scoped to this harness and retained previous
results. Both old/new read zero fine rows in the seven cases. The proof uses six
root coordinate components (24B unique bound payload), plus the existing frame
operands; those counters are not physical load/copy counts.

For supported bound cases old/new ms: near guard0 53.112/45.089, near guard1/256
44.776/46.158, side guard0 66.423/65.662, side guard1/256 67.516/68.535. Timings
are noisy; same final candidates, modest extra root work, no improvement asserted.
The initial pre-#1874 run is superseded; the projection algorithm changed, so the
actual cost comparison was repeated after integrating Master's fix.

## Reproduction and native control handoff

Use the existing project `.venv` (NumPy/Memray), with
`PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`.
Run `python test/experiments/render/compare_source_clip.py --output /tmp/outshine-clip-<commit>`
from clean measured sources. Ten final allocation traces/report.json are retained.
Control-only export: `python test/experiments/render/prepare_source_clip_controls.py --output /tmp/outshine-clip-controls-<commit>`.

The control exporter writes the unchanged whole original attributes/material slots,
explicit typed empty slots,13 UInt32 view-word files, placement words and SHA-bound
controls.json. Native `MaterialFieldInput::Prepare` correctly rejects empty geometry;
the empty slots are NOT a claimed working native input. Native #1875 instead runs
whole-input.json and clear-input.json with the same source/queries and only the
top-level `omit_source_draws` test flag changed. The owner forwards the same pass,
clears, compute filter and readback, suppressing only draws with actual counters.

Capture JSON retains the original exact eye/lens/raster and rotates the basis by
transpose(Ry).0° is the original visible control;63.5° is the Whole partial control.
Lighting observer stays original. Capture reconstruction `P*rotatedBasis` may
produce different Float32 words from the separate synthetic `baseVP*Ry` model.
Native controls MUST capture their actual operands; rebind the proof to those
words without tolerances. JSON requests are not new native upload captures.
Only control JSON/export tests changed after the frozen query cost measurement.

Current checks:63 focused cases, terminal0 in48.320s after #1874/#1876,
including exact Capture/flag/SHA/whole-source export and native-word rebinding. Independent rational box-corner planes
and explicit FP32 multiply/add in both term orders bound against the envelope;
all78374 original vertices independently satisfy Bottom in the seven examples.
Strict four-side exclusion, equal/near-boundary, partial, subnormal, NaN, overflow,
invalid limits, unreferenced points, guard-Whole and typed empty material-slot
exports are checked. No existing error limits or oracles changed.

Own #1320 acceptance: isolated Python view-demand reference, immutable prepared
source lifetime, metadata proof before old consumer/fine work, explicit Whole
fallback and conditional arithmetic assumptions. Callers keep borrowed view/placement
words stable throughout the synchronous query. Old entry point still owns its
consumers. No runtime cache/queue/publication mechanism. The finite native evidence below completes the bound control cases; general
hardware raster/World admission and actual avoided IO remain unproven.


## Finite native qualification

Master #1875/#1876, Native source33cb5aac7, integrated291189671: A18/Metal validation,
13Whole/Clear pairs. Seven outside captures are bit-exact in HDR/depth/integral;
five visible and65° partial controls differ. Each pair forwards/omits three actual
draws. Partial65° has46samples,10at the left edge and278nominal crossing triangles.
0° matches the existing original HDR/depth hashes; empty Geometry stays rejected.
Actual native helper/current CDB and shader provenance are Master's acceptance:
https://github.com/Weichwolf/outshine/issues/1875#issuecomment-6537474639.

`native-clip-controls.json` is the fixed native owner's reported Lens/pre-view
record from #1873/6537452288, not a GPU-buffer readback. `native_source_clip_controls`
checks the source manifest and exact UInt32 words before querying; image/counter
fields only score results afterwards. Native matrices differ0–6words from synthetic
requests. Rebinding all13actual native word sets gives the same seven strict Empty
results; five visible cases and65° retain nonempty/Whole. Source mismatch or malformed
word payload is rejected. No tolerance, pixel mask or ID oracle enters demand.

Supplemental existing-binary Iris/TGL GT2 composition at frozen7780350a4, terminal0:
`native_source_clip_composition.py` renders a visible two-triangle terrain opponent
alone and with the unchanged whole source, using0°,65°,90° cameras. Raw2560×1440;
background fills3686400samples in each case.0° changes204pixels,65° changes46;
90° changes zero and HDR/depth/integral words are all exact. Actual reported Lens
operands are equal across each pair and feed the CPU proof, not synthetic words.
Native copied program SHA b32442c8c71d6820e9731126f29c68588663975f29e259d1f38a2cfb9b0c95d2,
observed source SHA ad19f31d8576b74260521688743f98b38199059ca30f01f75f0c44c558e003f7.
These are existing-binary observations, NOT fresh CDB/native-owner Iris acceptance.
Both actual validation-layer executions have zero errors/warnings; all552SPIR-V
files and the binary stay byte-identical before/after. Diagnostic pictures/crops
opened locally (fixed1/32 exposure, nearest enlargement of native source pixels).
The new entry point preserves visible/partial detail and excludes only the empty
outside view; no World publication, shadow/simulation or frame/p99 claim.

Reproduce supplemental composition with the existing prepared native program and
matching loader/validation libraries: `python test/experiments/render/native_source_clip_composition.py --binary <existing-program> --shader-directory <its-shaders> --observed-native-driver-source <observed.cpp> --output <fresh-system-temp>`.
No native files/builds are changed by this driver. Actual full native commands,
ICD/layer/environment, input hashes and readback comparisons stay in report.json.

Primary starting points: SIGGRAPH2015 GPU-Driven Rendering Pipelines coarse CPU
selection (page14 previously inspected) and SIGGRAPH2000 Conservative Visibility
Preprocessing using Extended Projections (page2 inspected optically this block).
The latter distinguishes outer occludee bounds from opaque occluders; we use only
an enclosing root clip test, not its occlusion-map/PVS algorithm. The floating-point
bound is inherited from the explicitly conditional existing projection model.
