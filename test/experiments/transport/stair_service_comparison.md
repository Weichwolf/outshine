One supplied one-way service carriageway now extends the complete 105-source
Transport -> Buildings -> Vegetation experiment to 106 sources, 65 actual
building bodies and four whole SITE plants (#795). The added source is the
entire 38.556 m original MVT line `4347772092:0`, including its `access=no` tag.
Geometry does not grant public access: `public_routing_admitted` remains false
and native `admitted_source_ids` remains empty. This is an explicit Python
recipe, not automatic service discovery, a connected public route or native
integration. The sparse regional network and diagnostic materials remain.

The original inputs and prepared 102 -> 105 handoff are those of
[prepared growth](stair_prepared_growth_comparison.md) and
[the full consumer sequence](stair_candidate_plants_comparison.md).
No old receipts, source data, pins or limits are rewritten. Both new fixtures
reference the original Vienna fixture by SHA256 and preserve its full catalog.

## Procedure and alternatives

The original omission was semantic: the road recipe only recognized `minor`
with an `unclassified` two-lane profile. Relabeling a service source would lose
its semantics and use the wrong 5.5 m width/two offset vehicle paths. The new
`oneway_service_carriageway` role explicitly requires class `service`, ordinary
ground level and a supplied integer direction +1/-1. Its 3.5 m width, 4.5 m
clearance, 5.56 m/s speed and 0.03 crossfall limit are procedural assumptions.
The existing service gradient limit is retained. Missing direction does not
invent a bidirectional shared lane; private access does not invent a public
walk or vehicle portal. Existing minor/two-lane products retain their values.

SIGGRAPH-first basis is
[Chen et al., Interactive Procedural Street Modeling, 2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
whose page 3/Figure 3 was inspected optically: separate network/source decisions
from geometric realization. This work reuses the installed
[horizontal curve kernel](../../../lib/horizontal.md), existing bounded swept
rigid-body checks and exact straight prismatic sweeps; it adds no curve solver.
No foreign implementation is bundled or published cost factor adopted.

One lane has offset zero and follows the supplied source direction. Internal
curves use the existing symmetric G2 clothoid plan at 6 m minimum rear-axle
radius; the actual source-owned cap must contain the complete compact body.
Straight intervals use the full body hull, not centerline containment. Source
end axle positions stay inset by front/rear body extent plus 0.05 m. There is
no claim for traversing an endpoint, junction, 3D chassis or dynamic steering.
An explicitly overridden vehicle profile also governs curved sweeps.

Of 181 previously unused service lines, 28 supply one-way direction. Sequential
filters leave 19 non-ground sources, 138 without the required direction, four
outside the terrain, ten needing explicit old-ground contacts and ten separated
candidates. Three longer separated bent lines lack the original straight length
for the fixed 6 m curve recipe; this is a recipe refusal, not impossibility.
The retained curved fixture succeeds. The whole 32.896 m straight source
`3512792022:0` is refused with `actual module source gradient exceeded`; its
complete catalog/baseline independently verifies as retained. No bound is relaxed.

## Correctness and consumers

Initial measurements/producers use `56ab9a5e03df4220658647f7c8773067fe803fac`.
Final Source/Installed controls, transport regeneration, installed independent
reader and plant replay use `b32f1536f3756052aed8e1e017d4c9876a10a9a7`, including
the explicit vehicle override fix and current master `0b788bd80`.
The latter brings master's native water work; this feature changes only Python
experiments and fixtures, with no shared Plan, reservation or native API change.

Source and an isolated freshly built wheel each pass 47 controls: service,
seed, closed seed, southwest, road curves and frontiers. These cover both lane
directions, private/source attributes, ambiguous directions, wrong class/level,
old two-lane choices, oversized bodies, short/bent axes, actual curved seed
geometry and an overridden vehicle radius that must refuse. The wheel SHA256 is
`bfd91d01f864ef8d16e9f535706a7aae28e979766fd2d51884ce6945f0737cae`.

Independent saved-product readers bind the original source/profile, masks,
whole cap/body, heights, all necessary terrain and piers, in Float64/Float32.
The new cap has 65 vertices/92 triangles, the closed body 256 triangles and
volume 41.597292/41.597420 m3. Maximum normalized longitudinal/crossfall grade
is 0.73513/0.71612. Each precision checks 36 curved poses/18 bounded hull
intervals and two exact straight interior sweeps. Original source area and
topology are preserved within the existing 2.9296875 mm outward grid model.

An independent whole-field construction checks 34792 exact XYZ facets and
face attributes. Its module/local-layout/local-field arrays are bit-identical
to the prepared-snapshot path, the isolated Installed producer and the final
regeneration. Their common semantic array SHA256 is
`6f0a1cb675240db1f6b5e8845d7bab1c8ebb40c56bc62d0b71ac3f69f3fca437`.
The whole control uses the original resident53/ordered chain from the handoff;
the local path uses resident102 plus stored105. These have distinct bindings:
an attempted mixture correctly refused before construction. Old receipts stay.

The unchanged Buildings producer and fresh Installed full-scan reader check
110 original ledger entries, 100 valid polygon parts, ten invalid originals,
64 original bodies and 65 adjusted bodies with original roots/attributes/heights.
The plant producer and Installed independent reader replay all 394 habitat
parts, all 1024 proposals, their ordering and four successful attempts. Four
complete unchanged Pinus forms pass all SITE/whole transport/building conflicts.
This proves this complete finite input, not ecology or worldwide coverage.

## Work, bytes and measured costs

Linux i5-1135G7, Python 3.13.5, NumPy 2.5.4, Shapely 2.2.0/GEOS 3.14.1,
Clarabel 0.11.1; one BLAS/OMP thread. Normal cost processes have normal priority;
tests, rendering, consumers and allocation profiling use nice 10. Concurrent
research/building/test work makes these single observations, not a speed ratio.
The two constructors produce the same array hash after input/source preloading:

| Work/cost | Local construction | Whole diagnostic control |
|---|---:|---:|
| Construction wall time | 5.810 s | 63.271 s |
| Whole-process peak RSS | 141389824 B | 219197440 B |
| Context terrain faces | 127 | complete field |
| Unchanged outside XYZ triangles | retained | 34575 compared |
| Explicit retained terrain mesh copy | 0 B | complete diagnostic assembly |
| Global XY/label work buffer | avoided | 422112 B |
| Explicit outside XYZ compare buffers | avoided | 4978800 B |

The new patch replaces 57 faces by 217; terrain solve eliminates 101 fixed
heights and solves 41 variables/370 rows/299 A nonzeros. Both paths retain the
same patch and constraints. The local road window has three old sources,
300 body faces/13408 numeric bytes; its queries test 25 resident asset/patch
bounds. Replacement allocates a 24388 B old-face mask. Explicit terrain
conversion/datum copies are 2088 B and validation XYZ conversion 7140 B.
These named counters are not a complete GEOS/internal-copy census.

Separate Memray 1.20.0 local construction captures 6521887 allocation requests
(including zero-byte requests), 833838888 cumulative requested bytes and
1033836 B tracked heap peak after inputs/imports. The profiled output has the
same array hash. RSS includes preloaded state; tracked heap and RSS are not
additive, and requested bytes are not copied/live bytes. Native stacks are
disabled, Python allocator events enabled. The attempted whole allocation trace
hit the unchanged 20 s clearance check budget under instrumentation and refused;
there is no completed whole allocation comparison. Its normal control passes.
No profiling budget is raised; that failed trace is discarded, log retained.

Actual plant search performs 1418 distance calls/11966502 facet tests/80215
geometry candidates and 53781 logical materializer invocations (including cache
hits). Its bounded cache retains 149 facets after 54101 hits/149 misses.
End-to-end source plant production including reads/prototype validation/export
takes 21.641 s under concurrent load. These Python costs grant no native
warm-load, frame, A18 Pro or Iris Xe/Vulkan admission.

## Images and reproduction

Five private 1280x720 Eevee/16-sample diagnostics were rendered and opened:
matched before/after whole transport, matched 80 m near views and the complete
106-source/65-building/four-plant world. Blender 5.2.2 uses llvmpipe; the existing
renderer verifies source topology, Float32 positions, supplied normals and face
colours. The near image shows the complete bent carriageway and separate old
road, without claiming an endpoint connection. Whole transport changes only
251 pixels (82 by more than 1/255), box x1136..1165/y283..310; no black pixels.
Near changes also include local terrain triangulation/shading. Sparse coverage,
coarse terrain and flat diagnostic materials remain visible. No images enter
README/doc and these are research images, not the native Places gate.

Private current evidence is `/tmp/outshine-research-service-roads-c91a94d56/`;
required original handoff inputs remain under
`/tmp/outshine-transport-resume-1f10d55eb/`. Run from the checkout using `.venv`,
one BLAS/OMP thread and PYTHONPATH containing `lib/src`, `lib/tests`, and the
`transport`, `buildings`, `vegetation`, `assets`, `render` experiment directories.
Required extras are the declared transport/solids/places/assets/roofs packages;
roof fallback needs pySFCGAL 2.3.0 and system SFCGAL. Blender/Memray are diagnostic.

`stair_local_generate.py` then `stair_local_verify.py` take resident102,
`--prior-module outshine-follow105-stored/outshine-module-0`, either new service
fixture and a fresh explicit `--output /tmp/outshine-...` directory.
`stair_local_reference.py` takes resident53 plus every ordered module from
`reconstructed-local-chain.json` followed by `outshine-follow105-chain/outshine-module-0`,
`--baseline outshine-reference105`, the same fixture and its separately generated
`--local` product. `chain-commands.json` records exact successful control calls.
`stair_local_cost.py --mode local|whole` takes those respective inputs; optional
`--profile` records allocations separately, without changing proof budgets.

Consumers use `stair_occupancy_candidate_consumers.py` with original occupancy102
and both stored105/new service modules, then its fresh `--products` reader.
Feed those products/receipt to `stair_candidate_plants.py --order boundary` with
the verified `outshine-plants102` prototype; use `stair_candidate_plants_verify.py`
in a fresh Installed process. `stair_local_display.py`, `stair_consumer_display.py`
and `stair_plant_display.py` export saved bodies for `render_transport_research.py`;
headless rendering uses EGL/surfaceless and software GL, two threads. The final
array equality, all reader receipts, camera/import proofs, normal cost reports,
local allocation trace and `test/scripts/pixels.py` results remain in that temp
root. Library source is unchanged; use the isolated wheel target instead of
`lib/src` for Installed checks. `make lint-docs`, Ruff and diff checks pass.
