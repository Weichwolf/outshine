Type: defect
State: active
Architecture: ready
Parent: 2260
Depends: 2292
Priority: P0
Area: engine, render, streaming, road
Tags: determinism, publication, shadow, hockenheim

# Published geometry stays valid across streaming and capture history

## Evidence

`RuntimeScene::BindBuild` now clears empty replacement geometry/shadows.
Live structure uploads enter the published renderer scope. Their regression
fixtures prove caster removal and candidate/live-tile ownership. Camera-local
LOD remains implicated in small still/motion edge differences; it does not yet
explain malformed polygons. WI 2298 owns source-keyed products and selection.

Current `b6bf3b0e1` offline 220-s lap (`typed-admission-lap`, provider-check):
13,200 frames and 4575.927 m; all 538 deliveries are cached, no provider starts,
no contact gaps, p99 12.78 ms, peak heap 885.9 MiB. All frames are unrefined.
Opened all twelve motion PNGs: marks 2–4 show gigantic ripped facades/roof
triangles above the road; other marks remain schematic. The final motion frame
also contains deformed distant structures. Same-build fast-forward at 220 s
uses identical source counts but differs at 7348/921600 pixels, worst 195/255;
both captures are Playable, neither Refined. This is not an approved low LOD.
The artefact producer and whether corruption begins on CPU, GPU or temporal
history remain unproved. Prior matching Refined stills do not close this case.

Reproduce current motion with `run --offline --view lap --at-seconds 220
--motion --samples --cache-dir <complete-cache> --into provider-check --stats
src/assets/places/Hockenheimring.scenario`. The trace and mark2/3/4 PNGs are
retained there under `typed-admission-lap-lap-*`. Capture actual failing product
bounds/revisions and AOV before altering generation, publication or shading.

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

## Accepted-upload preservation step

`SubjectDraw::BeginMesh` currently calls `SubjectResidency::DropStaged` before
main-mesh replacement. That queue also owns already accepted independent piece
uploads. The 46.883-s motion reproduces ripped structures with 326 live pieces/
sources and 65 structure records. Queue cancellation is a concrete candidate.
Own the pending queue in SubjectResidency. Submit accepted uploads before any
BeginMesh generation/range/shape change; submission failure retains the queue
and the prior mesh state for retry. Shared/borrowed passes obey the same rule.
Expose one SubmitPendingUploads operation; remove discard-on-admission. No
empty upload creates a command. A native GPU fixture stages a piece, poisons its
destination independently, then clears/replaces the main mesh: readback must
recover the exact accepted piece vertices/indices. Inject submission failure:
generation/ownership stay unchanged and retry recovers all payloads. Establish
the failing fixture before correction, then rerender the current motion cases.

## Verified upload correction

Independent poisoned GPU destinations failed both vertex/index readbacks before
correction. BeginMesh now submits accepted residency uploads before mutation;
SetPose failure no longer discards other owners' queued uploads. The fixture
checks clear/replacement, submission refusal, unchanged generation/ownership,
and successful retry against exact native bytes in the current bound buffers.
Focused SubjectDraw/SubjectResidency: 3 PASS. The 46.883333-s offline motion
PNG `geometry-check/preserved-uploads-mark2-lap-tick2813.png` was opened: prior
sky-spanning wedges are absent. p50/p95/p99 2.296/8.850/13.383 ms; peak
643.947 MiB. Remaining stations/full lap, ownership and refined convergence
still require verification; this WI remains active.

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
