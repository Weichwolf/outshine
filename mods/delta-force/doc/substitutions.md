# Substitutions — what campaign one flies instead of a man on foot

> **The engine has no body.** Not a missing asset — a missing capability, and this file is the
> measurement of how much of Delta Force that costs. **No body model was invented** (`CLAUDE.md`
> Prinzip 1). Every role in [`campaign.md`](campaign.md) §6/§9 is mapped onto the nearest EXISTING
> module and every mapping carries the direction of its error, measured where it could be measured.
> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has no `test/`: the missions are the test.
>
> Sources for the module rows: `mods/f16/src/catalogue.fba` (borrowed through `depends`),
> `sim/src/modules/ground/FBGroundTarget.h` (2 rows), `sim/src/core/FBStore.h`,
> `sim/src/systems/FBNavSystem.h`. Provenance tags as in `campaign.md`; `[MESS]` = measured in this
> round by `fb-gym`, with the run named.

## Spec

### 1. The rule, and the one place this title differs from its three siblings

**One role, one substitute, one direction of error, stated.** Where a second candidate existed, the
refused one is named with its reason.

Comanche and Armored Fist substitute a *vehicle* for a *vehicle*. Delta Force substitutes a **jet for
a pedestrian**, and there is no axis on which that is a small error. So the honest form here is not
"the nearest row": it is **a list of the capabilities the title needs and the engine does not have**,
with the declaration that stands in for each and the number that says what the substitution deleted.

### 2. The player

| Role | Substitute | Why this row | Direction of the error |
|---|---|---|---|
| **Bravo Two**, one soldier on foot, all six missions | `f16` | it is the only thing in the tree that acts. `set task attack` exists on `f16` and `mig29` and on no catalogue row; a mover accepts no `set` line except `task route`; a ground target accepts none at all. There is no third candidate, not a rejected one | **total, in six named axes — §2.1** |
| **no wingman anywhere** | — | Delta Force sends one man. Insurrection, Flood, Weatherman and Headhunter carry 2–4 friendly *Squad Members* [BMS], but they are AI soldiers, not a second player unit | this is the one place the substitution is FAITHFUL, and it is faithful by accident: `mods/comanche` gets two aim points from its wingman, this campaign gets **one** |

#### 2.1 The six axes

| Axis | The original | The substitute | Factor | How measured |
|---|---|---|---|---|
| **Speed** | a walking man, ~1.4 m/s; `Shift+arrow` walks slower still `[MAN p.10]` | **300 kt = 154.3 m/s**, ground speed measured at release **171.5 m/s** | **110×** | `[MESS]` `mods/comanche` measured 300 kt as the LOWEST run-in speed that survives `set task attack`'s egress; below it the jet mushes in. Consequence here: the striker crosses Insurrection's 1052 m edge in **6.1 s** against the **751 s** a man needs |
| **Height** | eye at ~1.7 m standing, less crouched, less prone `[MAN p.14]` | **150 m** | **88×** | not chosen: `[MESS]` the throw range at release measures **918.4–919.1 m** in the five armed files, and `mods/comanche` measured the same delivery missing by 26.60 m released from 900 m against 8.01 m from 150 m. 150 m is the low end that works |
| **Posture** | stand / crouch / prone — three contact sets, three eye heights `[MAN p.10, p.14]` | **one, and it is airborne** | infinite | nothing in [`body-format.md`](../../../doc/body-format.md) is implemented |
| **Ground line of sight** | every mission is decided by whether a position 200–500 m away *overlooks* the objective (`terrain.md` §1) | **no LOS query at any height** | — | and D6 removes the terrain as well, so there is nothing to be occluded BY |
| **Cover** | hostiles behind barrels, in windows, in a guard tower `[BMS]` | **none** | — | without occlusion those placements carry no meaning; Insurrection's `tower, guard` is a soft block that happened to survive at 50.1 m `[MESS]` |
| **The firefight** | 33–96 hostiles who shoot back | **none of them can** | infinite | `[MESS]` a hostile `zsu23` 200 m from a friendly `target_soft`, 120 s: **zero weapon events, both INTACT**. A ground unit has no weapon against another ground unit |

### 3. The enemy, and the two-row ground catalogue

| Role | Substitute | Why this row | Direction of the error |
|---|---|---|---|
| **hostile soldier**, types 5005/5006/5007/5041, **435 records** across the six missions | `target_soft` | the tree has exactly TWO ground classes and this is the softer. **There is no refused alternative** — `target_hard` is a 42 t MBT's class (`mods/comanche` §3) and would be worse | the class carries a **±10 m** damage zone and a **2.8e3** structural fail threshold for a man 0.5 m wide who dies to one rifle round; and it cannot shoot (§2.1). `target_hard` is used **nowhere** in this campaign: no object in six missions is armoured |
| **the druglord**, type 5025, one instance | `target_soft`, **team neutral** | he must be alive at the end, and team is a targeting class in this engine. `hostile` would put him inside `kill team hostile` — an objective set that cannot be satisfied, which is a declaration defect, not a hard mission | he cannot be CAPTURED. "Taken alive" is expressible only as `objective protect unit druglord`, which is the same bit read the other way round |
| **friendly Squad Member**, 5002 and 5048 | `target_soft`, team friendly | there is nothing that walks | they do nothing, and the campaign's `Alpha 1 is down.` `[DLG]` has no object. They are declared so that fratricide is measurable: `[MESS]` **0 friendly losses in six runs** |
| **buildings, tents, hangars, villa parts, the guard tower, the C-130** | `target_soft`, **team neutral** | the goal texts ask for dead soldiers, never for buildings | `[MESS]` collateral is then a NUMBER and not a verdict: Headhunter destroyed `villa16` and Flood spared the `C130` at 97.0 m |
| **the convoy**, 6× 2010 + 3× 2015 | `target_soft`, team hostile, at the measured column positions | thin-skinned trucks, not armour | **it does not move** — §5 |
| **Black Widow**, the extraction asset (type 2021) | `target_soft`, team friendly | `objective protect unit` needs a health bit and a position, and a mover has neither in a useful place | an aircraft that flies in becomes a stationary soft object. It cannot be shot down here, only bombed by its own side |

### 4. Weapons

| Role | Substitute | Why | Direction of the error |
|---|---|---|---|
| **M4 / MP5 / SAW / M40A1 / Barrett**, the mission's PRIMARY weapon `[MAN p.8]` | `set gun_rounds 30` — one M4 magazine | it is the only round-count the format takes | **the primary weapon of an infantry title has no target.** `modules/ground/FBGroundTarget.h` states it in its own source: presented area and extent are zero *"because gun bundles are resolved against aircraft only"*. `[MESS]` **zero gun events in all six runs**, 1 518 sim-seconds, against 435 hostile records |
| **2 satchel charges** with radio detonator `[MAN p.9]` | 2 × `mk82` | the lightest air-to-ground store in the tree — `core/FBStore.h` carries mk82 500 lb, gbu12 610, agm88 780, cbu87 950, fab250 551, fab500 1102, mk84 2039. **Nothing smaller exists** | **227 kg against roughly 10 kg.** `[MESS]` the measured consequence is Weatherman: one bomb destroys all NINE buildings of Objective Gale, which the original requires two satchel charges and a walk through 96 hostiles to place |
| the second satchel charge | **nothing** | `set task attack` is one pass and one release | `[MESS]` 2 declared, **1 fired**, in all five armed files; station 7 is loaded at t = 300 in every one |
| **6 fragmentation grenades, M203, 2 claymores, 2 LAWs, laser designator, Ka-Bar** `[MAN p.9]` | **nothing** | no row, no mechanism, and four of them are anti-personnel weapons in a tree whose damage model resolves against structures | the entire close-quarters half of the gear screen is absent |
| **`Copperhead` fire mission**, laser-designated artillery `[GAME]` | **nothing** | no off-map fire support | no Peru mission is known to enable it either (`hud.md` Gaps), so this one costs nothing measurable |

### 5. Movement, navigation and the clock

| Role | Substitute | Direction of the error |
|---|---|---|
| **the player's route** — `IP`, `CP ALPHA`…`CP FOXTROT`, `OBJ <weather word>`, `EP` `[CAMP]`, displayed as `WP3: CP BRAVO (117m)` `[SHOT]` | the striker's own two `wp` lines: aim point, then extraction point or egress | **`campaign.md`'s open question does not block anything.** The `.BMS` marker type that carries the player chain is unidentified — and it is not needed: the insertion point, the objective centroid, the named groups and (for Bad Habit) the extraction aircraft are all MEASURED coordinates, and a route through them is a route through the game's own geometry. What is lost is the intermediate checkpoints, i.e. the shape of the walk, not its ends |
| **the convoy's 1 891 m route** (3 AI chains × 12 nodes) | **nothing: nine static objects** | **there is no ground mover in this tree.** Every mover row in `mods/f16/src/catalogue.fba` is an aircraft, and a mover accepts no `set` line except `task route` (`mods/comanche` §3.1). Bad Habit's own reading rule — *"a run that arrives late fails on geometry"* — therefore has no object, and this file says so rather than reporting a green |
| **the walk itself**, 273–543 m from insertion to objective | a **3 000 m run-in** on the bearing to the aim point, spawning OUTSIDE the mission box | `set task attack` anchors its run-in at the phase's first tick and cannot correct a lateral offset (`mods/comanche` D6: 249–8 382 m across track). The run-in is **4×–11× the mission's own ingress** |
| **the extraction point**, in the three missions that carry the goal | Bad Habit's measured (553.9, 1242.7); Weatherman's and Masquerade's are **placed on their own insertion point** | their coordinates were not recorded (`campaign.md` §6 gives the type, the count and the rule "at the edge of the mission area", not the position). Error direction: an EP at the map edge becomes an EP 450.0 m and 506.7 m from the objective — **too close**, and §6 measures what that costs |
| **`Carrying: Code Book`** `[GAME]` | a waypoint on Objective Calm | the unit model has no item slot. Masquerade's goal 1 degrades from RETRIEVE to OVERFLY |
| **73 `tree, palm1` + 120 `tree, palm2`** (Bad Habit) | **nothing** | foliage is not a unit, and at 193 objects it would be the mission |
| **the `.TRN` presets**, day / night / dusk per mission (`terrain.md` §8) | a `time` line whose sun elevation is **checked**, not assumed | `[MESS]` the three day files measure **+87.48 / +62.13 / +57.85°**, the two night files **−69.11 / −73.05°**, and Headhunter's sunset preset **−1.10°** — civil twilight to a tenth of a degree. Date **1998-10-13**, the one dated artefact in the whole source set (the Manual Addendum's own stamp `[MAN]`); the game data carries no date |
| **the real ground**, 74–267 m of relief per box (`terrain.md` §5) | **flat 0 m** (`--elev const`) | and this is a CHOICE against available data. `[MESS]` **`fb-tiles` answers over all six boxes** — 629.30 / 537.00 / 516.55 / 638.00 / 281.90 / 688.72 m at the six centres, which closes `terrain.md`'s *"coverage for Peru is unverified"* gap and confirms its 274–1458 m band from below. `mod.json` names no `"dem"` because a baked raster is untracked by rule ([`doc/assets.md`](../../../doc/assets.md)) and a mission is a test that must run without a server |

### 6. The reconstruction rule for object positions, and why it cannot flatter the result

`campaign.md` §6 recorded, per mission, the hostile **count**, the hostile **centroid** and the
mission **footprint** — not the per-record coordinates. One rule, three clauses, applied everywhere:

| Case | Rule |
|---|---|
| a measured coordinate exists | it is used, unchanged |
| a **count** and a measured **extent** exist (the soldier force over its footprint; Flood's airfield 114 × 70 m; Weatherman's Objective Gale 86 × 59 m; Headhunter's villa 113.5 × 114.1 m) | N points on a square lattice, pitch = `sqrt(extent_x · extent_y / N)`, laid in Chebyshev rings so that **one object stands exactly on the measured centre** |
| a **centre** but no extent (Flood's 6-part village, Masquerade's two villages and its tent group, Weatherman's 2-part farm) | **one** object at that centre, part count in a comment |
| neither (Insurrection's two building groups and 2 vehicles, Headhunter's 2 vehicles, Bad Habit's 4 friendlies and its village) | **not declared**, and listed as an omission in the file that owns it |

Two properties of that rule matter and both are measured:

1. **Rings, not a row-major block.** A block puts the measured centroid in a lattice HOLE whenever
   `ceil(sqrt(N))` is even, and the aim point IS that centroid — the campaign's yield would then be
   decided by the parity of a reconstruction. `[MESS]` it was: the row-major first pass scored
   **0 / 58, 0 / 93, 0 / 82** on the three missions whose aim point is the soldier centroid; with
   rings the same three score **1 / 58, 1 / 93, 1 / 82**.
2. **The lattice cannot stack.** `[MESS]` over the five deliveries, everything within **31.3 m** of an
   impact died and nothing beyond **61.2 m** did. The lattice pitch is **118–235 m**, twice the widest
   kill measured, so no bomb ever gets two lattice neighbours. Where `mods/comanche` raised its yield
   because the source data stacked objects on one cell, nothing here is stacked — every yield above
   one comes from a group whose TIGHTNESS was measured (Gale's 23.7 m, the convoy's 10.2 m).

The band between 31.3 m and 61.2 m is not a radius: `[MESS]` Headhunter destroyed `villa16` at 47.4 m
and spared `villa17` at 55.0 m in the same burst, and Insurrection spared the guard tower at 50.1 m.
`FBGroundTargetSpec`'s Center zone is a ±10 m segment along each object's own heading, so the effective
distance is not the radial one.

## State

All six `.fbm` files in `../src/missions/` are built and run under `--mod ../mods/delta-force`. The
cast, weapon, movement and terrain mappings above are the whole set; `c02m01-insurrection.fbm` carries
all nine disclosures in full and the other five carry the short form and point back at it.

### 7. What the substitutions cost, measured across the whole campaign

One run per mission, `--elev const`, `fb-gym`, `--threads 1/2/4` byte-identical (524 telemetry files,
6 event logs). Verdict per file is its own reading rule.

| Play | Slot | Mission | Exit | Goals met | The number that decides |
|---|---|---|---|---|---|
| 1 | C02M01 | Insurrection | **3** | 1 / 58 hostiles | one bomb, one man: `h001` at 31.3 m from the impact. The guard tower at 50.1 m lived |
| 2 | C02M03 | Flood | **3** | 1 / 93 hostiles | the campaign's worst ratio, **1.1 %**. The `C130` stood at 97.0 m and was not touched — which the reading rule says changes no verdict |
| 3 | C02M04 | Weatherman | **0** | 9 / 9 crates + extraction | one 227 kg bomb takes a 86 × 59 m village of nine objects out to 55.7 m. The extraction goal was met **0.1 s** after the objective, over a 450.0 m leg the aircraft never flew |
| 4 | C02M05 | Bad Habit | **3** | 8 / 9 convoy + extraction | an 81 m column is longer than one Mk-82: `convoy09` at 72.0 m lived. The 844.6 m extraction leg is the campaign's ONLY navigation leg longer than the capture radius, and it took 4.9 s |
| 5 | C02M02 | Masquerade | **0** | 3 / 3 | zero releases, zero gun events. The only briefing sentence in the four NovaLogic mods that maps onto an objective keyword with no loss: *"without alerting the enemy"* → `objective no_fire` |
| 6 | C02M06 | Headhunter | **3** | 1 / 82 hostiles, druglord alive | `protect unit druglord` met at 99.8 m from the impact, and one neutral villa part destroyed at 47.4 m |

**20 of 251 declared goal objects, in 1 518 sim-seconds, with 5 bombs of 10 carried and 0 rounds of
180 fired.** The dominant term is not the airframe and not the weapon: it is that **an infantry title's
goal is a population and the engine's delivery is a point**. `mods/comanche` measured *one striker is
one aim point* against goal sets of 3 to 26; here the goal sets are 33 to 96 **men**, and the
substitution has no second aim point at all, because Delta Force sends one man and there is no wingman
to borrow.

### 8. The two greens, read honestly

Neither is a success of the simulation and both are stated as such in their own headers.

| Green | What it proves | What it does not |
|---|---|---|
| **Weatherman** exit 0 | that nine objects on a measured 86 × 59 m extent fall to one Mk-82 — a WEAPON measurement, and a direct consequence of substituting a 227 kg bomb for a satchel charge | anything about "penetrate enemy defenses": the 96 hostiles were never engaged, and goal 2 was met by the capture radius |
| **Masquerade** exit 0 | that the format can state all three of this mission's conditions, one of them exactly | anything about infiltration, stealth, night or a codebook. It is a jet flying 1.5 km without firing |

## Gaps

- **Every number above is ONE run of one geometry**, and unlike `mods/comanche` there is not even a
  hostile shot to make it interesting: nothing in these six missions can act, so a second run of the
  same file is the same run. The delivery numbers are trustworthy; there is nothing else to trust.
- **The aim point is a tasking, not data.** The campaign states an insertion point and an objective;
  which point a bomb goes on is chosen here by a stated rule (the goal's own object — the soldier
  centroid where the goal is soldiers, the crates where the goal is crates, the convoy midpoint where
  the goal is the convoy). A different rule gives different met counts.
- **The soldier lattice is a reconstruction and it is the largest one in the file.** §6 states what is
  measured (count, centroid, areal density) and what is not (arrangement, clustering, patrol routes,
  which of them are on the GPS map at all — `hud.md` §4 says part of the opposition is and part is not,
  and a mission declaration would have to carry that per unit; nothing here does).
- **The extraction point of Weatherman and Masquerade is on the insertion point**, which is a placement
  of last resort (§5). If those two coordinates are ever read out of the `.BMS` tail, both files change
  and both `waypoints` verdicts become meaningful.
- **`--elev const` was not compared against `--elev tiles`.** The DEM answers (§5) but no run was flown
  on it, so what 74–267 m of relief would do to a 150 m pass over these boxes is unmeasured. It cannot
  restore ground LOS, which is the deficit that matters.
- **Nothing was measured about the HUD.** [`hud.md`](hud.md) is a full specification of a display the
  tree cannot express at all; no part of this round touched it.
- **The 11 goal texts, the 34 radio lines and the six briefings exist only as comments.** A `.fbm`
  carries a reading rule for a machine. That is a format gap named in
  [`doc/mods.md`](../../../doc/mods.md) §2 and it is unchanged by this round.
