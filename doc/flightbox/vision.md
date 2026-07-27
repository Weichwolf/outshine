# Vision — what FlightBox is for

The direction, set by the project owner. This file changes by **decision**, not by building; every
other file in this collection is measured against it.

## The one sentence

**A tactical air-combat game on real physics** — as correct as necessary, as accessible as possible.

The physics is JSBSim; FlightBox is the world around it — global terrain, renderer, HUD, controls. The
aircraft is the F-16, flown as the pinned vanilla JSBSim model. Two quality axes count: **correct
rendering** and **realistic F-16 flight behaviour**.

## The staggered scale

Not everything is modelled to the same depth, and that is a decision, not a shortfall:

| Subject | Scale | Why |
|---|---|---|
| The F-16 | **exact** | it is the product |
| Sensors, weapon envelopes, the course of an engagement | **believable** | this is where the game happens |
| Enemy aircraft | **hit their envelope, nothing more** | you usually do not see your opponent |
| Turn-fight fidelity | **subordinate** | see above — the weapon systems matter more than the aircraft carrying them |
| Weather, clouds, night | **tactical layers, not decoration** | they change what you can see and what can see you |

The consequence is stated plainly so nobody mistakes it for neglect: a MiG-29 that fails a knife-fight
comparison is not a defect; a MiG-29 with the wrong envelope is.

## Two classes of mission

| Class | What it is | Rule |
|---|---|---|
| `sim/missions/*.fbm` | **measuring rigs for the gym** — the control loop works with these | they stay exactly as strict as they are: declared spawn, declared objectives, machine-readable verdict |
| Missions for humans | scenarios, later and looser | a scenario layer **over** the `.fbm` format (roadmap R9), not a second dialect |

## Anti-cheat is a game decision

The anti-cheat structure — the pilot sees only through simulated sensors, writes only through
simulated controls, cannot repair itself, is judged by two incorruptible outside judges — is load-
bearing for the *game*, not just for the engineering:

**a cheating opponent is noticed immediately.** An enemy that always knows where you are is not a hard
opponent, it is a broken one. That is why the boundaries are drawn structurally (include graph, private
mutators with a single friend, anonymous contacts, two-valued IFF) rather than by convention — see
[`sim/sensors.md`](sim/sensors.md) and [`sim/core.md`](sim/core.md).

The same reasoning applies to the pilot AI: it flies with the information a pilot has (radar picture
with age, RWR without range, a datum instead of the truth about a lost contact), which is what makes
its mistakes readable as mistakes.

## What follows from the physics choice

| Principle | Consequence |
|---|---|
| Do not rewrite physics — JSBSim is the truth | own code only at the seams: FDM adapter, control, renderer |
| JSBSim runs **in** the client | no wire protocol between physics and picture; they are the same process |
| The pinned model is the reference, not the real jet | a validated model property (e.g. ~190 °/s roll rate) is accepted, not a defect |
| The sim runs as fast as sensible | the maths is deterministic; if wall-clock speed changes the result, the coupling is a bug |
| Nothing is preloaded | every tile on demand → **every point on Earth is a valid start** |

## What FlightBox is not

- Not a study-sim of every switch in the cockpit: what is modelled must be *usable*, and what is not
  modelled is stated rather than faked.
- Not a physics rewrite: a number that cannot be derived, measured or honestly declared as `[SET]`
  does not get invented (`conventions.md`).
- Not a fleet of aircraft: other modules exist to prove the plug-in mechanism and to give the F-16
  something to fight. The F-16 is the product.
