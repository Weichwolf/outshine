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
| hands | rigid geometry on the rim is enough if it turns with the rim. Inverse kinematics is cost without return here |
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
| an open-world sandbox | every drive has an A and a B from `routes.xml`. Free roaming with no destination is not a mode |
| a tech demo | shipped, it carries no debug overlay, no wireframe, no free camera. Measurement lives in `test/`, not in the picture |
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
| manual gearbox with a visible lever, or automatic | **manual, with the lever modelled** | the core of this item is an interior one INHABITS. The lever is the second thing a driver touches, and 1987's shifter was half the appeal. It pulls a clutch into the `<vehicle>` declaration, which is engine work and belongs on the board |
| oncoming traffic | **in scope, after M8** | Test Drive without traffic is Test Drive without danger, and speed is only a transgression against something. board:1521 already asks what dense traffic needs; it becomes a `driver` milestone once the attract loop holds |
| can `driver` fail | **yes: a crash ends the drive** | without consequence the speed is not a transgression. No lives and no score -- the drive simply ends, and a new one is declared. That keeps the "no state one starts again from" boundary intact |

## The first thing to do

**M0.** Nothing in sections 2 or 4 can be measured against an offscreen renderer, and the
tyre defect in section 4 -- peak force at 3.9 deg of slip where a tyre needs 6-8 -- is the
first thing a running window would make felt.
