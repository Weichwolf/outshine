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

At Hockenheim 74.850 s/station 1373.068 m, the static and paced captures both
report `Refined=1`, road surface ID 1, depth 0.018272724 and the same upward
normal. Static RGB is (91,88,83), scene-linear (3602,3444,3196); paced RGB
is (4,10,26), scene-linear (168.75,374.75,830.5). Two fresh static runs and
a direct-first-declaration render are pixel-identical. The paced PNG shows a
large dark wedge. Sun height (45.671 deg) and shadow radius (7913.537 m)
agree, while atlas maximum depth and world geometry counts differ.

At 91.433 s/station 1830.597 m, one static Refined render produced enormous
overhead polygons and a dark road (4,10,26); the paced render gave a clear sky
and (93,90,85). Six immediate fresh static repeats and a further 20 fresh
processes all rendered the clear version. The measured dark-static and
bright-paced runs both had 538 cache deliveries, zero misses and zero provider
starts, so the rare failure is not a network miss. The current SurfaceIdentity
probe samples one pixel and did not
catch the overhead polygon during its one observed appearance.

A focused world-replacement regression found a second, deterministic path to
stale shadows: publishing zero-triangle geometry retained the previous
`SubjectDraw` mesh. The replacement still drew one caster, and 92,672 atlas
pixels remained nonzero. `RuntimeScene::BindBuild` now clears the renderer mesh
and placements for empty geometry; world publication invalidates the shadow
cache, and an empty cast resets its draw count. The same test now sees zero
casters and an all-zero atlas. `make format`, the focused suite and
`LINT_JOBS=2 make lint` pass. This proves empty-world replacement only; it does
not explain the Hockenheim paced/static colour split or rare overhead polygon.

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
--probe-pixel 640,650 --stats src/assets/places/Hockenheimring.scenario`
once as a still and once with `--motion`. Repeat with time `91.416667`.
Use distinct `--into` folders, retain the four PNGs and compare the `PIXEL`
rows and final publication/caster diagnostics. A single matching run does not
close the rare static failure; use a constructed deterministic fixture and a
bounded fresh-process repetition with a declared count.
