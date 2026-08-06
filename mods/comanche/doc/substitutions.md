# Substitutions — what campaign one flies instead of what it names

> **The engine has no rotorcraft.** Not a missing asset — a missing capability, and this file is the
> measurement of how much of Comanche that costs. **No flight model was invented** (`CLAUDE.md`
> Prinzip 1). Every role in [`campaign.md`](campaign.md) §2/§6/§9 is mapped onto the nearest EXISTING
> module and every mapping carries the direction of its error, measured where it could be measured.
> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has no `test/`: the missions are the test.
>
> Sources for the module rows: `mods/f16/src/catalogue.fba` (18 catalogue rows, borrowed through
> `depends`), `sim/src/core/FBSite.h` (`kSiteCatalogue`, 9 rows),
> `sim/src/modules/ground/FBGroundTarget.h` (2 rows), `sim/src/core/FBStore.h`. Provenance tags as in
> `campaign.md`; `[MESS]` = measured in this round by `fb-gym`, with the run named.

## Spec

### 1. The rule

**One role, one substitute, one direction of error, stated.** Where a second candidate existed, the
refused one is named with its reason — a substitution whose alternative is invisible is a preference,
not a decision.

**The decisive property wins**, and for this title the decisive properties are named by the manual
itself: speed band, hover, flight height, agility `[MAN p.2, 36, 37]`. Where no row carries any of
them, the substitution is recorded as a **failure with a number** rather than dressed up.

### 2. The player and the wingman

| Role | Substitute | Why this row | Direction of the error |
|---|---|---|---|
| **RAH-66 Comanche** (player, all 10) | `f16` | the only module in the tree with an air-to-ground phase at all. `set task attack` exists on `f16` and `mig29` and on no catalogue row, and a campaign whose every goal is a ground object has no other candidate | **total, in four named axes — §3** |
| **RAH-66 wingman** (type 4, 6 of 10) | `f16`, `flight mNN 2` | same row | same, plus: the game's wingman is a WEAPON the player aims (`N` designates, *his* Hellfire flies `[MAN p.51]`). No unit in this tree may command another unit's release, so the wingman here flies its OWN briefed pass — a different tactic with the same airframe |

#### 2.1 The four axes, measured

| Axis | The original | The substitute | Direction | How measured |
|---|---|---|---|---|
| **Speed** | simulated top **177 kt** `[MAN p.37]`, and NoE tactics slower still | **300 kt** | 1.70× too fast | `[MESS]` the campaign's own pass at **180 / 200 / 220 kt CRASHes** — `FBPilot`'s attack egress (120° turn + 500 m climb) bleeds the jet to **88 kt CAS at 543 m** and it mushes in; **250 and 275 kt CRASH** on mission 2's geometry; **300 kt survives every pass tried**. The lowest survivable number was taken, not a comfortable one |
| **Hover** | a commanded state (`*` = auto-hover at NoE height), on collective, cyclic, tail rotor and ground effect `[MAN p.18]` | **none of it exists** | infinite | nothing to measure |
| **Height** | ceiling ≈ **500 ft ASL**, flown at 10–50 m `[MAN p.2, 36]` | **150 m** for every pass | ≈ at the original's ceiling | `[MESS]` and it is the ONE axis where the substitution HELPS: the same CCRP delivery misses by **26.60 m** released from 900 m and by **8.01 m** from 150 m, so a `target_hard` that survives the high pass **dies** on the low one |
| **Agility** | **+3 g**, yaw decoupled from turn (fantail) | +9 g, yaw coupled | opposite sign, unmeasured | no run in this campaign reaches a turning fight |

### 3. The enemy

| Role | Substitute | Why this row | Direction of the error |
|---|---|---|---|
| **Ka-50 "Hokum" / Werewolf** (types 1 and 6, **82 objects** across the campaign) | `ah64` | the catalogue's rotorcraft **mover** row, and its kinematics are nearly exact: span **14.63 m** against the Ka-50's 14.5 m rotor, `max_ms` **101.3** against its published 350 km/h = 97.2 m/s `[MAN p.112]`, `bank_deg` 20 | **it is scenery.** `[MESS]` **82 Werewolves, 0 weapon events of any kind** across the ten missions. The campaign's central asymmetry — a Werewolf out-dashes and out-guns the Comanche `[MAN p.112–114]` — is deleted entirely, in the player's favour |
| **T-80 main battle tank** (type 2) | `target_hard` | a 42 t MBT is neither a house (`target_soft`) nor unhardened; the hard class needs ~8 m to be finished and ~15 m to be hurt, which is what a bomb has to do to an MBT | **it does not shoot and it does not move.** The manual's AT "Songster" anti-helicopter missile `[MAN p.119]` — *the enemy tank shoots back at aircraft* — has no expression: a ground target carries no weapon. **`zsu23` refused**: it would shoot, but it is a radar-directed AAA vehicle and would put an emitter in the RWR that a T-80 never had |
| **SA-8 Gecko** (type 5) | `sa8` | the catalogue row, with its own envelope, magazine, reaction time and command guidance | **none in kind.** This is the campaign's only exact substitution and the only thing in it that can hurt the player |
| **fuel / oil tank** (type 3) | `target_soft` | a thin-walled tank | none worth naming |

#### 3.1 Why not a fighter for the Werewolf, and why not a catalogue row of its own

Two alternatives existed and both were refused, one of them **after** it was measured:

| Refused | Would have given | Why not |
|---|---|---|
| `mig17` (T2, guns, no radar, visual acquisition) | a Werewolf that actually fights | it is a 250 kt jet whose fight happens at thousands of metres. A title whose entire subject is 50 m and 100 kt would be measuring a fixed-wing engagement and calling it Comanche. The *kinematic* error would then be the one hidden, and it is the larger of the two |
| a hand-written `ka50` catalogue row: mover, **T2**, `gun gsh301`, `gun_rounds 500`, `stations 4` | an armed rotorcraft, declared rather than coded | `[MESS]` **byte-identical to the unarmed `ah64`** — the row never fires. `FBAircraftSpec::CanEmploy` grants it a fire control, but T2's only combat phase is `Bfm`, and `set task bfm` is **REFUSED at spawn** on a row whose roll plant is unmeasured, which every mover's is. A mover accepts no `set` line except `task route`. **An armed rotorcraft is not declarable**, not merely unwritten |

And the other half of the same hole, measured from the player's side: an F-16 with `set task
intercept`, two AIM-9 and two AIM-120 against one `ah64` at 100 m gets **one** `RADAR_CONTACT` at
3.65 nm and **never fires**. A helicopter at 70–100 m/s sits inside every Doppler notch in the tree —
which `mods/f16/src/catalogue.fba` states about its own two rotorcraft rows in advance, and which is
therefore not a surprise but a confirmed consequence.

### 4. Weapons

| Role | Substitute | Why | Direction of the error |
|---|---|---|---|
| **AGM-114 Hellfire**, laser-guided, standoff **> 8 km**, lock held to impact `[MAN p.49]` | `mk82` | it is what `set task attack` can actually deliver. `mods/f22/doc/substitutions.md` §6.1 measured the alternative: `set task attack` **cannot** deliver a laser-guided weapon — the automatic phase releases and turns away, the designation breaks in flight, and the miss grows with the time of flight | **enormous, both ways.** No guidance and no standoff: release is at ~1 km instead of >8 km, and the aim error is 4–18 m `[MESS]` instead of a direct hit. But the warhead is **227 kg against the Hellfire's ~9 kg**, so one substituted "Hellfire" destroys a stack of four T-80s that four real ones would have had to hit individually |
| **AIM-92 Stinger**, IR, fire-and-forget, 1–2 km `[MAN p.50]` | `aim9` | the tree's only short-range infrared air-to-air round | longer-legged than the original, and **never fired in any of the ten missions**: there is nothing engageable in the air (§3) |
| **20 mm turreted Gatling, 500 rounds, 1 500 rds/min** `[MAN p.48, MIS]` | `set gun_rounds 500` on the F-16's M61A1 | the round count is the game's own and never varies in any of the twenty missions of the 1992 release `[MIS]` | the gun is nose-fixed instead of **turreted**, which deletes the off-axis gunnery the manual's helmet sight exists for. Never fired in any of the ten missions |
| **70 mm rockets, 62 per mission, flechette, lethal to 2 km** `[MAN p.49, MIS]` | **nothing** | there is no rocket store row in this tree. `hydra70` / `s8` are specified in `doc/air-to-ground.md` §3.5 and not built | the campaign's second weapon, present in all twenty missions of the release, is absent from all ten files |
| **155 mm / MLRS artillery, called by TAS coordinates**, 2–8 calls in 5 of 10 missions `[MAN p.50, MIS]` | **nothing** | no off-map fire support, no C2 net, no delayed-arrival ordnance | mission 7 is built around it (*"a few good men in the artillery ranks are behind you 100 %"*, 8 calls) and flies without it |
| **wingman Hellfire hand-off**, 6–14 per mission `[MIS]` | **nothing** | no unit may command another unit's release | the wingman flies its own pass instead (§2) |
| **automatic chaff and flare with manual override** `[MAN p.41, 46]` | `set cmds_mode auto` | the F-16's ALE-47 in automatic | the magazine is the F-16's own; the game states no count |

### 5. Terrain, ground and the clock

| Role | Substitute | Direction of the error |
|---|---|---|
| the four `D*.DTA` heightfields, 1 024², ~1.22 m per unit, **0–148 m of relief** (`terrain.md` §1–3) | **flat 0 m** (`--elev const`) | **terrain masking does not exist**, and the manual calls it *"the essence of modern helicopter warfare"* `[MAN p.2]`. Two structural reasons and neither is laziness: nothing in this tree loads a `Kyle DTA`, and **`mod.json` carries exactly one `"dem"`** while this campaign's four maps are anchored to four disjoint theatres (Peru, Utah, Hawaii, Afghanistan — `terrain.md` §6). One mod, one raster, four continents |
| the game's start **heading** on the player and the wingman | the **bearing to the briefed aim point** | `[MESS]` and it is an engine finding, not a preference: `set task attack` anchors its run-in leg at the phase's first tick and never corrects a lateral offset. Spawned on the game's own heading the strikers released **249.33 m** across track (mission 1) and **8 382.30 m** (mission 3). **The attack phase cannot fly itself onto its own run-in.** Every Werewolf and every ground object keeps the game's heading unaltered |
| the object **spawn delays** — mission 1's 14-ship stagger over ticks 60–190, mission 6's four waves at 3 600–7 200 | **all at t = 0** | `.fbm` is not a schedule (`doc/missions/syntax.md`). Mission 6's reading rule — *a slow run meets an enemy a fast run never sees* — has no object |
| the game's per-mission **fuel** (6 000–20 000, unit undecoded) | `set fuel_int_pct = fuel / 20 000` | the ratio against the campaign's own maximum is the only defensible mapping. `[MESS]` it does not reproduce the constraint: mission 3 at 30 % burns **2 091 → 1 506 lbs** over the full 600 s run with no BINGO and no warning |
| **night**, an image-intensifier palette swap over the same terrain (`terrain.md` §1) | a `time` line whose sun elevation is **checked**, not assumed | `[MESS]` the five night files measure **−65.08 / −65.11 / −83.05 / −70.05 / −71.56°** at the lead's spawn, the five day files **+81.37 / +37.89 / +44.47 / +56.61 / +44.51°**. What night then does is remove visual acquisition; there is no image intensifier and no green rendering anywhere in this tree |

## State

All ten `.fbm` files in `../src/missions/` are built and run under `--mod ../mods/comanche`. The cast,
weapon and terrain mappings above are the whole set; each `.fbm` repeats in its own header only the
disclosures it uses, and mission 1 carries all seven in full.

### 6. What the substitutions cost, measured across the whole campaign

One run per mission, `--elev const`, `fb-gym`. Verdict per file is its own reading rule.

| # | Mission | Goals met | Exit | The number that decides |
|---|---|---|---|---|
| 1 | Werewolves on Patrol | **4 / 26** | 1 | the exit is the WINGMAN's: a 9M33 direct hit (0.052 m, flux 4.94e6) at 997 m, while it climbed to its egress waypoint — **out of the NoE band**, where the SA-8 that could not reach it at 150 m could |
| 2 | The Last Sacrifice | **4 / 17** | 3 | two bombs, two stacked T-80 pairs, aimErrM 4.56 / 4.78. No SAM in the mission at all |
| 3 | Tactical Run | **1 / 5** | 3 | the "minimal" fuel is not minimal for the substitute: 2 091 → 1 506 lbs |
| 4 | Rivers Run Deep | **4 / 14** | 3 | five objects destroyed by two bombs; the briefing's *kill the Werewolves first* has no object |
| 5 | Night of Death | **2 / 3** | 3 | the campaign's best result, and the only goal set small enough that one striker per aim point is nearly enough. 54 SA-8 launches, closest 209.91 m |
| 6 | Thirsty Werewolves | **1 / 4** | 3 | 29 objects, all at t = 0; the two fights the reading rule demands collapse into one |
| 7 | Spiritual Reclamation | **2 / 11** | 3 | ONE bomb destroys FOUR objects stacked on cell 690,475, two of them goals. 8 artillery calls, 0 declarable |
| 8 | Volcanic Nightmare | **4 / 19** | 3 | the fragility class doing its job: `two`'s 14.44 m miss kills nothing, `lead`'s 7.73 m hit kills a four-T-80 stack |
| 9 | Valley of Instant Death | **1 / 16** | 3 | 14 Hellfires declared, 4 `mk82` carried, 1 released |
| 10 | Wolfpack | **2 / 12** | 3 | closest SA-8 approach of the campaign: **6.84 m** against a 6 m fuze. 8 Stingers declared, 0 fired |

**19 of 127 goals, 214 SA-8 launches for one kill, 82 Werewolves that never act.** The dominant term
is not the substitute airframe and not the weapon — it is that **one striker is one aim point**:
`set task attack` releases at the active waypoint, egresses, hands back to `Route`, and nothing in the
format or in `FBPilot` re-enters it. Blue is the game's own cast (the player, plus the wingman exactly
when the loadout's sixth field is non-zero, 6 of 10 `[MIS]`), so at most two aim points can be struck
against goal sets of 3 to 26. Yields above two come entirely from the source data stacking objects on
one cell.

## Gaps

- **Every number above is ONE run of one geometry.** `doc/doctrine-evolution.md`'s spawn-grid work
  exists because combat outcomes flip; the delivery numbers (no opposition before release) are the
  trustworthy ones, mission 1's wingman loss and mission 10's 6.84 m near miss are not.
- **The aim points are a tasking, not data.** The game states a start cell and a start heading and
  nothing else; which goal a striker attacks is chosen here by a stated rule (nearest starred ground
  object to that unit's spawn, and for the wingman one that does not share the lead's cell). A
  different rule gives different met counts, and the campaign has no route data to appeal to.
- **The 6 000-fuel constraint was not reproduced and no better mapping was found.** The game's fuel
  unit is undecoded (`campaign.md` §5); the ratio mapping is the only one with an argument behind it,
  and it measures nothing.
- **The SA-8's loft was not investigated.** `[MESS]` at 900 m a single site killed an F-16 outright,
  at 150 m its best round missed by 11.87 m against a 6 m fuze, and at 3 000 m by 30.29 m — but across
  the campaign 214 launches produced one kill, and many rounds climbed to 5–6 km and expired at their
  60 s limit with 3–7 km of closest approach. Whether that is geometry (a target running away) or a
  guidance defect is **not decided here**, and it is the one open engine question this campaign
  raises that is not about rotorcraft.
- **`target_hard` for the T-80 is an argument, not a source.** No source read gives hardening for any
  object in this campaign.
- **No mission was flown twice with a different run-in speed to see whether 300 kt is stable.** It was
  measured to survive on three geometries; the other seven were flown at it and did not crash, which
  is weaker evidence than it looks.
- **The unlock structure is recorded in the headers and enforced nowhere.** Missions 4–10 carry `*`
  `[MIS]`; no `.fbc` was written, because a campaign file carries destroyed units forward between
  steps and these ten missions stand on four disjoint theatres where carrying anything forward would
  be meaningless.
