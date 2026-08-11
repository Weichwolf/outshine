# Bugs

**What belongs here.** Something that exists and is wrong. `doc/requirements.md` says what must exist
and an unticked line there means *not built*; a line here means *built and broken*. If it has never
worked, it is a requirement. If it worked, or looks like it works, it is a bug.

**A line carries where it is and what decides it** — file and site, the measurement or the picture that
shows it, and what right would look like. A bug without a way to tell it is fixed is a rumour.

**A fixed bug is deleted, not struck through.** `git log` is the record.

---

## Silent success — a call that answers and nobody reads

The most expensive class in this tree, three instances found in one day, all with the same shape.

- **`RoofSurface::Cover` returns `void`, and `EarClip` bails silently** (`generators/draw/RoofSurface.cpp:32`, called at 209). When triangulation fails, `Covering` loops over an empty vector and draws nothing, then `BuildingMesh.cpp:383-387` draws `Gables`, `Eaves` and `Chimney` unconditionally — **a roof drawn as its own trim, floating in the sky with no covering**, visible in the shipped frame at (930,240)–(1190,370) of `after/street.png`. Right: `[[nodiscard]] bool Cover(...)`, and eaves, gable and chimney unspellable without a closed covering.
- **`treebench` measures nothing and reports success.** `clients/TreeBench.cpp:98` — `PlantsIn(assets)` on a missing or empty directory returns an empty list, the header prints, no row follows and the exit code is 0. Verified: `treebench --assets /private/tmp/nope-does-not-exist` exits 0. The round that made the bench enumerate its directory did so precisely because "a form nobody grew would otherwise have looked green"; zero forms still looks green. Right: an empty species set is a refusal that names the directory it looked in.
- **`emscripten_run_script_string` can return null and is not checked** (`clients/AppWasm.cpp:271`).
- **`emscripten_exit_pointerlock()` discards its result** (`clients/AppWasm.cpp:92`).

## Buildings

- **A dome on a rectilinear plan.** `ReadsAsRound` (`generators/draw/BuildingShape.cpp:258`) keys on corner count, fill and squareness — all of which a stepped rectilinear plan satisfies. Visible as a smooth 390 px arc over the town in `after/town.png` (620,255)–(1010,330). Right: add a turning test, every exterior angle ≤ 60°.
- **One ridge kinks over a bent plan.** `MassOf` applies Row → Wing → Setback once, top-down, to the whole. A bent bar fails `RowCut`'s `Fill ≥ 0.80`, is winged into two long masses, and neither piece is offered to `RowCut` again. Same defect on a T-plan. Right: a work list, ~15 lines.
- **The chimney reads as a 5 m post.** `BuildingMesh.cpp:338` runs the box from eaves to ridge + 0.85 m. The 0.85 above the ridge is correct practice; the box must terminate at the roof plane, not at the eaves.
- **The pavement's flag grid degenerates on inhabited ground.** `render/stages/BuildingDraw.cpp:339-340` builds its basis from `cross(upv, vec3f(0.577,0.577,0.577))`. That rotates with position on the globe, so flags never align with the kerb, and it is **exactly degenerate where up ∥ (1,1,1)/√3 — geocentric 35.264 N, 45.000 E**, near Kirkuk–Sulaymaniyah: `normalize` of ~0 → NaN. Root cause is the encoding: `uv.x < 0` spends the whole float on kind plus identity, leaving one metre coordinate, so any non-wall surface needing a 2-D pattern must invent a frame in the shader.
- **The footway ends mid-frontage** (`generators/draw/BuildingMesh.cpp:413-414`) — a per-edge binary accept/reject on a continuous stand-back. A consequence of the footway belonging to the building instead of to the street.
- **A wrong reason defends a right number.** `BuildingMesh.cpp` states "38–45 is the German pantile range (below 22 a pantile does not seal)". The ZVDH Regeldachneigung for a Hohlpfanne is H1 ≥ 35°, H2 ≥ 40°, and the absolute minimum for Ziegel is 10° with additional measures. 22° is neither. `kPitchOutbuildingDeg = 22` may be right; its stated reason is not. Likewise `MinAreaBox`'s comment "on L, T, U and notched bars every hull edge is a ring edge" is false — an L's hull has a chord.

## World and streaming

- **Nothing streams during play.** From t=31 s to t=77 s of `sim/logs/demo-walk-wasm-20260811T150518Z.csv`: `poolHttpGets` 310 flat, `poolFetchedMB` 28.36 flat, `tilesTotal` 130 flat, `tilesBuilt` 0, `tilesEvicted` and `poolEvictions` 0. `TargetTot` is recomputed from the eye every pass (`world/World.cpp:411-419`) and did not move by ±1 in 46 s, while the direction-sensitive `tilesInView` did move 45→47 — so orientation reached `Refine` and position did not. **The record cannot decide it**, because the telemetry carries no camera.
- **`heapKB` is `sbrk(0)`, a break that never falls** (`core/io/HeapProbe.cpp:25`). Proof inside one run: `heapKB` frozen at 237 096 from t=13 to t=77 while `byteCacheKB` grows 14 727 → 28 943. Every conclusion from that column's flatness is unsupported, and eviction — once built — will be invisible in the ledger.
- **`poolSumKB` omits `byteCacheKB`**, a published column, which is 29 % of the apparent accounting gap. The genuinely unattributed residual is 71 216 KiB, and it is non-monotone (42 886 at t=1, 88 438 at t=11, 71 216 at t=31), so it is not static data.
- **Nothing evicts.** `BuildingField`, `WaterField` and `StreetField` grow monotonically and their unit of removal does not exist. At 545 KiB of building heap per tile and ~29 MiB of real headroom, that is on the order of fifty tiles before exhaustion.
- **The in-cone priority boost is multiplicative at 20×** (`world/World.cpp:181`, 1.0 against 0.05). The reference adds a capped 0.5 to a 10-point scale and documents 1.0 as the setting that produces thrash.
- **`kGrace = 180` is counted in passes** (`world/World.cpp:32`) — 3.0 s at 60 fps and 6.0 s at 30, so the machine's pace decides what the world holds.
- **The byte cache finds its LRU victim by linear scan under a held lock** (`world/TilePool.cpp:236-240`), n ≈ 600 at 64 MiB of z14 tiles.
- **`World::Refine` builds no intermediate level** (`world/World.cpp:241`, and the traversal's own comment at 246) — correct for a cold start, wrong for travel, because there is then no ancestor rung to hold coverage while a fine rung streams.
- **`Sim::Features` gained a slice, but a feature inside the tile's 23.3 m buffer still yields twice.**
- **A crossing costs +1.77 ms at p50** against its neighbourhood, 1.03 of it the ring's own snapshot — in no column, because `Populate` runs after `Refine` inside one function.

## Light and shadow

- **A wall the sun cannot see is lit by the sun.** `render/stages/SurfaceLight.h:88` forms the near-field
  bounce as `nearE = (skyH + I.sun.xyz * (sunUp * sunVis * thruDir)) * alb`, where `sunUp = dot(sunDir,
  upv)` and `skyH` is the file's own **horizontal** irradiance (line 74-76). It is then weighted by
  `(0.5 + 0.5*ndu) * kSelfShelter`, so on a vertical façade (`ndu = 0`) the weight is 0.175 and the term
  carries the full horizontal solar irradiance regardless of where the sun stands relative to the wall.
  The comment three lines above states the intent exactly — "the SAME material under the SAME local
  light" — so this is a wrong expression under a right design, not a modelling choice. Numbers, at
  `alb = 0.5` and sun elevation 50° (`sunUp = 0.766`): the spurious irradiance is
  0.175 · 0.766 · 0.5 = 0.067 E⊥, against a legitimate sky term of (0.5)(1 − 0.35) · E_sky,horiz ≈
  0.325 · 0.10 = 0.033 — **twice the whole sky ambient on a shaded wall**, and it does not move when the
  sun's azimuth does. Every shaded façade, trunk and leaf back-face is flattened by it, which is the
  single largest gap between this picture and KCD's. Right: drive the near term from the irradiance the
  fragment already has — `(skyH * (0.5 + 0.5*ndu) + I.sun.xyz * max(dot(n, sunDir), 0.0) * sunVis *
  thruDir) * alb`. Decides it: two walls of one albedo facing 180° apart under one sun; their radiance
  ratio must follow their irradiance ratio, and today the shaded one does not change at all as the sun
  swings behind it.
- **The shadow bias is 0.82 m in the near cascade, and a different physical length in each.**
  `render/stages/ShadowStage.cpp:213` sets `par[1] = 1.5e-3` and calls it an "ortho depth bias";
  `ShadowSample.h`'s `refZ = ndc.z - C.par.y` subtracts it from a `[0,1]` depth whose range is
  `dz = zf - zn = 2R + 500` (`ShadowStage.cpp:192`). Cascade 0 has `R = 24`, so `dz = 548 m` and the bias
  is **0.822 m along the sun** — 17 texels of a 4.7 cm cascade-0 texel, where practice is one to three.
  The normal offset meant to carry the job is 0.07–0.16 m there, five times smaller, so the crude term
  dominates the refined one. Derived consequence: a shadow starts `bias · cos(el)` from its caster —
  0.63 m at 40° sun, 0.81 m at 11° — so every trunk, post, kerb and wall floats. And because `dz` grows
  with the cascade radius, the same constant is 2.5 m in cascade 3: one number, four different lengths.
  Right: state the bias in **metres along the light**, sized to that cascade's own `texelM`, and divide
  by that cascade's `dz` on the way into the uniform. Decides it: a vertical post on flat ground — the
  gap between its foot and the start of its shadow must stay under one cascade-0 texel.
  *Checked and ruled out as the cause:* the cascade **selection** is sound. Selection is by radial
  `length(rel)` against `far[c] = R_c` while the box is centred 0.5 R ahead, which looks like a
  fall-through hole, but for a visible fragment the worst offset is `R·sqrt(1.25 − cos φ)` and stays
  inside the box for any off-axis angle up to 75.5°; the 60° fov's corner is 49.6°.
- **Two adjacent terrain tiles compute two different normals at the posting they share.**
  `world/ChunkMesh.h:100-108` clamps the central difference at the grid border (`i0 = i > 0 ? i - 1 : i`),
  so the east edge of tile (x,y) is a one-sided difference toward the tile's interior and the west edge
  of tile (x+1,y) is the opposite one-sided difference toward *its* interior. The **positions** agree
  exactly — `osmmesh_tile_frac_to_geo(z,x,y,1,·)` and `(z,x+1,y,0,·)` are the same point, which is why
  there is no crack — but the shading normals do not, and `TerrainDraw`'s fragment builds its whole
  relief frame off the interpolated vertex normal (`nn = normalize(nrmIn)`, `groundMat`). The result is a
  lighting discontinuity along every tile boundary, a rectilinear grid at ~1.5 km spacing on z14. Right:
  sample one ghost posting beyond each edge — `osmmesh_tile_frac_to_geo` is defined outside `[0,1]` and
  needs no neighbouring tile, so this costs `2(gr + gc)` extra ellipsoid conversions and no streaming
  dependency — and give every drawn posting a centred difference. Decides it, and it is **decidable**
  with no reference: the normal at `fx = 1.0` of one tile against the normal at `fx = 0.0` of its
  neighbour must be the same vector.

## Frames and units

- **`TangentFrame::Geo` is not the inverse of `TangentFrame::Project`, and the bound written beside it is
  wrong by 1.9×.** `core/TangentFrame.h:37` states "over the 900 m a scatter reaches its error stays
  under a metre". `Project` is the exact ellipsoid projection (line 27); `Geo` is planar on
  `kMPerDeg = 111320`. Measured round trip `Project(Geo(e, n))` at longitude 9°: **1.771 m** at 900 m
  east and 0.735 m at 900 m north at 50 °N; 1.879 m / 0.410 m at 52.1 °N. It is a *scale* error, not
  noise, so it grows linearly — 5.9 m at 3 km east — and it is systematically eastward, so a whole stand
  is displaced the same way. The east term is wrong because the exact longitude scale is
  `N cos φ · π/180` = 71 700 m/deg at 50 °N against `kMPerDeg · cos φ` = 71 555, a ratio of 1.00203; the
  north term is wrong because the meridian scale is 111 229 m/deg there against 111 320. `Geo` feeds
  `clients/StandField.cpp:32`, whose result is immediately handed back to `Project`. Right: either make
  `Geo` the exact inverse (one Newton step on the ellipsoid, or invert through ECEF), or scale it with
  the frame's own `M` and `N cos φ` computed once in the constructor — and in either case correct the
  stated bound. Decides it: `Project(Geo(e, n)) == (e, n)` to a declared tolerance, a pure unit test with
  no reference.

## Picture

- **The demo road reads as a dirt track** since the unmapped substrate landed: the ground fragment uses the default row as the **runner-up** class where the structure has no second hit.
- **Crowns are too transparent at 30–80 m.** A stand reads as a wall of white trunks with a green fringe; no canopy closure. Opacity, not form.
- **The bow-tie crown persists**, reduced but not eliminated — two crowns in `horizon-after.png` still show a straight diagonal seam.
- **A near trunk reads as a straight grey slab**, not as a beech.
- **The hornbeam hedge reads as a young plantation** — ten stems in a row with a bare lower third. The grower has no cut response, which is also what blocks coppice stool and pollard.
- **Leaf lamina is wrong on small dense plants** — box comes out ~8 cm against a real 2 cm, because `CardLeafM` solves LAI ÷ crown projection ÷ card count and few cards means huge leaves.
- **The poplar's stem stands at 84 % of its own buckling height.** `assets/world/species/poplar.json`
  declares `height_m 30` and `dbh_cm 25`, derived from `H/D = 1.20`. The *convention* is right — height
  in m over DBH in cm is the forestry slenderness ratio × 1/100, and the file's spruce at 0.85 → 85 sits
  exactly in the documented Norway-spruce snow-break band, so that derivation is sound and is not the
  defect. The *value* is: 120 is past every published stability threshold, and Greenhill's limit says so
  without appeal to forestry practice. `h_crit = 0.792 (E/ρg)^{1/3} d^{2/3}`; with `E ≈ 1.0e10 Pa` and
  `ρ ≈ 700 kg/m³` for green poplar, `d = 0.25 m` gives `h_crit = 35.6 m`, so a 30 m stem is at 0.84 of
  critical where real trees stand at 0.2–0.6 (McMahon 1973). Run backwards at 0.6 the same relation gives
  `d ≈ 0.42 m`. `dbh_cm` is what the grower solves its whole radius cascade against, so the error is the
  drawn trunk: a 30 m mast rather than a tree. Right: `dbh_cm` 40–60, i.e. `H/D` 0.50–0.75, and the
  origin string amended — the crown of `Populus nigra 'Italica'` being narrow lowers `h_crit` further
  rather than excusing the slenderness, so the reason currently written there argues the wrong way.

## Declaration and build

- **`core/ClusterDag.h:72` reads `FB_TAU` from the environment** — the picture depends on an undeclared variable. **And it is one of six.** Also live: `FB_TAA` (`render/Renderer.h:318`, default on) switches temporal antialiasing, which changes both the pixels and `frameMs`; `FB_GEOM` (`render/GeometryIsolation.h:15`) disarms the shadow receivers; `FB_MOON_SCALE` (`Renderer.h:230,360`, applied at `Renderer.cpp:399`) scales the moon off its real 0.0045 rad; `FB_GROUND_CLASS_VIZ` (`TerrainDraw.cpp:642`) and `FB_TONE_PROBE` (`TaaStage.cpp:226`) replace the fragment outright. **None of the six appears in any telemetry column**, so two runs of one wasm hash are not comparable and no CSV can say which picture it measured — which is the same defect as a resolution that moves under load, wearing a different hat. Right: the four that change the picture leave the environment entirely; the two diagnostics stay and ride a published column.
- **`core/Mat4.h` is entirely dead, and the comment defending it names a test that does not exist.** `Mat4Identity`, `Mat4Mul`, `Mat4Perspective`, `Mat4LookAt`, `Vec3Normalize` and `Vec3Cross` have no caller outside `core/Camera.h`; inside `Camera.h`, `CameraBasisFrom`, `CameraAxes`, `HorizonDipRad`, `MvpTranslate`, `Frustum`, `FrustumFrom` and `AabbVisible` have none either. `CameraBasisEcef` is the only live function in the pair (`clients/Sim.cpp:497`, `clients/SubjectBench.cpp:239`) — verified repo-wide, not only under `sim/src`. `Camera.h:76` asserts "CameraBasisFrom above is NOT dead: sky dome and star field are an infinity pass in LOCAL render-ENU"; `SkyStage` and `StarsStage` call nothing in the file. Two comments say "Pinned in `test_camera.c`"; no such file exists anywhere in the tree. Three consequences, worst first: the dead `Mat4Perspective` builds a **GL-style [-1,1] reversed-Z** projection, so anyone reviving it under WebGPU's [0,1] clip volume silently loses everything past the mid-range; `outshine::Frustum` (`Camera.h:132`) and `outshine::Render::Frustum` (`render/Frustum.h`) are two spellings of one statement against "every statement has exactly one place"; and a false comment is worse than no comment. Right: delete `core/Mat4.h` and everything in `core/Camera.h` but `CameraBasisEcef`.
- **Five WGSL constants that decide the canopy carry no origin.** `render/Sward.h:59-63` declares `kMinSinEl 0.05`, `kLeafTrans 0.85`, `kTransIso 0.35`, `kTransFwd 2.6` and `kTransP 4.0` with no `[SET]`, no derivation and no unit — alone among the twenty constants in that function, and against the rule that every number carries its origin. Two are load-bearing. `kLeafTrans` multiplies the leaf colour on the transmitted path and its **name is the trap**: a green leaf at 550 nm has R ≈ 0.10 and T ≈ 0.085, so a literal "leaf transmittance" is 0.08 and someone will one day write it there and lose the whole back-lit canopy; what makes 0.85 right is that it is the *ratio* T/R, consistent with `kScatCut = sqrt(1 - ω)` and `ω = R + T = 0.185` on the line above. And `kTransIso + kTransFwd·cos^kTransP` is divided by the same `kInvPi` a Lambertian gets, with no statement anywhere that its hemispherical integral is 1 — so the transmitted path is not shown to conserve what the reflected path gives up. Right: one origin line per constant; rename `kLeafTrans` to what it is; and either normalise the lobe or state the deviation beside it.
- **Two headers guard themselves with reserved identifiers.** `core/Ephemeris.h:6` `#ifndef _EPHEMERIS_H` and `core/State.h:3` `#ifndef _FBSTATE_H`. A leading underscore followed by a capital is reserved to the implementation **in every scope** ([lex.name]/3) — undefined behaviour, not a style preference, and the rest of the tree already spells it `GEODESY_H`.
- **The winding is hard-coded at seven sites**; it belongs in the draw product beside the cluster list.
- **A stage that reports the same number every frame is reporting that it is not being measured.** In `after/town-spin.csv` (a rotation about a fixed point) `worldMs`, `meshMs`, `uploadMs`, `buildingMs`, `classMs`, `populateMs`, `nodes`, `drawnLeaves`, `draws`, `built` and `evicted` are all constant across 240 rows, and `distM` is 0.000 in every one — that run is not motion acceptance and was reported as if it were. Note the general form of the claim is false: of 89 columns in a live walk, 35 are constant and most legitimately so (identity, declared limits, stack ceilings). What is suspect is a *cost* column that does not move.
- **The log's timestamp is dead** — every `walk key` line carries `t=0.0` — and key repeat events are logged individually, so a held key floods the buffer.
- **`FacadeUv.h` has no `static_assert` anywhere**: 11 enumerators against a stride of 16, `kStyleCount 8` against 7 enumerators. A 17th `Facade` silently aliases identity 1.
- **`TreeGrower::GrowOnce` is ~130 lines** (`F.2`/`F.3`), and `TreeSpecies::Parse` is a 90-line flat key list (`F.3`).
