# Units and effects in the picture

**Subject:** drawing what the simulation already has — other aircraft, released stores, missiles,
ground targets, and the effects that belong to them (smoke, flares, chaff). Neighbours:
[`renderer.md`](renderer.md) (pass topology and the encode slots this occupies),
[`../missions/runtime.md`](../missions/runtime.md) (what a unit is and where its pose
comes from), [`../weapons.md`](../weapons.md) (what exists to be drawn).

Both halves are built and measured: the AIRCRAFT (`FBUnitsStage`, one indexed draw per unit) and the
EFFECTS (`FBSpritesStage`, one instanced draw for the whole frame) — engine flame, rocket plume, flares,
chaff.

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| A frame proof can say something about units | **met.** `/private…/proof-ramp` frame 0: an F-16 at 60 m, 157 px of silhouette, worst edge residual **1.69 px (1.07 %)** against the published pose |
| Units are drawn from the BORROWED registry `FBWorld::Units()`, never from a second source of truth | `world/FBWorld.cpp` is the only file that reads it for drawing; `make verify-layers` counts it separately from the perception boundary (**6 perception readers, 1 drawing-side viewer**) |
| A pose read for drawing is the published pose | `FBWorld::PublishUnits()` names `GetPose()`/`GetSignature()` and nothing else — no `fb_fdm_state`, no `Fdm()`; `render/FBUnitDraw.h` carries no simulation type, so there is no handle to write back through |
| Drawing costs nothing when nothing is there | **measured.** `payerne-full.fbm` (one unit, the camera's own): `[render units] cast=0 drawn=0`, `passcount passes=6`, and the three PNGs are **bit-identical** to the same run on the pre-round binary |
| Camera-relative ECEF like everything else | the model matrix is `[FBCameraBasisEcef | ecef − eye]`, built from the SAME function the camera is built from — see §3 |
| The pass count does not move | 6 before, 6 after (`[render passcount] passes=6 clouds=1 cloudPass=0 hud=1`); `FBUnitsStage` opens no pass |
| Effects are data of the unit that owns them, not renderer state | **met.** every effect input is published: the afterburner BIT (`FBUnitSignature::Afterburner`), the cartridges (`FBChaffCloud`/`FBFlareCloud` incl. their bloom time), the burn window (the STORE CATALOGUE's `Perf.BoostS + SustainS` looked up by the published type key) and the nozzle station (the MESH's own `nozzle*` nodes). `render/FBSpriteDraw.h` carries no simulation type, so there is no handle to write back through |
| An effect may not show what no sensor gives away | **measured.** sizes are metres and the sub-pixel floor keeps ENERGY, not size: a jet in full afterburner at **98.4 km changes zero pixels** — the frame is bit-identical to the pre-round binary (§10) |
| Effects cost nothing when there are none | **measured.** `payerne-full.fbm`: `[render sprites] cast=0 drawn=0`, `passes=6`, and the three PNGs are **bit-identical** to the same run on the pre-round binary |
| The flame hangs on the nozzle, not near the tail | **measured.** the drawn plume's root reprojects to **6.1722 m** from the model origin; the mesh's own nozzle station is `|(0, −0.0326, 6.1721)| = 6.1722 m` (§9.1) |
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
chooses the level.

**Built for effects.** ONE instanced draw for every effect in the frame, in the sprites slot of the
same pass (before the HUD), premultiplied so that `alpha = 0` makes the identical blend purely
additive — which is why one pipeline serves the emitters and the smoke.

| Piece | File | Note |
|---|---|---|
| JSON reader | `render/FBJson.h/.cpp` | flat node pool, borrowed-range strings; a missing key is an invalid `Ref`, never an error |
| GLB reader | `render/FBGlb.h/.cpp` | header + 2 chunks; positions/normals/UV/indices, node TRS tree, material base colour, PNG base-colour images via the one vendored `stb_image` (declared, never re-implemented). REFUSES what it cannot represent — skins, morph targets, sparse accessors, non-triangle modes, strided views, `node.matrix` |
| Model builder | `render/FBUnitModel.h/.cpp` | GPU-free. Flattens the node tree into one vertex range per LOD, each vertex tagged `(part, material)`; builds the part table from the sidecar's `components` rows |
| Draw record | `render/FBUnitDraw.h` | ECEF + body basis + 10 articulation floats + the type key. No simulation type |
| The stage | `render/stages/FBUnitsStage.h/.cpp` | pipeline, per-LOD bind group, the per-frame part matrices, the draw loop |
| Published articulation | `units/FBUnit.h` `FBUnitArticulation`, filled in `FBSimUnit::PublishPose` | ten channels off `fdm/FBFdm`'s new const surface getters |
| Effect record | `render/FBSpriteDraw.h` | ECEF centre, world axis, half-length, radius, premultiplied radiance, alpha, kind. No simulation type |
| The effect stage | `render/stages/FBSpritesStage.h/.cpp` | screen-space stretched billboard, three fragment profiles, far-to-near sort, the unresolved limit (§10) |
| Nozzle station | `render/FBUnitModel` `Lod::NozzleOff/NozzleRadiusM`, surfaced by `FBUnitsStage::Nozzle` | the bbox of every `nozzle*` node, taken at its aft face — the ASSET states it, the renderer reads it |
| The effect cast, per frame | `world/FBWorld::AddUnitEffects` / `AddSmokeTrail` | the same one file that builds the unit draws |
| The mission clock | `world/FBWorld::SetSimTimeS`, called by both clients | every effect is an AGE against a published bloom/launch time; without a clock no age-dependent effect draws at all |
| The cast, per frame | `world/FBWorld::PublishUnits()` | called at the end of `Update()`; `SetEyeUnitId` excludes the unit the camera rides |

Measured at load: `[render unit_model] type=f16 lods=4 parts=22 materials=8 tex0=2048
trisTotal=173330 lod0MaxRangeM=108 nozzleZ=6.17209 nozzleRadM=0.534732`. 173 330 = 107 706 + 41 342 + 14 366 + 9 916, the sidecar's four
`lods[].triangles` **exactly** — the reader drops no primitive. 22 parts = the static airframe plus the
21 hinges the sidecar's `components` rows expand to.

## Gaps

| # | Thing | Measurement |
|---|---|---|
| 1 | **The plume of a jet is the AFTERBURNER BIT and nothing finer.** No throttle, thrust or N1 is published anywhere per unit — `FBAirframeBlock` carries `EngineRunning` and no power at all — so there is no continuous quantity to modulate brightness or length with, and a dry engine draws no flame | that is not a loss for the picture (an F-16 at military power shows no visible flame in daylight) but it is a hard stop for a nozzle GLOW, which would need a temperature the tree does not have. Adding one is a SENSOR question first: `Afterburner` is what `sensors/FBIrstSystem` reads, and a finer figure would widen it |
| 1b | The oracle's trail resolution is the SCREENSHOT interval | `gpu_native` calls `FBWorld::Update` only on shot frames, so a crumb is laid at most once per `--interval`. At 1 s and 700 m/s that is a 700 m chord, and the launch instant is late by up to one interval (measured: motor flame from t=138.6 to 148.5 for a catalogue burn of 10.7 s). The browser updates every frame and has neither problem |
| 1c | A trail cuts corners between crumbs | `kTrailStepM` is 250 m: a hard-turning round's trail is a 250 m polyline through its own path, not a curve |
| 2 | **`L2` is unreachable.** The sidecar gives `L1` and `L2` the same `max_range_m` (692 m), so the first level whose stated range covers a distance is never `L2` | measured: 80 m → `lod=0`, 301 m → `lod=1`, 902 m → `lod=3`. `L2`'s own driver (`feature_m` 0.2353 / `pixel_angle_rad` 0.00054542) is **431 m**, i.e. BELOW `L1`'s 692 — the generator clamped it up to stay monotonic and thereby closed the level. Either the sidecar re-derives its steps or `L2` should not be shipped; the renderer states the table it was given rather than inventing one |
| 3 | Glass is opaque | one draw per unit is what keeps an empty registry free, so a BLEND material is composited against black at LOAD time (`f16_canopy` 0.62/0.70/0.64 × α 0.16 → 0.099 linear). Consequence: the cockpit tub behind the glass is occluded by the glass. A second, sorted pass would fix it and would cost a second draw per unit |
| 4 | Only the base colour is used | the asset ships ORM and normal maps at the same resolution; the reader decodes only images a `baseColorTexture` points at, so they cost nothing — but the airframe is lit as a flat Lambertian, with the terrain's exact weights (0.4 ambient + 3.0 N·L + overcast lift) |
| 5 | Culling is one half-space | a unit further than 20 m BEHIND the eye is dropped; anything in front costs a draw whether or not it is on screen. Correct for a cast of a handful, wrong for a hundred — `kMaxUnits` is 64 and a 65th unit is silently not drawn |
| 6 | The browser pays 12.5 MB | `web/gpu.data` is 13.47 MB (models + the aircraft XML + the moon that moved out of the binary with them, emcc forbids mixing `--embed` and `--preload`), against `gpu.wasm` 11.98 MB — was 12.93 MB embed-only. Lazy per-LOD fetch, mesh compression and dropping `L0` (0.12–0.24 % silhouette XOR, 8.3 MB of the 12.3) are all open |
| 7 | The strut is rigid in the picture | on the ground the drawn wheel sits **1.69 px = 0.16 m** above where the model's own `-1.819 m` puts it (§2). The mesh draws the gear at its built length while JSBSim compresses the struts under weight, and the sidecar already documents a second geometry delta of the same family (`gear_delta_wheelbase`: the mesh's nose wheel stands 0.503 m ahead of the computed contact point) |
| 8b | **A missile IS its plume today.** `aim9.glb`/`r73.glb` exist, `aim120.glb` does not, and no client registers any of them — so a round in flight is drawn as a flame and a trail with nothing in between. At the ranges a round is seen that is very nearly right, and it is why the plume was worth building before the mesh | measured, `bvr-duel.fbm` t=140.8: the round is a 1 px dot at 360 m |
| 8c | Nothing lights the smoke | a trail segment carries a fixed grey-white radiance (`kSmokeRadiance` 1.2) and does not know where the sun is, so a trail is equally bright with the sun behind it and in front of it. The terrain and the airframe both do the sun properly; the smoke does not, and it will show first at dusk |
| 8d | Flare and chaff sizes are the renderer's, not the simulation's | `core/FBCountermeasure.h` publishes a POINT with an age curve and no extent at all. The luminous ball (4.5 m) and the chaff cloud's dispersion (0.6 → 18 m) are therefore `[SET]` in `world/FBWorld.cpp` and marked as such. Nothing reads them back |
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

### 9 The four effects, and where every input comes from

The rule of this half is one sentence: **the renderer reads, it does not invent.** Every quantity that
belongs to the SIMULATION is read from what the unit publishes; every quantity that belongs to the
PICTURE is `[SET]` in `world/FBWorld.cpp` with that mark on it. The split, effect by effect:

| Effect | Read (published) | `[SET]` (rendering) |
|---|---|---|
| Engine flame | `FBUnitSignature::Afterburner` (off the ENGINE, `FBFdm::GetAugmentation`), the type key, and the nozzle station+radius off the MESH | plume length = 11 × nozzle radius, width = 0.95 ×, the radiance ramp |
| Rocket motor | that the unit is `FBUnitKind::Weapon`, its type key, and the catalogue's `Perf.BoostS + SustainS` for that key | flame half-length 2.5 m, radius 0.55 m, gain 1.7 |
| Motor trail | the round's published poses, frame by frame | 20 s life, 250 m crumb spacing, 96 crumbs, 1 → 16 m dispersion, α 0.80 → 0 |
| Flares / chaff | `FBFlareCloud`/`FBChaffCloud`: active, lat/lon/alt, bloom time — **and their age curves**, `FBFlareIrNorm`/`FBChaffRcsNorm`, the simulation's own free functions | the luminous ball 4.5 m, the chaff cloud 0.6 → 18 m, peak α 0.45 |

Two of those deserve their argument spelled out.

**The flare's intensity is the seeker's curve.** `FBFlareIrNorm` is documented as a radiometric history
(ignition 0.15 s, burnout 4.0 s). It is used unchanged as the VISIBLE intensity, because it is one
combustion and a second curve would be a second invention. The same holds for `FBChaffRcsNorm`: a
bundle that "is not a reflector yet" before bloom is not a cloud for an eye either, and it draws
nothing in that window.

**The burn window is the catalogue's.** `FBFindStore(sig.Visual.TypeName)->Perf` — AIM-120: 3.0 s boost
+ 7.7 s sustain = 10.7 s. Measured in `bvr-duel.fbm` (`--interval 1`, so the sample grid is 1.1 s):
`flame=2` from t=138.6 through t=148.5 and `flame=0` at t=149.6 — a 9.9…11.0 s window around the
catalogue's 10.7. What the renderer does add is the LAUNCH INSTANT: it is taken as the first frame the
round was published, which is the frame it was created in, up to one `Update()` (Gap 1b).

#### 9.1 The flame hangs on the nozzle, and that is checkable

The anchor is not a per-type offset table but the MESH: at load, `FBUnitModel::BuildLod` takes the
bounding box of every node whose name starts with `nozzle` and keeps its centre in x/y and its AFT face
in z. For the F-16 that is `nozzleZ=6.17209 nozzleRadM=0.534732` — independently reproduced from
`f16_L0.glb` outside the renderer: `nozzle.inner`/`nozzle.shroud`/`nozzle.petal.00…14` span
z ∈ [5.412, 6.1721], x ∈ [−0.5356, 0.5355], y ∈ [−0.5665, 0.5014], so the aft face is **6.1721** and
¼·(Δx + Δy) = **0.53475**.

The frame proof inverts the projection instead of trusting it. `proof-flame.fbm`, one frame:

```
[render unit_draw]   px=777.24  py=152.683 zndc=0.00110152   -> w = zn/zndc = 45.392 m
[render sprite_draw] kind=0 rootPx=798.841 rootPy=139.151 rangeM=39.2196
x_cam = (2px/W − 1)·w·asp/f      y_cam = ((1 − 2py/H) − shift)·w/f      z_cam = −w
   f = 1/tan30° = 1.7320508, asp = 1280/720, shift = 1 − ViewH/H = 1/3   (FBRenderer::MvpCamRel)
```

| point | camera-space (m) |
|---|---|
| model origin | (9.991, 23.828, −45.392) |
| plume root | (9.991, 21.439, −39.220) |
| distance | **6.1722 m** |

against the model's own nozzle station `|(0, −0.0326, 6.1721)| = 6.1722 m`. The two agree to the fourth
decimal, which is a statement about the AXES as well: a flipped forward axis would put the root 6.17 m
the other way and the distance would come out 12.3 m.

#### 9.2 Colour is radiance, and the tonemap decides what that looks like

The first flame was a **white blob** — measured, 255/255/255 over the first 75 % of the plume — and the
reason is the ACES fit in `FBTonemapStage`: it reaches 231/255 at radiance 1.0 and 250/255 at 3.0. Any
channel above ~1 is white no matter what the other two do, so a flame painted with three large channels
cannot be orange. Saturation has to live in the RATIO:

| | radiance | sRGB8 |
|---|---|---|
| plume core (`kFlameHot`) | (6.0, 2.2, 0.9) | 255, 246, 231 |
| plume tip (`kFlameCool`) | (0.9, 0.06, 0.04) | 229, 73, 60 |

interpolated GEOMETRICALLY along the plume (`hot·(cool/hot)^t`), which moves the ratio instead of the
sum. Stated consequence, and it is physics rather than a defect: an ADDITIVE emitter over a BRIGHT
background cannot be saturated — the background's own green channel is already at 208/255, so the same
plume that reads as fire against the sky reads as a pale smudge against sunlit terrain. It is at its
most convincing exactly where a real one is.

### 10 The unresolved limit — and why it is the anti-cheat gate

An effect is drawn at its physical size in metres and shrinks with range like the airframe beside it.
That alone is not enough, because a quad smaller than a pixel is not a smaller effect but a RANDOM one:
it lights a fragment only where it happens to cover a pixel centre. Measured, before the fix, on one
afterburner plume at four ranges: 479 m → 3 px, **672 m → 0 px**, 896 m → 1 px at 95/255, 1142 m → 1 px.
Blinking, not fading.

Two corrections, both derived:

1. **A floor of 0.7072 px on the half-extent.** The farthest any point can lie from the nearest pixel
   centre is √2/2 = 0.70711, so a quad whose inscribed radius reaches that always covers a sample.
2. **The energy is divided straight back out** — per axis, `gain = 1/(gw·gl)` — so the floor changes the
   SIZE and never the flux, and
3. below full resolution the fragment profile is replaced by its own INTEGRAL over the quad (the four
   means are numerically integrated from the very expressions in the shader, 512² samples:
   flame body 0.2375, flame core 0.0751, flare core 0.01454, flare halo 0.09973, smoke 0.60 long /
   0.24 round). A point source contributes its total flux, not the value of its profile at one
   arbitrary sample.

Measured after, same sweep, same mission (`proof-range.fbm`: the lead accelerates away in
augmentation, each frame diffed against the pre-round binary, which has no sprite stage at all):

| range | pixels changed | worst channel Δ |
|---|---|---|
| 59 m | 171 | 227 |
| 109 m | 30 | 190 |
| 196 m | 7 | 172 |
| 320 m | 3 | 143 |
| 479 m | 4 | 212 |
| 672 m | 3 | 92 |
| 896 m | 2 | 82 |
| 1142 m | 2 | 94 |
| 1403 m | 0 | 0 — *the afterburner went out*, `[render sprites] flame=1 → 0` |

Monotone in the count, and the count falls as 1/range² because that is what a solid angle does.

**And that is the anti-cheat statement, as a measurement rather than a promise:** in `bvr-duel.fbm` the
opposing jet is in full augmentation at **98.4 km** in frame 1 (`[render sprite_draw] kind=0
rangeM=98429.2`), and the frame is **bit-identical** to the same frame from the pre-round binary — zero
pixels, zero channels. The plume of a jet a hundred kilometres away tells the human player exactly what
his sensors tell him: nothing. The same rule carries the flare (`kFlareRadiusM` 4.5 m) and the trail.

What the rule does NOT claim: a missile trail at 3 km IS visible, and it is visible whether or not the
RWR has reported anything. That is not a leak, it is the physics of a smoke trail and it is the reason
an infrared shot is dangerous — the eye is a sensor the tree already models (`sensors/FBVisualSystem`),
and what it may see is decided by angular size and contrast, which is precisely what the floor above
computes. The one thing that would be a cheat is a MINIMUM SIZE without the energy correction; that is
what `FBTileLightsStage` does for city lights (1.3 px hard floor, no gain) and what this stage
deliberately does not.

### 11 What the effects round did NOT change

`build/fb-gym` is **byte-identical** to the pre-round binary
(`78e21a48e95545ae612a0310ef6ed86aca4758191b0d7e0d2c463e050e310340`, before and after a forced rebuild)
— it links `core/ fdm/ units/ sensors/ weapons/ systems/ pilot/ modules/ missions/` and none of those
directories was touched, so the 296-mission regression cannot move: it is the same file producing the
same deterministic output. Checked empirically on the render side as well: `gpu_native --mission
bvr-duel.fbm` writes byte-identical `telemetry.csv` and `telemetry_bandit.csv` before and after, and
`--threads 1/2/4` agree with each other. The pass count is 6 with effects in the frame and 6 without.
