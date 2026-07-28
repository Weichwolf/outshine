# The rest of the cast — the build list at four quantities per type

**Subject:** everything the ten campaigns name that is neither a flyable jet nor a ground threat — at the
resolution the campaigns actually decide with, and no deeper. The source is the aggregated cast table of
[`../../campaigns/INDEX.md`](../../campaigns/INDEX.md); the number in the *Camp.* column is how many of
the ten campaigns need the type.

**Home migration, 2026-07-28 — the AIR rows moved out.** Everything below under *"Air — the 'no' rows"*
and *"Air — the 'yes' rows"* is superseded by [`../air/`](../air/INDEX.md), which specifies `C7` as one
parametric class with eighteen sourced catalogue rows. Three things this file said were **kept and
sharpened** there: the dividing question (*does it have to react to what the player does?* → the
two-part test *manoeuvre decides ∧ envelope published*), the kinematic mover (design A, adopted for
eight of the eighteen rows), and the refusal to give every type a cheap JSBSim deck (design B, rejected
there for the same reason and for a second one — six rows publish nothing a drag polar can be inverted
from). **One thing is corrected:** design A proposed `FBUnitKind::Vehicle` for a mover. That value stays
a GROUND concept — an AIRBORNE mover must remain `FBUnitKind::Aircraft` or no radar in the tree can see
it, so **the mover is a motion property and not a kind**. The Sea and Ground sections below stand
unchanged.

**This file is deliberately shallow.** [`catalogue.md`](catalogue.md) carries manual-grade depth for the
nine ground threat rows because those decide six campaigns' central question. Nothing here decides more
than a mission's shape, so nothing here gets more than four quantities. **A row that a campaign later
proves to be decisive gets promoted to its own catalogue entry — it does not get deepened in place.**

---

## The four quantities

Every row is answered with exactly these, and a row that cannot answer all four is not ready to build:

| # | Quantity | Why it is one of the four |
|---|---|---|
| **1 · Presence** | what it is for the roster and the verdict: a `kill` target, a `protect` asset, the subject of an `identify`, or scenery that must merely exist | the mission verdict is the only thing that makes a unit's existence measurable |
| **2 · Motion** | static · **kinematic** (a declared track, no flight model) · **flown** (a real FDM and a module) | this is the expensive axis, and it is the one that decides unit-versus-module |
| **3 · Signature** | what foreign sensors may perceive: RCS, the afterburner bit, a radar emission, the IFF transponder, the visual extent | it decides at what range the mission starts happening |
| **4 · Weapon** | none · gun · a store catalogue row | it decides whether the thing is a threat or a task |

### The dividing question — unit or module

> **Does it have to react to what the player does?**

| Answer | Consequence | Cost |
|---|---|---|
| **No** — it flies a track, orbits, tows a boom, sails a course | **a unit**: a `kSiteCatalogue`-style row on the same data-driven class, with a *kinematic mover* instead of an FDM | one catalogue row |
| **Yes** — it manoeuvres, defends, shoots back in a fight | **a module** (`C7`): a JSBSim deck plus an `FBModule` plus a pilot | one airframe project |

**The kinematic mover is the one piece of machinery this file asks for and [`module.md`](module.md) does
not.** A unit whose pose advances along a declared waypoint track at a declared speed, with no `FBFdm`,
never shown to `FBFlightMonitor`, never flyable. Two designs:

| Design | Verdict |
|---|---|
| **A: kinematic mover** — the pose is integrated from the declared track; `FBUnitKind` gains `Vehicle` (an object that moves but does not fly) | **RECOMMENDED for every "no" row.** It is `C14`'s other half and it is what a ship, a column, a tanker and an AEW orbit all need. Its honest cost: a moving object whose motion is not physics, which principle 1 tolerates **only** because nothing flies it and nothing is judged by it |
| B: give every such type a cheap JSBSim deck | rejected: an invented aerodynamic object per type, integrated at 100 Hz, to reproduce a straight line — the same argument `weapons.md` §10.3 already made for the bunker, at ten times the scale |

Until it exists, every "no" row can be **spawned static** and is worth exactly what a parked aircraft is
worth: a target and a radar contact.

---

## Air — the "no" rows (units)

| Type | Camp. | 1 Presence | 2 Motion | 3 Signature | 4 Weapon |
|---|---|---|---|---|---|
| **AEW (E-3 / E-2C class)** | 4 | `protect` asset; and in O1 the *reason* the intercept works | kinematic orbit | **the only air row with a real emission**: a `SurfaceEarlyWarning`-class beam pointed outward, huge range gate, plus IFF and a large visual extent. RCS `[TODO]` | none |
| **Tanker (KC-135 class)** | 3 | `protect` asset; W2's subject is the fuel it gives, which needs `C5` (no refuelling exists) | kinematic track | RCS `[TODO]` (large), IFF on, large visual extent | none |
| **Bomber / ELINT / transport (the intercept subject)** | 2 | the subject of an `identify` — W5's and O2's whole task | kinematic track | **the decisive row**: a large visual extent (the eye identifies by presented size — `sensors.md` §9.7) and an IFF transponder that a mission can switch off. RCS `[TODO]` | none |
| **Jammer aircraft (EF-111 / 707 class)** | 2 | O1's decisive mechanism | kinematic track | it has nothing to emit — **`C13`, no jamming of any kind.** The row exists to be shot at | none |
| **RPV / expendable decoy** | 1 | O1's opening move; a `kill` target that is meant to be killed | kinematic track | a **small** RCS, no IFF, small visual extent — the whole point is that it looks like an aircraft to a radar and not to an eye | none |
| **Cruise missile / one-way vehicle** | 3 | a `kill` target with a deadline | already on the roadmap as **R7**; a weapon-as-unit with a flight plan | small RCS, no IFF | its own warhead |
| **Helicopter (Mi-8 / AH-64)** | 2 | `kill` target | kinematic track, low and slow | small RCS, low speed — and low speed is what puts it **inside the Doppler notch of every radar in the tree** (`sensors.md` §4.7), which is correct and free | `[TODO]` |

## Air — the "yes" rows (modules, `C7`)

These manoeuvre, so they are airframe projects and not catalogue rows. Listed here only so the boundary is
visible; their home is `C7`.

| Type | Camp. | Why it must react | The cheapest honest stand-in today |
|---|---|---|---|
| **F-15 class** | 4 | escort and opposition; it fights | the `f16` module with a different `set` block, declared in the mission header as a substitution |
| **MiG-21 / MiG-23 / MiG-17 / Su-7 / Su-20 / Su-22** | 3 | O3's entire force; they fight | `mig29`, which makes the defender materially **stronger** than history — the substitution direction O1/O3 already declare |
| **Mirage F1 / F-5 / Su-27** | ≤2 | ditto | ditto |
| **Anti-radiation shooter (F-4G / F-16CJ)** | 2 | the module exists; **the weapon does not** (`C8`, no HARM) | none — and this is the row that makes `C1` one-sided ([`module.md`](module.md) §Gaps headline) |

**One module family would serve all three period-Soviet rows**, as the campaigns index already notes — and
the same is true of the western ones. The build order for `C7` is a separate decision and is not made here.

---

## Sea

| Type | Camp. | 1 Presence | 2 Motion | 3 Signature | 4 Weapon |
|---|---|---|---|---|---|
| **Warship** | 2 | `kill` target (W3 coastal) or `protect` asset (W5 Baltic) | kinematic course, slow — **the row that most wants the mover**, because a ship at anchor is a building | very large RCS, a very large visual extent, and an **emission**: a surface search set, `SurfaceEarlyWarning`, and a fire-control set if it is armed | **the same `kSiteCatalogue` row form as a SAM battery** — a ship's air-defence system is a site that floats |
| **Merchant / neutral vessel** | 1 | scenery for W5's identification task | kinematic course | large visual extent, no emission, no IFF | none |

**A warship is a ground threat unit with a course.** That is not a simplification for convenience: search
set, fire-control set, one engagement channel, a store row, a health register — every field of
`FBSiteSpec` applies unchanged, and the only difference is quantity 2. If the mover exists, the ship costs
one catalogue row.

---

## Ground — the classes beyond the nine

| Type | Camp. | 1 Presence | 2 Motion | 3 Signature | 4 Weapon |
|---|---|---|---|---|---|
| `target_soft` / `target_hard` | 10 | **built.** `kill` targets, one `Structure` id | static | none — they do not radiate and no radar can see them | none |
| **Radar decoy** (ground, emitting, not lethal) | 1 | W4 names it as a decisive Serbian measure: a `kill` target that is **meant** to waste a weapon | static | **a `p18`-class emission with a fraction of its range gate** — a decoy that emits and cannot shoot is the `p18` row with `rounds 0` and a small `SearchRangeM`. **It costs one catalogue row and no new mechanism** | none |
| **Moving ground column** | 2 | W4's armour hunt, O3's crossing | kinematic — `C14`, and the mover again | none today; a column has no emission | none, unless it carries a `zsu23` |
| **Runway / airfield as a stateful object** | 3 | `C17`: cratered, closed, denied. Today one runway per mission and no state | static | — | — |
| **Heavy AAA (57 / 100 mm)** | 1 | W3's anchor names 100 mm explicitly | static | a fire-control set | **not the gun path** — the projectile pool retires a bundle at 3 s / 3 000 m and a heavy gun's employment is a fuzed bursting shell. [`module.md`](module.md) G6 |

---

## What this file deliberately does not contain

| Absent | Reason |
|---|---|
| Performance data for any air row | none of it decides anything at this resolution. A tanker's cruise speed is a `set` value in the mission that needs it |
| RCS values | every one is `[TODO]`. The tree has exactly two measured-against-each-other cross-sections (F-16 1.2 m², MiG-29 4.0 m²) and the fourth-root law makes a wrong value cheap to spot; inventing eleven more would not be |
| A build order | it follows from the campaigns, not from this list. The only ordering statement here is the dividing question, and it says: **the mover before any of them** |
| Any depth on the "yes" rows | they are `C7`, they are airframe projects, and a half-specified fighter is worse than an honest substitution |

## Related

| Place | Relationship |
|---|---|
| [`module.md`](module.md) | the class every "no" row is a catalogue entry of |
| [`catalogue.md`](catalogue.md) | the nine rows that got manual-grade depth, and why |
| [`../air/`](../air/INDEX.md) | **where the air rows went.** `C7` as one class with eighteen rows: the two-part flight-model test, the deck recipe, the five pilot tiers, the early-warning boundary and the attribution test |
| [`../../campaigns/INDEX.md`](../../campaigns/INDEX.md) | the aggregated cast table this file is the answer to, and the gap ids (`C5`, `C7`, `C8`, `C13`, `C14`, `C17`) each row hangs on |
