# Armored Fist (1994, NovaLogic) — Campaign One: "Overwatch"

> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod carries `doc/` and `src/` and has **no
> `test/`**: the missions are the test. Only `doc/` exists today.
> Every claim is sourced in [`sources.md`](sources.md). Terrain and real-world anchoring live in
> [`terrain.md`](terrain.md); displays in [`hud.md`](hud.md).

Provenance tag on every fact:

| Tag | Means |
|---|---|
| `[FSG]` | **measured in this run** from `FISTDATA/<name>.FSG` on the retail CD image — chunk-parsed, numbers reproducible (§7) |
| `[FSW]` / `[FSE]` | **verbatim** Western / Eastern briefing text from `FISTDATA/<name>.FSW` / `.FSE` on the CD |
| `[GUIDE p.N]` | Ed Dille, *Armored Fist: The Official Strategy Guide*, Prima 1994, printed page N |
| `[MAN p.N]` | *Armored Fist Benutzerhandbuch* (German Softgold re-release), printed page N — translated here, German quoted where load-bearing |
| `[MENU]` | read off the campaign-selection screenshot in the manual, p. 42 |
| `[DERIV]` | computed here; formula given |

---

## Spec

### 1. Frame

| Item | Value | Provenance |
|---|---|---|
| Game | **Armored Fist**, NovaLogic, 1994 — first title of the series | CD `ARMOREDFIST`, files dated 1994‑10‑07 |
| Engine | NovaLogic **VoxelSpace** | `[MAN p.10]` |
| Player role | **company commander**, 1–4 platoons, 1–4 vehicles each → **up to 16 vehicles** | `[MAN p.10]`, corroborated `[FSG]` (§7.3: exactly 8 platoon slots, 4 per side) |
| Campaigns shipped | **7** — one training + six combat | `[MENU]`, `[FSG]` file census |
| Missions shipped | **47** = TRAIN 4 + INDIA 7 + SAUDI 7 + AZER 7 + SYRIA 7 + CYPRUS 7 + UKRAINE 8 | `[FSG]` file census (51 `.FSG` total, minus `DEMO0–3`) |
| **First combat campaign** | **OVERWATCH** — files `INDIA1…INDIA7` | `[MENU]` (list order), `[GUIDE p.53]` (chapter 4), `[FSW]` (INDIA1: *"has been given it's first mission orders"*) |
| Playable side | **WESTERN** or **EASTERN**, chosen per campaign; every mission ships both briefings | `[MAN p.43]`, `[FSG]` (`.FSW` + `.FSE` present for all 43 combat missions) |
| Player platoon (West) | **1st Plt / Echo Co / 14th Armored Cavalry** — callsign **ECHO‑1** | `[FSW]` |
| Enemy platoon naming | **BANDIT 1…4** | `[GUIDE]` orders of battle |
| Western vehicles | **M1A2 Abrams** MBT, **M3 Bradley** CFV | `[MAN p.10]`, `[GUIDE p.4–8]` |
| Eastern vehicles | **T‑80** MBT, **BMP‑2** IFV | `[MAN p.10]`, `[GUIDE p.5–9]` |
| Air | **AH‑64 Apache** (West) / **Mi‑24 Hind** (East), as callable support and as threat | `[GUIDE p.10–12]` |

**The player commands a platoon of tracked vehicles, not a single machine.** That is the structural
difference from the aircraft mods and the reason §5 exists.

#### 1.1 The campaign list is a list, not a ladder

`[MENU]` order, verbatim from the *SELECT A CAMPAIGN* dialogue:

```
● TRAINING
● OVERWATCH
  CROSSED SWORDS
  FIRE HAMMER
  CERTAIN FURY
  AEGIS
  BURNING FROST
                        HARDWARE:  [ ] WESTERN   [ ] EASTERN
```

A bullet marks a campaign already won `[MAN p.42]`. Any listed campaign can be selected at any time;
**within** a campaign the campaign map gates missions by colour `[MAN p.49]`:

| Colour | Status |
|---|---|
| flashing | currently selected |
| green | not yet won, selectable |
| blue | won, selectable |
| **red** | **not selectable** — a prerequisite mission has not been won |

Progress in a campaign is auto-saved *as of the last mission won*; single missions run from the
**BATTLES** menu save nothing `[GUIDE p.19]`.

**The guide's chapter order is not the game's list order** — and the discrepancy is kept, not
smoothed:

| Position | `[MENU]` | `[GUIDE]` chapter |
|---|---|---|
| 1 | Overwatch | 4 Overwatch |
| 2 | Crossed Swords | 5 Crossed Swords |
| 3 | **Fire Hammer** | 6 **Aegis** |
| 4 | Certain Fury | 7 Certain Fury |
| 5 | **Aegis** | 8 **Fire Hammer** |
| 6 | Burning Frost | 9 Burning Frost |

Both agree Overwatch is first and Certain Fury fourth — the latter independently confirmed by the
guide's own narrative, *"despite your successes in the previous three campaigns"* `[GUIDE p.115]`.
Positions 3 and 5 are swapped between the two sources and **this run cannot decide which is the
game's intent**; the menu is the stronger source because it is the game itself.

#### 1.2 Campaign ↔ file-set mapping

Established by matching the guide's mission-name lists against the first line of each `.FSW`
briefing — 7/7, 7/7, 7/7, 7/7, 7/7, 8/8 exact except two names (§4).

| Campaign | Files | Missions | Theatre |
|---|---|---|---|
| TRAINING | `TRAIN1–4` | 4 | — |
| **OVERWATCH** | **`INDIA1–7`** | **7** | Pakistan (Sindh) vs. revolutionary India |
| CROSSED SWORDS | `SAUDI1–7` | 7 | Iran has occupied Iraq; Saudi frontier |
| FIRE HAMMER | `AZER1–7` | 7 | Azerbaijan / Caucasus |
| CERTAIN FURY | `SYRIA1–7` | 7 | Syrian invasion of Turkey |
| AEGIS | `CYPRUS1–7` | 7 | Cyprus |
| BURNING FROST | `UKRAINE1–8` | 8 | Ukraine, winter |

---

### 2. Overwatch — the situation

`[FSW]` `INDIA1.FSW`, verbatim:

```
THE PAKISTANI GOVERNMENT, UNDER THE BANNER OF A MULTILATERAL FORCE
(MLF), HAS ENTERED INTO HOSTILITIES WITH THE REVOLUTIONARY PARTY
THAT HAS SEIZED ILLEGAL CONTROL OF INDIA. THE PAKISTANI MISSION IS
TO RESTORE THE LEGAL GOVERNMENT TO POWER.

THE 14TH HAS BEEN MOBILIZED AND AIRLIFTED TO RAJASTAN AS AN
ADVISORY GROUP TO THE PAKISTANI ARMY.
```

Player = US 14th Armored Cavalry, advisory group to Pakistan, fighting Indian revolutionary forces
equipped with T‑80/BMP‑2/Mi‑24.

---

### 3. The seven missions

`[FSW]` supplies name, task and stated course heading. `[FSG]` supplies the mission time limit,
terrain files and platoon routes — **all measured in this run**. `[GUIDE]` supplies orders of battle.

| # | File | Name `[FSW]` | Stated heading `[FSW]` | Task `[FSW]` | Time limit `[FSG]` | Terrain `[FSG]` |
|---|---|---|---|---|---|---|
| 1 | `INDIA1` | **SLAUGHTERZONE!** | 036° NNE | find and neutralise the Indian **forward base**, incl. all patrolling MBT/IFV and **all artillery emplacements** | **15 min** | C06/D06 · 506 · 5.SKY |
| 2 | `INDIA2` | **NIGHT FORGER.** | — (running fight) | break free of enemy fields of fire; **neutralise all fielded Indian MBTs and IFVs** | **5 min** | C32/D32 · 232 · 2.SKY |
| 3 | `INDIA3` | **RUBICON.** | 220° SSW | join **ECHO‑2** already in contact; destroy enemy **field command encampment incl. emplaced artillery** | **15 min** | C03/D03 · 503 · 5.SKY |
| 4 | `INDIA4` | **THUNDERCLAP.** | 336° NNW | destroy the Indian rebel **field-ready logistics depot** | **15 min** | C06/D06 · 506 · 5.SKY |
| 5 | `INDIA5` | **NIGHT'S QUEST.** | 238° SSW | sanction enemy **field command bunkers** and the enemy **command troop** dug in on the encampment periphery | **15 min** | C07/D07 · 707 · **7.SKY** |
| 6 | `INDIA6` | **WAR HAMMER.** | 224° SSW (ECHO‑2 211°, ECHO‑3 258°) | neutralise the enemy **in-field refuel depot** | **15 min** | C32/D32 · 532 · 5.SKY |
| 7 | `INDIA7` | **CORROSION.** | 136° SSE (ECHO‑2 186°) | sanction all **communications links**, then **all elements within the enemy compound** | **30 min** | C32/D32 · 832 · 8.SKY |

Time limit = `SHDR` byte 5 of the `.FSG`, in minutes `[FSG]`. Cross-checked against the guide twice:
Thunderclap *"11 objectives in 15 minutes"* `[GUIDE p.63]` and Night's Quest *"13 objectives in
15 minutes"* `[GUIDE p.66]`. Both match the measured 15.

**Only `INDIA5` uses sky 7**, the one sky asset absent from the editor's map table and one fifth the
size of the others (14 985 B vs. 72–111 kB) — i.e. **the night sky** `[FSG]`. See §4 for why that
contradicts the guide about mission 2.

#### 3.1 Orders of battle `[GUIDE]`

Blue = player. Red = AI. Column alignment in the OCR'd tables is ambiguous where a column has fewer
rows than its neighbour; those cells are marked `?`.

| # | Blue | Bandit 1 | Bandit 2 | Bandit 3 | Bandit 4 | `[GUIDE p.]` |
|---|---|---|---|---|---|---|
| 1 | Echo 1: 1 M1, 1 M3 | 2 T‑80, 2 BMP | 2 T‑80, 2 BMP | 2 T‑80 | — | 54 |
| 2 | Echo 1: 1 M1, 1 M3 | 2 T‑80, 2 BMP | 3 T‑80, 1 BMP | 2 T‑80, 2 BMP | — | 58 |
| 3 | Echo 1: 1 M1, 1 M3 | 2 T‑80, 2 BMP | 2 T‑80 | — | — | 61 |
| 4 | Echo 1: 2 M1, 2 M3 | 2 T‑80, 2 BMP | 3 T‑80, 1 BMP | 1 BMP, 1 T‑80 | — | 63 |
| 5 | Echo 1: 3 M1, 1 M3 · Echo 2: 1 M1, 1 M3 | 2 T‑80, 2 BMP | 1 T‑80, 3 BMP | 2 T‑80, 2 BMP | 2 T‑80, 2 BMP | 65 |
| 6 | Echo 1: 2 M1, 1 M3 · Echo 2: 2 M1, ? · Echo 3: 1 M1, 1 M3 ? | 3 T‑80, 1 BMP | 4 T‑80, 1 BMP | 3 T‑80, 2 BMP | 1 T‑80 | 68 |
| 7 | Echo 1: 3 M1, 1 M3 · Echo 2: 1 M1, 1 M3 | 2 T‑80, 2 BMP | 3 T‑80, 1 BMP ? | 3 T‑80 | 1 T‑80 | 71 |

Mission 1 is stated as **outnumbered 5-to-1** `[GUIDE p.55]`; mission 6 as **enveloped from the
outset**, Bandits 1–3 converging from left, right and front `[GUIDE p.68]`.

Additional order-of-battle elements named in the debriefings but absent from the tables:
**reinforced bunkers** along ridgelines (2), **dug-in** T‑80/BMP (4), **satellite dishes** in the
objective area (5), **fuel and propane tanks** (6), **Mi‑24 Hinds** (3: *"a minimum of two"*; 7:
*"heavy Hind activity"*), **minefields** (4, 5, 6, 7) `[GUIDE p.58–71]`.

#### 3.2 Success condition — the same rule for every mission

> *"Missionsziele müssen zerstört werden, um die Mission erfolgreich beenden zu können."*
> — mission goals must be destroyed to finish the mission successfully. `[MAN p.55]`

| Element | Rule | Provenance |
|---|---|---|
| Win | every object flagged as a **mission goal** destroyed | `[MAN p.55]` |
| Live counter | **`GOALS REMAINING: n`** printed in the gunsight; `n = 0` ⇒ won | `[MAN p.78–82]` (screenshots) |
| Loss | time limit expires, or the company is destroyed | `[FSG]` limit; loss condition not stated in either source — see `## Gaps` |
| Goal marking | flashing cross over the object in edit mode; flashing dots on the tactical map and threat display in play | `[MAN p.55]` |
| Goals are not only vehicles | *"jedes Fahrzeug, jede Artilleriebasis oder jedes Ziel"* — vehicles, artillery bases, and placed targets can all be flagged | `[MAN p.55]` |

Goal counts are therefore **per mission data, not per mission type**. Two are documented:
**11** (Thunderclap) and **13** (Night's Quest) `[GUIDE p.63, 66]`. The remaining five were not
recovered — the `DCBS` object table that carries the flag was not decoded (§7.4).

---

### 4. Two contradictions, kept

**(a) Mission 2's name.** The briefing header is **`NIGHT FORGER.`** `[FSW]`. The guide's contents,
chapter heading and index all say **Night Forger** `[GUIDE p.57]`. But the campaign-map screenshot
printed as Figure 4‑1 labels that mission **`Reforger`** `[GUIDE p.53]` — read directly off a
600 dpi render of the page in this run, not from OCR. Both readings are legible and both are from
the same book. No resolution offered.

**(b) Mission 2 is or is not at night.** The guide: *"This is your first night mission, so you may
experience a slight disorientation at first in working with the thermal sights"* `[GUIDE p.58]`. The
data: `INDIA2` loads `2.SKY` with palette `232` — the same daylight sky/palette pair the mission
editor lists as **"WINDY MEADOWS"** `[FSG]`, while the one unmistakably nocturnal asset (`7.SKY`)
is loaded only by `INDIA5` `[FSG]`. **The measurement contradicts the prose.** Most likely the guide
was written against a pre-release build; that is a guess and is not asserted.

Two further name divergences exist outside Overwatch and are recorded because they bear on how far
the guide can be trusted: `SYRIA3.FSW` reads **`THE GATHERING.`** where the guide says *Seconds*,
and `AZER1.FSW` reads **`BLOODFEUD!`** where the guide says *Iron Will* `[FSW]`, `[GUIDE p.121,
137]`.

---

### 5. Platoon command — how the original does it

This is the part the engine does not have, so it is recorded exhaustively.

#### 5.1 Two seats, one company

| Seat | What it is | Reach |
|---|---|---|
| **In a vehicle** | first-person M1A2 / M3 / T‑80 / BMP‑2 console ([`hud.md`](hud.md)) | drive, traverse, load, fire, smoke, call air/artillery — **one vehicle at a time** |
| **CCV** — Command & Control Vehicle | overhead strategic map + company roster | orders to all platoons; **cannot be driven** and stays behind friendly lines |

Switching is instant: click the upper-left corner of the console or press `Esc` to reach the CCV;
`Space` from the CCV drops you into the selected vehicle `[GUIDE p.18, 26]`. The vehicle you are not
in is run by its platoon AI.

#### 5.2 The CCV is not omniscient — and this matters

> *"being in the CCV does not mean you're omniscient. What you see on the map represents \[a picture
> of] the battlefield based only upon available reconnaissance … If your units make no visual
> contact with the enemy, no new information is provided … Dated enemy positions disappear from the
> map, but this doesn't mean they've been destroyed. They merely have returned to unlocated status."*
> `[GUIDE p.18–19]`

A 1994 tank game already enforces the boundary this tree spends its structure defending: **the
command layer sees only what the units perceived, and stale contacts decay rather than persist.**
Air units are explicitly recommended as reconnaissance platforms *because of their greater visual
horizon* `[GUIDE p.19]`.

#### 5.3 The four standing orders per platoon

Set on the **Company Status** screen, per platoon, changeable at any time during a mission
`[GUIDE p.20–25]`, `[MAN p.68–71]`. **Measured in the data as four `uint16` per platoon slot** —
see §7.3.

| Parameter | Values | Effect |
|---|---|---|
| **CMDR** (commander) | **No Action · Rookie · Standard · Experienced** | acquisition speed, aggression, willingness to break formation. *No Action* never acts — a way to hold a platoon in reserve. Experienced platoons change formation on the fly |
| **ADV** (advance) | **Road March · Advance to Contact · Opportunity · Patrol** | see below |
| **FORM** (formation) | **Diamond · Line · Column · Wedge · Refuse Left · Refuse Right** (6) | see below |
| **SPEED** | **Slow … Maximum** (4 steps) | faster = harder to hit, but more likely to blunder into a field of fire and less likely to shoot first |

**ADV semantics** `[GUIDE p.21–23]`:

| Order | Behaviour | Interaction with CMDR |
|---|---|---|
| Road March | follow waypoints, hold formation, do not deviate | Rookie returns fire only under heavy threat; Standard always returns fire; **Experienced proactively engages targets of opportunity and calls fire support in transit** |
| Advance to Contact | follow waypoints but **break formation to pursue** contacts, then rejoin at the nearest waypoint | Rookie breaks off under heavy resistance; Standard over-pursues and tries to handle it alone; Experienced pursues but calls combined arms |
| Opportunity | **stay put**, take shots of opportunity — the dug-in mode | Experienced + dug-in + Opportunity is described as devastating |
| Patrol | loop the waypoint path indefinitely, almost never deviate | otherwise as Road March |

**Formation semantics** `[GUIDE p.23–25]`:

| Formation | Use |
|---|---|
| Diamond | all-round; a rear-centre M3/BMP gives the platoon air cover |
| Line (abreast) | maximum firepower forward — only when the threat axis is known |
| Column | movement through restricted ground, minefields, passes; **highest fratricide risk** |
| Wedge | fast flank defence with forward fire; weak to the rear |
| Refuse Left / Right | wedge variant biased to the forward left / right quadrant |

Standing orders are a *tuning surface the guide actively uses*: mission 5 instructs setting **both**
platoons to `Rookie / Opportunity / Wedge / Medium`, mission 6 sets Echo 3 to
`Experienced / Opportunity / Wedge / Fast` and the others to `Rookie / Opportunity / Wedge / Fast`
`[GUIDE p.66, 69]`.

#### 5.4 Waypoints

Plotted from the CCV in edit mode, per platoon, **up to 32 per platoon** `[FSG]` (§7.3).
The vehicle's own tactical map shows a **red triangle** to steer to the next waypoint, replaced by a
**yellow** one pointing at the next mission goal when no waypoints are set `[MAN p.31]`. The manual
is explicit that the triangle is not a route planner: *"Die Ausrichtung der Dreiecke mit dem 'X'
bedeutet NICHT, daß Sie dem besten Weg zum Ziel folgen"* — obstacles, mines and patrols are not
considered `[MAN p.31]`.

#### 5.5 Supporting arms

| Asset | Called from | Notes |
|---|---|---|
| **Air support** | vehicle console (`Unterstützung` block) or CCV | AH‑64 / Mi‑24; the CCV shows what the pilot sees, so it doubles as reconnaissance `[GUIDE p.70]`. Standard and Experienced commanders **spend the whole mission's allocation on their own** if left alone `[GUIDE p.22]` |
| **Artillery (FIST)** | same | indirect fire onto the laser boresight point; can be walked onto a target by watching the grey puffs on the tactical display. Firing above a ridge crest drops rounds on the reverse slope `[GUIDE p.55]` |

Both are **finite per scenario** `[GUIDE p.22]`. **Every battle has air bases**; a battle may have
**up to three artillery bases**; both may be unavailable or delayed, a downed helicopter never flies
again, and artillery rides on trucks that can be destroyed — full detail in
[`hud.md`](hud.md) §7a. The number of sorties an air base holds is not stated anywhere.

---

### 6. Reading rule per mission

The `.fbm` header in this tree carries how its exit code is read (`CLAUDE.md`). None of these
missions exists yet, so each rule below is stated as **intent** — one sentence, the thing that decides.

| # | Name | Reading rule |
|---|---|---|
| 1 | Slaughterzone! | PASS only if every flagged goal of the forward base is destroyed within 15 min **and** Echo‑1's M1 survives — a 2-vs-6 opening that the AI wins by smoke discipline, not by attrition. |
| 2 | Night Forger | PASS only if Echo‑1 is clear of the pursuing platoon and every fielded MBT/IFV is dead within **5 min** — the shortest limit in the campaign, so a run that stops to fight in place is a fail even if it kills everything. |
| 3 | Rubicon | PASS only if the field command encampment and its emplaced artillery are destroyed within 15 min; Echo‑2 losses do **not** fail the run, because Echo‑2 begins the mission already attrited. |
| 4 | Thunderclap | PASS only if **11** goals fall within 15 min; the second enemy platoon is a tertiary threat and any run that turns to engage it has failed the reading even if it later wins. |
| 5 | Night's Quest | PASS only if **13** goals fall within 15 min at night on thermal sights, with the rear-threat platoon killed *first* — a run that leads with the forward vehicle is judged wrong regardless of outcome. |
| 6 | War Hammer | PASS only if the refuel depot is destroyed within 15 min while enveloped from three sides at mission start; loss of Echo‑3's M1 is expected and does not fail the run. |
| 7 | Corrosion | PASS only if the communications links **and then** every element of the enemy compound are destroyed within **30 min** — the one mission where the order of the two objectives is part of the verdict. |

---

### 7. Measurement method — `.FSG` mission files

Every `[FSG]` number above comes from here. Reproducible from the retail CD image.

#### 7.1 Container

No obfuscation of any kind; filenames and briefing text are plain. A `.FSG` is a flat chunk stream:

```
tag  : 4 bytes ASCII
size : uint16 little-endian
data : <size> bytes
```

Chunk order is fixed in all 51 files: `SHDR` `DCBS` `PATH` `STMP` `PINF` `BINF` `TERM`.
Derivation of the header shape: `SHDR`'s size field reads 0x36 and the next tag begins at 0x3C =
4 + 2 + 0x36 — so the size word is 2 bytes and excludes the 6-byte header. Verified against all
seven chunks of `SAUDI1.FSG` and re-verified on every file (`TERM` always lands exactly at EOF−6).

#### 7.2 Chunk contents

| Chunk | Size | Content |
|---|---|---|
| `SHDR` | **always 54 B** | 6-byte header + **6 coordinate pairs** (int32 x, int32 y). Header byte 5 = **mission time limit in minutes** |
| `DCBS` | variable | the object table — vehicles, buildings, targets, goal flags. **Not decoded** (§7.4) |
| `PATH` | **always 2144 B** | **8 platoon slots × 268 B** = uint32 waypoint count + 8 reserved bytes + up to **32 waypoints** × (int32 x, int32 y) |
| `STMP` | **always 258 B** | uint16 count + up to 32 coordinate pairs |
| `PINF` | **always 176 B** | **8 platoon slots × 22 B**; only the first four uint16 of each are used |
| `BINF` | **always 70 B** | four 16-byte NUL-padded filenames: heightmap `D??.KLC`, colour map `C??.KLC`, palette `???.pal`, sky `?.SKY` |
| `TERM` | 0 | end |

Coordinates are **signed 24.8 fixed point**: value / 256 = world units. Across all 51 missions the
waypoint range is **X −28 020 … +30 161, Y −11 973 … +39 099** — consistent with a signed
16-bit-cell world of 65 536 units laid over the 256 × 256 heightmap, i.e. **256 units per height
cell** `[DERIV]`. The heightmap dimension is read from the `KLC1` header of `D30.KLC`
(`uint32 256 × uint32 256`); the colour map is `512 × 512`.

**Axis convention, measured.** Twelve missions state a course heading in their briefing text. Testing
the four possible interpretations of the first path leg against those headings:

| Convention | mean absolute error over 12 missions |
|---|---|
| **bearing = atan2(Δx, Δy)** — **+X east, +Y north** | **35.9°** |
| atan2(Δx, −Δy) | 87.4° |
| atan2(−Δx, Δy) | 92.6° |
| atan2(−Δx, −Δy) | 144.1° |

**+X = east, +Y = north.** The winner is unambiguous. The residual 36° is *not* measurement error —
seven of twelve legs land within 20° (best: `SAUDI2` 6°, `SAUDI3` 10°, `SAUDI1` 13°), while two are
worse than 75°. The briefing heading is a rounded statement of the general axis of advance, not the
first plotted leg.

#### 7.3 `PINF` = the four standing orders, and it closes a loop

Each 22-byte record uses four uint16, whose observed value ranges across all 51 missions are
**0–3, 0–3, 0–5, 0–3**. Those are exactly the cardinalities of §5.3: **CMDR (4) · ADV (4) ·
FORM (6) · SPEED (4)**. Which index maps to which label is **not** established — only the shape is.

Slots 0–3 carry blue platoons and 4–7 red, inferred from `INDIA5`, where the guide names Echo 1–2 and
Bandit 1–4 and the file has records in slots {0,1} and {4,5,6,7}. **8 slots = 4 platoons per side**,
which is precisely the manual's *"zwischen einem und vier Zügen … bis zu 16 Fahrzeugen"*
`[MAN p.10]`. Two independent sources, one number.

**A zero record is a valid platoon** (`No Action / Road March / Diamond / Slow` under the hypothesis
above), so platoon *count* cannot be read from `PINF` by testing for non-zero. It cannot be read from
`PATH` either — dug-in platoons carry no route.

#### 7.4 What was not decoded

`DCBS` is the object table and the only place the per-mission goal count, unit types and start
positions can live. Its records are **variable length** — measured inter-coordinate strides in
`SAUDI1` are 17, 28, 44, 61, 69, 97, 160 and 257 bytes — so a fixed-stride walk does not work and no
record header was identified. Not decoded in this run; see `## Gaps`.

---

## State

**All seven missions are built and run.** `src/missions/c01m01…c01m07`, plus
`src/campaigns/c01-overwatch.fbc` and the baked theatre DEM (`src/data/`). Every substitution and every
number behind the results is [`substitutions.md`](substitutions.md); the reading rules of §6 are in each
file's own header, restated against what the engine can actually judge.

| # | Mission | exit | goals dead / declared | why |
|---|---|---|---|---|
| 1 | Slaughterzone! | 3 | 4 / 12 | 2 delivery units, 4 aim points; 6 T-80s unreachable |
| 2 | Night Forger | 3 | 3 / 12 | 2 delivery units, 3 platoons; 7 T-80s unreachable |
| 3 | Rubicon | **0** | 2 / 2 | both installations under one canister — **won** |
| 4 | Thunderclap | 3 | 4 / 11 | 4 of the 11 are in the platoon the reading rule forbids engaging |
| 5 | Night's Quest | 3 | 2 / 13 | 9 bunkers against 5 Mk 84 at 62.5–70.0 m; none inside 17.7 m |
| 6 | War Hammer | **0** | 2 / 2 | refuel depot and fuel tanks — **won** |
| 7 | Corrosion | 3 | 4 / 6 | both compound T-80s stood |

`--threads 1/2/4` byte-identical over 162 telemetry files and all seven `events.log`. Campaign exit 3
(the worst mission's, which is not the campaign's verdict — see the `.fbc` header).

**Two of seven fly their own condition, and the five that do not fail for exactly two reasons**:
`target_hard` is unreachable by any weapon this tree can deliver on a 40 km run-in (0 of 29 deliveries
inside 17.7 m, mean 46.0 m), and a delivery unit gets ONE pass, so a mission with 11 or 13 goals cannot
service them. Both are measurements this campaign was built to take, not defects.

## Gaps

- **`DCBS` is undecoded**, so five of seven goal counts, all unit spawn positions, all unit types and
  the goal flag itself are unrecovered. Everything §3 says about *what is in* a mission comes from
  prose, not from the file.
- **The loss condition is unstated** in both manual and guide. Time-out is certain; whether losing
  every vehicle, or losing the CCV, or losing only the flagged player vehicle ends the mission is
  not documented and was not measured.
- **The `PINF` enum orderings are a hypothesis.** Cardinalities match §5.3 exactly; index→label does
  not follow from that and was not tested in-game.
- **Air and artillery allocations per mission are not quantified** by any source found.
- **Mission 2's night status is contradictory** (§4b) and mission 2's name is contradictory (§4a).
- **The guide's campaign order contradicts the game menu** at positions 3 and 5 (§1.1).
- **The reading rules of §6 are only half judgeable.** Three of the seven ("Echo-1's M1 survives",
  "Echo-2 losses do not fail the run", "loss of Echo-3's M1 is expected") turn on a blue loss, and no
  red unit in Overwatch has a sensor or a weapon, so **no mission in this campaign is losable**. Two
  more turn on ORDER (mission 5's rear-threat-first, mission 7's links-then-compound) and `objective`
  carries no sequence. Measured, not argued: `substitutions.md` §6.1 and §6.6.
- **Engine, not source**: nothing in this tree can run a tracked vehicle, a platoon, or a commander
  seat. The undeclarables this campaign names, per [`doc/mods.md`](../../../doc/mods.md) §2:
  tracked-vehicle contacts and drive torque; a formation/standing-order layer that is *declared* and
  not coded; a second seat (CCV) that is a client on the same simulation; decaying contact memory as
  a first-class thing rather than a sensor side-effect; per-title HUD as declaration
  ([`hud.md`](hud.md)); `FBSystemId` is a closed 14-entry aircraft enum with no room for an M1A2.
