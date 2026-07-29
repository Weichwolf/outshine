# `modules/air/` — the catalogue aircraft

**`C7`: one flying unit that is not a module.** The third level below the module, and the gap that
**degrades nine of ten campaigns** ([`../../campaigns/INDEX.md`](../../campaigns/INDEX.md)): every
missing type is replaced by a present one, every replacement changes the answer, and each is declared
one at a time in a mission header.

**Built and armed.** The class exists (`sim/src/modules/air/`), four of the ten deck rows are inside
their anchor bands, and since 2026-07-29 the armed rows can employ what they declare — `module.md`
§Spec 12's fire control, first catalogue kill measured. Two limits are named there and not glossed: the
two pure-cannon rows cannot RANGE a gun (A14), and no row can yet FIGHT with one (A15).

Why now: [`../ground/`](../ground/INDEX.md) proved the build form — **one parametric class, nine
catalogue rows, six FlightBox-own missile decks from one generated recipe.** This directory does the
same for airframes.

## The three files

| File | What it answers |
|---|---|
| [`module.md`](module.md) | **the contract.** The third level (module / catalogue cell / unit); the two-part flight-model test; the five pilot tiers and the gate that admits a row to one; how it sees without widening the registry gate; **the early-warning row and the one interface price it costs**; damage; the mission grammar; and the **attribution test** that separates "he lost as a MiG-21" from "he lost as a coarse deck" |
| [`flight-model-recipe.md`](flight-model-recipe.md) | **the procedure.** Eight anchors per row, the closed-form drag inversion, `e` derived from the catalogue's only published `CD0`/`(L/D)max` pair, two thrust analogies and one turbojet reference to build, the nine build steps, and the deviation bands — **derived from the one existing generated deck's measured misses, not chosen** |
| [`catalogue.md`](catalogue.md) | **the data.** Eighteen rows, each with acquisition range and frame time, weapon envelope, whether the weapon binds the shooter, warning receiver and countermeasures, and the eight performance anchors. Every number with a source and a tier; ten disputes carried unresolved. A second table says, per row, what its FIRE CONTROL can do and which rows still cannot employ anything |

## The one-sentence answers

| Question | Answer |
|---|---|
| Does a catalogue cell fly on JSBSim? | **Ten do, eight do not**, on a two-part test: a deck iff *(the row's own manoeuvre decides an outcome)* ∧ *(its envelope is published)*. The test never splits a row, because fighter data **is** envelope data and nobody ever published a drag polar for a tanker |
| Where does the deck come from? | **one recipe, eight anchors, closed-form inversion.** The thrust deck is frozen by a declared analogy and all residual error is absorbed by the drag polar — the rule `../mig29/flight-model-spec.md` wrote before its deck existed and which correctly predicted that deck's two worst misses |
| How much pilot does a row get? | **five tiers**, and a tier is a declared TASK SET plus the hooks a row's own *measured* deck supplies — not a class. `FBAirPilot` adds exactly two states (`Orbit`, `Drag`); everything above them is `pilot/FBPilot` unchanged |
| Does the early-warning row break the perception boundary? | **No, and the ground's solution carries verbatim** — the report is a POINT with the sender's own look age and no id field; the member re-centres its own antenna and must still find, firm and gate the target. **The price is one thing `air-defence-network.md` deferred**: a second comms slot on the receiving fighter, because its Datalink block already carries Link-16 |
| How do you know an opponent lost as itself? | `band_deck ≤ 0.25 × band_doctrine` on the tournament instrument `duels.md` already runs, plus a control cell that swaps the row's deck for a pinned one. **Both bands are printed beside every result** |
| What does it cost the rest of the tree? | 7 store rows, 7 generated missile decks, 6 gun rows, 2 `set` keys, 1 sensor derivation — and **zero** new seeker kinds, emitter kinds or health ids |

## Schema

The four sections `## Spec` → `## State` → `## Gaps` → `## Knowledge` in [`module.md`](module.md), as
everywhere in `doc/`. [`catalogue.md`](catalogue.md) is **reference data** and carries the source/tier
schema of [`../mig29/`](../mig29/INDEX.md) instead; [`flight-model-recipe.md`](flight-model-recipe.md)
carries a **thin** four-section frame around its three-column build order, for the same reason
[`../mig29/flight-model-spec.md`](../mig29/flight-model-spec.md) does.

**Confidence tiers** are the campaigns' own: `[T1]` official · `[T3]` established specialist literature ·
`[T4]` encyclopaedic · `[DISPUTED]` both carried · `[SET]` a FlightBox setting with a reason ·
`[DERIVED]` computed from a named relation · `[TODO]` not sourced and not set.

**The sourcing is thin in one specific direction and the catalogue says so first:** the
**flight-performance** half is well sourced (seventeen of eighteen rows publish a complete specification
set, and five of the ten deck rows close **both** free consistency probes to under 1.5 %), while the
**sensor and weapon** half is not — no scan period is published for any airborne fire-control radar
here, and **no radar range in the catalogue states the target size it was measured against except the
Su-27's.**
