Type: feature
Area: scenario
Tags: scope

# apps/driver is a Test Drive game: worldwide, declarative, and the camera sits in the seat

**The owner's direction, 2026-08-22, and it outranks everything on this list:**

- **worldwide, any route**: two coordinates from anywhere on earth to anywhere, driven by the
  engine's own driver or by the player -- Munich-Hamburg is route 1 of board:1524's hundred, not
  a special case
- **FPV is the focus**: the camera sits in the driver's position; the first-person picture is the
  game, chase and framing shots are tooling
- **the game is DECLARATIVE**: it ships assets, scenario setup and an HTML/CSS UI with
  JavaScript-like elements -- and nothing else. Everything that behaves lives in the engine;
  a driver binary that computes grading, rings, relays or cameras in its own C++ is engine work
  wearing a tool's clothes, and each such piece migrates behind the engine's interfaces
- **the engine delivers everything except the car assets**: in this development phase the F31
  model stands in for every car; NPC traffic wears the same model recoloured -- so missing
  vehicle variety is declared, and missing TRAFFIC is a gap

## What must become true

- [ ] a second route, declared only by two coordinates, drives and pictures without any code
      change (the hundred of board:1524 are the destination)
- [ ] the stills/window drivers shrink toward declarations: ground grading, horizon ring, relay
      pacing and camera composition move behind engine interfaces (the compositor's own job by
      CLAUDE.md's decomposition)
- [ ] the in-game UI (speed, route, prompts) is declared in the engine's own markup/style tree
      (src/ui), not painted by the tool
- [ ] NPC cars: the same declared vehicle, a recolour per instance, placed by a compositor

## Comments

The two review crons are aligned with this: the magazine tester (even hours) judges the
first-person picture of a worldwide test-drive game; the technical reviewer (odd hours) judges
the tree against RAGE and Unreal -- including whether the driver is still doing engine work.

Technical review round 1 ranked the migration list by how much C++ leaves the tool:
1. `Lie()` -- terrain meshing + road grading, ~170 lines -> a Ground-side grading pass + a
   terrain generator kind (the "road travels as data through Ground" rule, executed)
2. relay pacing / streaming, ~60 lines -> compositor residency + floating origin in world;
   "it being in a tool is the single loudest statement that the compositor layer is unfinished"
3. camera rigs (~100 lines) -> a view-rig unit consuming the ALREADY-declared scenario Views
4. asset registration (~50 lines) -> Rigging.cpp, which exists for exactly this
5. cut/fill instrumentation (~40 lines) -> the scenario suite, board:1571's instrument family

---

# The requirement, sharpened (2026-08-24)

Owner direction the same day: *"driver ist eine Neuinterpretation des alten DOS-Spiels Test
Drive mit moderner Grafik basierend auf outshine lib. Demo modus mit self play und interaktiv
vom benutzer gesteuert. immer FPV mit sicht aus fahrerposition. die auto modelle müssen
innenraum modeliert haben. zuerst nur den F31 verwenden."* Written out below by a vehicle-dynamics
lens and a games-review lens, against `CLAUDE.md`, `f31.scenario`, `routes.xml`, the window
driver, `src/sim/Rigging.cpp` and `src/actor/body/Shear.cpp`.

## 1. The core

A drive from A to B on a real road of this planet, seen only from the driver's seat of a car
one wants to own. The draw is not the race, it is the PLACE: the Gotthard, the Stelvio, the
Nullarbor -- layers `routes.xml` already names. Four things are timeless in Test Drive
(Accolade, 1987): the cockpit view as the ONLY view; the public road instead of the circuit;
speed as transgression rather than lap time; and the car as a desired object whose interior one
inhabits.

Not rebuilt, because they were 1987's poverty rather than its virtue: one scripted road (here:
the planet), lives and a score, police as a random spawn, engine damage on a wrong gear, and
the tile scroller that replaced driving physics with a table.

## 2. FPV cockpit -- what must hold, measurably

The F31 asset carries **no semantic node names** (board:1511), so every line below is a
measurement at the asset and not a convention.

| criterion | number | origin |
|---|---|---|
| vFOV of the `eyes` view | 65 deg | `f31.scenario`, declared |
| hFOV that implies at 16:9 | **97.1 deg** | derived, `2*atan(tan(32.5)*16/9)` |
| geometrically correct hFOV, 15 cm phone at 30 cm | **28.1 deg** | derived, `2*atan(7.5/30)` |
| angle per pixel, vertical | **5.42 arcmin** | derived, `65 deg / 720` |
| eye point above ground | 1.220 m | `[SET]`, board:1511, **unconfirmed** |
| eye point left of centre | 0.494 m | measured at the asset |

**Position on the 3.5x over-wide FOV: it stays.** The criterion is not geometric fidelity but
the TANGENT POINT -- drivers fixate the inner kerb where it turns back on itself, about 1 s
before corner entry (Land & Lee, *Where we look when we steer*, Nature 369:742, 1994). Testable:
on the `hairpin` layer of `routes.xml` (Stelvio, Trollstigen, Tianmen) the tangent point must
lie inside the frame in **>= 99 % of frames**. If it falls out, the FOV is wrong -- not the feel.

| requirement | criterion |
|---|---|
| interior visible | A-pillars, dashboard, door card, headliner in frame; no view out through the bodywork |
| no penetration | over a whole route: 0 frames where the near plane cuts interior geometry |
| instrument readable | speed read from a still to **+/-5 km/h in <= 0.5 s**, over >= 20 stills |
| digit floor | a digit under **3 px is below acuity** (derived: 20/20 is 1 arcmin per stroke, 5 strokes = 5 arcmin = 0.9 px, times 3 for margin). Below it the cluster becomes a declared overlay in the cluster's own plane -- never a HUD pasted over the picture |
| steering wheel | wheel angle = input x steering ratio, visible, lagging the behaviour by **<= 1 frame** |
| steering ratio | **absent from `f31.scenario`.** Without it the visible wheel is invented. At 16:1 `[SET]` the lock is 479 deg per side |
| lock at the road wheel | **29.95 deg** derived, `atan(2.810/(5.65-0.774))`, from `Rigging.cpp:170`, turningCircle 11.3 m, track 1.548 m |
| interior mirror | required -- it is the only rearward direction in a view with no external camera |
| door mirrors | rendered or static, by budget. The criterion is a number: **ms per mirror at 720p**, published against the 16.67 |
| head motion | neck as spring-damper, travel capped. **The cap must be measured, not set**: a constant-radius corner at a known lateral acceleration, recorded, then compared. Until then 30 mm at 1 g `[SET]`, expressly provisional |
| camera stillness | frame-to-frame angular change on straight level road at constant speed: **p99 < 5.42 arcmin** (derived: one pixel). Above that, it is visible shake |

**Uncertain:** whether the gaze should actively lead into the corner (head turn toward the
tangent point) or whether that is only bearable with head tracking. Both camps exist in
sim racing. A reading trial decides it, not a derivation.

## 3. Demo mode (self-play)

Under an FPV constraint the camera cannot stage anything. **The driving has to be the staging.**

| requirement | criterion |
|---|---|
| the mind shows intent | a braking point before the corner, a turn-in point, power back on -- not constant speed. board:1502 already yields it from curvature, `v = sqrt(a_lat/kappa)` |
| the mind shows reserve | lateral acceleration is taken to the declared reserve, visible as the tangent point moving inward through the corner |
| route choice | from `routes.xml`, by layer, deterministic from a seed. **Not random from the road network** -- the layers are the curated part and they already exist |
| segment length | 45-90 s, then a cut to another place. `[SET]`, from arcade attract practice; **expressly arbitrary without a better source** |
| the cut | hard. No fade, no fly-over -- a fly-over would be an external camera |
| takeover | within **one frame** of the input event; no mode window, no countdown |
| hand-back | on release, the player's steer blends to the mind's over tau = 0.5 s `[SET]`. Criterion: yaw rate shows **no step > 2 %** against the mind's free-running trace |
| the mind never stops computing | it goes on publishing what it WOULD have done while the player drives -- already true in the window driver, and the reason the handover is readable |
| nothing over the picture | "press start" belongs in the overlay stage, not in the cockpit image |

Today's window driver hands over at 5 % of the route for 1 s, hard. That is a proof, not a game.

## 4. Feel -- what decides, and what can be measured rather than asserted

| quantity | target | origin | state / method |
|---|---|---|---|
| slip angle at peak force | **6-8 deg** | Pacejka, *Tyre and Vehicle Dynamics*, 3rd ed., Magic Formula | **is 3.9 deg**, derived: `0.95 * (1610*9.81/4) / 55000 = 0.0682 rad`. `Rigging.cpp:144` sets `corneringNPerRad` per wheel, `Shear.cpp:14` is linear. **This is a defect: the car breaks away at half the slip a tyre needs and feels snappy.** Either C is too high or the curve is missing |
| longitudinal slip at peak force | 0.10-0.15 | Pacejka, ibid. | not findable in the tyre. Must be checked before traction is claimed |
| wheel load | sums to `m*g` at rest, transfer `m*a*h/L` | Newton | **already measured**: 1251.37 N against 1236.13 N predicted, 1.23 % (board:1511) |
| steady-state yaw rate | `r = v*kappa` | ISO 4138, steady-state circular test | drivable with the synthetic circle of board:1504 |
| yaw response to a steer step | peak time **0.15-0.35 s** | ISO 7401, lateral transient response; passenger-car typical | *confidence medium* -- the span is experience, not quoted from the standard. Method: steer step at 80 km/h, record yaw rate |
| understeer gradient | 2-4 deg/g understeering `[SET]` | production-car practice | *confidence medium*. Method: ISO 4138, steer angle against lateral acceleration |
| double lane change | passes without lifting a wheel | ISO 3888-2 | a regression case on the control route `route id=0` |
| steering torque / FFB | -- | -- | **out of scope.** No wheel is attached to an A18 Pro. The quantity is self-aligning torque from trail; it gets declared when a wheel is attached, not before |
| input to photon | **p99 <= 50 ms** | derived, 3 frames at 60 Hz | method: SDL event stamp to present. **True photon latency needs a high-speed camera** (240 fps gives 4.2 ms resolution); without one, pipeline latency is published AND NAMED as such. board:1491 has that box open |
| frame pacing | p99 of \|dt - 16.67 ms\| **<= 2.0 ms** | `[SET]` | the mean is forbidden by the house rules; the distribution is already in the window driver |
| camera stillness | p99 < 5.42 arcmin/frame | derived, one pixel | see section 2 |

**Not asserted because not measurable:** "weight", "grip", "directness". Each of those words
either decomposes into a row above or it is empty.

## 5. What `driver` is NOT

| not | the boundary that prevents it |
|---|---|
| a racing game | no lap time, no rivals, no grid, no restart button. There is no state one "starts again" from |
| a tech demo in the pejorative sense | it IS a showcase and shows the whole library on purpose -- but shipped it carries no debug overlay, no wireframe and no free camera. Measurement lives in `test/`, not in the picture |
| a licensed simulator | one model, the F31. Further models are assets, not scope |
| a second home for engine code | `driver` DECLARES. Every verb it would need is a board item in `src/`, never a `.cpp` under `apps/` |
| an external-camera experience | the `chase` view in `f31.scenario` belongs to the proof (board:1551, "both persons used"), **not to the game**. It leaves the game scenario as soon as that proof stands elsewhere |

## 6. Milestones

| # | what | how one knows it stands |
|---|---|---|
| **M0** | a real SDL window with `InputPump` under `apps/driver/` | a key moves the car, and **input-to-present latency is published** (p50/p95/p99). No `SDL_CreateWindow` exists under `apps/` today. Without M0 sections 2 and 4 are unmeasurable, which is why it is first |
| **M1** | the eye point is confirmed | 1.220 m / 0.494 m are looked at and confirmed or replaced; board:1511's box is ticked |
| **M2** | cockpit in frame | over a whole route: 0 near-plane penetrations, A-pillars and dashboard visible, tangent point inside the frame in >= 99 % of `hairpin` frames |
| **M3** | tyre curve | slip angle at peak force lands in 6-8 deg; the negative control is red against today's linear curve |
| **M4** | wheel and hands | steering ratio declared, the wheel in frame follows the input within <= 1 frame, measured |
| **M5** | instruments readable | the reading trial over 20 stills reaches +/-5 km/h in <= 0.5 s |
| **M6** | interior mirror | it shows the road behind, and its cost in ms stands beside the 16.67 |
| **M7** | soft hand-back | yaw-rate step at the return to the mind < 2 % |
| **M8** | attract loop | one undisturbed hour of self-play over >= 3 layers of `routes.xml`, no crash, no heap growth, p99 < 16.67 ms |
| **M9** | on the device | the same hour on an A18 Pro, p50/p95/p99 published |

**Why that order:** M0 before everything, because every criterion in 2 and 4 is a measurement at
a running window and today's drivers render offscreen. M3 before M4, because a visible wheel on
a snappy car only shows the twitch better. M8 before M9, because a defect invisible on the
development machine is undiagnosable on the phone.

## Three questions the requirement raised, decided

The lens left three open for the owner. They are decided here so the item is not blocked, and
they are marked as decisions so they can be overruled.

| question | decision | why |
|---|---|---|
| manual gearbox with a visible lever, or automatic | **manual, with the lever modelled** | the core is an interior one INHABITS. The lever is the second thing a driver touches, and 1987's shifter was half the appeal. It pulls a clutch into the `<vehicle>` declaration, which is engine work and belongs on the board |
| can `driver` fail | **yes: a crash ends the drive** | without consequence the speed is not a transgression. No lives and no score -- the drive ends and a new one is declared |

## The first thing to do

**M0.** Nothing in sections 2 or 4 can be measured against an offscreen renderer, and the
tyre defect in section 4 -- peak force at 3.9 deg of slip where a tyre needs 6-8 -- is the
first thing a running window would make felt.

---

# Owner correction, 2026-08-24 -- and it overrules the section above

> *"driver ist eine open world sandbox! start und ziel frei wählbar. hände müssen nicht
> modeliert werden. was fehlt und besonders wichtig ist, ist die grafik der umwelt. strassen,
> häuser, vegetation. wir wollen alle outshine lib fähigkeiten zeigen. driver ist ein showcase.
> presets für historisch bedeutsame routen in der routenauswahl und sonst freie suche von start
> und ziel. wichtig auch andere autos und verkehr. vorerst immer den f31 in verschiedenen
> farben verwenden. weitere auto modelle wenn alles sonst ok ist. tageszeit realtime, wetter
> live."*

Three lines of the lens above are **withdrawn**: "not an open-world sandbox" was wrong, "hands"
is not required, and "a tech demo" needed splitting -- `driver` IS a showcase and shows the
whole library on purpose; what it must not carry is a debug overlay in the shipped picture.

## What `driver` is, restated

**A showcase for every capability the library has, driven in first person, anywhere on earth.**
The drive is the frame; the world is the subject. A route is chosen either from a preset of
historically significant roads or by free search of a start and a destination -- the planet is
the map, not a list.

| pillar | what it demands | where it stands today |
|---|---|---|
| **the world's picture** | roads, buildings, vegetation, drawn well enough that the drive is worth looking at | the weakest pillar and the most important. `Forest`, `Buildings`, `Water`, `Infrastructure` and `Ribbon` exist; nothing has been judged on how it LOOKS at 720p60 from a seat |
| **free start and destination** | two coordinates anywhere, no curated pair required | `Network::Plan` already takes two waypoints; the presets are a convenience layer over it, never the mechanism |
| **presets** | historically significant routes, named, in the selection | `routes.xml` already carries layers; they become the preset list |
| **traffic and other cars** | other vehicles on the same road, driven by the same actor chain | absent. The chain is declared for it (`DrivenBy` a mind, possession as a relink) and nothing instances a second vehicle |
| **one model, many colours** | the F31 in different colours until everything else holds | a material override per instance, declared -- not a second asset |
| **time of day, real time** | the sun where it actually is, now | `Ephemeris` exists and the sky stages are green |
| **weather, live** | the actual weather at the coordinate being driven | absent. A provider in `src/data`, a declaration in the scenario, and the medium stages already take the parameters |

## What this changes in the milestones

M0 stays first -- nothing is measurable without a window. After it the order changes, because
the owner's weakest pillar is the picture and not the tyre:

| # | what | how one knows it stands |
|---|---|---|
| **M0** | a real SDL window with `InputPump` under `apps/driver/` | a key moves the car; input-to-present latency published (p50/p95/p99) |
| **M1** | **the world looks like somewhere** | a still from the seat on three declared routes shows road, buildings and vegetation together, and each of the three is judged against a photograph of that place rather than against itself |
| **M2** | free start and destination | two coordinates typed anywhere on earth produce a drive, or a named refusal that says why -- no curated pair |
| **M3** | presets | the historically significant routes stand in the selection, named, seeded from `routes.xml` |
| **M4** | time of day, real time | the sun's elevation at the driven coordinate matches an ephemeris to a stated tolerance, published |
| **M5** | weather, live | a fetched observation for the coordinate reaches the medium, and a refusal is named when the provider is silent |
| **M6** | traffic | a second vehicle on the same road, driven by a mind through the same seam, one asset recoloured |
| **M7** | cockpit in frame | 0 near-plane penetrations over a route; A-pillars and dashboard visible; tangent point inside the frame in >= 99 % of `hairpin` frames |
| **M8** | tyre curve | slip angle at peak force in 6-8 deg; negative control red against today's linear curve |
| **M9** | attract loop, then the device | an undisturbed hour of self-play, then the same hour on an A18 Pro, p50/p95/p99 published |

**Why the picture moved ahead of the tyre.** A showcase is judged on what it shows. A car that
breaks away at 3.9 degrees of slip is a defect worth its own item (it has one implicitly in
section 4), but nobody looks at a drive whose world is untextured boxes. The tyre stays on the
list because feel is a capability too -- it is no longer the thing that goes first.

## What still holds from the lens

Sections 1 to 4 above stand as written except the three withdrawn lines: the FOV and tangent-point
criterion, the instrument-readability floor, the camera-stillness bound of one pixel, the
measurable feel table, and the demo-mode criteria are all owner-independent measurements and
none of them is affected by the world being open rather than curated.

---

## The picture bar, and it belongs to THIS app (owner, 2026-08-24)

> *"grafisch wollen wir AAA niveau - gran turismo 7 ps4 ist was grafisch auf dem a18 pro
> erreichbar sein sollte"*
>
> *"Gran Turismo 7 on PS4 bezieht sich auf die driver app damit du screenshots vergleichen
> kannst. outshine lib allgemein ist nicht game spezifisch. hier ist die referenz vor allem
> unreal und rage engine."*

The bar is **GT7 on PS4, at 720p60 on the A18 Pro**, and it is a bar for `apps/driver` and not
for the library. outshine's own references stay Unreal and RAGE; an engine has no picture to
match, an application does. This section was first written into CLAUDE.md and taken back out the
same hour -- a library that names a game is the same defect as one that names a planet.

The value of a SHIPPED title on KNOWN silicon is that it stops being an adjective and becomes a
table, and the comparison is by SCREENSHOT: the same corner, the same time of day, side by side.

| row | fetched from the reference | measured in apps/driver |
|---|---|---|
| triangles on screen, p50 / p99 | | |
| draws per frame, p50 / p99 | | board:1538's sweep is the instrument |
| what a draw costs on this device | -- | board:1538's sweep |
| materials per vehicle, how many unique | | |
| lights reaching a frame, how many cast | | |
| shadow resolution and cascade count | | |
| texture residency in bytes | | |
| the frame split: geometry, shading, post | -- | |

GT7 on PS4 is 1080p60 on GCN silicon; this device is a 5-core TBDR part and 720p is 44 % of
those pixels. Whether that trade is favourable is exactly the claim this item refuses to make
from memory: **the reference column is FETCHED, not recalled**, each row citing its source, and
a row nobody can source stays EMPTY rather than being estimated.

- [ ] The reference column is fetched and cited, row by row.
- [ ] The measured column comes from cases under `apps/driver/test/`, p50/p95/p99 over a moving
      camera, never a mean.
- [ ] The screenshots stand side by side: same road class, same weather, same hour.
- [ ] The largest gap between the columns is what the queue works next, stated as such.
