Type: defect
State: active
Architecture: ready
Parent: 2260
Depends: 2292
Priority: P0
Area: engine, render, streaming, road
Tags: determinism, publication, shadow, hockenheim

# Refined road image is independent of capture history

## Evidence

At 74.850 s/station 1373.068 m, an early paced capture had a dark road
(4,10,26) while a Refined still showed (91,88,83). A final Refined preload
and renderer settle removed that dark wedge; both now report the same road
surface ID, depth, normal and RGB. `RuntimeScene::BindBuild` also now clears
empty replacement geometry and its shadows: a regression sees zero casters
and a zero atlas instead of 92,672 stale pixels. Live structure-tile uploads
enter the published renderer scope, preventing a retired candidate from
receiving their piece handles; the candidate/live-tile fixture checks both.

One of 27 earlier fresh 91.433-s stills showed enormous overhead polygons;
the other runs were clear with 538 cache deliveries and zero provider starts.
No failing provenance row exists, so its precise cause remains unproved.

The 74.85-s still/motion comparison had also mixed qualities: without a
probe the still stopped at Playable, while motion reached Refined. That gave
38,013/921,600 differing pixels (4.125%), mostly roofs and facades. With
explicit `--quality refined` on both current captures, only 130 pixels differ
(0.0141%), 120 by more than 1/255; worst is 112/255 at a building edge.
The final road probe is (91,88,83) in both. Both PNGs were opened: no dark
wedge or overhead polygon, but a few building edges differ. A same-build
still with/without `--probe-pixel` was pixel-identical. Two no-probe stills
varied only by 1/255. The 4,491-frame motion run had zero missing contacts,
p99 13.13 ms, 11 frames over 16.67 ms and 538 cached deliveries.

Tile 24 previously had identical source/height/street inputs but different
camera-eye bakes (51/1337 Fine/Shell still, 38/1350 paced; eye offset about
109 m). Camera-local LOD and 64-m bake reuse remain the leading explanation
for the few large pixel differences, not yet a demonstrated cause of the rare
polygon. WI 2298 specifies source-keyed cell/level products and render-time
selection; this WI owns final image/shadow convergence and the rare failure.

## Ownership and solution direction

`src/engine/Laying.cpp` and ground/structure publication own the coherent
native products; `src/render/scene` and shadow stages own caster selection,
atlas matrices and invalidation. The final capture must use geometry and
shadow products derived from one published source/product revision; if they
cannot be paired, retain the last coherent pair or return an explicit capture
error. Do not redefine `settled(Refined)` as general renderer readiness.
Record producer revision, geometry bounds, caster identity/digest and
camera-relative light matrix for the final frame in compact diagnostic rows.
Determine whether the overhead polygon is malformed native geometry, a stale
GPU buffer/transform, or a caster drawn with stale bounds before changing
lighting. Invalidate or rebuild only the affected derived product at
publication. No route-specific hide, global fill or relaxed readiness.
Trace which `SceneResources::PlacePiece` owner creates sources absent from
`TilePieces::Standing_`; registration, replacement and candidate abandonment
must release each owned handle exactly once. Prove the source/residency/owner
count invariant at every publication boundary before changing shading.

## Falsifiable acceptance

- Isolated native road plus one wall: camera jump and paced approach to the
  same final pose/time/revision converge to the same geometry, shadow and
  display pixel after declared settle frames. Removing the wall removes its
  shadow; rotating the light moves it. A deliberately incomplete revision
  cannot report Refined.
- Pinned Hockenheim at both stations: repeat static and paced captures with
  fixed cache/source, compare final producer/caster revisions and AOV/pixel
  values. No giant overhead polygon, missing caster or dark/bright flip.
  Capture a failing provenance row before any corrective code change.
- Focused native/candidate/shadow tests, opened PNGs, `make format` and
  `LINT_JOBS=2 make lint` pass. Record frame and memory costs separately.

## Reproduction

Run `build/outshine-client run --offline --view lap --at-seconds 74.85
--quality refined --probe-pixel 640,650 --stats src/assets/places/Hockenheimring.scenario`
once as a still and once with `--motion`. Repeat with time `91.416667`.
Use distinct `--into` folders, retain the four PNGs and compare the `PIXEL`
rows and final publication/caster diagnostics. A single matching run does not
close the rare static failure; use a constructed deterministic fixture and a
bounded fresh-process repetition with a declared count.
