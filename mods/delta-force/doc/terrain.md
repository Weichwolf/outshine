# Delta Force (1998), Campaign One PERU — terrain: footprint, anchoring, bounding boxes

> **Source documents.** Same as [`campaign.md`](campaign.md): the shipped game data read out of the
> retail CD image in this run (`C02M0n.BMS`, `C2Mn.TRN`, `DFG1_D…DFG4_D.PCX`, `DF.EXE`) and the
> publisher-hosted *Delta Force Field Manual* FM 365-7. Real-world relief in §4 is **measured** in this
> run from SRTM 30 m via the Open Topo Data API. Full list in [`sources.md`](sources.md).
> Form per [`doc/mods.md`](../../../doc/mods.md) §3 — *"the real bounding box per mission AND why this
> real place"*. Provenance tags as in [`campaign.md`](campaign.md); additionally `[SRTM]` = sampled
> here from SRTM 30 m.

**Read this first — it decides how everything below may be used.**

| | What it covers | Confidence |
|---|---|---|
| **Measured, game side** | the internal geometry of each mission in **metres** — every object's position relative to the insertion point, scale calibrated six ways | **high** (§7 of `campaign.md`, errors 1–3 m on 130–690 m) |
| **Measured, real side** | the relief of every candidate real box, from SRTM 30 m | **high** — numbers in §4, reproducible |
| **Reconstructed** | *which* real place each mission is put on | **low to medium**, and the reasoning is stated per mission |
| **Refuted** | that the game's own two real place names can be used as anchors | **measured** — §3, both fail on relief |

NovaLogic's terrain is invented. It is a **1024 × 1024 8-bit heightfield that tiles seamlessly**
(§2), with **no projection, no datum and no documented cell size**. Nothing below turns game terrain
into real terrain. It places the *measured footprint* of each mission onto a real window whose
*measured character* can carry the mission the briefing describes.

## Spec

### 1. What one Peru mission actually needs

From the measured object layout `[BMS]` and the briefing prose `[CAMP]`, per mission — this is the
requirement list that a real window has to satisfy.

| # | Mission | Footprint (measured) | Ground it must supply |
|---|---|---|---|
| 1 | Insurrection | 1052 × 771 m | a ranch on open ground; **a position 250–350 m away that overlooks it**; a ridge carrying a guard tower |
| 2 | Masquerade | 1537 × 1186 m | a tent camp on the flat; **hills west of it** for a concealed approach; two abandoned village sites 655 m and 693 m out; night |
| 3 | Flood | 1151 × 1236 m | **an airstrip long enough for a C-130 on near-level ground**; a river (`water_height 28`, the only Peru mission with water); a village ~300 m north; rising ground within 150–200 m of both flanks |
| 4 | Weatherman | 1364 × 1397 m | a village of nine buildings on a bench 86 × 59 m; an outlying farm; **hills 300–475 m north-west with a field of fire onto the village** |
| 5 | Bad Habit | 1612 × 2037 m | **a road with a continuous 1.9 km run**; palm forest; an ambush position ~225 m north-west of a village that **looks down onto the road**; night |
| 6 | Headhunter | 1458 × 980 m | a level bench ≥ 115 × 115 m for a walled villa with a courtyard and a **limousine drive**; **hills north of it** that snipers hold; dusk |

Two hard constraints follow and they pull in opposite directions: **Flood and Headhunter need flat
ground** (an airstrip, a paved compound), **Insurrection, Weatherman, Bad Habit and Masquerade need
dominant ground within 200–500 m of the objective**. A single Andean valley wall gives the second and
destroys the first.

### 2. The game's heightfield — measured, and why it cannot be georeferenced

| Property | Measurement |
|---|---|
| Size | **1024 × 1024, 8 bit**, PCX RLE — `DFG1_D` … `DFG4_D.PCX` |
| Byte range | dfg1 12–244 (mean 121) · dfg2 0–255 (mean 73) · dfg3 7–252 (mean 64) · dfg4 15–211 (mean 77) |
| **Tiles seamlessly** | mean \|Δ\| between column 0 and column 1023 = **1.01 / 0.55 / 0.62 / 1.20**, against **1.07 / 0.60 / 0.34 / 1.31** for adjacent columns 0↔1 and **40.9 / 60.5 / 51.4 / 47.2** for a random column pair. Wrap-around is as smooth as a neighbour — that is deliberate tiling |
| Terrain definition | plain text key/value, `.TRN`. **No scale key exists**: the parser's complete key list in `DF.EXE` is `terrain_name, terrain_creator, saturation, gamma, sun_slope, sky_height, water_height, water_opacity, color_map, elev_map, detail_map, detail_color, detail_elev, detail_shade, sky_map, sky_palette, water_map, water_palette, camouflage, green, brown, white, filter, turbidity, horizon, type, dirt, grass, snow, cement, sand, packed_dirt` |
| Cell size in metres | **not found.** Not in `.TRN`, not a key in `DF.EXE`; it is compiled in |
| Vertical scale | **not found.** The height byte has no documented metre value |
| Terrain sharing | six missions, **four** heightfields: dfg1 = Insurrection + Bad Habit · dfg2 = Flood · dfg3 = Masquerade + Headhunter · dfg4 = Weatherman |
| **Water exists in exactly one mission** | `water_height` is **28** for Flood and **0** for all five others; and the lowest terrain byte is 12 (dfg1), 7 (dfg3), 15 (dfg4) — all **above** the water plane. So on those three heightfields no water surface can be visible anywhere, at any place on the tile. Only dfg2 reaches 0 and carries a river |

**Consequence for a rebuild, and it is a convenience, not a loss:** because the map tiles and the
missions are ≤ 2.1 km across, the *tile* is irrelevant. What has to be reproduced is the **measured
footprint**, and that is known in metres to ±3 m.

The campaign-screen terrain thumbnails `C2M1SM…C2M6SM.PCX` (256 × 256, read as images in this run;
mission 1 and 5 are byte-identical, as are 2 and 6) show what the invented ground is: a **dissected
surface with dendritic drainage** — branching valleys, rounded interfluves, no plains and no
escarpments.

### 3. The game's own place names — and the measurement that disqualifies them as anchors

The mission *statements* name no real place. The **long briefings do** `[CAMP]`, and this is the
strongest kind of anchor available — the game naming its own ground:

| Where | Verbatim |
|---|---|
| Insurrection (Briefing1) | *"an abandoned ranch just south of the **Colombian border**"* · *"This base, approximately **100km south of the Colombian border**"* |
| Flood (Briefing3, both variants) | *"an airfield **north of Cuzco** operated by the druglord's mercenary forces"* |
| Bad Habit (Briefing5) | *"to an airfield somewhere **near Cuzco**"* · convoy moving *"along the southern edge of the designated **Special Operations Area (SOA)**"* |
| Masquerade (Win2) | *"proceed to our Special Operations Base (SOB) **south of the city of Chiclayo**"* |

**The set is internally impossible.** The Peru–Colombia border is the Putumayo, 2–3 °S in the Amazon
lowland; Cusco is 13.5 °S in the Andes, **≈ 1250 km** away; Chiclayo is on the north coast at 6.8 °S.
No single SOA contains them.

Worse for anchoring: **each named place was tested against SRTM 30 m and each fails the requirement
list of §1.** This is the measurement, not an opinion — 10 × 10 sample grids, box sizes as stated:

| Named place | Sample box centre | Box | min | max | **relief** | median \|Δh\| between samples |
|---|---|---|---|---|---|---|
| 100 km due south of San Antonio del Estrecho on the Putumayo border (the border town's coordinate + 0.898° of latitude) | −3.3470, −72.6683 | 1.6 × 1.6 km | 88 m | 106 m | **18 m** | 2 m per 178 m |
| Quillabamba, the nearest lowland town north of Cusco (Wikipedia coordinate) | −12.8450, −72.6900 | 1.6 × 1.6 km | 971 m | 1620 m | **649 m** | 49 m per 178 m |
| sample point 2.3 km north-west of it, on the valley floor | −12.8300, −72.7050 | 1.4 × 1.4 km | 970 m | 1616 m | **646 m** | 37 m per 156 m |
| Echarate, on the Urubamba valley road (Wikipedia coordinate) | −12.7676, −72.5769 | 1.8 × 2.2 km | 890 m | 1294 m | **404 m** | 16 m per 244 m |
| sample point, lower Urubamba | −12.6300, −73.0500 | 1.4 × 1.4 km | 700 m | 1393 m | **693 m** | 40 m per 156 m |
| sample point, Urubamba further downstream | −12.4300, −73.1000 | 1.4 × 1.4 km | 1214 m | 1794 m | **580 m** | 39 m per 156 m |

- **The Colombian-border site is flat**: 18 m of relief across the whole of Insurrection's footprint.
  Insurrection needs *"a position overlooking Objective Breeze"* and *"the watch station on the ridge"*.
  Neither exists there. The literal reading is not buildable.
- **Every site north of Cusco is a gorge**: 580–693 m of relief per 1.4 km, i.e. 40–50 % mean slope.
  Flood needs a C-130 airstrip and Headhunter a limousine drive. Neither exists there.

**Therefore: the game's toponyms are recorded as fiction, not used as anchors.** That is the honest
outcome of naming them — the check was possible only because the game named them, and the check failed.

### 4. Anchoring rule, and the region chosen

Rule, applied in order:

1. **Peru** — stated by the campaign screen `[MAN p.16]` and by every briefing.
2. **The real subject of the fiction.** Mercenaries hired by a coca druglord, US special operations
   forces, airstrips, convoys, caches: the real counterpart is the **Alto Huallaga**, the coca and
   cocaine-processing centre of Peru in the late 1980s and 1990s, and the theatre of the real US
   counter-narcotics deployment **Operation Snowcap**, whose forward base sat at Santa Lucía in that
   same valley. This is what the campaign is a fictionalisation of.
3. **The measured requirement list of §1 decides the exact window**, not the theme. Each box below was
   sampled with SRTM 30 m and kept only if its relief matches what the mission needs.

Target band derived from §1: a mission needs a knoll 40–120 m above its objective at 200–500 m
distance, i.e. **relief 70–300 m across the box** with a median inter-sample step of 5–40 m — except
Flood, which additionally needs a level strip inside that box.

### 5. Bounding box per mission

Box = **measured object footprint + 250 m margin on every side** (the margin carries insertion
approach and extraction egress, both of which sit at the edge of the object cloud).

```
Δlat = (NS/2) / 111320                        Δlon = (EW/2) / (111320 · cos φ)
Insurrection: NS = 771+500 = 1271  →  Δlat = 635.5/111320   = 0.005709°
              EW = 1052+500 = 1552 →  Δlon = 776/(111320·cos 8.98°) = 0.007057°
```

Format `min_lat, min_lon, max_lat, max_lon`, WGS84 decimal degrees.

| # | Mission | Box size | Centre | Bounding box |
|---|---|---|---|---|
| 1 | **Insurrection** | 1552 × 1271 m | −8.9800, −76.1600 | `-8.9857, -76.1671, -8.9743, -76.1529` |
| 2 | **Masquerade** | 2037 × 1686 m | −8.7500, −76.2000 | `-8.7576, -76.2093, -8.7424, -76.1907` |
| 3 | **Flood** | 1651 × 1736 m | −8.2150, −76.5650 | `-8.2228, -76.5725, -8.2072, -76.5575` |
| 4 | **Weatherman** | 1864 × 1898 m | −9.1800, −75.9600 | `-9.1885, -75.9685, -9.1715, -75.9515` |
| 5 | **Bad Habit** | 2112 × 2537 m | −9.0369, −75.5075 | `-9.0483, -75.5171, -9.0255, -75.4979` |
| 6 | **Headhunter** | 1958 × 1480 m | −9.2900, −75.9900 | `-9.2966, -75.9989, -9.2834, -75.9811` |

Why each one, with the SRTM sample that justifies it. **`rotate`** is not decoration: the mission
layout has to be turned by that angle before it is laid on the box, because the ground the mission
needs on a given bearing has to land on the ground the box actually has (§6, orientation is free).

| # | Real place | Relief | median step | high ground in box | mission needs it at | **rotate** |
|---|---|---|---|---|---|---|
| 1 | Huallaga left-bank hills west of Aucayacu — rolling coca hill country with dominant knolls; a ranch clearing with ground above it at 250–350 m is exactly this landform | **175 m** | 15 m per 141 m, p90 39 m | 190°, 495 m S of centre | 172° — the guard tower sits S of the camp | **−18°** |
| 2 | Huallaga valley floor with a hill mass in one corner: 77 of 100 samples in the lowest fifth of the range, i.e. **flat floor plus one steep corner** — the tent camp and two village sites on the flat, the concealed approach on the corner | **233 m** | 5 m per 187 m, p90 44 m | 050°, NE corner | 270° — *"the hills west of Objective Calm"* | **−140°** |
| 3 | Tocache valley floor with the western foothill front inside the box; **the real Tocache airstrip lies 4.5 km at 061° from the centre**, and the Huallaga runs this valley — the one mission with water | **99 m** | 6 m per 193 m, p90 25 m | 208°, SW | 270° — rising ground off the strip's flank | **+62°** |
| 4 | Hills north of Tingo María: a bench for the nine-building village with higher ground for the supporting team | **100 m** | 7 m per 211 m, p90 26 m | 128°, SE | 315° — Alpha *"in the hills approximately 300m northwest of the village"* | **−173°** |
| 5 | Aguaytía, on the Federico Basadre trunk road Tingo María–Pucallpa at the river bridge — a real long-distance road corridor in palm forest with low banks above it | **74 m** | 5 m per 282 m, p90 25 m | 327°, NW | 315° — the ambush position NW of the village, above the road | **−12°** |
| 6 | Tingo María, valley floor under the Bella Durmiente ridge. Relief 267 m against a median step of 13 m and 65 of 100 samples in the lowest fifth: **a flat bench with steep ground on one side** — the walled villa with its courtyard and car on the bench, the snipers on the steep side | **267 m** | 13 m per 164 m, p90 54 m | 134°, SE | 000° — *"the hills north of the villa"* | **−134°** |

**Two boxes are already aligned** (Insurrection −18°, Bad Habit −12°, both inside the resolution of a
10 × 10 sample grid); **one needs turning nearly end for end** (Weatherman −173°); the other three need
62–140°. That is the honest price of anchoring on measured landform instead of on a place name, and it
is cheap: the mission's internal geometry is preserved exactly, only its compass alignment against the
real DEM changes.

Method: high ground = the maximum of the 10 × 10 sample grid, bearing taken from the box centre;
*needs* = the bearing from the objective to the ground the briefing or the object layout requires;
`rotate` = the difference, applied to the whole layout. **The grid is coarse** — 141–282 m between
samples — so a bearing is good to roughly ±20°, and a second maximum of nearly equal height would not
show up at all. These angles are a starting alignment, not a fit.

**Campaign theatre box** (union of all six + 0.01° margin):

| | Value |
|---|---|
| min_lat | **−9.307** |
| min_lon | **−76.582** |
| max_lat | **−8.197** |
| max_lon | **−75.488** |

≈ 123 km north–south × 120 km east–west. It contains Tocache, Aucayacu, Tingo María and Aguaytía —
the Huallaga valley and its road to Pucallpa.

### 6. What this reconstruction does not claim

- **It is not where the game is set.** §3 measured that the game's own two location statements cannot
  host their own missions. The boxes are chosen so the *missions* work on real ground, in the real
  region the fiction is drawn from.
- **No feature-by-feature correspondence.** No claim that a particular real bend of the Huallaga is
  Flood's river, or that a real building is the villa. The measured object layout is placed onto the
  box; the real DEM/OSM supplies ground of the right character and the right amplitude.
- **Orientation is free, and §5 uses that freedom.** The game's map has no north; §7 of
  [`campaign.md`](campaign.md) establishes +y = north *inside the game's own frame* from six radio
  lines, but nothing binds that frame to the real DEM. The `rotate` column is exactly this freedom
  being spent, and it is stated per mission rather than hidden.
- **Elevation is not anchored.** Boxes sit at 274–1458 m real elevation; the game's height byte has no
  metre value (§2), so the vertical exaggeration of a rebuild is a free parameter.

### 7. Distances that must survive the transfer

If the reconstruction is redone with different boxes, these measured relations are the invariants —
they are what makes each mission the mission `[BMS]` / `[CAMP]`:

| Mission | Invariant |
|---|---|
| Insurrection | objective 273 m at 225° from the IP; Alpha at 130 m / 146°; Charlie at 235 m / 287°; one guard tower |
| Masquerade | objective 367 m at 080°; villages at 693 m / 295° and 655 m / 242° from the objective |
| Flood | objective 528 m at 183°; airfield group 114 × 70 m; village group 370 m at 036° from it |
| Weatherman | objective 308 m at 061°; farm 106 m at 355°; village 86 × 59 m |
| Bad Habit | convoy route 1891 m; column 81 m long, heading 175; friendlies 409–445 m at 300–323° from the village |
| Headhunter | villa 113.5 × 114.1 m, 509 m at 281° from the IP; druglord inside it |

## State

**Nothing built.** No `.fbm`, no terrain fetch for these boxes, no check that `fb-tiles` has DEM
coverage for them.

## Gaps

- **Terrain cell size unknown**, so the game's own map scale cannot be stated (§2). Four approaches
  were considered and none is decisive; the only sound one — reading the compiled constant out of
  `DF.EXE` — was not attempted in this run.
- **HUD grid scale unknown.** The grid box shows `I13` / `M13` `[SHOT]` `[MAN p.14]`. `DF.EXE` holds
  `@MKEY%03i`, `Map Keys` and a `%c%c` format, but no grid parameter; with the cell size missing the
  grid origin and pitch are underdetermined.
- **The briefing-map label coordinates are not metric.** Each map-key string in `DFCAMP02.BIN` carries
  two 16-bit numbers (measured range a = 357–698, b = 329–720). Tested as a linear map to world
  coordinates on Insurrection: IP→Alpha gives 3.33 m per unit in x and 5.53 m per unit in y;
  IP→Charlie gives 4.30 and 2.88. **Not a projection — hand-placed label anchors.** Recorded so nobody
  repeats the test.
- **`fb-tiles` coverage for Peru is unverified.** The engine's DEM path is proven for Switzerland
  (`sim/`), not for −9 °S.
- **Ground at ten metres does not exist in the engine.** From
  [`doc/mods.md`](../../../doc/mods.md) §2: *"all three: ground seen from ten metres — terrain,
  buildings and foliage at that scale"*. These boxes are 1.5–2.5 km across and the player walks them:
  they need metre-scale terrain, 193 palm trees in Bad Habit alone, and buildings a man can enter and
  shoot out of. Nothing in the renderer or the collision path is built for that scale.
- **No cover, no ground LOS.** Repeated from [`campaign.md`](campaign.md) `## Gaps` because it is a
  *terrain* deficit as much as a body deficit: the whole campaign is decided by whether a position
  overlooks another, and the sim cannot answer that question at man height.
