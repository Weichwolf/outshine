# Clients — walk and wasm

**Subject:** the two programs that link the engine, and what each of them is allowed to be. The floor
plan is in [`../architecture.md`](../architecture.md); the build targets and gates in
[`../build-and-ops.md`](../build-and-ops.md).

A client adds exactly one thing — an entry point and an output medium. **Nothing about the physics may
depend on which client is running.**

`gpu_walk` and wasm are **the same bench in two toolchains**: both link `PEDESTRIAN_SRCS` +
`render/` + `world/`. Both hard-wire `mods/demo/scene.json`
([`../goal.md`](../goal.md) §1) and read nothing else — no mission, no menu, no mod scan. What the
browser adds is a swapchain and a main loop.

## Spec

### All of them

| Contract | Acceptance / measurement anchor |
|---|---|
| The simulation is identical across clients | same declaration, same result; the renderer is a bolt-on, never a dependency of the physics |
| **The sim tick is a property of the SIMULATION, not a client's choice** | one constant, and every client advances the sim in whole multiples of it. Acceptance: the same run gives the same numbers at ANY frame rate. **Nothing implements this today** — the tick constant and the one loop that owned it were deleted with the combat layer |
| **A declared clock binds every client** | one declared instant, one sun angle everywhere; a client flag that contradicts it is a **boot error, never a precedence** |
| **No client runs a sim loop — every client DRIVES the one loop** | a client calls into the loop and is TOLD the run state; it may not write its own tick. Acceptance was `verify-guards` (a client that drops the verdict does not compile), and **that gate is gone with its subject** — nothing enforces this today |
| Only a client may apply initial conditions | the boot spawn is the single state writer |
| A client owns exactly one entity registry and passes it down at tick time | the world layer only **borrows** it for drawing |

### `gpu_walk` — the pedestrian frame oracle

| Contract | Acceptance / measurement anchor |
|---|---|
| **A camera at eye height over the declared scene, the terrain streamer, one PNG — and nothing else** | no loop, no entities, no sensors, no overlay |
| **It links `render/` + `world/` and the `core/` value translation units those two reference** | `PEDESTRIAN_SRCS` in `sim/Makefile`, and nothing else. `nm` shows no simulation symbol — there is no simulation layer left to show |
| **The scene is a FILE, not a command line** | `mods/demo/scene.json`, hard-wired. Every field is required and none has a default: an incomplete declaration stops the boot rather than being filled in |
| **The ground is DERIVED, never declared** | the scene states a height above ground; `fb_stream_ground` answers from the same DEM the mesh is built from. An unresolved sample stops the boot — a substituted plateau would move the whole picture |
| **The scene's `fovDeg` is honoured, not compared** | it reaches `Renderer::SetFovDeg` and from there the projection, the atmosphere uniform and `FrameContext::FovDeg`; there is no engine-side constant left to disagree with. Verified: the demo scene at 30° instead of 60° renders a clean 2× zoom, sky and terrain still on one horizon |
| **The WHOLE scene must be resident before the shot, and a pass count does not buy that** | since the tile pool the loading is asynchronous: at `--warm 240` the reference scene stood at `progress` 0.25, at 600 at 1 (measured 2026-08-07). So `--warm N` is now the **CEILING** and the run warms until `World::Resident()` — the geometry target cut complete (`progress == 1`), the 3×3 OSM building block decoded, and no building DAG in flight. **When the ceiling bites it is an error with a message** (`warm_ceiling_reached`, exit 2, no PNG written), never a quiet substitute: a picture of a half-loaded scene is not a measurement. Off a warm tile server the reference scene reaches residency in 232–570 passes depending on the pool width |
| **The shot is a function of the SCENE, not of the tiles' arrival order** | residency alone is not enough — the TAA history is built while the tiles arrive, and two runs whose tiles landed in a different order used to differ in 0.43–0.48 % of pixels with a bit-identical depth buffer (reproduced 2026-08-07: three runs of the old procedure, three md5s). So the oracle calls `Renderer::ResetTemporal()` after residency and renders `TemporalSettleFrames()` = **128** more frames **without stepping the world**, then reads back. Acceptance: `tools/determinism.py`, 16 runs at `FB_TILEWORKERS` 1/2/4/6 (residency at 517/437/343/260 passes), **one** md5 `b9a48a34…`. `--settle N` overrides the number; the curve that fixed it is in [`../render/stages/taa.md`](../render/stages/taa.md) §7 |
| **A declared walk stays declared** | `--walkE/N` metres per pass are applied over `--walk N` passes (default 240) and the warm-up then converges standing still. Tying the walk's length to the warm-up would make its end standpoint a property of the network |
| A pedestrian carries no glass | he registers no `OverlayStage`, and there is no overlay group left to register — it was deleted with the avionics layer, so the scene keeps all its lines |
| **Every frame reports its triangle count** | the budget curve ([`../goal.md`](../goal.md) §5) is a series, not a point |

### `gpu_walk --rig` — the subject bench

[`../goal.md`](../goal.md) §3 requires a subject to be rendered ALONE before it enters the scene. That
bench is a **mode of the frame oracle, not a third client**: a client is an entry point plus an output
medium, and the bench changes neither — it is the same binary, the same `Renderer`, the same stages,
with the scene's *inputs* replaced by declared ones. A third client would be a third truth about the
same picture; two of them already have to be kept identical by construction.

**What it replaces and what it may not.** The bench replaces the WORLD (no `World::Open`, no tile
stream, no OSM, no DEM, no buildings) and the LIGHT (declared, never inherited). It may not replace
anything that draws the SUBJECT.

| Contract | Acceptance / measurement anchor |
|---|---|
| **The subject is drawn by the scene's own stage, shader and lighting path — there is no bench-side copy of any of it** | the blades come from `render/stages/GroundCoverStage` fed through `Renderer::SetGroundField`, the same call `world/World` makes. A bench that generated its own geometry would measure itself |
| **The floor is the subject's OWN declared substrate, and the neutral card stands beside the plant and never behind it** | `render/stages/BenchGroundStage` draws two surfaces. The PLANE takes the linear reflectance of the ground-material class the vegetation template references (`wiese` → `grasfilz`, **0.1430 / 0.0964 / 0.0400**, luminance 0.1023) — read from `assets/world/ground-materials.json` and **not** from the resolved `Row::Ground`, whose `swardClosure` (1.0 for `wiese`) has already overwritten it with the aggregate blade colour for the far field. The CARD is 18 % neutral, upright, square to the sight line, **an eighth of the frame wide at the frame's left edge**, and as tall as the frame or 1.5 × the subject, whichever is more. Neither is the scene's classified ground, so the ground shader's own defects still stay off the plant's account — what changes is that the contrast question the bench exists for, *soil against blade*, is now askable |
| **The light is DECLARED and NAMED, three cases plus a turntable, and every output carries its name and its relative bearing** | `frontlit` (sun el 11°, azimuth = camera yaw + 180°, over the camera's shoulder), `backlit` (el 11°, azimuth = camera yaw, behind the subject), `skylight` (a closed deck, `cloudCover` 1.0 — the engine's own overcast, so the direct beam is extinguished by the same `cloudSunThru` the scene uses and nothing is switched off by hand). `turn###` is a FAMILY and not a fourth light: the sun stays where that view's own `frontlit` put it and the camera azimuth steps over 360°. Both families are indexed by the same logged quantity, `sunRelDeg = sunAz − camAz`, so `turn000` is `frontlit` to the bit |
| **The matrix is FULL — every view under every light** | 7 views × 3 lights = 21, plus 8 turntable steps on `portrait`, `tuft` and `eye` = 45, plus a 0/6/12 m/s WIND row on `portrait`, `tuft`, `sward` and `eye` = **57 images**. There is no per-view light mask: a ragged matrix is a hole nobody declared, and the one that stood here cost the `botanist` his coverage verdict — `sward` carried `skylight` alone, so a black pixel on it could be blade or shadow and no second light existed to decide |
| **The three lights are radiometrically comparable** | ONE manual exposure for the whole run, derived from the `frontlit` case's own measured irradiance and then held: `KeyEv = log2(kSceneExposure · 0.18 / π · E_horiz)`. Auto-exposure per image would normalise each light and make the transmission/reflection comparison meaningless. The card is the proof it holds: at a given `sunRelDeg` it reads the same code in every view of a run |
| **Every shot measures what it contains, and an EMPTY view is distinguishable from a BLACK one** | three renders of one camera — subject + card, card + floor, floor — separate the three things a bench frame can hold. `fillPct` is the subject's share of the OPEN picture (the card's own area removed), `cardPct` and `cardMedian` say whether the reference survived and what it read, `subjectMedian` is the subject's own median code. Taken off the DEPTH buffer and the readback, never off a hue: a dry blade has no hue a test could find, and a colour difference counts the plant's shadow on bare floor beside it as plant (measured on `wiese`: 64.7 % against 13.0 % of actual cover). A frame with `fillPct ≥ 10` and `subjectMedian ≤ 1` raises `rig subject_below_floor` |
| **Scale is in the image, not only in the log** | every view declares its FRAME WIDTH at the subject plane; the camera distance follows from it and the declared `fovDeg` (`d = 0.5·W / tan(atan(tan(0.5·fov)·w/h))`). A calibrated bar with a printed length is burned into the readback, and the neutral card carries a grid whose spacing is a 1-2-5 rounding of `frameWidth/8`. Both are logged with the frame |
| **It is a bench for VEGETATION, not for grass** | nothing in it names a species or a layer: a view is `(frame width, pitch, eye height, azimuth)` and the subject height is a number. `--rig-height 25` frames a 25 m tree — floor radius, grid spacing, eye height and camera distance all follow from it, and no line of code changes |
| **It never touches the scene** | no `World`, no network, no `fb-tiles`; `BenchGroundStage` self-gates on "no plane declared" and adds no `Begin*Pass`. Acceptance: `sim/walk-demo.png` before and after is **0 differing pixels** |
| **A view that the engine cannot honestly fill is reported as a gap, never rendered** | an image of an unmodelled state is worse than a named hole ([`../goal.md`](../goal.md) §7). `_wind` was refused on exactly that ground and is rendered as of 2026-08-07, because the flow now exists; `_season` is still refused |
| **The WIND is a bench parameter and it blows ACROSS the frame** | the row declares the 10 m met wind, not a bend: 0 / 6 / 12 m/s from `camAz + 270`, so it blows toward the camera's right and a lean is a lean and not a foreshortening. Same geometry, same light, same held exposure — a difference between two of the three files can be nothing else |
| **A measurement over TIME needs a second clock, and the scene does not grow one** | `--wind-t S` sets the WIND clock and `--seq N --seq-dt S --seq-out DIR` writes a frame sequence after the warm-up, advancing that clock alone: one standpoint, one sun, one streaming state, one moving quantity. `--depth` writes the matching `%%04d.f32` beside every frame. `--wind-deg/--wind-ms` bracket the declared wind the way `--eye/--yaw` bracket the declared standpoint. `--wind-probe PATH` is the STATE channel — the same `WindField` object the stage is handed, sampled on a world line with no GPU in the path. **A temporal filter cannot be judged on a still camera**, so the sequence also carries `--seq-yaw D` (degrees per frame) and `--seq-stepE/N M` (metres per frame); with all three at zero it is the wind-only sequence the previous round used |

**The view list.** Two critics asked for it; the `art-director`'s lights are orthogonal to the
`botanist`'s framings, so the bench is a *view × light* matrix and both lists fall out of it. Every
cell of it is written.

| View | Frame width | Camera | Turntable | Asked by |
|---|---|---|---|---|
| `portrait` | subject height × 16/9 | horizontal, at mid-height | yes | `art-director` |
| `a` / `b` | from a declared 1 m distance | horizontal, at mid-height, azimuth 0° / 90° | no | `botanist` |
| `closeup_hd` | **0.060 m** at the blade foot | horizontal | no | `botanist` |
| `tuft` | **0.40 m** | −35°, obliquely from above | yes | `botanist` |
| `sward` | **1.00 m**, square crop = 1.000 m² | −90°, nadir | no | `botanist` |
| `eye` | **4.0 m** | −3°, eye 1.70 m | yes | `botanist` |

`sward` is a MEASUREMENT and not only a picture: at nadir `fillPct` IS the botanical coverage, and at
no other pointing is it, so only there is it also logged as `coverPct`. It carries **no** card — its
picture is the measured square metre, and a card inside it would be area subtracted from the answer.

**`backlit` and `turn180` are the same LIGHT and not the same PICTURE, and the card proves both
halves.** `backlit` moves the sun and leaves the camera; the turntable moves the camera and leaves the
sun. Both reach `sunRelDeg = 0`, but the camera then stands on opposite faces of a stand whose blades
are hashed out of their world cells, so the two frames sample different blades. Measured on `wiese`,
`portrait`, one run, one shader build: the neutral card — the one surface whose orientation relative to
the camera is identical in both — reads **code 67 in both**, while the subject's median reads **64**
(`backlit`, camera 0°) against **94** (`turn180`, camera 180°). A four-face control at that same
framing and that same bearing gives 64 / 88 / 94 / 106 for camera 0 / 90 / 180 / 270 — a **42-code**
spread, inside which the 30-code gap sits. At `sunRelDeg = 180` the same control gives 50 / 71 / 74 / 84,
a 34-code spread. **The residual is the stand's own face-to-face variance, not a lighting difference.**
Making the two identical would cost a measurement either way: moving `backlit`'s camera would make the
frontlit/backlit pair two different compositions and destroy the transmission comparison the
`art-director` reads off it, and moving the turntable's sun with the camera would make eight identical
pictures. So both stay, and `sunRelDeg` is what reads them against each other.

### wasm — the browser

| Contract | Acceptance / measurement anchor |
|---|---|
| Same source list, other toolchain (emcc/wasm32) — cross-compile, not a second architecture | `make -C sim wasm` builds the app **and** the tile worker |
| **The same scene as the native oracle, from the same file** | `mods/demo/scene.json` is preloaded into Emscripten's virtual FS by the build; the client reads it and the vegetation table and nothing else |
| **No menu, no mod scan, no mission** | the page is a canvas; the only exported symbols are `_main`, `_malloc`, `_free` |
| **The browser advances a WIND clock and not a second sky** | `SetWindClock(wall time)` while `SetSkyClock` keeps the scene's declared moment, so the ground cover moves in the browser and the shadows stand where the scene put them |
| **Its picture must be the native oracle's picture** | same triangle count within the streaming difference, same pass count, same sun |
| **`L` files the standpoint, and the line alone must be enough to rebuild the frame** | one press appends **one JSON line** to **`sim/shots/shots.jsonl`** (host path; `SHOT_ROOT` = `shots/` inside `fb-sim`, bind-mounted by `sim/up.sh`) and posts the canvas beside it as `sim/shots/NAME.png`. The line carries `camera{lat,lon,yawDeg,pitchDeg}`, the `scene{}` identity it was taken under (path, lat/lon, eyeM, yaw, pitch, fov, utcS) and a `derived{groundM,altAslM,sunElDeg,sunAzDeg}` block whose only purpose is to be **subtracted** — a DEM or an ephemeris that disagreed is the one difference no pixel comparison would attribute. `gpu_walk --snapshot PATH` loads it, refuses a line whose `scene` is not this scene, refuses to be combined with `--stepE/--stepN/--yaw/--pitch`, and logs the four deltas. **PNG first, then the line that names it**, so a reader tailing the log never reaches an entry whose image is not on disk. One-way and diagnostic: nothing the server holds is ever served back into a simulation, and `mods/demo/scene.json` does not grow by a field |
| **It is the ONLY client that is steered** | WASD in the horizontal view plane (Shift = 3×), pointer-lock free look at 0.12 deg/px with pitch clamped to ±89° and no roll, `R` back to the declared standpoint. It ends at `Renderer::SetCameraBasis` — `Renderer.cpp` learns nothing. `gpu_walk` keeps its fixed stand so measurements stay comparable between rounds. Measured in headless Chromium: **1.402 m/s** walking and **4.205 m/s** with Shift over 20 s holds (declared 1.4 / 4.2); 600 px of pointer travel turned the head 72.0° (600 × 0.12); ESC released the lock and 800 px of unlocked travel moved yaw and pitch by 0 |

## State

| Client | State |
|---|---|
| `gpu_walk` | **built and measured, 2026-08-06.** Links warning-free under `-Wall -Wextra -Wpedantic -Werror`. `mods/demo/scene.json` after the standpoint move ([`../goal.md`](../goal.md) §2 — 52.10602 N, 9.43453 E, eye 1.70 m, yaw 280, 2026-08-06T17:40:00Z): ground **100.596 m** (`/elev?block=1` answers 100.60), sun **el 11.202 / az 282.601**, **7** render passes, **312,442** triangles (130 draws, 54 terrain tiles, 246,284 blades, 49,152 building vertices), `classVramMB` 32.5. The ground field converges to **3822 of 3822** cells covered with `classUnknown=0`, modal template **`wiese`** at 1702 cells — the old point returned `laubmischwald`. Frame: `sim/walk-demo.png` |
| wasm | **built, 2026-08-06**, same scene preloaded from the same file. **The browser run has not been re-measured since the standpoint moved** — the numbers that stood here (7 passes, 552,899 triangles, sun disc at the predicted pixel) were taken at 52.10499/9.43424 and are not carried forward |
| **the standpoint log, end to end** | **measured 2026-08-07.** Headless Chromium on the deployed app, 75 s settle, one `L`: `shot_posted … pngBytes 759942 http 200`, one line appended to `sim/shots/shots.jsonl`. `gpu_walk --size 1280x720 --snapshot shots/shot-20260807T111409Z-001.json` reproduced it and logged the subtraction — `dGroundM −3.133e-05 m`, `dAltAslM −3.133e-05 m`, `dSunElDeg −3.549e-05°`, `dSunAzDeg −4.458e-05°`. **The two 1280 × 720 frames differ by: 89.75 % of pixels bit-identical, 99.44 % within 2 codes, mean |Δ| 0.188, p99 = 1, max 61.** Every pixel over 8 codes — **2 087, 0.226 %** — lies in the 51-row band `y 330…381`, i.e. the distant town on the horizon, which is the two clients' streaming cut and not the standpoint |
| `gpu_walk --rig` | **built and measured, 2026-08-07.** `--rig wiese` writes **45** images to `sim/bench/` — 7 views × 3 lights + 3 × 8 turntable steps — with no network at all, in **13 s**. Lens 30° = **44.78 mm** on 35 mm format, `focalPx` **1343.54**; the macro views switch to a held working distance and a derived lens (`closeup_hd`: 0.600 m at **426.7 mm**, frame width 0.060 m, **0.0469 mm/px**). One exposure for the whole run, `KeyEv` **−3.887**, metered off the `frontlit` case (`horizE` 0.1608, sunY 0.5874, skyY 0.0487). The isolated patch is one field quad, **1.228 m × 2.000 m**, and the stand snaps to its centre (moved 0.540 m E / 0.854 m N). Floor `grasfilz`, card 0.18: the card holds **12.4 %** of the frame where the framing allows it (`portrait` 7.57 %, `eye` 10.35 %, `a` 10.94 %) and reads **171 / 67 / 90** at `sunRelDeg` 180 / 0 / skylight in EVERY view of the run. `sward` is **1.000 m²** as a 720 × 720 crop and measures **93.51 %** cover by depth under all three lights. `--rig-height 25` re-frames every view for a 25 m subject with no code change (`portrait` 44.44 × 25 m from 50 m with a 5.56 × 37.5 m card at 12.41 % of frame, `eye` re-aims from −3° to **+11.63°**, `sward` stays 1 m²) and the fill measurement names the consequence itself: `fillPct` **0** on every view but `sward` and `eye`, because a 0.3 m sward is all the subject there is. **It never touches the scene**: `sim/walk-demo.png` rendered by a binary built before and after this round is **byte-identical**, 7 passes and 1,459,400 triangles both times |

Everything these clients were once measured against — the scenarios, the mods, the meshes — is deleted.
**No figure from those runs is carried into this file.**

What survives as *knowledge* rather than as a number, because each is a property of code that still
stands:

| Claim | Why it still holds |
|---|---|
| **The browser must not own a tick.** It once wrote itself a second loop, called the monitors and threw the result away — so an entity that had already hit the ground kept being integrated while the judge's own K.O. line stood in the console | the fix was structural: ONE loop, its tick private, the entity tick surface friend-locked to it, the run state `[[nodiscard]]`. **All of that is deleted.** The lesson stands; nothing enforces it, and whatever loop comes next must re-earn the shape |
| **A throttled slot must be handed its own PERIOD, not the frame `dt`.** A slot that accumulates the outer `dt` runs its own clock at `dt / period` of sim speed | the module base that carried the coupling is deleted. Recorded so the next scheduler does not rediscover it |
| **A point cache for ground elevation must be KEYED, and even keyed it is the wrong granularity.** An unkeyed cache answers every question with the last point that resolved, so the answer depends on fetch arrival order; a keyed one leaves every per-tick sample permanently unresolved, because a moving body's position is new each tick and the reply always lands after the tick that asked | the fix is the TILE and not the point ([`../world/terrain.md`](../world/terrain.md) §3.3), and it is in place |
| **The browser and a headless run did not agree on a timed decision**, and the elevation source was ruled out as the cause | what is left is the decision under the browser's pacing. A browser run is **not deterministic and not claimed to be** ([`../mods.md`](../mods.md) §1); this is what that costs |

## Gaps

| # | Client | Thing |
|---|---|---|
| 3 | walk | **It has no body.** The stand point is a command-line argument, not a simulated pedestrian: nothing walks, nothing collides, the eye height is a number the caller supplies. That is the point of the target and it is also its limit |
| 5 | wasm | **No overlay symbology for a body that is not an aircraft.** What the previous era drew was an aircraft's glass, declared by a title in a dead format. A first-person pedestrian needs a different surface, and none is specified |
| 8 | walk/wasm | **The near field is black at a low sun.** Measured mean luminance of the lower half of `walk-demo.png` is **7.1 / 255** while the mid-distance reads 200+; at a 54-degree sun the same frame is correctly lit. There is no auto-exposure, so `totalHorizY = 0.058` tonemaps to nothing. This is the ground-shader step's problem, not the scene's |
| 9 | wasm | **Half closed.** `windDeg`/`windMs` now reach `Renderer::SetWind` → `GroundCoverStage` and drive the ground cover; the browser advances the WIND clock off wall time while the sky keeps the scene's declared moment. What still drives nothing is the `ConstantWindWeather` in `world/` — two paths carry the same declaration and only one is read. `cloudCover` reaches the atmosphere uniform and tints the sky dome, but no deck is declared so the volumetric pass stays off (`cloudPass=0`) |
| 10 | wasm | **The walker is a camera, not a body.** The eye rides the DEM (`fb_stream_ground` + the scene's `eyeM`) and nothing else: no collision with buildings or terrain slope, no step height, no gravity, no fall. Walking into a wall walks through it. That is [`../body-format.md`](../body-format.md)'s job and is not started |
| 10a | wasm | **`R` teleports, and a BODY must never be allowed to.** `CLAUDE.md`'s rule is *„wirken nur über simulierte Systeme — einziger State-Schreiber ist der Boot-Spawn"*, and a jump to the declared standpoint is exactly the write that rule forbids. It is admissible today **only because gap 10 holds**: a camera is not a participant, so nothing in the world can react to it moving impossibly. **The moment the walker becomes a body, `R` is a cheat** and must either go or become an explicitly named debug path outside the simulated one — it may not simply survive the transition. Recorded here so it is not inherited by accident |
| 11 | wasm | **A cold DEM tile freezes the eye height instead of stopping.** Boot refuses an unresolved ground, but a walker who crosses into a tile that has not streamed keeps the last resolved height until it arrives. The alternative — dropping to sea level — is worse; the honest fix is to bound how long it may lag |
| 12 | render | **There is no text stage at all.** The glyph pipeline lived in `HudStage`, deleted 2026-08-07 with the avionics group. No client calls `SetLoadingScreen`, so nothing regresses; a pedestrian loading screen needs its own text stage in `render/` |
| 13 | rig | **„A single plant" is not renderable, and the reason is structural.** A blade has no identity above its instance index: it is hashed out of its world cell in `GroundCoverStage`'s vertex shader, and the only quantity that can switch it off is the COVERAGE field, declared every `kFieldStride · kCellM` = 2 m. The smallest region that can carry cover 1 with cover 0 around it is therefore one field quad — measured **1.228 m × 2.000 m** at 52.1° N — and that is what `portrait`, `a`, `b` and `closeup_hd` frame the near edge of. A tuft, a shoot count, a leaf as an addressable object and the `botanist`'s `_leaf` view all need the plant to become a BODY ([`../body-format.md`](../body-format.md)), not a hashed instance |
| 14 | rig | **`_wind` is rendered as of 2026-08-07, and the refusal is redeemed.** Four framings (`portrait`, `tuft`, `sward`, `eye`) each carry a 0 / 6 / 12 m/s row on identical geometry, light and exposure — twelve files, `wiese-<view>-wind{00,06,12}.png`. They are actually different: on `tuft`, 96.9 / 98.3 / 97.9 % of pixels differ by more than two codes between the three pairings (mean \|Δ\| 46.3 / 53.6 / 50.4 codes, max 190), and the subject's median display code falls 132 → 119 → 113 as the stand lies over. The named lights stay at zero wind by decision — a still-life judges FORM, which is what the botanist pinned, and it keeps those 45 files comparable between rounds |
| 15 | rig | **`_season` is refused for the same reason.** No season parameter exists anywhere — the epoch/decay regulator of [`../vision.md`](../vision.md) is not built — so four dates would be four copies |
| 16 | rig | **Nothing casts a sun shadow onto the card.** `ShadowStage`'s casters are the OSM building prisms only, and the bench has no buildings, so `csmSunVis` returns 1 everywhere: the contact shading visible at a blade's foot is `GroundCoverStage`'s own `shade` ramp (0.45 at the base, 1.0 at the tip) plus screen-space AO, and not a cast shadow. The `art-director`'s „Kontaktverschattung am Ansatz" is therefore readable as *what exists today*, not as what a shadow would give |
| 17 | rig | **The bench cannot draw a tree yet.** `--rig-height 25` re-frames every view correctly and the numbers prove it, but the only subject the engine has is the grass layer, so a 25 m run photographs 0.3 m blades in a 25 m frame. The framing half of the bench is done; the subject half waits on step 5 |
| 18 | rig | **The card's rendered colour is the illuminant's, not neutral.** Its reflectance is exactly 0.18 neutral, so under an 11° sun (`sunRGB` 0.783/0.561/0.342) it reads warm beige and only under `skylight` does it read grey. That is correct radiometry and it is also a trap for a critic reading hue off the `frontlit` frames — the neutral reference is the `skylight` pair |
| 19 | rig | **A macro view inside a closed canopy cannot carry a visible reference, and the bench now says so instead of pretending.** Measured `cardPct`: `closeup_hd` **0 %** (the card stands at the patch's near edge and the outermost blades lean in front of it), `tuft` **0.5–2.5 %**. Both are frames whose `fillPct` is 90–98 % — there is no line of sight to a reference left. The reference for those two views is therefore the run's held exposure and the neighbouring wide views, and the log states it |
| 20 | rig | **`closeup_hd` renders 89.60 % filled at median code 0.** Measured 2026-08-07: the frame carries geometry (`fillPct` 89.60, `blades` 230,770 instanced) and the subject's median code is exactly 0 under all three lights, so `rig subject_below_floor` fires on all three. It is a `GroundCoverStage` shading defect and not a bench one — `portrait-skylight` fires on the same measurement at `fillPct` 83.63 — and the bench's own share of it is closed: an empty view and a black one are now different numbers. **The proof that it was never empty is in the artefact:** the sky pass writes 144/177/216 over the sward in `portrait`, and no sky pixel can be code 0, yet the closeup's top four rows and its horizon row are 100 % (0,0,0) |
| 21 | rig | **`closeup_hd` cannot show a ligula or a cross-section, at any exposure.** A blade is a zero-thickness parallel-sided ribbon with a 4 cm taper (`GroundCoverStage`) — there is no sheath, no ligula and no section to photograph. What 60 mm of frame width CAN answer is blade width (declared 11 mm, ~5 blades across the frame), the litter/floor transition and the base's contact shading. The rest waits on the plant becoming a body ([Gap 13](#gaps)) |
| 22 | rig | **The floor is visible beyond the blade draw radius and reads as a step.** The stand ends where a blade's width stops covering τ pixels (`min(RadiusM, MaxWidthM · fPx / τ)`) while the bench floor reaches the eye's geometric horizon, so `eye` shows a band of bare `grasfilz` above the sward. It is logged as `bladeRadiusM` with every shot so it is identifiable, but it is still a hard chroma step in the picture — the world hides it with `swardClosure`, which the bench must not use |
