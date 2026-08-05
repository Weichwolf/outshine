# Armored Fist — sources

> Form per [`doc/mods.md`](../../../doc/mods.md) §3. Every claim in
> [`campaign.md`](campaign.md), [`terrain.md`](terrain.md) and [`hud.md`](hud.md) resolves to a row
> here. Contradictions between sources are kept in the topic files, not reconciled here.

---

## Spec

### 1. Source ranking used

Per the brief: **manual PDF > game files > let's play with a visible mission sequence > fan wiki >
MobyGames.** In practice the ranking inverted for two classes of fact and the reason is worth
recording: **the game files are unobfuscated and therefore stronger than any prose for anything
numeric** (mission time limits, terrain assignment, platoon slots, waypoint routes), while the
**manual and the strategy guide are the only sources for anything named** (mission names in English,
campaign order, standing-order semantics, display element names). Neither replaces the other.

| Class | Source | Weight in these files |
|---|---|---|
| **CD** | the retail CD-ROM image, unencrypted, plain filenames | **strongest for numbers.** All `[FSG]`, `[FSW]`, `[FSE]` claims |
| **EN** | the **English electronic manual**, 149 pp. (§4b) | **strongest for element names, controls and mechanics.** All `[EN]` claims |
| **GUIDE** | *Armored Fist: The Official Strategy Guide*, Ed Dille, Prima 1994 | **strongest for orders of battle and AI semantics.** Also the only reproduction of a campaign map found |
| **MAN** | *Armored Fist Benutzerhandbuch*, German Softgold re-release, 86 pp. | **strongest for the labelled console diagrams** (Appendix A), which the English manual does not carry in the same form |
| **SHOT** | retail screenshots | cross-check only |
| — | let's play with a visible mission sequence | **not sought** — the CD gave the full sequence directly |
| — | fan wiki, MobyGames | **not used** for any claim |

---

### 2. Primary — the game

| Item | Value |
|---|---|
| archive.org item | `Armored_Fist_1994_Nova_Logic` — <https://archive.org/details/Armored_Fist_1994_Nova_Logic> |
| File | `Armored Fist (1994)(Nova Logic).iso` |
| Size | **48 273 408 bytes** |
| SHA-256 | `e8973f20400542963e2691529f2a21aab7910e884d50f2f6ada5488171695931` |
| Volume label | `ARMOREDFIST` |
| File dates | 1994‑10‑07 (data), 1994‑09‑13 / 1994‑10‑05 (installer) |
| Extracted with | `bsdtar -xf` |
| Layout | `FISTBASE/` (7 files: `FIST.RUN`, `FIST.DAT`, drivers, `LOADGAME.EXE`) · `FISTDATA/` (**392 files**) · `COPIER.EXE` · `CDINSTAL.BAT` · `LOADGAME.EXE` |

**No obfuscation.** Unlike the F‑22 title, nothing is XOR-veiled and there is no container archive:
filenames are plain ISO‑9660 entries and the briefing texts are plain CP437 with CRLF. The only
reverse engineering needed was the `.FSG` chunk format ([`campaign.md`](campaign.md) §7).

File census of `FISTDATA/` relevant to these files:

| Extension | Count | Content |
|---|---|---|
| `.FSG` | **51** | mission scenario files — 47 campaign + `DEMO0–3` |
| `.FSW` | **43** | Western briefing text, plain CP437 |
| `.FSE` | **43** | Eastern briefing text, plain CP437 |
| `.KLC` | 18 | voxel maps — 8 `C??`/`D??` pairs + `BURM_C2`/`BURM_D2` |
| `.MRL` | 18 | panel and screen bitmaps, incl. six campaign maps `AZER_ CYPR_ PAKI_ SAUD_ TURK_ UKRA_` — **not decoded** |
| `.M00/.M08/.M16/.M32/.MAL` | 34 each | vehicle and effect models at four LODs + palette |
| `.SKY` | 4 | `2` `5` `8` (day variants) and `7` (14 985 B — night) |
| `.DTL` | 3 | `LOW` `MEDIUM` `HIGH`, 2052 B each |
| `.BIN` | 16 | sounds + `MDFS.BIN`, the editor's map table |
| `.KDV` | 8 | full-motion video (intro, camp, win, lose) — **not examined** |
| `.CAM` `.MS3` | 7 / 10 | camera paths, music — **not examined** |

Files actually parsed in this run: all 51 `.FSG`, all 43 `.FSW`, spot-checked `.FSE`, `MDFS.BIN`,
`C30.KLC`/`D30.KLC` headers.

Also fetched and **not used**: `AF14CD.ZIP`, the v1.4 CD patch —
<https://archive.org/details/AF14CD_ZIP>. It contains only replacement executables and drivers
(`FIST.DAT`, `FIST.RUN`, `MGAVIDEO.DVR`, `SOUNDDVR.DVR`, `SOUNDSET.EXE`, `LOADGAME.EXE`,
`FIST_ALT.BAT`), **no readme and no data files**, so it changes nothing above.

`FIST.RUN` and `FIST.DAT` are compressed 32-bit DOS-extender binaries (Doug Huffman extender,
1991–94) and were **not** unpacked; no string table was recovered from them.

---

### 3. Primary — Armored Fist: The Official Strategy Guide

| Item | Value |
|---|---|
| Author / publisher | **Ed Dille**, Prima Publishing, 1994 |
| ISBN | 1‑55958‑761‑X · LCCN 94‑68402 |
| archive.org item | <https://archive.org/details/armored-fist-strategy-guide> |
| Used | `Armored_Fist_Strategy_Guide_djvu.txt` (269 002 B) for text; `Armored_Fist_Strategy_Guide.pdf` (11 524 897 B) rendered at 400–600 dpi for figures |
| Page numbering | printed page numbers, cross-checked via `..._page_numbers.json` (leaf → page) |

Pages cited:

| Page(s) | Content |
|---|---|
| vii–viii | contents — the six combat campaign names and all 47 mission names |
| 4–12 | vehicle primers and loadouts (M1A2, T‑80, M3, BMP‑2, Mi‑24, AH‑64) |
| 13–15 | *Top Secret Data* — auto turret control, gearshift, company identification, smoke grenades |
| 15–17 | damage model: ordnance precedence table, hits-to-kill table, **16 facing zones** and their damage multipliers |
| 18–19 | CCV, and the **non-omniscience** statement |
| 19–27 | CCV menu bar, Company Status, commander / advance / formation / speed semantics |
| 30–33 | edit-mode tool kit |
| **53** | **Figure 4‑1, the Overwatch campaign map** — the georeferencing source for [`terrain.md`](terrain.md) |
| 54–72 | Overwatch missions 1–7: orders of battle, debriefings, objective counts |
| 75–93 | Crossed Swords (used only for the "1000 metres" statement, p. 85) |
| 115 | Certain Fury opening — *"despite your successes in the previous three campaigns"* |
| 121, 137 | mission names that disagree with the game files |

**The guide is the source with the most internal contradictions** and they are recorded, not
smoothed: chapter order vs. the game menu, *Reforger* vs *Night Forger*, mission 2's night status,
two different missile-reload times, two mission names that no `.FSW` supports. Its *orders of
battle* have no independent check at all — `DCBS` is undecoded — and are therefore the least
verified numbers in these files.

OCR caveat: the `djvu.txt` mangles table column alignment. Every table reproduced from it is marked
where the alignment is ambiguous, and the two figures read for [`terrain.md`](terrain.md) and
[`campaign.md`](campaign.md) §4a were read **visually from a 600 dpi render**, not from OCR.

---

### 4. Primary — the manual (German)

| Item | Value |
|---|---|
| Title | *Armored Fist Benutzerhandbuch* |
| Publisher | German licence, **Softgold Computerspiele GmbH** (A Funsoft Company), © 1994 NovaLogic |
| archive.org item | <https://archive.org/details/armored-fist-1994-german-re-release-box-cd> |
| Used | `Armored Fist (1994)_manual-de_djvu.txt` (106 572 B) for text; `..._manual-de_text.pdf` (11 287 489 B) rendered at 400–600 dpi for the labelled diagrams |
| Extent | 91 leaves, printed pages 1–86 |

Pages cited:

| Page(s) | Content |
|---|---|
| 10 | *Über Armored Fist* — company = **1–4 platoons × 1–4 vehicles = up to 16**; VoxelSpace; the two sides' equipment |
| 26–32 | vehicle controls and displays: throttle/brake, steering, gearshift, speedometer, fuel, sight, turret control, azimuth, magnification, night devices, tactical map + full colour legend, threat display, weapon selection, weapon status |
| 33–36 | ammunition types, target acquisition, firing, smoke, auto/manual turret, engine smoke |
| **42–44** | **main menu and the `SELECT A CAMPAIGN` list** — the campaign-order source |
| 45–47 | Settings |
| **49** | campaign map: mission-box colour code, and a screenshot showing **Tbilisi / Rustavi** |
| 51 | mission orders screen |
| 52–62 | CCV, menu and status bar, floating tool kit |
| 55 | **mission goals must be destroyed to win**; goal marking |
| 63–67 | edit mode: mission time, terrain choice, base/mine/vehicle/tree/target placement |
| 68–71 | Company Status: CMDR, ADV, FORM, SPEED |
| **78–85** | **Appendix A — labelled diagrams** of M1A2, T‑80, M3, BMP‑2, external view, CCV map, menu/status bar, tool kit, company status |
| 86 | keyboard reference — **not transcribed** |

The German is quoted in the topic files wherever the wording is load-bearing. It is **not** the
primary manual — see §4b.

---

### 4b. Primary — the manual (English electronic edition)

**This source was found late in the run and refuted a claim these files had already made.** An
earlier state of [`hud.md`](hud.md) and of §7 below said *"no English manual for Armored Fist (1994)
was found"* and translated every display element out of the German. That was wrong.

| Item | Value |
|---|---|
| Title page | `ARMORED [FIST]™ · Armored Battlefield Simulator · USERS MANUAL` |
| Copyright | **© 1994 NovaLogic, Inc.**; trademarks *Armored Fist*, *Voxel Space* |
| Nature | the **on-line electronic manual** — hypertext, with T.O.C. and Index icons on most pages |
| Extent | **149 PDF pages**, printed pages 1–143 |
| PDF metadata | Title `ComCover.man` · Creator *Adobe PageMaker 6.0* · Producer *Acrobat Distiller 2.0 for Power Macintosh* · created 1996‑08‑08 |
| SHA-256 | `3c78452e89ff34ae7c16d1f1363bedb5607248359701dde7a160af82139f3590` |
| Size | 3 178 996 bytes |
| **Retrieval URL** | **unrecorded** — the file was already present in the shared working directory when this run began, and its origin could not be reconstructed. Identified here by hash and by intrinsic content. This is a real weakness of the citation and is stated rather than papered over. |

It is a **different document from the German print manual**, not a translation of it: 143 printed
pages against 86, with a `ARMORED COMBAT STRATEGIES` appendix (pp. 104–137), a key reference chart
(p. 138) and quick-reference screens (pp. 150–154) that the German edition does not have. The German
edition in turn carries the labelled Appendix A console diagrams in a form the English one does not.
**Both are needed.**

Pages cited:

| Page(s) | Content |
|---|---|
| 21–27 | mouse, keyboard shortcuts, joystick, input devices |
| 33–34 | Gear Shift, Speedometer, Fuel Gauge |
| 35–38 | **Viewport Display**, Viewport Slew, **Viewport / Turret Angle**, **Viewport Magnify**, **Night Vision** |
| 39–40 | **Tactical Map ("Spin Map")** incl. the full colour legend, magnification, `^` steering cues; **Threat Indicator Display** ("pie slice") |
| 41 | **Weapon Select**, **Weapon Status Indicator** |
| 42–44 | **Ordnance** — Sabot/APDS, HEAT, HEP, HE, MG, TOW/AT‑6, HEI, SAM, and each one's role |
| 44–47 | **Target Lock** (laser range finder), Manual Aiming, Firing, **Smoke Grenades** |
| 48–49 | **Auto / Manual Turret Control**, Turret Control Settings, **Engine Smoke** |
| 50–53 | CCV entry, Pause, **Air Support**, **Artillery Support** — incl. *every battle has air bases* and *up to three artillery bases* |
| 54–58 | Main Menu, Campaigns, Battles, Review, Settings |
| 59–63 | Settings menu |
| 64–65 | Campaign Map, Mission Orders, colour code |
| 66–78 | CCV, strategic map, menu and status bar, floating tool kit |
| 79–103 | edit mode, Company Status, enemy parameters |
| 115 | *"Overwatch/support by fire"* defined as a **mission type** — the first campaign is named after a doctrinal term |
| 104–137, 138, 150–154 | strategies appendix, key reference chart, quick-reference screens — **not read** |

---

### 5. Screenshots

| Use | URL | Read |
|---|---|---|
| **T‑80 console**, German build, 320×200 | <https://www.myabandonware.com/media/screenshots/a/armored-fist-2dm/armored-fist_12.png> | yes — ammunition counts, `VERBLEIBENDE ZIELE: 12`, `IIT`/`TRG`, `ENG SM`, `AIR`/`ART`, `MPH 39`, turret `000`, map heading `112°` |
| **Main menu** | <https://www.myabandonware.com/media/screenshots/a/armored-fist-2dm/armored-fist_2.png> | yes — menu item list confirmed |
| external view | <https://www.myabandonware.com/media/screenshots/a/armored-fist-2dm/armored-fist_13.png> | yes — no HUD content |
| game page (13 screenshots listed) | <https://www.myabandonware.com/game/armored-fist-2dm> | — |

Screenshots 3–11 returned HTML rather than PNG and were not obtained.

---

### 6. Geographic references

Used only in [`terrain.md`](terrain.md) §3, as anchors for the georeferencing fit.

| Place | Coordinate used | Role |
|---|---|---|
| Karachi | 24.8607 N, 67.0011 E | fit anchor |
| Hyderabad (Sindh) | 25.3960 N, 68.3578 E | fit anchor |
| Sanghar | 26.0470 N, 68.9490 E | fit anchor |
| Barmer | 25.7500 N, 71.3833 E | **independent check**, not in the fit |
| Dadu | 26.7319 N, 67.7750 E | **rejected** — inconsistent with the other three |

Coordinates are the standard published values for these settlements; they are used to a precision of
0.01°, which is an order of magnitude finer than the 0.083° worst-case residual of the fit, so their
exact provenance does not affect any result.

---

### 7. Searched and not found

| Sought | Result |
|---|---|
| ~~English manual, Armored Fist (1994)~~ | ~~not found~~ — **REFUTED within this run, see §4b.** Web search for it failed on archive.org (title and full-text), myabandonware, abandonia and freegameempire, which return only the *Armored Fist 2* manual (<https://archive.org/details/armoredfist2usermanual>), a different game. The English manual was then found **locally**, in the shared working directory, and the "not found" claim had already been written into two files before that. The lesson recorded, not softened: *a failed web search is evidence about the web, not about the artefact.* |
| **Let's play with a visible mission sequence** | **not sought.** The CD carries all 47 mission names, briefings and objectives directly; a video could add nothing that is not already measured |
| **Mission goal counts for 5 of 7 Overwatch missions** | **not recovered** — they live in the undecoded `DCBS` chunk |
| **`DCBS` record layout** | **not decoded.** Variable-length records; measured inter-coordinate strides in `SAUDI1` of 17, 28, 44, 61, 69, 97, 160, 257 bytes with no identified record header |
| **World unit → metre scale** | **not determined.** Three attempts documented in [`terrain.md`](terrain.md) §6 |
| **Loss condition** | **not stated** by either manual or the guide |
| ~~Air / artillery allocation per mission~~ | **partly answered by §4b**: every battle has air bases, a battle may have **up to three artillery bases**, both may be unavailable or delayed, a downed helicopter is gone for good, artillery rides on destructible trucks `[EN p.51–53]`. The **number of sorties an air base holds** remains unstated |
| **`.MRL` bitmap format** | **not decoded** — so no campaign map or console panel was read from the game's own art |
| **`.KLC` voxel compression** | **not decoded** beyond the `KLC1` header (magic + two uint32 dimensions) |
| **`.KDV` video** | **not examined** — 8 files, 41 MB, includes `WCAMP.KDV` (12 MB) which is presumably the Western campaign intro and may carry narrative |
| **`.REF`-equivalent / `PINF` enum orderings** | cardinalities measured, index→label **not** established |
| MobyGames page | **not consulted** for any claim |

---

### 8. Reproduction

Everything tagged `[FSG]` can be re-derived from the SHA-256 above with a chunk walker of about
twenty lines; the format is given in full in [`campaign.md`](campaign.md) §7.1–7.3. Everything tagged
`[GUIDE]` or `[MAN]` carries a printed page number. Everything tagged `[DERIV]` carries its formula
at the point of use.

**No game content was copied into `mods/`.** The CD image, the extracted `FISTDATA/`, the PDFs and
the screenshots were downloaded to a scratch directory and deleted after this run; only the
reconstruction in these four files remains.

### 8.1 What this run refuted

Three statements were written and then overturned by later evidence in the same run. They are listed
because a distillation that only records what survived is not checkable.

| Claimed | Corrected to | By what |
|---|---|---|
| *"No English manual for Armored Fist (1994) was found"* | **it exists, 149 pp., and is the authoritative source for every display element name** | §4b — found locally after the web search had failed |
| Air and artillery allocations *"not quantified by any source"* | **quantified**: air bases in every battle, up to three artillery bases, both destructible and exhaustible | `[EN p.51–53]` |
| Auto Turret Control *"re-centres the turret when a lock is lost"* (guide) | **it also actively tracks the locked target** while lock holds — the guide states only half of it | `[EN p.48]` |

## State

Nothing built.

## Gaps

- **Every order-of-battle number in [`campaign.md`](campaign.md) §3.1 rests on a single source** (the
  guide) with no data cross-check, because `DCBS` is undecoded. That is the weakest layer of these
  files and should be the first thing re-measured.
- **The English manual has no recorded retrieval URL** (§4b) — only a hash. Anyone re-deriving these
  files has to find it again.
- **Pages 104–149 of the English manual were not read** — the strategies appendix and the key
  reference chart. The latter would complete [`hud.md`](hud.md) §2's key table.
- **`WCAMP.KDV` and `ECAMP.KDV` were never opened** and are the most likely remaining source of
  campaign narrative in the shipped product.
- **Air/artillery detail lives in [`hud.md`](hud.md) §7a**, with [`campaign.md`](campaign.md) §5.5
  pointing at it rather than repeating it. If that split turns out to be the wrong cut, §7a is the
  one to move.
