# Lazy road surface LOD products

Owner: #1955 / #915. Analytic Upper-surface reference; no native port approval.
Extends the qualified position model in `road_surface_lod.py` without changing its
existing defaults, approximation formula, source recipes or stored geometry words.

`products(curve, errors_m, max_stations=..., max_queries=...)` yields strictly
coarse-to-fine products. Input curves remain immutable during iteration.

- Requesting only the first product performs no finer subdivision or allocation.
- Each next level refines the accepted frontier. Dyadic partitions remain nested;
  unchanged intervals reuse their measured bound and endpoint data.
- The generated buffers and position bounds match independent `prepare` calls
  at every requested error. No fine mesh is an input or simplification source.
- Query admission precedes pose evaluation. The caller bounds cumulative queries
  and stations; finer failure leaves already yielded coarse products valid.
- Geometry buffers are independent per level and can become independently readable
  asset payloads. This Python iterator does not implement the asset DB or residency.
- Storing every level duplicates buffer data. Generate/cache demanded levels within
  the shared DB budget; the comparison's retained validation buffers are not a runtime
  residency policy or a measured generator peak.

```sh
PYTHONPATH=lib/src python3 -B -m unittest discover \
  -s test/experiments/transport -p 'test_road_surface*.py'
PYTHONPATH=lib/src python3 -B test/experiments/transport/compare_road_surface_levels.py \
  --output /tmp/outshine-road-levels --plot-ordinal 59592
```

Use a Python environment with the existing reference dependencies; plotting additionally
uses matplotlib. Output stays outside the checkout. The driver verifies the fixed input
archive and checks that producer files do not change during the comparison. Every retained
original recipe is compared, rather than only successful candidates.

Errors 16/4/1/0.25 m are comparison inputs, not runtime distance bands or a new allowed
screen error. Camera projection and product bounds must choose actual runtime demand.
Upper boundaries do not contain final native port stitching. Normal interpolation,
material/UV behavior, junctions, bridge lower/wall/cap geometry and whole-world placement
remain separate requirements. Existing references for those contracts remain authoritative.
No native speed, full road coverage or 720p60 claim follows from this experiment.

## Native sampling step

`src/generators/road/meshing/RoadDisplaySampler.h` consumes a native `ReferenceLine`
and caller-supplied station/pose budgets. It produces only ordered render stations,
position-model bounds and work counters. `Sweep` can consume those stations directly
by using the full curve length as its uniform step. No Fine ribbon is an input.
Source admission precedes qualification/evaluation; exhaustion returns an error without
a partial plan. Screen projection, cache keys/LOD metadata and World selection remain
consumer responsibilities.

Native spirals use Gauss8, whereas the Python reconstruction uses Gauss16.
`ReferenceLine::PlanarTruncationBoundM` therefore uses the native rule:
`C8 = (8!)^4 / (17 * (16!)^3)`, rounded upward, with dimensionless majorants
`a = max|curvature| * L`, `b = |curvature_rate| * L^2`,
`B0=1`, `B1=a`, `B(m+1)=a*Bm+m*b*B(m-1)`, remainder `L*C8*B16`.
Every query pays for integration from its original stored segment entry, including
a short queried tail of a long segment. Closed arcs/straight segments use no quadrature.
Node/weight values and native integration operations remain unchanged.

The sampler combines per-source-span second derivatives of height/bank/offset curves,
seam residuals, triangle twist, Float32 rounding in local and placed source frames,
final contact displacement and twice the local source quadrature truncation allowance.
`RoadRibbonContacts` returns the exact curve-ordered Float32 rings used by
`AppendRoadRibbon`; both use the same port ordering. Contacts are supplied in the source
east/up/south frame. The maximum endpoint displacement consumes the same position budget;
it is never hidden by refining geometry. Reversed/nonfinite rings are rejected.
It rejects offset folds, tangent poles,
unbounded derivatives and inadequate source accuracy before render subdivision.
The result is an analytic position-model bound, not a universal libm/IEEE certificate:
accumulated source pose, coefficient/evaluation FP error and later world transforms
remain unqualified. Normal interpolation and complete bridge bodies
remain unqualified too. This module does not yet authorize World replacement or a lower
runtime LOD error. Native controls independently inspect actual emitted triangle
interpolation and exact quadratic chord error; a captured-corpus/native comparison is
still required before integration.

The contact controls use the production ribbon placement/stitching path. Four captured
Wien ways supply endpoint coordinates, source DEM heights and widths for isolated
straight-approach controls; their original curved routes, stairs, bridges and failed
final junction plan are not reconstructed by that test. Separate controls cover imposed
contact movement, reversed rings and a source-frame Float32 precision floor. These are
endpoint/model tests, not a complete native transport-corpus or World acceptance.

`ProfiledRoadMesher(RoadDisplayRequest)` integrates sampling into the existing fitted
profile path: no Fine mesh is built first. Sampling pose/station budgets span all
fitted pieces in one Sweep; fitting and unchanged corner construction are separate work.
The default mesher retains the original path. Display mode accepts Upper surfaces only;
failed sampling or a failed required corner rolls back all four appended vertex/index
streams while preserving preexisting mesh contents. No silent Fine fallback is emitted
under a coarse request. Page publication must still require complete source coverage.
The 1024 m planar straight control emits six rather than 192 triangles with identical
contact words; this is a control-case geometry reduction, not a Tokyo or GPU speedup.
World/page demand and canonical recipe persistence remain unconnected.

Road-page addresses can separately identify `UpperModelErrorM`. The optional value
describes the fitted Upper position model only; it is not full FP/material/normal or
device qualification. Version 4 encodes the value and the appearance variant explicitly.
Absent values retain the original version 2/3 address bytes and keys. Nonpositive/
nonfinite values, incomplete replacement diagnostics and empty replacements for nonempty
source sets are rejected. Spatial `Level` remains separate from the model budget.

Both native road-page producers accept this explicit model demand with caller-supplied
per-Sweep station/pose limits. Closed/step bodies and incomplete page coverage fail;
they never fall back to fine geometry under a display key. PreparedRoadPages persists
successful display products independently of originals. Hits require neither producer
nor sampling budget. Header admission checks framing size without pretending an empty
display page is a valid replacement. The six-versus-192-triangle cache control is one
straight planar recipe, not a scene performance result. Sampling limits exclude fitting,
corner generation and total page allocation; those costs still need admission evidence.

RoadPageManifest version 3 carries each optional Upper model budget next to its product
key and appearance, so metadata lookup does not decode geometry. Original-only manifests
retain version 1/2 bytes. Original/display variants may describe the same source partition,
but restoration requires a nonoverlapping selected cut and verifies payload/model identity
before adopting geometry. An explicitly selected display page restores while its original
payload has a deliberately invalid CRC; the original metadata remains present. This checks
selection before payload IO, not an automatic camera/LOD policy or model qualification.

`StoredTransportRoadPageProducer` composes the existing functional TransportPlan store
and native page producer. Binding performs no plan IO. Only Produce, invoked by the
existing page miss path on a worker, reads the exact saved definition/input binding.
Read/decompression buffers and decoded plan capacities have separate caller-supplied
limits, reserved in its input capacity before loading; they are not a complete bound
on validation/meshing/allocation peaks. The read limit is rechecked before blob IO.
Fresh-process controls build a display miss from a stored plan, then evict that plan:
the page hit still succeeds without sampling or source inputs. Appearance bindings
remain explicit caller inputs. No second recipe format, source reload, solver or job
graph is introduced. This does not migrate the legacy World planner or supply an
automatic camera/pass selection policy; grouped misses should share a loaded plan
lease through the existing bound producer rather than decode the same plan per page.

`UpperWithOriginalBodies` is a separate explicit product policy. Only open Upper
surfaces are sampled; closed shells, step bodies and junctions retain the original
native construction. This is deliberate composition, not a failed surface silently
falling back under a coarse key. An incomplete Upper or original body rejects the whole
replacement. Controls compare all body attribute words with original construction;
step indices change only by their appended vertex base. The Upper model remains
unqualified for full FP, normals/material response and final GPU transforms.
Page format 5 and manifest format 4 identify the policy; all older original/Upper-only
formats and identities remain unchanged. Restoration checks the exact policy before
geometry adoption. This permits shared spatial pages without forcing separate body
draws; camera/pass policy and World selection are still independent integration work.

`SourceProductSelection::GeometryErrorLimitM` inverts the existing geometric projection
bound for generation demand. With perspective bound `p = f*e*(sqrt(2)+s)/(z-e)`,
the inverse is `e = p*z/(f*(sqrt(2)+s)+p)`. Orthographic demand uses pixels per metre.
The panoramic overload covers all rotations and source-bound expansion: for depth
scale `S`, displacement scale `F` and nearest distance `d`, its inverse is
`e = p*d/(F*S+p*(S+1))`. Motion reduces the available depth/distance first.
Denominators round outward, budgets inward, and the same forward function rechecks
every admitted positive value. Invalid/overflowing demands return zero, preserving
the original. Metadata candidate selection uses the same inverse; native road demand
still needs wiring to qualified source bounds and exact view/pass/reuse domains.
These are candidate position budgets, not material/normal/GPU qualification.
