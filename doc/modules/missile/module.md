# Missile — the guided round as a module

**Contributes:** `simulated` `instances`

**Subject:** what a weapon *does* after separation — seeker, guidance law, uplink, and the airframe that
carries them. The *model* a round flies on is `../stores/module.md`; the release decision is
[`../../weapons.md`](../../weapons.md); the delivery chain that computes an aim point is
[`../../air-to-ground.md`](../../air-to-ground.md).

## Spec

### 0. Why it is a module and not a store

A store on a pylon has no behaviour — it is mass and drag on the carrier's deck. The moment it separates
it becomes an actor with its own state, its own physics and its own decisions, judged by the same two
monitors as any other unit. That is the definition of a module here, so it is one.

**`simulated`** because it holds per-tick state no generator can reproduce and no dataset carries.
**`instances`** because a round in flight is a placed asset like any other, backed by that state — the
merger [`../../module-contract.md`](../../module-contract.md) §4 makes explicit.

### 1. The parts

| File | Holds |
|---|---|
| `FBMissileModule` | the composition; what this round declares it can |
| `FBMissileSeeker` | the interface: what a seeker sees, and the boundary it may not reach past |
| `FBMissileIrSeeker` · `FBMissileArSeeker` | infrared and active-radar realisations |
| `FBMissileGuidance` | the laws — proportional navigation, and the Paveway **pursuit** law with a rate-stabilised bang-bang relay |
| `FBMissileUplink` | mid-course from the launcher; one of the six files that may see the unit registry |

### 2. The boundary that matters

`FBMissileUplink` is a **perception reader** — it is one of the six files `make -C sim verify-layers`
counts, and the count is a gate. A round that could see the world directly would be the cheat the whole
tree is arranged to prevent; it sees what its launcher sends and what its own seeker resolves, and
nothing else.

## State

Built and flown. Measured 2026-08-05/06:

| | |
|---|---|
| Paveway pursuit law | **14.45 m** against a correctly designated spot (`lgb-designate`), after `C30` removed a total-error veto that sat over the two relay channels |
| the bang-bang's own cost | **3.9 m** against a smooth gravity-compensated pursuit autopilot on the same geometry — the published *"relatively little effect on accuracy"*, measured rather than quoted |
| proportional navigation | AMRAAM miss 0.98 m after the closure-rate repair (`X-20`) |
| illumination proof | `lgb-lase-broken` still dumps ballistically at 1 906 m — the seeker genuinely needs the spot |

## Gaps

- **`N11` — no lase doctrine.** Sources agree crews lase **8–12 s before impact**, not from release; FlightBox lases from release and `brief_lase_s` is the only expression of a window. Measured on unchanged geometry: 33.8 s → 14.40 m · 13.3 s → **9.52 m**, inside the published band. No pilot behaviour and no `.fbm` grammar expresses "lase late".
- **One-sidedness in the terminal phase.** The pitch channel commands `−1` on **0.000** of it, so the miss is short in all 15 runs, where the sources say *"short or long"*. Whether that is the dead band's null or the deck is not isolated — and a number without an isolated cause is not a repair.
- **No test directory.** `sim/test/modules/missile/` exists; the declaration form of [`../../testing.md`](../../testing.md) has not reached it. `verify-trees` names it.
- **214 SA-8 launches produced one kill** (Comanche round). Many rounds climbed to 5–6 km and expired at their 60 s limit with 3–7 km closest approach. Geometry or a guidance defect: **not decided**.
