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

Fresh offline captures reproduced both colours with 538 cache deliveries.
Bright/dark runs had 92/102 native piece sources; the excess came from owned
renderer sources, not draw-table rows. `Engine::inspect()` reports live
structure digest, sources, residency and triangles.

One source-count split is a renderer-state ownership error. A retired ground
candidate can remain active while `AdvanceStructureBuilds` publishes a live
tile; its piece operations entered the private candidate. In one trace,
published tile 16 held slot 10:1 after the candidate reused that slot.
`PublishStructureTile` now explicitly enters the published-world scope. A
candidate/live-tile fixture checks both registries. Eight fresh offline
74.85-s static captures gave the bright road probe (91,88,83). Before final
settle, motion at 74.85 s and 91.433 s was dark. At 74.85 s, both
worlds have 46 structure tiles and 92 native sources, yet tile 24's bake
digest differs (3,759,789,901,417,230,002 vs 1,467,637,370,733,713,420),
both with fine heights; native triangles differ by 1,178. `RawTile::Eye`
drives structure LOD and accepted bakes reuse an eye within 64 m. Tile 24 had
identical height/street digests, 29 height sources and qualified inputs, but
its accepted bake eyes differed by about 109 m; Fine/Shell counts were
51/1337 static and 38/1350 paced. The static client also preloads Refined and
settles the renderer, while the paced client previously took its final PNG
without either step. After current-eye acceptance became part of Refined,
the 74.85 s paced run correctly reported Refined=0 before final preload.
With motion-end Refined preload and settle, the probe matches static RGB 91/88/83
and the dark wedge is gone. Fresh same-build PNGs still differ in 5,679 of
921,600 pixels (0.616%, worst channel 118/255), confined to 43 horizon rows:
accepted camera-local LOD within the 64 m reuse radius still needs a stable
representation. The paced 4,491 frames were all unsettled before final preload;
p99 was 17.24 ms, with 53 frames over the 16.67 ms target.

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
--probe-pixel 640,650 --stats src/assets/places/Hockenheimring.scenario`
once as a still and once with `--motion`. Repeat with time `91.416667`.
Use distinct `--into` folders, retain the four PNGs and compare the `PIXEL`
rows and final publication/caster diagnostics. A single matching run does not
close the rare static failure; use a constructed deterministic fixture and a
bounded fresh-process repetition with a declared count.
