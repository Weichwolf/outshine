# `modules/ground/` — the unit level

**`C1`: one ground unit that emits and shoots.** Step 2 of the owner goal, and the only open contract the
foundation round left behind. **Specified here, not built.** No line of `sim/` was touched to write this
directory.

Why this and not something else: after the two flyable jets and the two ground-target kinds, the next
**four rows** of [`../../campaigns/INDEX.md`](../../campaigns/INDEX.md)'s cast table — a radiating
radar/GCI site (7 campaigns), AAA and MANPADS (6), a fixed SAM battery (5), a mobile SAM battery (5) — are
**one system four times over**. `C1` blocks six campaigns and degrades three, and it is the only gap whose
absence removes not the *ability* to make tactical decisions but their *reason*.

## The three files

| File | What it answers |
|---|---|
| [`module.md`](module.md) | **the contract.** Where the line between a module and a unit runs and why; the class, its slots and its layer; how it sees without widening the registry gate; what it radiates and the one core change that needs; what it launches; the engagement machine; damage; the mission grammar; the ten acceptance criteria — and the five open questions of [`../../weapons.md`](../../weapons.md), answered |
| [`catalogue.md`](catalogue.md) | **the data.** Nine rows — `p18` `sa2` `sa3` `sa6` `sa8` `zsu23` `zu23` `sa7` `sa18` — each with its search set and its fire-control set *separated*, band, envelope, altitude band, reaction time, engagement channels, whether the guidance binds the emitter to the target, and what killing or countering it does. Every number with a source and a tier; seven disputes carried unresolved |
| [`cast.md`](cast.md) | **the rest of the campaign cast**, at four quantities per type — presence, motion, signature, weapon — plus the one question that decides whether a type is a unit or a module |

## The one-sentence answers

| Question | Answer |
|---|---|
| Why is this not a module? | A module is a **flown** airframe: FDM, avionics bus, pilot phase machine, one class per type, a reference base of its own. A site has none of those and needs none — it needs damage, roster presence, an emission and a missile, and all four already exist |
| What do the two levels share? | The whole of `units/FBSimUnit`: identity, team, published pose and signature, health register, damage model, roster, telemetry file, mission judge, the `.fbm` `unit` block, the registry key |
| How is the anti-cheat boundary held? | Its four detectors **derive** from bases that already read the registry, and a derivation adds no `#include`. `tools/verify_layers.py`'s `RESTRICTED` table stays at **six** files, and that is an acceptance criterion, not an intention |
| Does it inherit the health register? | No — it **uses** it. `FBSystemHealth` stays monotone, private-mutator, one-friend and unchanged; five existing ids carry everything a site has, and killing its `Radar` silences it through a coupling written years before this |
| What is the one core change asked for? | `FBUnitSignature` carries **two** emitter beams instead of one, because a battery is two antennas and collapsing them deletes the search→track transition for every observer except the one being tracked |

## Schema

The four sections `## Spec` → `## State` → `## Gaps` → `## Knowledge` in [`module.md`](module.md), as
everywhere in `doc/`. [`catalogue.md`](catalogue.md) and [`cast.md`](cast.md) are **reference data** and
carry the source/tier schema of [`../mig29/`](../mig29/INDEX.md) instead — the same exemption
[`../../INDEX.md`](../../INDEX.md) grants the module reference bases.

**Confidence tiers** are the campaigns' own, unchanged: `[T1]` official · `[T3]` established specialist
literature · `[T4]` encyclopaedic · `[DISPUTED]` both carried · `[SET]` a FlightBox setting with a reason ·
`[DERIVED]` computed from a named relation · `[TODO]` not sourced and not set.

**The sourcing is thin and says so:** eight of the nine catalogue rows rest on `[T4]`, one has a `[T3]`
monograph that disagrees with the `[T4]` entry, and no `[T1]` threat handbook was read. The envelope
numbers are the load-bearing ones and they are the ones a `[T1]` source would most likely move.
