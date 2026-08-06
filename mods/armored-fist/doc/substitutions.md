# Substitutions — what Overwatch flies instead of what it names

> Armored Fist is a TANK game and this engine has no tracked vehicle. **No vehicle model was invented**
> (`CLAUDE.md` Prinzip 1). Every role in [`campaign.md`](campaign.md) §1/§3 is mapped onto the nearest
> EXISTING module and every mapping is recorded here with the direction of its error. Form per
> [`doc/mods.md`](../../../doc/mods.md) §3. A mod has no `test/`: the missions are the test.
>
> Sources for the module rows: `mods/f16/src/catalogue.fba` (18 catalogue rows, borrowed through
> `depends`), `sim/src/modules/ground/FBGroundTarget.h` (2 rows), `sim/src/core/FBSite.h`
> (`kSiteCatalogue`, 9 rows — none used, see §3), `sim/src/core/FBStore.h`. Provenance tags as in
> `campaign.md`.

## Spec

### 0. The one substitution everything else hangs from

**The player's vehicle cannot be represented at all, so the acting unit changes species.** Four
independent facts, each read out of the code rather than inferred:

| What is missing | Where it is visible |
|---|---|
| nothing drives | `modules/ground/FBGroundModule::Run` is empty by design; a ground unit's whole per-tick behaviour is that its pose is the declared one |
| nothing on the ground sees | `sensors/FBRadarSystem.cpp:86` and `FBIrstSystem.cpp:126` skip every unit that is not `FBUnitKind::Aircraft`; `FBVisualSystem` dropped its species filter on 2026-08-05 and keeps only units whose catalogue DECLARES an extent — both ground classes declare `0.0, 0.0, 0.0` |
| nothing on the ground shoots | `FBGroundTargetSpec` is `{Key, FBDamageLayout}` and nothing else: no station, no gun, no fire control. A `target_hard` cannot be given a weapon without inventing a class |
| an air-defence position is not a substitute | `FBSiteModule`'s engagement machine reads its own radar block, and that radar returns aircraft only — a SAM cannot see a tank |

So: **Echo company flies as `module f16`, one jet per vehicle of the [GUIDE] order of battle.** The
direction of that error is not a nuance and is stated in every mission header (D1). What survives is
the mission's SHAPE — axis of advance, objective list, order of battle, time limit, clock. What is lost
is everything the original is ABOUT.

**The alternative was considered and refused.** `ah64` exists in the catalogue, so Echo's callable air
support could have been the acting unit — closer to the original's own fiction. It is a **kinematic
mover**: no flight model, no stations, no gun, no fire control, `radar_az_half_deg 0`. An `ah64`-only
campaign delivers nothing at all and measures nothing. `f16` is refused as a portrait of an M1A2 and
taken as the only thing in the tree that can execute a mission task.

### 1. The rule for everything below

**One role, one substitute, one direction of error, stated.** Where two candidates existed the refused
one is named with its reason. **The decisive property wins**: armour beats mass, footprint beats
warhead weight, guidance beats calibre.

### 2. Vehicles

| Role | Substitute | Why this row | Direction of the error |
|---|---|---|---|
| **M1A2 Abrams** (player, Echo company) | `f16` + one air-to-ground store | the only module with `set task attack` besides `mig29`, hence the only thing in the tree that can execute "destroy that" | **enormous, both ways.** An F-16 with a canister levels in one pass what a tank platoon needs an afternoon and its losses for; it also cannot occupy ground, cannot be ambushed, has no 120 mm gun, no thermal sight, no smoke and no crew. `mig29` was refused: it has the attack phase but is the opposing side's airframe |
| **M3 Bradley CFV** | `f16`, same load as the M1A2 | it is a VEHICLE of the blue order of battle and the substitution is of the whole company's combat power onto one flyable thing | **overstates it by more than the M1A2 row does.** A Bradley is a scout carrier with a 25 mm gun and TOW; giving it a 2 000 lb bomb is absurd. Dropping it instead would have understated blue by a third to a half with no source saying so, so it is declared and the error is named |
| **T-80 MBT** (Bandit platoons) | `target_hard` | armour is the decisive property, and `target_hard` is the tree's armoured rung: Mk 84 fail radius **17.7 m**, degrade **33.5 m**, CBU-87 flux 8 % of threshold = nothing | **understates the tank, against blue.** A 2 000 lb bomb kills a 46-tonne MBT well beyond 17.7 m. `target_soft` was refused: its 100 m Mk 84 fail radius would make an MBT more fragile than a truck |
| **BMP-2 IFV** | `target_soft` | thin aluminium, and the CBU-87 is the anti-light-armour area weapon the original's own doctrine would use | **overstates its fragility, in blue's favour.** `target_soft`'s 100 m Mk 84 fail radius is a building's; a 14-tonne IFV is roughly 30–40 m. The ladder has two rungs and an IFV falls between them — `target_hard` at 17.7 m would understate by about as much in the other direction |
| **Mi-24 Hind** (missions 3, 7) | `mi8` (Mi-8MT) | a Soviet helicopter: low, slow, inside the Doppler notch of every radar in the tree, so a visual/IR target and not a radar one — the platform property is right | **total and one-sided.** A Hind exists to kill armour; `mi8` is a mover with no stations, no gun and no fire control. It transits and threatens nothing. `mig23`/`mig21` were refused: they can shoot, but only at aircraft, which would invent an air-to-air fight Overwatch does not have |
| **AH-64 Apache** (mission 6, callable support) | `ah64` | the catalogue's own row, same airframe | it carries nothing and reports to nobody: there is no channel by which a mover's picture reaches another unit, which is precisely what [GUIDE p.70] uses it for |

### 3. Positions and structures

| Role | Substitute | Why | Direction of the error |
|---|---|---|---|
| forward base (1), field command encampment (3), logistics depot (4), refuel depot + fuel/propane tanks (6), comms links + compound (7) | `target_soft` | buildings and stores dumps; `target_soft` IS "an unprotected installation" | none worth naming, except that each is declared ONCE because no source gives a count (§5) |
| artillery emplacements (1, 3) | `target_soft` | towed guns in the open, no overhead protection | understated in count, not in class |
| field command bunkers (5) | `target_hard` | the word in the briefing is "bunker" and `target_hard` is documented as exactly that | none; this is the one row where the class is named by the source |
| satellite dishes (5) | folded into the nine `target_hard` bunkers | [GUIDE p.65] names them in the same objective area and the 9-way split is not recoverable | **pessimistic**: a dish is soft and is carried here as hard, so mission 5 is judged harder than it was |
| **minefields** (4, 5, 6, 7) | **nothing** | there is no mine, no ground movement and nothing to trigger one | the whole approach-march risk of four missions is absent |
| **air defence** | **nothing, deliberately** | no Overwatch source names a SAM or an AAA position, so declaring one would be invention | see §6.1 — it makes the campaign unlosable |

### 4. Weapons

| Role | Substitute | Why | Direction of the error |
|---|---|---|---|
| the tank platoon's fires against soft targets | `cbu87` | a 400 × 200 m footprint at 3.11e3 J/m², above `target_soft`'s 2.8e3 threshold and at 8 % of `target_hard`'s — so it kills every BMP and every installation under it and does literally nothing to a T-80. That split is the correct one for a cluster munition against mixed armour | the weapon is right for the job and wrong for the shooter. Nothing else in the tree takes a whole platoon in one pass |
| the tank platoon's fires against armour | `mk84` | the heaviest unguided store in the catalogue, and `gbu12` is refused for the reason `mods/f22/doc/substitutions.md` §6.1 measured: a laser round's designation breaks in the egress turn and the miss grows with time of flight | **it does not work.** 10 Mk 84 released across the campaign, 0 inside `target_hard`'s 17.7 m, 0 T-80s and 0 bunkers destroyed |
| the M1A2's 120 mm gun, the M3's 25 mm and TOW, smoke, the FIST artillery call | **nothing** | direct fire from a ground vehicle has no representation; the tree's gun bundles resolve against aircraft only (`FBGroundTarget.h` states so outright) | the mission's actual mechanic is absent |

### 5. The counting rule, and where it bites

`campaign.md` §3.2: goals are per-mission DATA and the flag lives in the `DCBS` chunk, which is **not
decoded**. Two goal counts survive in prose — 11 (Thunderclap) and 13 (Night's Quest) — and nothing
else does. So:

| Situation | Rule applied |
|---|---|
| the [FSW] task names an element in the plural with no count | declared **ONCE**, and the understatement is stated in the file |
| a goal count exists and the order of battle closes on it | used: mission 4's 10 vehicles + 1 depot = **11** exactly, the one place a guide count and a guide table check each other |
| a goal count exists and the order of battle does NOT close on it | split with the assumption named: mission 5's 13 = 9 bunkers + a 4-vehicle command troop, where "a platoon is 4 vehicles" comes from `PINF`'s 8 slots / 4 per side and `[MAN p.10]` |
| which platoon is which named thing | by elimination, and said so: Bandit 4 = the command troop (5), Bandit 1 = the compound garrison (7) |

### 6. What the substitutions cost, measured

#### 6.1 The campaign is not losable, and that is the headline

Red has no sensor, no weapon and no motion in all seven files. `objective survive` is met by every blue
unit in every run; no blue unit was ever damaged. **The loss half of all seven reading rules
(`campaign.md` §6) has no object**, and with it: mission 1's "Echo-1's M1 survives", mission 3's
"Echo-2 losses do not fail the run", mission 6's "loss of Echo-3's M1 is expected". Three reading rules
out of seven are half unjudged for one reason.

#### 6.2 One pass per unit sets every mission's arithmetic

`pilot/FBPilot::AttackCommands` latches `AtkReleased_` after the single pickle, egresses and returns to
the route phase; nothing resets it. A delivery unit therefore delivers ONE weapon at ONE aim point.
Consequence: **a mission with 13 goals needs 13 delivery units**, and Echo company has six.

| Mission | goals declared | delivery units | goals dead |
|---|---|---|---|
| 1 Slaughterzone! | 12 | 2 | **4** |
| 2 Night Forger | 12 | 2 | **3** |
| 3 Rubicon | 2 | 2 | **2** ✔ |
| 4 Thunderclap | 11 | 4 (one platoon forbidden) | **4** |
| 5 Night's Quest | 13 | 6 | **2** |
| 6 War Hammer | 2 | 7 | **2** ✔ |
| 7 Corrosion | 6 | 6 | **4** |

#### 6.3 Nothing armoured died, and the number behind that

**29 deliveries**, mean `aimErrM` **46.04 m**, range **18.39 … 71.93 m**. `target_hard` needs **17.7 m**.
**Zero deliveries reached it; zero `target_hard` units were destroyed in the entire campaign** — six
T-80s in mission 1, seven in 2, two in 3, six in 4, nine bunkers plus seven T-80s in 5, eleven in 6,
nine in 7.

The five Mk 84 flown at mission 5's nine bunkers are the largest single sample: **62.46, 63.60, 63.71,
66.20, 69.96 m**, five identically flown point-target deliveries, none within a factor of three of the
requirement.

#### 6.4 The delivery error is a function of the run-in LENGTH, and that is a lever this mod pulled blind

Isolated on the f16 fixture's own flat geometry — same store, same 900 m AGL, same 450 kt, one variable:

| Run-in | `aimErrM` | `aimAcrossM` |
|---|---|---|
| 25 km, heading 090 (`mods/f16/src/missions/attack-ccrp.fbm`) | **12.41** | **+10.99** |
| 40 km, heading 090 | **40.04** | **−33.93** |
| 40 km, heading 000 | **42.61** | **−36.87** |

Not the heading and not this mod's coordinates: the attack leg is anchored where the pass begins and
the lateral hold **overshoots** on a long one — the sign of the across error FLIPS between 25 and 40 km,
so it oscillates rather than drifts. Across the campaign's own 29 deliveries the across error is
negative in **25 of 29** (mean −34.9 m) while the along error is positive in **29 of 29** (mean +24.4 m):
every bomb in this campaign fell **long and left**.

**Every mission flies 40 km.** At 25 km a Mk 84 would land inside `target_hard`'s 17.7 m fail radius and
the campaign's armoured half would begin to die. The 40 km was chosen for the approach march BEFORE any
of this was measured; it stays, because shortening it now would be tuning the scenario until it passes.
The lever is named and numbered instead.

#### 6.5 The clock is a stamp and the night mission is not dark

`INDIA5` is the campaign's only night mission by its own sky asset, and the file declares 1900Z — the
runner's own `mission CLOCK` line answers **sunElDeg −69.39**, so the reading of `7.SKY` is confirmed by
the ephemeris. **Nothing else changes.** `FBVisualSystem` is the only sensor with a daylight term and it
never sees a ground target anyway; the F-16 has no thermal sight; the original's entire point here —
"working with the thermal sights" — has no representation. A night mission and a day mission differ in
this tree by one telemetry column.

#### 6.6 The order clause cannot be judged

Mission 7 is the one mission whose verdict depends on SEQUENCE ("the communications links **and then**
every element of the compound"). `objective` carries no ordering and `FBMissionMonitor` judges a set.
The declaration puts the comms links 2 km short of the compound on the run-in so the geometry enforces
what the grammar cannot, and it worked — `damage CLUSTER` at **t = 164.8** for the links and
**t = 173.7** for the compound. **That is an observation, not a verdict**: nothing fails a run that takes
them in the wrong order, because nothing can observe the order.

## State

All seven `.fbm` files and `c01-overwatch.fbc` are built and run. `--threads 1/2/4` byte-identical over
162 telemetry files and all seven `events.log`. Standalone: exit **3/3/0/3/3/0/3**; campaign exit 3
(the worst mission's). Two of seven fly their own condition.

## Gaps

- **`DCBS` is still undecoded**, so five of seven goal SETS in this mod are the [FSW] task text read as
  a list, not the game's own flags. If it is ever decoded, every `objective` line here is re-derived.
- **The world scale is still undetermined**, so the 2 km platoon separations, the 40 m vehicle spacing
  and the 40 km run-in are all [SET]. §6.4 shows the last of the three decides the campaign's outcome;
  the first two decide who falls under a canister footprint (measured on the way here: at 55 m spacing
  a 4-vehicle platoon's outboard vehicle fell outside the 100 m half-width and survived).
- **`target_soft` for a BMP-2 is unmeasured against anything.** No source in this tree gives a lethal
  radius for a 2 000 lb bomb against an IFV, so "overstates by roughly 2.5×" is an argument, not a
  measurement.
- **The 29 deliveries are one run each.** The mission set is deterministic, so a re-run reproduces them
  exactly; what is NOT established is how they would move under a different release altitude, speed or
  bias. No sweep was flown.
- **Mission 5's "rear-threat platoon" is unidentified** in any source read, so half of its reading rule
  has no object even setting aside the missing ordering.
- **The campaign's branch structure is unknown** ([`campaign.md`](campaign.md) §1.1), so `c01-overwatch.fbc`
  runs the seven straight through and asserts nothing about gating.
