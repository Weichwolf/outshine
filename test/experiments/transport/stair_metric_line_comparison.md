# Complete cached profiles within the existing metric bound

Issue #614, child of #600; Python-only correction at `fa6185482934d6fdf4657b5a84cd4f7388774908`. The accepted
72-source world is unchanged. This fixes a false refusal in complete line
queries; it does not admit a surface contact, add sources or establish routing.

## Actual cap and two distinct query failures

The compact [fixed cap](fixtures/stair-metric-end-profile-vienna.json) contains
13 used vertices/13 actual faces from verified footway
`openfreemap:14/8937/5678:transportation:80922970:62`, original endpoint
[1436,2166], 1.8 m full profile. XYZ/winding are preserved; only vertex indices
are compacted. Its metadata binds source module/report/reader, frame and PBF.
The data NPZ SHA256 is
`be729127cc52010c705417a84a4df58b84dc56503acd1d53b8e66408fef07a7a`.

All three real height samples succeed within the existing 1e-7 m metric.
Largest observed sample lateral distance is about 2.0463e-10 m. Exact triangle/line
intersection nevertheless returns only half the 1.8 m profile. A separate exact
set difference between a two-point query and its clipped pieces can report the
entire 1.8 m missing: floating intersection coordinates differ slightly from the
original line. Neither observation establishes a physical surface gap.
Three successful samples alone are insufficient: a control with a real 1 m
intermediate hole passes all three samples but still fails the whole query.

## Correction and mathematical scope

`whole_line_pieces` keeps the previous exact successful branch and all returned
piece/parent bytes. Only when that branch has a missing interval:

1. Let d=1e-7 m and r=64·epsilon·max(1,|XY|). Refuse if 2r≥d. Buffer each
   unchanged parent triangle by d−2r to propose line pieces, retaining parents.
2. Project piece coordinates to scalar distances along the original query;
   merge those intervals numerically. Avoid exact line-set-difference coverage.
3. Check both returned coordinates and their canonical query projections, plus
   any original query corners inside each interval, against their actual parent
   triangle: maximum metric distance+r≤d. Total uncovered query length must be≤r.

For each straight query segment the parent triangle is convex. Distances at
its endpoints bound distances throughout that segment: interpolate nearest
triangle points to obtain a same-weight point inside the triangle. Query
corners are checked separately. A remaining gap has path length at most r;
distance to a closed set is 1-Lipschitz by the triangle inequality, so the
endpoint distance budget still bounds that whole gap by d. The buffer proposes
candidates; its success alone is not acceptance.

The reserve is a numerical policy, not a proof of universal GEOS rounding
error. Conservative near-limit refusals remain possible. Coordinates/meshes,
heights, profiles, frames, solver limits and the existing d bound are unchanged.
Parent planes remain available to the existing whole height/wall checks;
line support alone proves neither height agreement nor functional contact.
Normal old source/body/world validation remains mandatory.

Used by cached actual profiles, attached-root whole queries and wall strips.
Seven controls cover the real profile/cached endpoint, byte-identical exact
path, three-sample hole, inside/outside distance, actual 0.3 m whole wall,
Double/Float32 rejection of 1 mm height disagreement and the 32768-face cap.

## Validation and unchanged accepted world

78 source/78 installed affected controls pass with `-W error`. Fresh wheel
SHA256 `404b355b17389cf8dd92ae94e028e9de7866ceea1ee2c1864d43a1a8a3736934`
contains 205 source-identical modules, including master's #607 glTF cleanup;
43 actually imported reference modules in a fresh installed query resolve inside
the wheel target, with no source fallback. Ruff/format, functions≤120 and
diff/docs checks pass separately from native gates.

Final readers at that commit take 16.393172/12.229327 s,
process RSS 152350720/152977408 B. Shared Linux x86_64
WSL2/i5-1135G7 VM, Python 3.13/NumPy 2.2.4/Shapely 2.2.0; software rendering
and other work overlap. No load/frame/device timing is inferred.

Both three-/two-point counter exports are fresh and array-exact between source
and installed runs. Each returns 4 parent pieces, zero uncovered scalar length,
and 432/320 B packed arrays. Seven warmed normal source repetitions have medians
1.002/0.974 ms; installed 1.699/1.194 ms under shared load. Internal allocations/
copy totals and phase heap are unmeasured; no speedup/memory claim. Raw probe
ru_maxrss readings 87.7/1508.9 MB differ; a separate fresh installed query records
VmHWM 85360 KiB. That anomaly is not explained or treated as query-heap evidence.
Fresh source/installed72-source readers use disposable hardlink clones excluding
verification/display. Original receipts stay protected. All seven original
exports are actually checked by the readers; process exit alone is insufficient.
Source recipe/body/field/display bytes are unchanged. Previous whole720p images
and their [matched comparison](stair_narrow_profile_comparison.md) remain the
current accepted image evidence; no new visual gain or gallery publication.

- `final-reader72-source.json` SHA256
  `caf8f48e9d59d2cd0070e27a1d75bc59334a9f22a1e7fc29471157aec305c3f2`.
- `final-reader72-installed.json` SHA256
  `e6d4a4f72c301ae59a45246b4f5fb15d6ccfa319a4f9e0726cdf63aa92a678d3`.
- `protected72.json` SHA256
  `d64061a831549368b47e56ad80a66b9a92fdbca608b9384415db301d0a5e3e31`.
- `probe-equivalence.json` SHA256
  `0f6a76c0c8d261be38cadfdfb1853140b9cc6f17f5a2fd53343d41d1d1bc5ad8`.

## Larger construction remains an explicit hypothesis

Separately, a private four-source trial on the 72-source world uses boxes at new
and cached original nodes, analytic full-profile sections, shared constructed
node caps and three explicit geometric reuse pairs. It is not the old exact
profile-hull recipe. These model changes are not included in this correction.
Local construction and full diagnostic reconstruction agree on all module/
layout/field bytes; old bridges and 124 piers pass the whole physical checks.
Two target sources are minor/oneway roads; an explicit pedestrian crossing/access
recipe is still absent. Their geometry does not satisfy the initial same-kind
footway policy. No fresh independent recipe/port reader exists for that trial, so
`contact_admission=false`; the accepted world stays at 72 sources.

A tested common Float32 frame with origin [-640,448] m was rejected: it repairs this local terrain but
collapses 20 old body faces/five old bridge underside projections elsewhere.
Old canonical bodies were untouched; their new quantized representation failed.
The subsequent whole trial uses the original world frame. Closer projection,
free-region union and preconformance alone did not solve the original trial.
No frame/tolerance change is smuggled into this correction. #600/#602 remain
active for explicit whole contact topology/provenance and reproducible recipes.

Private frozen prototype commit 23674b1a5, explicit non-admission; report SHA256
`66a6d5f3d81db825ffe4b92a2e3b34f1a92b55af061eeb252b17375c2306bca0`.
All four full 1280×720 images were opened: 655/
869 changed pixels at identical cameras/lights/helpers. They show a small
physical path extension, with the same weak coverage/material presentation.
These are unadmitted hypotheses, not accepted-world improvements. EEVEE/llvmpipe,
no native/GPU/frame proof. `hypothesis-image-proof.json` SHA256
`1a9a42cf4c5e15231d834cd428749332092a95602ddeb11ebd525e49d9308e5a`.
Current full input/output captures and actual images remain only /tmp; earlier
duplicate trial trees are removed after retaining necessary negative examples.

## Reproduction and sources

`TMPDIR=/tmp`, one BLAS/OMP thread,
`PYTHONPATH=lib/src:test/experiments/transport`; use a fresh wheel target instead
of lib/src for installed checks. Run `python -W error -m unittest
stair_metric_line_checks stair_profile_mask_checks stair_endpoint_checks
stair_growth_checks stair_frontier_checks stair_seed_checks stair_local_checks
stair_module_checks stair_module_chain_checks stair_reference_checks
stair_junction_precision_checks`. The new fixed case is independently usable;
no private old world is needed for these tests. Reproduce the accepted72er
readers using the chain in the linked predecessor report, never overwriting
an existing verification.json. Private results live only under
`/tmp/outshine-transport-overlap-port-6d012d3b2/`.

SIGGRAPH starting point: Deering1995,
[page3/section4](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
visually reread: local representation controls position precision; changing an
entire old scene's quantization requires its own whole-scene proof.
Primary library contracts: [Cartesian distance](https://shapely.readthedocs.io/en/stable/reference/shapely.distance.html),
[projected line station](https://shapely.readthedocs.io/en/stable/reference/shapely.line_locate_point.html),
[buffer and polygonal arc approximation](https://shapely.readthedocs.io/en/stable/reference/shapely.buffer.html).
The interval/distance argument above is our derivation, not a vendor-code copy.
No new SDK method, dependency or shared/native API.
