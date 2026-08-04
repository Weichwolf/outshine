# Units and effects in the picture

**Subject:** drawing what the simulation already has — other aircraft, released stores, missiles,
ground targets, and the effects that belong to them (smoke, flares, chaff). Neighbours:
[`renderer.md`](renderer.md) (pass topology and the encode slots this occupies),
[`../missions/runtime.md`](../missions/runtime.md) (what a unit is and where its pose
comes from), [`../weapons.md`](../weapons.md) (what exists to be drawn).

The AIRCRAFT half is built and measured. The EFFECT half (`FBSpritesStage`) is still spec only.

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| A frame proof can say something about units | **met.** `/private…/proof-ramp` frame 0: an F-16 at 60 m, 157 px of silhouette, worst edge residual **1.69 px (1.07 %)** against the published pose |
| Units are drawn from the BORROWED registry `FBWorld::Units()`, never from a second source of truth | `world/FBWorld.cpp` is the only file that reads it for drawing; `make verify-layers` counts it separately from the perception boundary (**6 perception readers, 1 drawing-side viewer**) |
| A pose read for drawing is the published pose | `FBWorld::PublishUnits()` names `GetPose()`/`GetSignature()` and nothing else — no `fb_fdm_state`, no `Fdm()`; `render/FBUnitDraw.h` carries no simulation type, so there is no handle to write back through |
| Drawing costs nothing when nothing is there | **measured.** `payerne-full.fbm` (one unit, the camera's own): `[render units] cast=0 drawn=0`, `passcount passes=6`, and the three PNGs are **bit-identical** to the same run on the pre-round binary |
| Camera-relative ECEF like everything else | the model matrix is `[FBCameraBasisEcef | ecef − eye]`, built from the SAME function the camera is built from — see §3 |
| The pass count does not move | 6 before, 6 after (`[render passcount] passes=6 clouds=1 cloudPass=0 hud=1`); `FBUnitsStage` opens no pass |
| Effects are data of the unit that owns them, not renderer state | **not built.** chaff clouds and flares exist as published signature data (`core/FBCountermeasure.h`); the sprite stage is still NoOp |
| A retired unit disappears | `FBSimUnit::Retire` clears the signature, and an empty `Visual.TypeName` is exactly the "no model, no draw" branch of `PublishUnits` — so retirement removes the draw without a second rule |

Resolved out of the open list at the head of the round:

| Question | Answer taken |
|---|---|
| Model source for a unit mesh | `sim/assets/models/<type>_L*.glb` + `<type>.asset.json`, read by a stdlib-only GLB reader in `render/`. `<type>` is the module registry key the unit already publishes (`Visual.TypeName`), so nothing new names an airframe |
| LOD policy | the sidecar's own `lod_switch.steps[].max_range_m` table, read at load, applied by slant range — no second opinion in the renderer (§4) |
| Ground targets: silhouette or marker | undecided and untouched; a ground module publishes no `Visual.TypeName` today, so it simply does not draw |

## State

**Built for aircraft.** One indexed draw per visible unit, in the units slot of the scene pass, from
the published pose; the sidecar's part table drives the moving surfaces; the sidecar's LOD table
chooses the level. `FBSpritesStage` is untouched and still NoOp.

| Piece | File | Note |
|---|---|---|
| JSON reader | `render/FBJson.h/.cpp` | flat node pool, borrowed-range strings; a missing key is an invalid `Ref`, never an error |
| GLB reader | `render/FBGlb.h/.cpp` | header + 2 chunks; positions/normals/UV/indices, node TRS tree, material base colour, PNG base-colour images via the one vendored `stb_image` (declared, never re-implemented). REFUSES what it cannot represent — skins, morph targets, sparse accessors, non-triangle modes, strided views, `node.matrix` |
| Model builder | `render/FBUnitModel.h/.cpp` | GPU-free. Flattens the node tree into one vertex range per LOD, each vertex tagged `(part, material)`; builds the part table from the sidecar's `components` rows |
| Draw record | `render/FBUnitDraw.h` | ECEF + body basis + 10 articulation floats + the type key. No simulation type |
| The stage | `render/stages/FBUnitsStage.h/.cpp` | pipeline, per-LOD bind group, the per-frame part matrices, the draw loop |
| Published articulation | `units/FBUnit.h` `FBUnitArticulation`, filled in `FBSimUnit::PublishPose` | ten channels off `fdm/FBFdm`'s new const surface getters |
| The cast, per frame | `world/FBWorld::PublishUnits()` | called at the end of `Update()`; `SetEyeUnitId` excludes the unit the camera rides |

Measured at load: `[render unit_model] type=f16 lods=4 parts=22 materials=8 tex0=2048
trisTotal=173330 lod0MaxRangeM=108`. 173 330 = 107 706 + 41 342 + 14 366 + 9 916, the sidecar's four
`lods[].triangles` **exactly** — the reader drops no primitive. 22 parts = the static airframe plus the
21 hinges the sidecar's `components` rows expand to.

## Gaps

| # | Thing | Measurement |
|---|---|---|
| 1 | `FBSpritesStage` is still NoOp | chaff, flares, smoke and missile plumes remain invisible; the slot is in place and self-gates |
| 2 | **`L2` is unreachable.** The sidecar gives `L1` and `L2` the same `max_range_m` (692 m), so the first level whose stated range covers a distance is never `L2` | measured: 80 m → `lod=0`, 301 m → `lod=1`, 902 m → `lod=3`. `L2`'s own driver (`feature_m` 0.2353 / `pixel_angle_rad` 0.00054542) is **431 m**, i.e. BELOW `L1`'s 692 — the generator clamped it up to stay monotonic and thereby closed the level. Either the sidecar re-derives its steps or `L2` should not be shipped; the renderer states the table it was given rather than inventing one |
| 3 | Glass is opaque | one draw per unit is what keeps an empty registry free, so a BLEND material is composited against black at LOAD time (`f16_canopy` 0.62/0.70/0.64 × α 0.16 → 0.099 linear). Consequence: the cockpit tub behind the glass is occluded by the glass. A second, sorted pass would fix it and would cost a second draw per unit |
| 4 | Only the base colour is used | the asset ships ORM and normal maps at the same resolution; the reader decodes only images a `baseColorTexture` points at, so they cost nothing — but the airframe is lit as a flat Lambertian, with the terrain's exact weights (0.4 ambient + 3.0 N·L + overcast lift) |
| 5 | Culling is one half-space | a unit further than 20 m BEHIND the eye is dropped; anything in front costs a draw whether or not it is on screen. Correct for a cast of a handful, wrong for a hundred — `kMaxUnits` is 64 and a 65th unit is silently not drawn |
| 6 | The browser pays 12.5 MB | `web/gpu.data` is 13.47 MB (models + the aircraft XML + the moon that moved out of the binary with them, emcc forbids mixing `--embed` and `--preload`), against `gpu.wasm` 11.98 MB — was 12.93 MB embed-only. Lazy per-LOD fetch, mesh compression and dropping `L0` (0.12–0.24 % silhouette XOR, 8.3 MB of the 12.3) are all open |
| 7 | The strut is rigid in the picture | on the ground the drawn wheel sits **1.69 px = 0.16 m** above where the model's own `-1.819 m` puts it (§2). The mesh draws the gear at its built length while JSBSim compresses the struts under weight, and the sidecar already documents a second geometry delta of the same family (`gear_delta_wheelbase`: the mesh's nose wheel stands 0.503 m ahead of the computed contact point) |
| 8 | Ground targets and stores have no mesh registered | `aim9.glb`, `mk82.glb` and `r73.glb` exist beside the F-16 and no client loads them; a released store publishes no `Visual.TypeName` either |

## Knowledge

### 1 Why one draw per unit, and what the vertex carries

A glTF airframe is a node tree: 113 nodes, 101 meshes, 133 primitives, 8 materials for `f16_L0`. Drawn
naively that is 133 draws and 133 matrix uploads per aeroplane. Instead every mesh is baked at LOAD
time into the frame of its nearest ARTICULATED ancestor and tagged with that part's index:

```
vertex = { P[3], N[3], Uv[2], Tag }        36 B
Tag    = part index (low 16) | material index (high 16)
```

so a whole aeroplane is **one `DrawIndexed`** and the only per-frame work is up to 22 small matrices.
The part matrices ride a storage buffer indexed by `instance_index`, which the draw selects through
`firstInstance` — the same trick `FBTilesStage` uses for its per-tile table.

The chain is built so a hinge can nest (`gear.main.l` → `gear.main.l.knuckle` → wheel):

```
part 0            = [ body→ECEF | ecef − eye ]                 (the static airframe)
part p            = part[parent(p)] · Base[p] · Rx(θ_p)
Base[p]           = everything above p, then p's own T·R      (its scale stays BELOW the hinge)
vertices under p  = pre-multiplied by the chain from p down to their own node
```

Parts are allocated in traversal order, so a parent's matrix is always finished before its child reads
it — no second pass, no sort.

### 2 The frame proof, and how it is checkable without trusting the renderer

Venue: two F-16s on the Payerne threshold, the camera riding one, the other **60 m straight ahead and
turned 90°** — the aspect matters, because with the target's length axis exactly along the camera's
`+right` every point of it is at the SAME forward distance and the projection of the span is a pure
scale that needs no camera attitude at all.

`[render unit_draw]` reports the model origin through the same `Mvp20` the draw used:
`rangeM=60.0141 lod=0 tris=107706 px=641.11 py=235.787`. The prediction below is re-derived from
`FBRenderer::MvpCamRel` and the sidecar's bbox, and the measurement is the PNG's own pixels
(green-dominant HUD masked out, sky is blue-dominant, ground near-white):

```
px/rad = 0.5·W·f/asp = 0.5·1280·(1/tan30°)/(1280/720) = 623.538          f = 1/tan(fov/2), fov = 60°
model bbox (glTF axes, from f16.asset.json's BUILT axes x=span y=length z=height):
   nose Z = −8.845   tail Z = +6.218   fin Y = +3.267   wheel Y = −1.819
```

| edge | predicted | measured | residual |
|---|---|---|---|
| nose (right) | 733.01 | 733.00 | **−0.01 px** |
| tail (left) | 576.51 | 576.00 | −0.51 px |
| fin (top) | 201.84 | 201.00 | −0.84 px |
| wheel (bottom) | 254.69 | 253.00 | −1.69 px |
| length span | 156.50 | 157.00 | +0.50 px |

Worst residual **1.69 px on a 157 px silhouette = 1.07 %**, and it is the one edge with a stated
physical cause (Gap 7). The check is also a check of the AXES: the model's arms about its origin are
asymmetric (8.845 m forward, 6.218 m aft), so a flipped forward axis would move the two side edges by
±65 px, not by half a pixel.

### 3 Body axes, and why they come from the camera's own function

`FBUnitDraw::Rot` is column-major `[right | up | −fwd]` from
`FBCameraBasisEcef(yaw, pitch, roll, lat, lon)` — the very function that builds the camera basis. glTF
is `+X` right, `+Y` up, `−Z` forward, so the third column is the negated forward vector and nothing
else needs saying. A jet drawn at a pose and a camera placed at that pose therefore cannot disagree,
because there is one implementation of "what the attitude means" and both call it.

### 4 The LOD table is the sidecar's, not the renderer's

`FBUnitModel::PickLod(range)` returns the first level whose `lod_switch.steps[i].max_range_m` still
covers the slant range, and the last level otherwise (`null` = no upper bound). Measured in one frame
with three targets:

| range | level | triangles drawn |
|---|---|---|
| 80.1 m | `L0` | 107 706 |
| 300.7 m | `L1` | 41 342 |
| 901.7 m | `L3` | 9 916 |

`L2` never appears — that is a property of the TABLE, not of the code, and it is Gap 2.

### 5 The moving parts

The sidecar's `components` rows carry the node name, the JSBSim property it reads and the deflection
limits; the renderer maps the property to one of ten published channels
(`units/FBUnit.h FBUnitArticulation`) and nothing else. Two spellings of the `node` cell are expanded
(`ctl.speedbrake.0..3`, `gear.door.main.l/.r`), so the node names stay in the asset that owns them.

The surfaces are read off the FDM, never off a command: the FCS schedules and rate-limits, so a drawn
aileron following the stick would be a second, disagreeing aeroplane. `fdm/FBFdm` gained nine const
getters for exactly that, and a model that declares none of the properties reads 0 (SimGear's
`getDoubleValue` returns its default for a missing node — an airframe without a tailhook simply has
none).

ONE sign rule is the renderer's and not the sidecar's, because the sidecar cannot express it: a
`gear/gear-pos-norm` node is INVERTED unless its name starts with `gear.door.`. The asset is built
standing on its wheels, so a LEG's zero pose is EXTENDED — which is where `gear-pos-norm` reads 1 —
while a DOOR is built flush and opens as the value rises.

Measured, `proof-lod.fbm`, `near` at 80 m with the gear retracting after an air spawn:

| t | published `gearPos` | drawn `gear.main.l` |
|---|---|---|
| 2.0 s | 0.667 | 28.0° |
| — | rule | `hi·(1 − norm)` = 84° · 0.333 = 28.0° |

and the pixels follow: the four-frame strip at t = 2/4/6/8 s shows the legs go from fully extended to a
clean belly as `gearPos` runs 0.65 → 0.32 → 0.03 → 0. Control surfaces are live in the same line —
`ailLDeg=-4.29 dhtLDeg=5.74 rudDeg=-2.76` on the spawn transient of `proof-surf.fbm` — through the
identical code path.

### 6 Light

The airframe shader is the terrain's, term for term: `base·(0.4 + 0.15·(1−sunThru) + 3.0·N·L·gate)·light`,
then the weather haze out of the shared `stages/FBAtmoHaze.h` with the same sky-view inscatter. That is
deliberate and not laziness — a jet lit by a second model sits IN FRONT of the world instead of in it,
and the haze is what makes a distant one fade at the rate the mountain behind it fades.

### 7 Where the renderer may NOT reach, and how it is held to it

`render/FBUnitDraw.h` mentions no type from `units/`, `fdm/` or `modules/`; the mapping from the
published pose to the record happens once, in `world/FBWorld.cpp`, which `make verify-layers` now lists
in its own category:

> **`DRAW_VIEWERS`** — neither a perceiver nor an owner. It reads everything a unit publishes, degrades
> none of it, and cannot feed anything back; `render/` sits ABOVE `modules/` and `pilot/` in the rank
> order, so no module and no pilot can reach it. A reader added here does NOT widen what an AI may know;
> one added to `PERCEPTION_READERS` does, which is why the two counts stay apart.

The gate prints both: **6 registry reader(s) inside the perception boundary, 1 drawing-side viewer(s)**.

The articulation rides `FBUnitPose` rather than `FBUnitSignature` on purpose. A sensor slot already
receives exact geodetic ground truth from `GetPose()` and is trusted to degrade it; a deflected aileron
adds no capability that lat/lon did not already hand over, so there is no second gate to build and none
was built.

### 8 What the round did NOT change

296 of 296 missions run byte-identical telemetry and the same exit code against the pre-round binary
(`--threads 1`, and `1/2/4` agree with each other). A single-unit mission's PNGs are bit-identical too.
The sim-side change is nine const property reads per pose barrier and ten floats — inert by
construction, and measured to be so.
