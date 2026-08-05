# Comanche: Maximum Overkill — Campaign One: terrain, scale, anchoring, bounding boxes

> **Sources:** the four voxel maps of the **1992 three-floppy release** (`C1–C4.DTA`, `D1–D4.DTA`),
> decoded here; the mission records of `1.MIS` and `0.MIS`, decoded here; the NovaLogic
> `USER'S MANUAL`. Method in [`sources.md`](sources.md) §2.
> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has no `test/`.

**Read this table first — it decides how everything below may be used.**

| | What it covers | Confidence |
|---|---|---|
| **Measured** | the four heightfields and colour maps, cell for cell; every object and start position in map cells | **high** — the files decode to exactly 1 024 × 1 024 bytes with no residue |
| **Derived** | the **vertical** scale, from the manual's stated 500 ft ceiling against the measured peak height | **medium** — the bracket is ±3 %, but it rests on one sentence |
| **Derived** | the **horizontal** scale, from reinforcement delay against range, cross-checked against terrain slope | **medium-low** — two independent methods agree to within a factor 1.4 |
| **Reconstructed** | which real place each map is | **map 3 medium** (the game names Kīlauea), **maps 1 and 4 low** (the game names a country only, and in the *training* campaign), **map 2 none at all** (§6) |
| **Assigned** | the degree boxes in §7 | **low.** They are a placement of an invented layout onto a real region, and are offered as an engineering choice, not a finding |

NovaLogic's voxel terrain is invented. There is **no projection, no datum, no scale bar and no north
arrow anywhere in the game or its manual.** Nothing below turns game terrain into real terrain.

## Spec

### 1. The map files of the 1992 release `[DTA]`

| File | Count | Size | Content |
|---|---|---|---|
| `C1.DTA` … `C4.DTA` | 4 | 0.58–0.81 MB | **colour maps**, 1 024 × 1 024, 8-bit indexed |
| `D1.DTA` … `D4.DTA` | 4 | 0.56–0.67 MB | **height maps**, 1 024 × 1 024, one byte per cell |
| `P11 P12 P21 P22 P23 P31 P32 P33 P34 P43 P44` | 11 | 941 B | per-mission **day** palette + a 16 × 22 indexed image |
| `NP14 NP23 NP24 NP34 NP44` | 5 | 941 B | per-mission **night** palette + the same |

**Four terrain maps for twenty missions.** Naming is `[n]p<map><variant>.dta`: the first digit selects
the `C`/`D` pair, the leading `n` selects night.

Format, established here and reproducible: 8-byte magic `Kyle DTA`, `uint16` (width−1) at offset 8,
`uint16` (height−1) at offset 10, payload from **0x80** in PCX-style RLE (`byte ≥ 0xC0` → run of
`byte & 0x3F`, next byte is the value), and a **768-byte VGA palette appended at the end of the file**.
All eight terrain files decode to exactly 1 048 576 bytes with the palette left over — the check that
the reading is right.

**The `n` files are the image intensifier**, measured, not assumed. Mean palette RGB:

| Pair | day | night |
|---|---|---|
| `P23` / `NP23` | 122.2, 106.3, 99.3 | **87.4, 131.5, 88.2** |
| `P44` / `NP44` | 134.2, 121.8, 89.9 | **104.9, 137.5, 87.5** |
| `P11` / `NP14` | 113.8, 115.5, 93.8 | **100.5, 136.6, 87.2** |

Day palettes are red-dominant, night palettes green-dominant — exactly the manual's *"tones of green
and black … your Image Intensifiers and Thermal Imagers are on-line"* `[MAN p.22]`.

### 2. Terrain statistics, measured `[DTA]`

Height is one byte per cell; gradient is `|Δh|` between adjacent cells, over both axes, 2 095 104
samples per map.

| Map | h min | **h max** | h mean | h σ | h = 0 | water¹ | ∇ p50 | ∇ p90 | ∇ p99 | ∇ p99.9 | ∇ max |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **1** | 0 | **118** | 41.9 | 26.6 | 4.2 % | 0.4 % | 0 | 1 | 3 | 6 | **33** |
| **2** | 0 | **110** | 33.8 | 24.6 | 8.8 % | **6.4 %** | 0 | 2 | 4 | 7 | 19 |
| **3** | 0 | **116** | 45.8 | 29.3 | 7.1 % | **0.0 %** | 0 | 1 | 3 | 5 | 12 |
| **4** | 0 | **121** | 55.9 | 31.6 | 10.5 % | **0.0 %** | 0 | 2 | 4 | 6 | 9 |

¹ fraction of colour-map cells whose palette entry is blue-dominant (`B > R+25` and `B > G+15`).

Character of each map, read off the decoded height and colour maps:

| Map | Morphology |
|---|---|
| **1** | broad green valleys between brown rock ridges and low mesas; **meandering perennial rivers with lakes**; a large **stepped pyramid** stamped into the colour map north of centre |
| **2** | plateau incised by a **dendritic canyon network with permanent water in the floors**; mesas and buttes between; bare tan-orange rock, no vegetation; one grey playa |
| **3** | **volcanic**: black and dark-red rock, bright red flows crossing the map, pale ash fields; **no water at all**; the smoothest terrain of the four (∇ max 12) |
| **4** | one broad **flat sand-floored valley system** meandering through rolling rock hills with scattered light-sand mesas; **no water at all**; the highest relief and the *lowest* peak gradient — big, smooth, dry |

### 3. Vertical scale — derived `[DERIV]`

Two statements from the manual:

> *"In this simulation the Comanche's ceiling is limited to about **500 ft. above sea level**."* `[MAN p.36]`
> *"even when flying at maximum altitude, if you go over a mountain, your altimeter may register only a
> few feet above the ground."* `[MAN p.36]`

So the highest terrain sits **just below** the ceiling. The highest cell in any 1992 map is **121**
(map 4). Let `C` be the height byte the ceiling corresponds to; then `C ≥ 121`, and `C` cannot be far
above 121 or the second sentence would be false.

```
C = 121  →  500/121 = 4.13 ft = 1.259 m per height unit   (peak exactly at the ceiling)
C = 125  →  500/125 = 4.00 ft = 1.219 m                   (peak 484 ft, 16 ft of clearance)
C = 128  →  500/128 = 3.91 ft = 1.191 m                   (peak 473 ft, 27 ft of clearance)
```

**1 height unit = 1.19–1.26 m; adopt 1.22 m (4 ft).** The bracket is ±3 % across the whole plausible
range of `C`, which is why this is usable even though `C` itself is unknown.

Consequence: **total relief of the game world is 0–148 m.** Maps 2 and 3 top out at 134 m and 141 m.
Nothing in Comanche is a mountain in the ordinary sense; every "mountain" the manual mentions is a
100–150 m hill.

### 4. Horizontal scale — derived, two independent ways `[DERIV]`

Nothing states it. Both derivations below are reconstructions.

#### 4.1 Reinforcement delay against range

*Thirsty Werewolves* posts four fuel depots and spawns a Werewolf wave at each with a delay. The waves
arrive **in order of distance from the player start** (91, 301):

| Depot | Cell | Range from start | Delay |
|---|---|---|---|
| A | 576, 328 | 486 | 3 600 |
| C | 790, 341 | 700 | 5 400 |
| B | 838, 505 | 774 | 6 300 |
| D | 279, 858 | 588 | 7 200 |

The first three are monotone; least squares on them:

```
d = 100.4 + 0.1084 · t      cells, t in delay units      (R² = 0.98 on three points)
```

If the delay unit is a tick, the player's assumed transit rate is `0.1084 · f` cells per second:

```
f = 60 Hz  →  6.50 cells/s        f = 70 Hz  →  7.59 cells/s
```

against the simulated top speed of **177 kt = 91.0 m/s** `[MAN p.37]`:

```
              at 91.0 m/s (top)      at 73 m/s (80 %)
60 Hz            14.0 m/cell            11.2 m/cell
70 Hz            12.0 m/cell             9.6 m/cell
```

**9.6–14.0 m per cell.** Depot D breaks the ordering (588 cells at 7 200) and is excluded, stated
rather than hidden.

#### 4.2 Terrain slope realism

Independent of §4.1. With 1.22 m per height unit, a 99th-percentile gradient of 3–4 height units per
cell is 3.7–4.9 m of rise per cell. Real dissected terrain has a 99th-percentile slope of roughly
25–35°:

```
cell = 4.3 m / tan(30°) = 7.4 m        cell = 4.3 m / tan(25°) = 9.2 m
cell = 4.3 m / tan(35°) = 6.1 m
```

**6–9 m per cell** — and at 10 m per cell the 99.9th percentile (5–7 units = 6.1–8.5 m) becomes 31–40°,
and map 1's single worst gradient (33 units = 40 m in one cell) becomes a 76° canyon wall. All three
percentiles land where real terrain lands.

#### 4.3 Adopted

**1 map cell = 10 m.** It sits inside both brackets, it is the only round value that does, and it makes
the world **10.24 km × 10.24 km**.

Sanity check against the one distance the game states in prose: *Wolfpack*'s *"Every Werewolf within
50 klicks is on your tail"* `[MIS]`. Measured farthest Werewolf from the player start: **239 cells =
2.4 km**. The prose is hyperbole by a factor 20 — the bound is satisfied and carries no information.

The manual's Hellfire *"standoff range greater than 8 km"* `[MAN p.49]` is **larger than the map**.
That is a real inconsistency between the manual's real-weapon data and the game's geometry, and it is
recorded rather than resolved.

### 5. Coordinate frame `[DERIV]`

Derived in [`campaign.md`](campaign.md) §7 from six wingman spawns (five of six agree):

```
X = image column = east          Y = image row = south          row 0 = north
heading 0 = N, 64 = E, 128 = S, 192 = W          (256 steps to the circle)
lat = lat_c + (512 − Y) · 10 m / 111 132
lon = lon_c + (X − 512) · 10 m / (111 320 · cos lat_c)
```

`lat_c, lon_c` is the real anchor placed at cell (512, 512).

### 6. Which real place — what the game says, and what it does not

This is the part [`doc/mods.md`](../../../doc/mods.md) §3 demands and that has no other home.

**The 1992 campaign under study names no place at all.** Ten briefings, zero toponyms — the enemy is
"the KGB", the settings are "the desert", "a small valley", "an ancient sacrificial temple". The
georeferences come from the **training** campaign `0.MIS`, which reuses the same four maps:

| Map | What `0.MIS` says | Mission | Strength |
|---|---|---|---|
| **1** | *"Spread out over the **lush green hills of Peru**"* | *Mayan Malay* (`p11`) | a country, and a contradiction — §6.1 |
| **2** | *"Vast numbers of enemy tanks … **in the desert**"* | *Wing Leader* (`p23`) | a landscape type, no place |
| **3** | *"a treacherous volcanic region on the **south side of Kilauea**"* | *The Kilauea Encounter* (`p32`) | **a named volcano.** The only hard georeference in the 1992 release |
| **4** | *"A small faction of the **Afghan** military …"* | *Afghan Unrest* (`np44`) | a country, no place |

**The same map is reused across incompatible geographies**, deliberately: map 4 is Afghanistan in
training and an unnamed KGB valley in the campaign; map 3 is Hawaii in training and a secret base
hidden from satellites by *"rising heat and cloud cover"* in the campaign. Any single real anchor is
therefore a **choice**, not a fact, and the choice below is made on morphology.

#### 6.1 Map 1 — the contradiction is in the game

The colour map carries a **Mesoamerican stepped pyramid**. The briefing says **Peru**. Mayan pyramids
are Mexican and Guatemalan; Peru is Inca and Moche. The mission's own title, *"Mayan Malay"*, mixes a
third region in. **Not smoothed.** Two defensible anchors follow from the two halves of the
contradiction:

| Anchor on | Place | Fits | Fails |
|---|---|---|---|
| the **text** (chosen) | **Moyobamba, Alto Mayo, San Martín, Peru** — 6.033 S, 76.967 W | rolling forested hills with 100–250 m relief; a large meandering river with oxbows and ox-bow lakes; dense green | no stepped pyramid within 1 000 km |
| the **architecture** | Petén, Guatemala (Tikal) | the pyramid, and lush green karst hills of the right relief | the game says Peru |

**Chosen: Peru**, because it is the only place the game names for this map. Recorded so the other reading
survives.

#### 6.2 Map 2 — no source names anything

Morphology: a plateau cut by a **dendritic canyon system with permanent water in the floor**, mesas and
buttes between, bare tan-orange rock, no vegetation, one playa, 134 m of relief, 6.4 % water. That is a
short list of real places. Proposed: the **Green River / Colorado confluence, Canyonlands, Utah**
(38.18917 N, 109.88528 W).

| Why it fits | Where it strains |
|---|---|
| perennial river meandering in a deeply incised, dendritically branching canyon inside an arid plateau — the defining combination, and rare | real canyon rims there stand 300–500 m above the river; the game gives 134 m |
| bare orange-tan sandstone with dark cap rock, exactly the colour map's two dominant families | |
| mesas and buttes as isolated remnants between drainages | |
| no vegetation cover anywhere, matching a colour map with no green | |

The relief shortfall is not repaired. Comanche's whole world is 148 m tall (§3); no real canyon country
of the right *shape* is also of the right *depth*, and shape was chosen over depth.

#### 6.3 Map 3 — the game names it

*"the south side of Kilauea"*, Hawaii `[MIS0]`. Anchor: **Kaʻū Desert, 19.40861 N, 155.29667 W**
(Kīlauea summit itself: 19.421097 N, 155.286762 W).

| Why it fits | Where it strains |
|---|---|
| **0.0 % water** measured — the Kaʻū Desert has no surface water at all, which is true of very few landscapes | Kīlauea's south flank drops ~1 100 m from summit to sea over ~10 km; the map gives 141 m |
| bright red flows across dark rock and pale ash fields: the colour map is a lava field | the box's north edge (19.4547) **straddles the summit caldera** (19.4211), and the game's map has no caldera. Shifting the anchor ~5 km south would put the box wholly on the flank at the cost of leaving the one coordinate any source actually gives |
| the **smoothest** terrain of the four (∇ max 12 against 33 on map 1) — pāhoehoe and ash-mantled slopes are smooth at this cell size | the map has no coast, so the box must also stay clear of the shoreline; it does |

#### 6.4 Map 4 — country only

*"Afghan"* `[MIS0]`. Morphology: one broad **flat sand-floored valley** winding through rolling rock
hills, scattered light-sand mesas, **0.0 % water**, the largest relief (148 m) and the *lowest* peak
gradient of the four — a large, smooth, entirely dry drainage. Proposed: the **Khash Rud valley,
Khash Rod District, Nimruz, Afghanistan** (31.7773 N, 62.9724 E).

| Why it fits | Where it strains |
|---|---|
| a broad braided sand-floored wadi cutting rolling gravel hills, dry for most of the year | the game's valley floor is at height 0 everywhere, i.e. perfectly flat; a real wadi floor is not |
| no water, no vegetation, no settlement in the colour map | Nimruz relief over 10 km is nearer 60–100 m than 148 m |
| 10 % of the map at height 0 — the largest flat fraction of the four, which is what a big alluvial floor looks like | |

### 7. Bounding boxes — reconstructed

Cell box = all objects plus the player start, **+20 cells of margin**. Degrees from §5 with the anchors
of §6 at cell (512, 512) and 10 m per cell.

#### 7.1 Full map extent, per map

| Map | Anchor | min lat | max lat | min lon | max lon |
|---|---|---|---|---|---|
| **1** | Moyobamba, Peru | **−6.0790** | **−5.9869** | **−77.0132** | **−76.9208** |
| **2** | Canyonlands, Utah | **+38.1432** | **+38.2352** | **−109.9438** | **−109.8269** |
| **3** | Kaʻū Desert, Hawaii | **+19.3626** | **+19.4547** | **−155.3454** | **−155.2480** |
| **4** | Khash Rud, Afghanistan | **+31.7313** | **+31.8234** | **+62.9183** | **+63.0264** |

Each is 10.24 km square: 0.0921° of latitude, and 0.0924° / 0.1169° / 0.0974° / 0.1081° of longitude.

#### 7.2 Per mission

| # | Mission | Map | cells X | cells Y | min lat | max lat | min lon | max lon |
|---|---|---|---|---|---|---|---|---|
| 1 | Werewolves on Patrol | 2 | 55–885 | 230–740 | **+38.1687** | **+38.2145** | **−109.9375** | **−109.8426** |
| 2 | The Last Sacrifice | 1 | 103–770 | 60–870 | **−6.0652** | **−5.9923** | **−77.0039** | **−76.9437** |
| 3 | Tactical Run | 2 | 0–450 | 0–730 | **+38.1696** | **+38.2352** | **−109.9438** | **−109.8924** |
| 4 | Rivers Run Deep | 2 | 40–980 | 10–720 | **+38.1705** | **+38.2343** | **−109.9392** | **−109.8318** |
| 5 | Night of Death | 3 | 0–970 | 80–1001 | **+19.3646** | **+19.4475** | **−155.3454** | **−155.2530** |
| 6 | Thirsty Werewolves | 4 | 6–960 | 132–928 | **+31.7399** | **+31.8115** | **+62.9189** | **+63.0197** |
| 7 | Spiritual Reclamation | 1 | 305–750 | 60–830 | **−6.0616** | **−5.9923** | **−76.9857** | **−76.9455** |
| 8 | Volcanic Nightmare | 3 | 10–620 | 240–870 | **+19.3764** | **+19.4331** | **−155.3445** | **−155.2864** |
| 9 | Valley of Instant Death | 4 | 0–561 | 195–752 | **+31.7557** | **+31.8058** | **+62.9183** | **+62.9776** |
| 10 | Wolfpack | 4 | 55–845 | 121–984 | **+31.7348** | **+31.8125** | **+62.9241** | **+63.0076** |

**Seven of ten missions use over 70 % of the map.** These are not small arenas with a target at one end;
they are whole-map sweeps, and the reinforcement delays of §4.1 exist because a whole-map sweep takes
minutes.

### 8. What this means for a rebuild

| Decision | Consequence |
|---|---|
| The heightfields are **data we hold**, cell for cell | a rebuild does not need to invent terrain; it needs to decide whether to fly the *original* heightfield or real DEM under the boxes of §7 — and the two are not interchangeable, because the game's relief is 148 m and every real box's is larger |
| The world is **10 km square and 150 m tall** | tile LOD, draw distance and terrain-following gains are all sized off this, not off an F-16's world |
| **0.0 % water on two of four maps**, 6.4 % on one | water rendering is required by exactly one map |
| Night is **a palette swap on the same terrain**, not a lighting model | the cheapest correct implementation is the original's, and it is measurable against the shipped palettes |

## State

**Nothing.** No terrain loader for these maps, no `.fbm`, no rotorcraft. The maps have been decoded and
measured in this run; nothing consumes the measurements.

## Gaps

- **The horizontal scale is the weakest number in this mod.** Two methods bracket 6–14 m; 10 m was
  adopted. If the delay unit is not a tick, §4.1 collapses and only §4.2 survives.
- **The loop rate is unknown.** 60 Hz and 70 Hz both appear in §4.1 and differ by 17 % in the result.
- **`C` — the height byte at the 500 ft ceiling — is unknown.** The ±3 % bracket in §3 holds only if
  the manual's "few feet above the ground" is literal.
- **No map is georeferenced by the campaign under study.** All four anchors come from the *training*
  campaign or from morphology; the boxes in §7 are engineering choices.
- **The 941-byte `p*` / `np*` files are decoded only as far as their palette.** Their 16 × 22 indexed
  image is not interpreted, and it is per-mission, not per-map — so something mission-specific is being
  carried that this reconstruction does not see.
- **Map objects are not in the height or colour map.** The pyramid on map 1 is painted into the *colour*
  map, so it has no geometry; whether the engine gives it collision is unknown.
- **Nothing has been rendered from these maps** in this tree. The morphology descriptions in §2 and §6
  come from top-down images, not from the game's own perspective view, which is what a player actually
  judges terrain by.
- **Whether the CD altered the maps is untested.** The CD carries the same file names; they were not
  compared byte for byte.
