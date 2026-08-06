# Armored Fist — Overwatch: terrain and real-world anchoring

> Form per [`doc/mods.md`](../../../doc/mods.md) §3. Companion to [`campaign.md`](campaign.md);
> sources in [`sources.md`](sources.md). Tags as in `campaign.md`
> (`[FSG]` measured from the CD, `[GUIDE p.N]`, `[MAN p.N]`, `[DERIV]`).

---

## Spec

### 1. What is measured and what is reconstructed — stated first

| | |
|---|---|
| **Measured** `[FSG]` | which voxel map, palette and sky each mission loads; each platoon's plotted route in game units; the mission time limit. All re-derived in this run from the retail CD, not quoted. |
| **Measured** `[GUIDE p.53]` | the pixel positions of seven mission markers and four real cities on the printed **Overwatch campaign map**, read off a 600 dpi render of the page. |
| **Reconstructed** `[DERIV]` | the lat/lon of each mission, by fitting a plate-carrée model to three of those cities (§3). |
| **Chosen, not measured** | the 10 km box size around each point (§4). The game's world-unit → metre scale was **not determined** (§6) — so nothing here can size a battlefield from the data. |

NovaLogic's voxel terrain is invented and carries no projection, no datum and no georeference. The
anchoring below is a reconstruction from a printed map, and §5 shows it disagrees with the terrain
the game actually loads.

---

### 2. Why this real region: the game names it

Three independent statements, none of them inference:

| Source | Statement |
|---|---|
| `INDIA1.FSW` | *"THE PAKISTANI GOVERNMENT … HAS ENTERED INTO HOSTILITIES WITH THE REVOLUTIONARY PARTY THAT HAS SEIZED ILLEGAL CONTROL OF INDIA"* · *"THE 14TH HAS BEEN MOBILIZED AND AIRLIFTED TO **RAJASTAN**"* |
| `[GUIDE p.53]` Figure 4‑1, *The Overwatch Campaign Map* | a real relief map captioned **PAKISTAN** and **RAJAS[THAN]**, showing **Karachi**, **Hyderabad**, **Dadu**, **Sanghar**, **Bärmer**, the Indus and its distributaries, and the Indo‑Pakistani border as a dotted line — with the seven mission markers placed on it |
| `[MAN p.49]` | the same screen for another campaign shows **Tbilisi** and **Rustavi**, i.e. the in-game campaign map is a real-geography map in every campaign, not a fantasy one |

So the theatre is **Sindh (Pakistan) and western Rajasthan (India)** — the lower Indus valley and the
Thar desert on both sides of the border. That is not an interpretation; the game's own campaign map
draws it with real city names.

The file-set name is `INDIA*`; the campaign map graphic in `FISTDATA` is almost certainly
`PAKI_.MRL` — the six `*_.MRL` bitmaps (`AZER_`, `CYPR_`, `PAKI_`, `SAUD_`, `TURK_`, `UKRA_`) match
the six combat campaigns one-for-one, with each named for a *theatre* rather than for the mission
file prefix (`PAKI_` ↔ `INDIA*`, `TURK_` ↔ `SYRIA*`). **This mapping is inferred from the count and
the names; the `.MRL` bitmaps were not decoded.**

---

### 3. Georeferencing the campaign map — method and residuals

**Model.** North-up plate carrée with a single scale, three unknowns:

```
lon = lon0 + k·x
lat = lat0 − k·y            x, y in pixels of the 1900 × 985 render of Figure 4-1
```

**Anchors** — three cities whose markers are unambiguous on the render:

| City | pixel (x, y) | real lat / lon |
|---|---|---|
| Karachi | 385, 800 | 24.8607 N, 67.0011 E |
| Hyderabad (Sindh) | 866, 660 | 25.3960 N, 68.3578 E |
| Sanghar | 1060, 443 | 26.0470 N, 68.9490 E |

**Least-squares solution:**

```
lon0 = 65.82053     lat0 = 27.31377     k = 0.0029620 °/px   (337.6 px per degree)
1 map pixel = 0.329 km
```

**Residuals** — the check that makes this usable rather than decorative:

| City | predicted | real | Δlat | Δlon |
|---|---|---|---|---|
| Karachi | 24.9438 N, 66.9611 E | 24.8607 N, 67.0011 E | **+0.083°** | −0.040° |
| Hyderabad | 25.3585 N, 68.3860 E | 25.3960 N, 68.3578 E | −0.037° | +0.028° |
| Sanghar | 26.0014 N, 68.9608 E | 26.0470 N, 68.9490 E | −0.046° | +0.012° |

Worst residual **0.083° ≈ 9 km**. That the same k fits latitude and longitude to that tolerance also
says the base map is plate carrée, not a conic projection.

**Independent check, not used in the fit:** the *Bärmer* label sits at pixel (1800, 495), which the
model puts at **25.848 N, 71.152 E**. Real Barmer is 25.750 N, 71.383 E — off by 0.10° in latitude
and 0.23° in longitude, the latter explained by the label being drawn to the left of its city. The
fit survives a point it never saw.

**One anchor was rejected.** The *Dadu* label's marker sits at pixel (152, 384). Dadu is at
67.775 E, i.e. **east** of Karachi (67.001 E), so it must lie to the **right** of Karachi's pixel
(385) — and it does not; it is 233 px to the left. Including it destroys the fit; the other three are
mutually consistent to 9 km. **Dadu's placement on the campaign map is wrong, or that marker is not
Dadu.** Recorded, not smoothed. Reconstructed positions west of ~68 E should be treated with extra
suspicion because of it.

**Full map frame:** lat 24.396 … 27.314 N, lon 65.821 … 71.448 E — 324 km N–S × 625 km E–W.

---

### 4. Bounding box per mission

**Centres** are the mission-marker pixel positions run through §3. Marker → mission is by printed
label, except mission 4, which is unlabelled on the map and is assigned by elimination (§4.1).

**Box size is a choice, not a measurement**: ±5 km in each direction, giving a 10 km × 10 km square.
Reason: the box must hold a platoon action plus its approach march, and 10 km is the scale at which
that is true for tracked vehicles under a 15-minute clock. It is **not** derived from the game — see
§6. Converted with 1° lat = 110.98 km and 1° lon = 111.32·cos(lat) km `[DERIV]`.

| # | Name | marker px | centre lat | centre lon | min_lat | min_lon | max_lat | max_lon | real ground |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Slaughterzone! | 1400, 205 | 26.7066 | 69.9673 | **26.6615** | **69.9171** | **26.7516** | **70.0176** | Thar desert, Pakistani side, ~40 km W of the border |
| 2 | Night Forger | 1682, 243 | 26.5940 | 70.8026 | **26.5490** | **70.7524** | **26.6391** | **70.8528** | Thar desert, **inside Rajasthan** |
| 3 | Rubicon | 1600, 487 | 25.8713 | 70.5597 | **25.8262** | **70.5098** | **25.9163** | **70.6096** | Rajasthan, ~80 km W of Barmer |
| 4 | Thunderclap | 1303, 415 | 26.0845 | 69.6800 | **26.0395** | **69.6300** | **26.1296** | **69.7300** | border belt E of Sanghar |
| 5 | Night's Quest | 1010, 487 | 25.8713 | 68.8122 | **25.8262** | **68.7622** | **25.9163** | **68.8621** | lower Indus plain, S of Sanghar |
| 6 | War Hammer | 690, 793 | 24.9649 | 67.8643 | **24.9199** | **67.8148** | **25.0100** | **67.9139** | Indus delta margin, ~85 km E of Karachi |
| 7 | Corrosion | 428, 782 | 24.9975 | 67.0883 | **24.9524** | **67.0387** | **25.0425** | **67.1378** | immediately N/NE of Karachi |

**Campaign union box** (all seven, with the same ±5 km margin):

| | value |
|---|---|
| min_lat | **24.920** |
| min_lon | **67.038** |
| max_lat | **26.752** |
| max_lon | **70.853** |

≈ 203 km N–S × 383 km E–W.

#### 4.1 The unlabelled marker

Seven markers, six printed labels: *Slaughterzone*, *Reforger*, *Night's Quest*, *Rubicon*,
*Corrosion*, *War Hammer*. The seventh marker sits immediately right of the city name **Sanghar**,
where its own label would collide with it. **Thunderclap** is the only mission with no marker, so
that marker is Thunderclap `[DERIV]`. Confidence: high on the elimination, but nothing on the map
states it.

(On *Reforger* vs *Night Forger* for mission 2, see [`campaign.md`](campaign.md) §4a.)

---

### 5. The voxel terrain does not match the real region

Each mission's `BINF` chunk names its heightmap, colour map, palette and sky `[FSG]`. Cross-referenced
against `FISTDATA/MDFS.BIN`, the mission editor's map table, which gives each `C??`+sky pair a name:

| # | Mission | height / colour | palette | sky | editor name for that pair `[FSG]` |
|---|---|---|---|---|---|
| 1 | Slaughterzone! | D06 / C06 | 506 | 5 | **CLDY GREEN LAND** |
| 2 | Night Forger | D32 / C32 | 232 | 2 | **WINDY MEADOWS** |
| 3 | Rubicon | D03 / C03 | 503 | 5 | **DYING LOWLANDS** |
| 4 | Thunderclap | D06 / C06 | 506 | 5 | **CLDY GREEN LAND** |
| 5 | Night's Quest | D07 / C07 | 707 | **7** | *(no entry — 7.SKY is not in the editor table)*; C07 by day is **DRY DESERT PASS / DESERT PASSAGES** |
| 6 | War Hammer | D32 / C32 | 532 | 5 | **GREEN WETLANDS** |
| 7 | Corrosion | D32 / C32 | 832 | 8 | *(no entry — C32 has only 232 and 532)* |

**Six of seven Overwatch missions run on green, wet, meadow or lowland terrain. The campaign map puts
all seven in the Thar desert and the lower Indus.** Only Night's Quest uses a desert map — and that
one is the night mission, where it is least visible. The eight terrain pairs shipped with the game
are:

| Pair | Editor names |
|---|---|
| C03/D03 | DYING LOWLANDS · MARSH VALLEYS |
| C06/D06 | WIND BLOWN HILL · CLDY GREEN LAND |
| C07/D07 | DRY DESERT PASS · DESERT PASSAGES |
| C08/D08 | CHISELED DESERT |
| C12/D12 | DRIFTING SANDS |
| C30/D30 | CLOUDY DESERT · DESERT SUMMER |
| C31/D31 | DRIFTING SNOWS · WINTER SOLSTICE · DEAD OF WINTER |
| C32/D32 | GREEN WETLANDS · WINDY MEADOWS |

Eight terrains, 43 combat missions across six theatres — the game reuses them freely and does **not**
match terrain to theatre. Desert maps exist (C07, C08, C12, C30) and Crossed Swords in Saudi Arabia
uses them; Overwatch in the Thar does not.

**Consequence for a rebuild, stated plainly:** faithfulness to the *original* and faithfulness to the
*real place* pull apart here, and they cannot both be satisfied. This tree's principle is real
terrain from real DEM, so the boxes in §4 are the deliverable and the green voxel maps are recorded
as what the original did, not as what to reproduce.

For reference, the real ground in the union box: the Indus floodplain in the west (0–50 m, irrigated,
flat), the Thar/Tharparkar dune belt across the centre and east (50–250 m, longitudinal dunes, no
surface water), the Kirthar foothills off the box's NW corner, and the Indus delta and Arabian Sea
coast at the SW corner near Karachi. No relief anywhere in the box exceeds a few hundred metres.

---

### 6. World scale — not determined

| What | Status |
|---|---|
| Heightmap | **256 × 256** cells — from the `KLC1` header of `D30.KLC` `[FSG]` |
| Colour map | **512 × 512** — from `C30.KLC` `[FSG]` |
| Coordinate span | X −28 020 … +30 161, Y −11 973 … +39 099 world units across all 51 missions `[FSG]` |
| Units per height cell | **256** `[DERIV]` — 65 536-unit signed world over 256 cells |
| **Metres per unit** | **NOT DETERMINED** |

Three attempts and why each failed:

1. **From the CCV map legend.** The manual's CCV screenshot carries a scale bar reading **"150 M"**
   `[MAN p.83]` (glyph ambiguous — could be 160; read off a 600 dpi crop in this run). Useless
   without knowing the game-unit extent of the visible map at that zoom, which the screenshot does
   not give.
2. **From the guide's one distance statement.** *"Echo 1 and 2 defend fire bases separated
   geographically by roughly 1000 meters"* `[GUIDE p.85]`, for `SAUDI4`. The two defending platoons
   are **dug in**, so they carry no route in `PATH`, and the four routes present belong to the
   attackers. The pair could not be located without decoding `DCBS`.
3. **From route length ÷ time limit.** Player-platoon route lengths in Overwatch span 253 to 2483
   units for the same 15-minute limit — a factor of ten `[FSG]`. No single speed assumption is
   consistent, so this yields nothing.

Until `DCBS` is decoded or the game is instrumented, any metres-per-unit figure would be invented.
None is given.

#### 6.1 Measured route geometry, Overwatch

Player platoon = `PATH` slot 0. Bearings by the convention established in
[`campaign.md`](campaign.md) §7.2 (**+X east, +Y north**). Lengths in world units — deliberately not
converted.

| # | Mission | waypoints | start (x, y) | net displacement | net bearing | route length | route extent |
|---|---|---|---|---|---|---|---|
| 1 | Slaughterzone! | 4 | −3300, 6629 | 239 | 027.6° | 253 | 204 × 261 |
| 2 | Night Forger | 9 | −20680, 11894 | 722 | 250.3° | 761 | 928 × 504 |
| 3 | Rubicon | 6 | 8899, −3068 | 232 | 285.5° | 238 | 544 × 350 |
| 4 | Thunderclap | 5 | 3930, −7274 | 345 | 008.8° | 443 | 969 × 1084 |
| 5 | Night's Quest | 10 | 26749, 21956 | 2223 | 247.9° | 2483 | 2796 × 1360 |
| 6 | War Hammer | 6 | −4522, 538 | 1969 | 246.7° | 2421 | 2463 × 1776 |
| 7 | Corrosion | 4 | 7427, −181 | 544 | 171.8° | 1079 | 1489 × 2208 |

Platoons carrying a plotted route: 3, 5, 3, 3, 6, 8, 6 respectively — a **lower bound** on the number
of platoons, since dug-in platoons have none.

Every mission uses a different corner of its map, and no two missions in the campaign share both a
map and a region. The maps are shared across campaigns (C32 appears in Overwatch, Crossed Swords,
Certain Fury, Aegis, Fire Hammer, Burning Frost and Training), so **a map is a tile library, not a
place**.

#### 6.2 `STMP` — probably minefields

`STMP` holds 0–32 coordinate pairs `[FSG]`. Correlating its count against whether the briefing
mentions mines, over Overwatch:

| # | `STMP` n | briefing mentions mines? |
|---|---|---|
| 1 | 0 | no |
| 2 | 0 | no |
| 3 | **0** | **yes** — *"saturated with mine fields and pillboxes"* |
| 4 | 16 | yes |
| 5 | 4 | yes |
| 6 | 12 | yes |
| 7 | 6 | not in briefing, but `[GUIDE p.72]` warns of *"mine concentrations"* |

Six of seven agree, one does not. **Hypothesis, not a finding**; the counterexample is `INDIA3`.
`STMP` could equally be tree stands — the editor places both `[MAN p.65–66]`.

---

## State

**Built and flown.** `src/data/sindh-dem-90m.bin` is the union box of §4 widened to hold every
coordinate the seven missions name, Terrarium z13 onto a 90 m grid: **24.65–27.10 N / 66.55–71.40 E**,
5 402 × 3 017, 32.60 MB, 7 345 unique tiles, **0 holes**, elevation **−33 … 2 028 m** (mean 144.0 m),
`/elev` agreement bias **+0.14 m** rms **1.63 m** max **7.86 m** over 400 points. Recipe:
`python3 sim/tools/bake_dem.py --region sindh --verify 400`, 5.3 min. Full sheet:
[`../src/data/README.md`](../src/data/README.md).

`[MEAS]` ground under the seven objective centres: 75.1 / 248.6 / 138.8 / 42.9 / 21.1 / 62.2 / 51.7 m —
Night Forger highest in the Thar dune belt, Night's Quest lowest on the Indus plain, exactly the
gradient §5 predicts for the real region.

**The relief decided nothing.** Every `stores DELIVERY` line reports the CCRP computation plane against
the real ground at the impact point; over 29 deliveries the mean disagreement is **0.65 m** and the
worst **2.44 m**. The ground under an Overwatch battlefield is flat, so the DEM is here because the
theatre is real, not because it changes a result — and that is now measured rather than assumed.

## Gaps

- **Metres per world unit is unknown** (§6), so no mission's battlefield can be sized from the
  original. Every extent above is in game units and stays that way.
- **The 10 km box is a choice** (§4), not a measurement, and will need revising the moment §6 is
  answered. What was built uses the box's CENTRE only: the run-in is 40 km and the egress 25 km, both
  outside it, both [SET]. The 40 km turned out to decide the campaign's armoured half
  ([`substitutions.md`](substitutions.md) §6.4) — a scale answer would replace it with a derived number.
- **Dadu is misplaced on the campaign map** or misidentified here (§3); positions west of ~68 E carry
  the extra doubt.
- **Marker → mission for Thunderclap is by elimination** (§4.1).
- **The `.MRL` campaign-map bitmaps were not decoded**, so the marker positions come from a printed
  reproduction of one campaign's map rather than from the game's own asset. The other five campaigns
  have no equivalent reproduction in the sources used here.
- **The original's terrain contradicts the original's own geography** (§5). This is a conflict of
  intent, not a defect to fix, and the resolution chosen — real DEM, invented terrain discarded — is
  a decision recorded here, not a finding.
- **Engine**: nothing in this tree renders or drives ground at the ten-metre scale a tank fight
  needs — terrain, buildings, foliage and tracked contact are all absent
  ([`doc/mods.md`](../../../doc/mods.md) §2).
