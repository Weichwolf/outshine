# Delta Force (NovaLogic, 1998) — Campaign One: PERU

> **Source documents.**
> 1. **The shipped game data**, read directly out of the retail CD-ROM image over HTTP range requests
>    and parsed in this run: `DFBASE.PFF` → `DFCAMPS.BIN`, `DFCAMP02.BIN`, `DFGAME.BIN`,
>    `DFDLG02.BIN`, `C02M01…C02M06.BMS`; `DF.PFF` → `C2M1…C2M6.TRN`; `DF.EXE`.
>    Nothing from the game is copied into this tree — only measurements.
> 2. **Delta Force Field Manual FM 365-7 + Manual Addendum FM 365-7A**, NovaLogic 1998, 30 PDF pages,
>    publisher-hosted. Cited as `[MAN p.N]` = printed page, `(PDF n)` = page in the PDF.
> 3. A prior research pass, `scratchpad/novalogic/delta-force.md` — **checked, not believed**; the
>    seven claims it got wrong are refuted by measurement in [`sources.md`](sources.md) §5.
>
> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has **no `test/`** — the missions are the test.

Provenance tag on every fact:

| Tag | Means |
|---|---|
| `[BMS]` | measured in this run from `C02M0n.BMS` — object type, position (16.16 fixed point), heading |
| `[CAMP]` | verbatim string from `DFCAMP02.BIN` / `DFCAMPS.BIN` (`RTXT` table) |
| `[DLG]` | verbatim string from `DFDLG02.BIN` — the in-game radio/Information-Link lines |
| `[GAME]` | verbatim string from `DFGAME.BIN` |
| `[TRN]` | key/value from the mission's `.TRN` terrain definition |
| `[MAN p.N]` | manual, printed page N |
| `[SHOT]` | publisher-hosted screenshot of the original, read as an image in this run |

## Spec

### 1. Frame

| Item | Value | Provenance |
|---|---|---|
| Game | Delta Force, NovaLogic, 1998 — first title of the series | — |
| Campaigns | **five**, in menu order **PERU · CHAD · INDONESIA · UZBEKISTAN · NOVAYA ZEMLYA** | `[MAN p.16]` (PDF 21), read as an image at 900 dpi |
| Campaign One | **PERU** | ditto — first entry of the `Campaign Selection:` list |
| Missions in PERU | **6** (`C02M01`…`C02M06`) | `[BMS]` archive index |
| Missions shipped | **40** single-player = 6 + 6 + 8 + 10 + 10, plus 22 multiplayer maps `C08M*` | `[BMS]` archive index |
| **Internal number of PERU is `C02`** | there is **no `C01`**; menu order ≠ file numbering | `[BMS]` archive index |
| Player call sign | **BRAVO TWO**, the team is **Bravo Team** | `[CAMP]` `NAMEID001`; all six briefings |
| Friendly teams | **ALPHA ONE / ALPHA TWO / CHARLIE ONE / CHARLIE TWO** | `[CAMP]` `NAMEID002`–`005` |
| Extraction asset | **Black Widow** | `[CAMP]` Briefing6, `[CAMP]` `M02SUBFAIL01`, `[DLG]` *"Black Widow is down."* |
| Chain of command | JFSOCC (Joint Force Special Operations Component Commander), J2 intelligence, SOA (Special Operations Area), SOB (Special Operations Base) | `[CAMP]` all briefings |
| Antagonist | an unnamed **druglord** in Peru with a hired mercenary army; one unique person type in the whole campaign | `[CAMP]`, `[BMS]` type 5025, exactly 1 instance |
| Mission designer | **Steve McNally** — all six | `[BMS]` header field at +0x2C |

Campaign frame text, verbatim `[CAMP]` (inserted into several briefings as a conditional block):

```
After several years of successful governmental programs to curtail Coca production, the most
powerful druglord in Peru has hired a large mercenary army to fend off Peruvian drug enforcement
officials, help guard drug shipments bound for the United States and intimidate growers farming
Coca leaves. In response to Peru's requests for assistance with this severe threat to its internal
stability, the United States has pledged to assign its most elite force to help defeat the druglord
and his army.
```

### 2. The six missions

File slot order is **not** play order. Both are given.

| Play # | Slot | Name | Working title `[BMS]` | Objective code name | Terrain `[TRN]` |
|---|---|---|---|---|---|
| 1 | `C02M01` | **Insurrection** | `C02M01 - Clear Camp` | Objective **Breeze** | `C2M1.TRN` "Jungle Terrain 1" (Rod Parong) |
| 2 | `C02M03` | **Flood** | `C02M03 - Capture Airfield` | Objective **Rain** | `C2M3.TRN` "Jungle" (Rod Parong) |
| 3 | `C02M04` | **Weatherman** | `C02M04 - Blow up storehouse` | Objective **Gale** | `C2M4.TRN` "Sniper Super Fly" |
| 4 | `C02M05` | **Bad Habit** | `C02M05 - Blow Up Convoy` | Objective **Hail** | `C2M5.TRN` "Jungle Terrain 1" (Rod Parong) |
| 5 | `C02M02` | **Masquerade** | `C02M02 - Steal Radio Book` | Objective **Calm** | `C2M2.TRN` "Death & More Death" (Jason Tull) |
| 6 | `C02M06` | **Headhunter** | `C02M06 - Capture Druglord` | Objective **Storm** | `C2M6.TRN` "Death & More Death" (Jason Tull) |

Six objective code names are weather words; the campaign uses six shared checkpoint names
(`CP ALPHA` … `CP FOXTROT`) plus `IP` and `EP` `[CAMP]` group *Waypoint Names*.

### 3. Order is a graph, not a list

The campaign screen offers a **campaign map with mission nodes**, and several nodes can be open at
once. The unlock edges are stated in the debrief texts `[CAMP]`:

```
Insurrection ──▶ Flood        Win1: "...suggest the location of an airfield being used by the
             │                       mercenaries as a transshipment point..."
             └──▶ Weatherman   Win1: "...point to the location of one of the druglord's supply caches..."
Flood        ──▶ Masquerade   Win3: "...will be dispatched to acquire a codebook..."
Weatherman   ──▶ Bad Habit    Win4: "...stand by for orders to move against these supply convoys."
Bad Habit    ──▶ Masquerade   Win5: "...stand prepared to acquire one of these codebooks."
Masquerade   ──▶ Headhunter   Win2: "...the codebook contained the location of the druglord's villa."
```

**Masquerade has two predecessors.** After Insurrection, Flood *and* Weatherman are open at the same
time. The play order in §2 is one legal linearisation, not the only one.

**Briefings are conditional on progress.** The briefing texts contain inserts
`{CV:n}Text` / `{CV:!n}Text` = *"if mission n is / is not already complete"*, and `{ADD}MissionNx` =
*"append block Nx"*, and `{MnLk}word` = *"link this word to map key k of mission n"* `[CAMP]`.
Measured occurrences: Briefing1 `{CV:!7}`, Briefing3 `{CV:4}/{CV:!4}`, Briefing4 `{CV:!7} {CV:1}/{CV:!1}`,
Briefing5 `{CV:!1}/{CV:1} {CV:!3}/{CV:3}`. Flag **7** has no mission — its meaning is not resolved.

### 4. Objectives and success conditions

`Statement` = the one-line *Overview:* on the campaign screen `[CAMP]` `DFCAMPS.BIN`.
`Goals` = the G-key list `[CAMP]` `DFCAMP02.BIN` group *Goals* — **this is the machine-readable
success condition**, eleven strings, mapped to their keys by table order and cross-checked by content
(each text names its own mission's objective).

| # | Mission | Statement `[CAMP]` | Goals `[CAMP]` | Fail goals `[CAMP]` |
|---|---|---|---|---|
| 1 | Insurrection | "Eliminate the mercenary detachment located at Objective Breeze." | `M01SUBGOAL01` Eliminate all enemy soldiers at Objective Breeze. | — |
| 2 | Flood | "Capture all vehicles, equipment, and contraband at the enemy airfield designated Objective Rain." | `M03SUBGOAL01` Eliminate all enemy soldiers at Objective Rain. | — |
| 3 | Weatherman | "Penetrate enemy defenses, then locate and destroy a cache of drugs and munitions, designated Objective Gale." | `M04SUBGOAL01` Destroy the enemy crates at Objective Gale. · `M04SUBGOAL02` Reach the extraction point. | — |
| 4 | Bad Habit | "Intercept and destroy the druglord's convoy, designated Objective Hail." | `M05SUBGOAL01` Destroy the enemy convoy. · `M05SUBGOAL02` Reach the extraction point. | — |
| 5 | Masquerade | "Infiltrate the mercenary base-camp, designated Objective Calm, and steal a copy of the enemy's encryption codebook." | `M02SUBGOAL01` Retrieve the code book from Objective Calm. · `M02SUBGOAL02` Reach the extraction point. | `M02SUBFAIL01` Black Widow is eliminated. |
| 6 | Headhunter | "Eliminate the remaining mercenaries protecting the druglord's villa, designated Objective Storm, and capture the druglord." | `M06SUBGOAL01` Eliminate all enemy soldiers at Objective Storm. | `M06SUBFAIL01` The enemy druglord is killed. |

Three consequences the prose hides:

1. **Extraction is a goal in exactly three missions** — Weatherman, Bad Habit, Masquerade. Insurrection,
   Flood and Headhunter have **no** `Reach the extraction point` goal, and correspondingly carry **no**
   extraction aircraft object `[BMS]` (§6).
2. **Headhunter's "capture the druglord" is modelled as a fail condition**, not a goal: the goal is
   the firefight, and shooting the target loses the mission.
3. **Flood's statement says "capture all vehicles, equipment and contraband"; its goal says "eliminate
   all enemy soldiers."** The C-130, the tents and the oil tank are scenery. This is a contradiction
   inside the shipped data and is kept, not smoothed.

### 5. Radio lines — what the player is actually told in-mission

Verbatim `[DLG]` `DFDLG02.BIN`, 34 strings, the entire campaign's Information-Link text.
The direction word in each line is confirmed against the measured object layout (§7).

| Mission | Lines |
|---|---|
| Insurrection | `Proceed southwest and neutralize all hostiles.` |
| Flood | `Proceed south and secure the airstrip.` · `Notebook was destroyed.` |
| Weatherman | `Proceed northeast to the village.` · `Locate the cache, and destroy it.` · `Use satchel charges to destroy all crates.` · `Proceed to extraction point.` |
| Bad Habit | `Proceed north to road, intercept and destroy convoy.` · `Proceed to extraction point.` |
| Masquerade | `Proceed east and obtain enemy code book.` · `Do so without alerting the enemy to your presence.` · `Proceed to extraction point.` |
| Headhunter | `Proceed west and eliminate all hostiles in the villa.` · `Do not shoot the target, he must be taken alive.` |
| any | `Alpha 1 is down.` · `Alpha 2 is down.` · `Bravo 1 is down.` · `Charlie 1 is down.` · `Charlie 2 is down.` · `Black Widow is down.` (each stored several times — one per voice take) |

`Use satchel charges to destroy all crates.` is a **loadout constraint**: Weatherman is unwinnable
without the secondary-weapon slot set to satchel charges `[MAN p.9]` (PDF 15).

### 6. Measured layout per mission `[BMS]`

Origin is the mission's own world zero; **+x = east, +y = north** (proven six times over in §7).
Insertion point = mean of the `start, player` markers (type 6001, 8–9 per mission for co-op).

| Mission | Objects | E–W span | N–S span | Insertion point | Enemy centroid | from IP |
|---|---|---|---|---|---|---|
| Insurrection | 492 | **1052 m** | **771 m** | (−187.6, −108.0) | (−380.2, −301.4) | 273 m, 225° |
| Masquerade | 725 | **1537 m** | **1186 m** | (−645.3, −545.0) | (−283.0, −483.8) | 367 m, 080° |
| Flood | 948 | **1151 m** | **1236 m** | (559.0, 319.1) | (534.2, −208.0) | 528 m, 183° |
| Weatherman | 819 | **1364 m** | **1397 m** | (261.4, 122.5) | (531.2, 271.0) | 308 m, 061° |
| Bad Habit | 582 | **1612 m** | **2037 m** | (295.7, −281.9) | (62.3, 208.9) | 543 m, 335° |
| Headhunter | 536 | **1458 m** | **980 m** | (749.9, −381.9) | (350.5, −307.7) | 406 m, 281° |

**The largest mission of the campaign spans 1.6 × 2.0 km.** No Peru mission exceeds 2.1 km on an edge.

Named contents, all `[BMS]` unless marked, item names from `[GAME]` `STR_ITM*`:

| Mission | Contents |
|---|---|
| Insurrection | 58 hostiles · **4 Squad Member (type 5002)** — Alpha pair at 129/131 m bearing 142–146°, Charlie pair at 232/236 m bearing 287° · one **`tower, guard`** (3014) at (−370.6, −373.2) · two building groups (3 and 4 parts) · 2 vehicles (type 2009) |
| Masquerade | 33 hostiles · **zero friendly soldiers of any type** — matches the briefing *"no other team members have been assigned to this operation"* · tent group `tent, netting` + `tent, long` + 2× `tent, small` at (−139.1, −522.7) = Objective Calm · two abandoned villages: 7 parts at (−765.9, −227.8) and 4 parts at (−715.5, −832.9) · 1 aircraft (2021) |
| Flood | 93 hostiles · 4 Squad Member, two pairs ~890 m and ~948 m south of the IP · **one `C130` (2004)** at (462.9, −311.5), heading 090 · airfield group: 2 hangars (3021), `oil tank`, `tent, netting`, `tower, guard`, centre (463.9, −342.7), extent 114 × 70 m · village group 6 parts at (681.2, −43.5) |
| Weatherman | 96 hostiles · 2 Squad Member 605/613 m north · **village of 9 building objects** (8× type 3016 + 1× 3047) at (583.1, 437.1), extent 86 × 59 m = Objective Gale · abandoned farm (1× 3016 + 1× 3017) at (252.3, 228.0), 106 m from the IP at bearing 355° · 1 aircraft (2021) |
| Bad Habit | 73 hostiles · **4 friendlies of a second type (5048)** at 409–445 m, bearing 300–323° from the village — the briefing's Alpha and Charlie · **convoy: 9 vehicles, 6× type 2010 + 3× type 2015**, column (252.8, 491.5) → (267.4, 410.1), **81 m long**, all heading 175 · convoy route = 3 parallel AI chains of 12 nodes, **1891 / 1853 / 1924 m** long · 73 `tree, palm1` + 120 `tree, palm2` · 1 aircraft (2021) |
| Headhunter | 82 hostiles · 4 Squad Member at 209 m west and 165 m north-west of the villa · **villa = 32 parts (types 3048–3062)**, centre (263.1, −233.7), x 201.8…315.2, y −293.0…−178.9, extent **113.5 × 114.1 m** · **the druglord: type 5025, exactly one instance in the whole campaign**, at (260.9, −234.3), inside the villa · 2 vehicles (2009, 2012) — one is the limousine visible in `[MAN p.14]` |

**Type 2021 is the extraction aircraft.** It appears exactly once in exactly the three missions that
carry a `Reach the extraction point` goal, in no other Peru mission, and always at the edge of the
mission area (Bad Habit: (553.9, 1242.7), the northernmost object of the whole mission). That is an
inference from a perfect 3-of-3 / 0-of-3 split, not a name in the data.

### 7. Scale and axes — measured, not assumed

Positions are `int32 / 65536`. **The unit is the metre.** Four independent checks against distances
the briefings state in metres:

| Briefing claim `[CAMP]` | Measured `[BMS]` | Error |
|---|---|---|
| Insurrection: Alpha "approximately **130 meters southeast** of your position" | 128.8 m / 130.6 m at 146° / 142° | **1 m** |
| Insurrection: Charlie "about **240 metres west**" | 232.4 m / 235.7 m at 287° | 5–8 m |
| Masquerade: "Abandoned village **690m northwest** of enemy base-camp" | 692.7 m at 295° | **3 m** |
| Masquerade: "Abandoned village **720m southwest**" | 654.6 m at 242° | 65 m |
| Bad Habit: Alpha and Charlie "inserted **500m northwest** of the village" | 409–445 m at 300–323° | 55–91 m |
| Headhunter: insertion "**450m east** of Objective Storm" | 509 m to villa centre, 430 m to its east face | −20…+59 m |

**Axes: +x = east, +y = north.** Six independent confirmations — the direction word of every
mission's opening radio line `[DLG]` against the measured IP→objective bearing `[BMS]`:

| Mission | Radio says | Measured bearing |
|---|---|---|
| Insurrection | southwest | 225° |
| Masquerade | east | 080° |
| Flood | south | 183° |
| Weatherman | northeast | 061° |
| Bad Habit | north (to the road) | 335° |
| Headhunter | west | 281° |

### 8. Time of day — measured from the terrain definitions `[TRN]`

Two shipped Peru missions run the **night preset**, and this was not previously noticed. The preset is
`sky_palette grengrad` + `filter 80,160,30` + `saturation 0` + `gamma 85–95`.

| Mission | `sky_palette` | `filter` | `gamma` | `saturation` | `sun_slope` | Reading |
|---|---|---|---|---|---|---|
| Insurrection | `skygrad4` | 120,130,128 | 140 | 115 | 90 | **day, high sun** |
| Masquerade | `grengrad` | **80,160,30** | 85 | **0** | 20 | **night** |
| Flood | `skygrad1` | 120,130,128 | 140 | 115 | 70 | day |
| Weatherman | `skygrad2` | 128,128,128 | 128 | 99 | 70 | day, neutral filter |
| Bad Habit | `grengrad` | **80,160,30** | 95 | **0** | 90 | **night** |
| Headhunter | `snsgrad7` | 138,118,140 | 140 | 128 | 20 | **dusk** — confirmed by the orange sky in `[MAN p.14]` |

Cross-check that closes it: separate night variants `C2M1N/C2M3N/C2M4N/C2M6N.TRN` exist for the four
**day** missions and **do not exist for Masquerade and Bad Habit** — because those two already are the
night preset. Masquerade being night also explains its briefing (*"the need for absolute stealth"*,
no squadmates) and its radio line (*"without alerting the enemy"*).

Only **Flood** has water: `water_height 28` `[TRN]`; every other Peru mission has `water_height 0`.

### 9. Opposition inventory `[BMS]`

| Class | Types present in PERU | Count |
|---|---|---|
| Hostile soldiers | 5005, 5006, 5007, 5041 | 58 / 33 / 93 / 96 / 73 / 82 per mission (§6) |
| Unique person | **5025** — the druglord | 1 (Headhunter) |
| Friendly soldiers | **5002** `Squad Member` `[GAME]`, and **5048** in Bad Habit | 4 / 0 / 4 / 2 / 4 (5048) / 4 |
| Vehicles | 2004 `C130` `[GAME]`, 2009, 2010, 2012, 2015, 2021 | — |
| Buildings | 3008 `oil tank`, 3009 `tent, netting`, 3010 `tent, long`, 3011 `tent, small`, 3014 `tower, guard`, 3016–3021, 3044–3047, 3048–3062 | — |

**Absent from the whole campaign** — an exhaustive type count, not a walkthrough claim: no
`BMP-2` (2001), no `BRDM` (2002), no `havoc` (2005), no `humvee` (2006), no `LCAC` (2007), no
`m1a2` (2008), and **no bridge objects at all** (`bridge 1a` 3001 / `bridge 1b` 3002 appear zero times).
Any bridge or ford the player crosses is painted terrain, not an object.

Per-soldier attributes `[BMS]`: every person record carries two scalars in fields +0x20 and +0x24 and a
constant 100 in +0x28. Measured ranges:

| Type | field +0x20 | field +0x24 |
|---|---|---|
| 5002 Squad Member (friendly) | 125 – 300 | 100 – 150 |
| 5048 (friendly, Bad Habit) | 75 | 100 |
| 5005 / 5006 / 5007 / 5041 (hostile) | 10 – 30 | 40 – 80 |
| 5025 druglord | 20 | 50 |

Friendlies carry values **5–10× the hostiles'**. The semantics of the two fields is not proven; the
separation is.

Briefing strengths `[CAMP]` differ from the record counts and both are kept:
Insurrection 10–20 (58 records) · Masquerade 5–8 at the objective plus 20+ patrolling (33) ·
Flood 10–20 plus 8–10 in the village (93) · Weatherman ~30 (96) · Bad Habit 20–30, *"the majority of
those patrols consist of two-man teams"* (73) · Headhunter 20–30 (82). The record count is every
soldier on the map; the briefing figure is the estimate for the objective.

### 10. Reading rule per mission

One sentence each, in the sense of `.fbm` header reading rules — how a run's outcome must be judged.

| # | Mission | Reading rule |
|---|---|---|
| 1 | **Insurrection** | Success is the hostile-alive count reaching zero and nothing else — there is no extraction goal, so a run that clears the camp and then dies walking home still passed. |
| 2 | **Flood** | Judge on hostiles only: the statement promises captured vehicles and contraband, but the goal table asks for dead soldiers, so destroying or sparing the C-130 changes no verdict. |
| 3 | **Weatherman** | Two goals in order — crates destroyed, then extraction reached — and a run that carries no satchel charges is a loadout defect, not an AI failure, because the crates cannot be destroyed without them. |
| 4 | **Bad Habit** | The convoy moves along a 1.9 km route, so a run that arrives late fails on geometry rather than on marksmanship; all nine vehicles and then the extraction point. |
| 5 | **Masquerade** | Three ways to fail and only one to pass: no codebook, no extraction, or Black Widow destroyed — and the run is alone, so any friendly unit appearing in it is a mod defect. |
| 6 | **Headhunter** | A run can win every firefight and still lose: the druglord must be alive at the end, so the kill counter is not the verdict. |

## State

**Nothing built.** `mods/delta-force/` carries this `doc/` only. There is no `src/`, no `mod.json`, no
`.fbm`, and no loader that would give them meaning ([`doc/mods.md`](../../../doc/mods.md) `## Gaps`).

## Gaps

- **The player's waypoint chain is not resolved.** The names exist (`IP`, `EP`, `CP ALPHA`…`CP FOXTROT`,
  `OBJ BREEZE/CALM/RAIN/GALE/HAIL/STORM` `[CAMP]`) and the HUD shows them (`WP3: CP BRAVO (117m)`
  `[SHOT]`), but which `.BMS` marker type carries them is unidentified. Marker types 6007, 6008, 6011,
  6013, 6029–6038 were inspected; type 6005 is `waypoint` `[GAME]` and holds **AI patrol chains**
  (Insurrection 136 nodes, Weatherman 267). Without this, a mission's route cannot be declared.
- **Item names above the shipped table are unknown.** `DFGAME.BIN` names 79 low IDs; PERU uses
  **84 distinct type IDs and the table covers 16 of them** (measured). The full definitions are in
  `ITEMS.DEF`, which is encrypted (byte entropy 7.994 / 8, all 256 values present, no zlib/LZ
  signature). So the convoy's two vehicle types, the villa's 32 part types and all four hostile
  soldier types have **classes but no model names**.
- **`{CV:!7}` has no mission 7.** The conditional-briefing flag space is larger than the mission set;
  what flag 7 is remains unknown.
- **Enemy counts are records, not spawns.** Whether all 58/93/96 records spawn in single player, or a
  subset by difficulty, is not established. `Enemy AI: Hard` and co-op *"increase in the number of
  enemies encountered"* `[MAN p.6, Addendum p.6]` say the number is variable; the mechanism is not in
  the fields inspected.
- **A man on foot is the one thing the engine cannot do at all.** From
  [`doc/mods.md`](../../../doc/mods.md) §2: *"Delta Force: a man on foot — ditto, plus a segment tree
  with foot contacts that come and go"*. Concretely missing, and each item blocks a mission above:
  - **segment tree with intermittent foot contacts** — stand / crouch / prone are three postures with
    different contact sets and different eye heights `[MAN p.10]`; nothing in
    [`body-format.md`](../../../doc/body-format.md) is implemented.
  - **ground line of sight** — every mission is decided by whether a position 200–500 m away
    *overlooks* the objective; the sim has no terrain LOS query at man height.
  - **cover** — hostiles are placed behind barrels, in windows and in a guard tower; without an
    occlusion model those placements carry no meaning.
  - **carried mission items** — `Carrying: Code Book` `[GAME]` is a state the unit model has no slot for.
  - **a briefing** — `.fbm` carries a reading rule for a machine, not the six briefing texts a player
    would need.
