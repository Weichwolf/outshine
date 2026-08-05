# Delta Force (1998), Campaign One PERU — sources, method, and what was refuted

> Every claim in [`campaign.md`](campaign.md), [`terrain.md`](terrain.md) and [`hud.md`](hud.md) is
> reachable from this file. Contradictions between sources are recorded, not smoothed.
> Form per [`doc/mods.md`](../../../doc/mods.md) §3.

## Spec

### 1. Source ranking, as applied

| Rank | Class | Used for |
|---|---|---|
| 1 | **Shipped game data**, parsed in this run | mission names, order, objectives, goals, briefings, radio lines, object inventory and geometry, terrain parameters, HUD strings |
| 2 | **Manual PDF** (publisher-hosted), page images read at 400–900 dpi | campaign list, HUD elements, keys, gear, settings |
| 3 | **Publisher-hosted screenshots** of the original | HUD cross-check, colours |
| 4 | Wikipedia / reference works | real-world coordinates and background only |
| — | Walkthroughs, fan wikis, MobyGames | **not used.** Every question they would have answered was answered by rank 1 |

Where rank 2 and rank 1 disagree, rank 1 wins and the disagreement is written down
(one such case: [`campaign.md`](campaign.md) §4, Flood's statement vs its goal — but that one is
*inside* rank 1).

### 2. The game data — access path and formats

The retail CD image was read **over HTTP range requests**; it was never downloaded whole and nothing
from it is stored in this repository.

| Step | Detail |
|---|---|
| Image | `DELTAFORCE.iso`, 709 451 776 B — <https://archive.org/details/DELTAFORCE>. ISO 9660 parsed directly: PVD at byte 32768, root directory at LBA 20 |
| **Pressing caveat** | this disc also carries `/DF2DEMO` and its `MANUALS/DFMAN.PDF` is dated **2000-06-06**, so it is a **later re-release**, not the 1998 first pressing. Mission data is assumed unchanged; that assumption is **not verified** |
| Archive 1 | `/DFSETUP/DFBASE.PFF`, LBA 22666 = byte 46 419 968, **640 entries** |
| Archive 2 | `/DFSETUP/DF.PFF`, LBA 81737 = byte 167 397 376, **656 entries** |
| Executable | `/DFSETUP/DF.EXE`, LBA 22127, 1 102 860 B |
| On-disc manuals | `/MANUALS/DFMANUAL.PDF` (Addendum, 1998-10-13) and `/MANUALS/DFMAN.PDF` (Field Manual, 2000-06-06) |
| **PFF3 header** | `u32 headerSize=20` · `char[4] "PFF3"` · `u32 numFiles` · `u32 entrySize=32` · `u32 dirOffset`. Entry: `u32 flags` · `u32 offset` · `u32 size` · `u32 time` · `char[16] name`. Offsets are relative to the archive start. **No obfuscation** — unlike the XOR-masked index of NovaLogic's F-22 `RESOURCE.RES` |
| Known ISO defect | directory entries of the large files carry impossible lengths (`DF.PFF` = 1 744 782 237 B on a 709 MB disc). Real sizes come from the LBA gaps and agree with the PFF headers |

**`RTXT` string tables** (`DFCAMPS.BIN`, `DFCAMP02.BIN`, `DFGAME.BIN`, `DFDLG02.BIN`) — format derived
in this run:

```
0x00  char[4] "RTXT"
0x04  u32 keyTableOffset (from 0x04)      0x08  u32 keyTableSize      0x0C  u32 stringCount
0x10  stringCount × 16 B index: u32 dataOffset · u16 a · u16 b · u32 groupIndex · u32 0
      string data, NUL-terminated
      key table: nGroups × (u32 firstKeyOffset, u32 keyCount), then nGroups group names,
      then the keys themselves
```

Strings map to keys **by order within the group**. That mapping is self-validating in the one place it
matters: the *Goals* group's key list is **not sorted** (`M04, M03, M02, M06, M06, M05, M05, M04, M02,
M01, M02`) and every text names its own mission's objective. A wrong mapping would show immediately.

**`.BMS` mission files** — layout derived and then confirmed by the scale calibration:

```
0x00 "BMS\r"   0x04 char[40] working title   0x2C char[40] author
0x54 char[16] terrain file   0x64 char[16] dialogue set
0x10C  object array, 96 B (24 × int32) per record, terminated by an all-zero record:
       +0x00 item type id   +0x08 instance id
       +0x10 x (east)   +0x14 y (north)   +0x18 z — all int32 fixed point 16.16, unit = METRE
       +0x20, +0x24 two per-unit scalars   +0x28 constant 100
       +0x34 heading, degrees 0…360        +0x48 group/chain word, 0x2D<node><chain><sub>
```

Worked example, Insurrection record 0: `x = −16 697 856 / 65536 = −254.79 m`,
`y = −19 924 992 / 65536 = −304.03 m`.

**The metre is proven, not assumed** — six checks in [`campaign.md`](campaign.md) §7, best three:
Alpha at "approximately 130 meters" measures 128.8 m (error 1 m); "about 240 metres west" measures
232.4 m; "690m northwest" measures 692.7 m (error 3 m).

**The axis convention is proven six times over**: every mission's opening radio line in `DFDLG02.BIN`
names a direction, and all six agree with the measured insertion-point→objective bearing
([`campaign.md`](campaign.md) §7).

### 3. Source list

**Game data (rank 1)**

1. `DELTAFORCE.iso` — <https://archive.org/details/DELTAFORCE>. Files read:
   `DFBASE.PFF` → `DFCAMPS.BIN`, `DFCAMP02.BIN`, `DFGAME.BIN`, `DFDLG02.BIN`,
   `C02M01…C02M06.BMS`, `C2M1SM…C2M6SM.PCX`;
   `DF.PFF` → `C2M1…C2M6.TRN`, `C2M1N/C2M3N/C2M4N/C2M6N.TRN`, `DFG1_D…DFG4_D.PCX`;
   `DF.EXE`; `/MANUALS/*`; `/README.TXT`.

**Manual (rank 2)**

2. *Delta Force Field Manual* FM 365-7 + *Manual Addendum* FM 365-7A, NovaLogic 1998, 30 PDF pages —
   <https://cdn.cloudflare.steamstatic.com/steam/apps/32620/manuals/DFMANUAL_EN.pdf>.
   It is a **combination of the two PDFs that ship on the disc** — `/MANUALS/DFMANUAL.PDF`
   (Addendum, 5 pp., 1998-10-13) and `/MANUALS/DFMAN.PDF` (Field Manual, 25 pp., 2000-06-06) — not a
   copy of either: measured page alignment is **PDF 1 blank · PDF 2–6 = the Addendum's 5 pages,
   printed 1–9 · PDF 7–30 = the Field Manual's pages 2–25, printed 1–24** (its blank cover dropped).
   The mapping is not a constant offset: PDF 18 is a **spread** carrying printed 12–13, so printed =
   PDF − 6 before it and PDF − 5 after. All page numbers cited in these files were taken from the
   printed footers, not computed.
   Pages used: Addendum printed **3–4** (PDF 3, HUD diagram), **5–6** (PDF 4, extra keys, co-op),
   **7** (PDF 5, scoring); Field Manual printed **5–7** (PDF 11–13, settings), **8–9** (PDF 14–15,
   gear), **10–11** (PDF 16–17, controls), **12–13** (PDF 18, keyboard layout), **14–15**
   (PDF 19–20, game screen + GPS legend), **16** (PDF 21, campaign selection).
   Pages 3, 19, 20, 21 were rendered to PNG and **read as images**; everything else from the text layer.

**Screenshots (rank 3)**

3. Insurrection, full HUD, 1280×720 —
   <https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/32620/ss_9703197cd01bdf0ab5941707dd88262d6ac428eb.1920x1080.jpg>
4. Green observation view with `Heading: 302` / `Distance:` —
   <https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/32620/0000008795.1920x1080.jpg>
5. Store page — <https://store.steampowered.com/app/32620/>

**Real-world (rank 4)** — used only in [`terrain.md`](terrain.md)

6. Relief: **Open Topo Data**, SRTM 30 m — <https://api.opentopodata.org/v1/srtm30m>. Every box in
   §3 and §5 of `terrain.md` was sampled on a 10×10 grid; the published min/max/relief/median are
   those samples.
7. Coordinates, from the Wikipedia coordinates API:
   Tocache −8.18889, −76.51389 · Tocache Airport −8.19528, −76.52917 ·
   Aguaytía −9.03694, −75.50750 · Tingo María −9.29528, −75.99750 ·
   Monzón District −9.27960, −76.39600 · Huánuco −9.92946, −76.23971 · Pucallpa −8.38333, −74.55000 ·
   Cusco −13.51690, −71.97860 · Chiclayo −6.77167, −79.83833 · Quillabamba −12.86806, −72.69306 ·
   Echarate District −12.76760, −72.57690 · Putumayo River −3.13500, −67.97417.
8. San Antonio del Estrecho (on the Peru–Colombia border) 2.44833 °S, 72.66833 °W —
   <https://en.wikipedia.org/wiki/San_Antonio_del_Estrecho>
9. Operation Snowcap (1987–1995) and the Santa Lucía base with its airstrip in the Upper Huallaga —
   <https://en.wikipedia.org/wiki/Operation_Snowcap> ·
   <https://www.dea.gov/sites/default/files/1994-1998%20p%2076-91.pdf>

**Prior research pass (checked, not trusted)**

10. `scratchpad/novalogic/delta-force.md`, 2026-08-05 — outside this repository. Its structural
    findings that this run **reproduced independently** are listed in §4; its errors in §5.

### 4. What the prior pass got right and this run confirmed

Reproduced from the disc, by re-deriving the formats rather than trusting the description:

- PERU is campaign **one** in the menu and **`C02`** internally; there is no `C01`; 6 + 6 + 8 + 10 + 10
  = 40 single-player missions.
- The six mission names, working titles, author (Steve McNally) and terrain assignments.
- The eleven *Goals* strings and therefore the extraction-goal split (Weatherman, Bad Habit,
  Masquerade have one; Insurrection, Flood, Headhunter do not).
- The six briefings verbatim, the `{CV:n}` / `{ADD}` / `{MnLk}` markup, the unlock chain from the
  Win texts.
- The measured mission footprints (1052×771 … 1612×2037 m) and insertion points, to the decimal.
- The object inventory per mission, including `tower, guard` ×1 in Insurrection, the `C130` in Flood,
  the druglord as unique type 5025, 6× 2010 + 3× 2015 in the convoy, 73 + 120 palms in Bad Habit.
- Item-name table limits: `DFGAME.BIN` names 79 low IDs; `ITEMS.DEF` is encrypted (entropy 7.994/8).
- The `.TRN` parser key list in `DF.EXE`, and therefore that **no terrain scale key exists** in DF1 —
  the "Terrain Scale Bits 19=192ft/20=384ft/21=768ft" table belongs to a later NovaLogic generation.
- 1024×1024 8-bit heightfields that tile seamlessly (wrap difference ≈ neighbour difference).

### 5. What the prior pass got wrong — refuted by measurement in this run

| Prior claim | Measurement | Verdict |
|---|---|---|
| Bad Habit's convoy route is *"ein KI-Wegpunktzug (Typ 6005, Gruppe 4, **15 Knoten**)"*, 1891 m | there are **three** parallel chains of **12** nodes each, 1891 / 1853 / 1924 m — one lane per convoy file. Its own coordinate list had 12 entries | **wrong on both count and structure** |
| Weatherman's objective is *"ein Gebäudehaufen aus **exakt 9 Häusern (Typ 3016)**"* | the village cluster holds **9 building objects: 8× type 3016 + 1× type 3047**. The mission's ninth 3016 stands in the abandoned farm 400 m away | **wrong**; the count is right for the cluster, the type is not |
| Headhunter's villa is *"113 × 121 m"* | x 201.8…315.2, y −293.0…−178.9 → **113.5 × 114.1 m** | wrong in one dimension by 7 m |
| Duplicate person records are alternate spawns and *"tragen im Feld +0x1C unterschiedliche Werte"* | field +0x1C is **constant (= 4) for every person in five of the six missions**, so it cannot select anything; and the co-located records are **2–4 m apart** — two men in one post, not two versions of one man. The stated "62 records in Insurrection" also mixes 58 hostiles with 4 friendly Squad Members | **wrong**, and the mechanism it proposed does not exist |
| Peru has *"Fluss mit **Brücken**, Uferzonen, Furten — Missionen 1, 2, 4"* | **zero** bridge objects (`bridge 1a` 3001, `bridge 1b` 3002) in all six missions; and `water_height` is 0 in five of six while the lowest terrain byte on those heightfields is 7–15, i.e. **above** the water plane — **no water surface can appear anywhere on them**. Only Flood has water | **wrong**; taken from a walkthrough and never checked |
| The `.TRN` table lists Masquerade and Bad Habit as ordinary missions; night is mentioned only as *"Nachtvarianten existieren für einen Teil der Missionen"* | Masquerade and Bad Habit **ship with the night preset** (`grengrad` / `filter 80,160,30` / `saturation 0`), and are precisely the two missions for which **no separate `N` variant exists** — because they already are the night version. Headhunter runs a **sunset** palette (`snsgrad7`, `sun_slope 20`), confirmed by the orange sky of the manual's own screenshot | **missed**, and it changes two missions' character |
| Cuzco and the Colombian border are *"zwei harte Anker"* that strengthen the bounding boxes | the boxes it published are in the Alto Huallaga, 700–1250 km from those anchors — the file asserts both and reconciles neither. Measured here: the border site has **18 m** of relief across a whole mission footprint and the Cusco sites have **580–693 m** per 1.4 km. Neither can host its own mission | **internally inconsistent**, and both anchors fail on measurement |

Two further prior statements are **narrowed** rather than refuted: the grid-coordinate examples are
`I13` (screenshot) and `M13` (manual) — `F6` does not appear in either; and the green observation view
is identified here as the **binocular** mode, from the `Heading:` / `Distance:` / `Distance: 1km+`
strings in `DFGAME.BIN` plus the absence of any night-vision key, which the prior pass left open.

### 6. Contradictions left standing

| Where | Contradiction |
|---|---|
| Inside the game data | Flood's campaign-screen statement promises *"Capture all vehicles, equipment, and contraband"*; its only goal is *"Eliminate all enemy soldiers at Objective Rain."* |
| Inside the game data | the four named real places (Colombian border, Cuzco ×2, Chiclayo) are up to 1250 km apart and cannot be one Special Operations Area |
| Inside the manual | *Game Screen* text puts the GPS map on F9/F10 (both Addendum p.3 and FM p.14); the key reference p.11 and the keyboard drawing pp.12–13 put F9 on the Forward Observer and the GPS maps on F10/F11 |
| Between briefing and object layout | Insurrection's briefing puts the watch station *"on the ridge north of the ranch"*; the mission's only `tower, guard` sits at (−370.6, −373.2), **south** of both building groups and 322 m SSW of the insertion point |
| Between briefing and object layout | Headhunter's briefing inserts Alpha *"500m west"* and Charlie *"500m northwest"* of the villa; measured 209 m west and 165 m north-west |
| Manual vs game data | the gear screen offers five primary weapons; `DFGAME.BIN` carries names for 39, including Shotgun, Stinger, RPG, AK-47, Garrote and Smoke Grenade |

## State

**Nothing built.** These four files are the whole of `mods/delta-force/`.

## Gaps

- **The pressing was not verified against the 1998 original.** §2 records that the disc read is a
  post-1999 re-release. If NovaLogic changed mission data between pressings, every measurement here
  is of the later data.
- **`ITEMS.DEF` is encrypted**, so most of PERU's object types have a class but no model name:
  the six missions use **84 distinct type IDs, of which the shipped name table covers 16** — measured.
  The 68 unnamed ones include every soldier type, both convoy vehicle types and all 32 villa parts
  (details in [`campaign.md`](campaign.md) `## Gaps`).
- **The `.BMS` tail is unparsed.** 31 244 B follow the object array in Insurrection alone; it holds
  index lists (groups of object indices) and is very probably where the player waypoint chain lives.
  Not decoded.
- **`DFDLG02.DBF` and `DFDLG02.PWF` were not read** — 11 536 B and 5.4 MB, presumably the audio index
  and the speech for the radio lines in [`campaign.md`](campaign.md) §5. The text is in hand; the audio
  is not, and [`doc/mods.md`](../../../doc/mods.md) §3's pool would want both as `hash → bytes`.
- **The other four campaigns were not touched.** `DFCAMP03…06.BIN` and 34 further `.BMS` files were
  located but not parsed; only the campaign-screen one-liners for all 40 missions were read.
- **No walkthrough or fan source was consulted at all.** That was unnecessary here, but it also means
  nothing cross-checks the game data from outside. If a claim above is wrong, it is wrong because the
  parse is wrong, and nothing in this file would catch it.
