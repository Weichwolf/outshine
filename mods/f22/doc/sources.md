# F-22 Lightning II — sources, and what each one is worth

> **Source document:** research distillation
> `scratchpad/novalogic/f22.md`, §0 (l. 3–39), §1 (l. 42–81), §10 (l. 531–549), §11 (l. 553–593).
> Extended in this run by a direct retrieval of the manual's full text (§3).
> Form per [`doc/mods.md`](../../../doc/mods.md) §3: *every claim with URL + page; manual-vs-wiki
> contradictions kept, not smoothed*.

## Spec

### 1. Source classes, ranked by what they can settle

| Class | Source | Settles | Cannot settle |
|---|---|---|---|
| **RES** | `RESOURCE.RES`, the game's data archive — 32,869,830 bytes, 1,067 entries, decrypted and parsed **by the source research** | mission names, briefings, object coordinates, per-mission unit inventory. **Strongest, because it is measurable** | anything about presentation, and anything in the unparsed 28 bytes per object record |
| **MAN** | the printed manual | HUD and cockpit — it **names** the elements in labelled diagrams; platform and enemy inventory; campaign prose | mission-level facts: it lists five campaigns without breakdown |
| **PATCH** | `F22PATCH.ZIP` → `INSTALL.BIN` | briefing texts (a subset of RES), errata, campaign 5 | — |
| **SHOT** | screenshots | cross-check on the HUD; the virtual-cockpit framing the manual never mentions | anything quantitative |
| — | a Let's Play with a documented mission order | — | **none found**, §5 |

### 2. Primary — game data

| # | Item | URL | Notes |
|---|---|---|---|
| 1 | `RESOURCE.RES` from the DOS release | <https://archive.org/details/msdos_F-22_Lightning_II_1996> | obtained by the source research via archive.org's zip viewer. **Not fetched in this run** — every `[ORF]` fact in these files is second-hand |
| 2 | Patch v1.01.00.18 | <https://archive.org/details/F22PATCH> | briefing texts, errata, campaign 5, waypoint numbering, the "Nanchuka" spelling correction |

**Archive format, as documented by the source research** (reproducible, and the reason RES outranks
everything else):

| Step | Finding |
|---|---|
| header | magic `RESOURCE2xxx` (12 bytes); dword @0x0C = entry count = **1067** |
| index | from 0x10, **20 bytes per entry** = 12-byte name + `uint32` offset + `uint32` length; index ends 0x536C, first payload 0x5380; all 1067 offsets inside the file |
| name obfuscation | the 12 name bytes are **XOR with the 4-byte key `AD DE ED AC`**, repeating. Derived, not guessed: three consecutive entries differed only in the fourth byte by +1 (`9D 9E 9F`), and `'1'^'2' = 0x03 = 0x9D^0x9E`; the padding after short names showed period 4 and thus the key. Check: `9F EC BA E5 E3 98 A0 82 E0 97 A9 AC` → `22WINFM.MID` |
| content | 636 `.PCX` · 144 `.WAV` · 89 `.PAK` (3D models) · **49 `.ORF`** · 49 `.REF` · 44 `.MID` · 42 `.TXT` · 8 `.BMP` · 4 `.BIN` · 1 `.MSN` |
| mission files | `C<KK>M<MM>.ORF` (objects + text), `.REF` (each exactly 24,576 bytes, **contents not examined**), `.TXT` (briefing). Counts: C01=8 · C02=7 · C03=8 · C04=8 · C05=5 = 36 |
| object table | after the asset name list, a `uint32` object count, then **40-byte records**; the first 12 bytes are three `int32` = **(X east, Y altitude, Z north)**. Record length taken from the constant 0x28 stride between recurring coordinate triples. Ground objects carry Y = −2; flights carry 2,500 / 5,000 / 10,000 / 20,000 |
| **bytes 12–39** | **not decoded** — carry type, squadron assignment and behaviour |

### 3. Primary — the manual

**F-22 Lightning II (1996) PC Manual, NovaLogic**
· item <https://archive.org/details/f-22-lightning-2-pc-manual>
· full text <https://archive.org/stream/f-22-lightning-2-pc-manual/F-22_Lightning_2_Manual_djvu.txt>

**Retrieved and searched directly in this run.** Operational note: the `/download/…_djvu.txt` URL the
source research cites returned **HTTP 500**; the `/stream/…` URL served the same text inside HTML and
worked (≈ 346 kB after tag stripping).

**Page-number confidence is not uniform, and this is the single most important caveat about MAN.** The
source research took page numbers from the table of contents (pp. i–ii) and confirmed exactly one
against a surviving OCR page footer. This run confirmed three:

| Page | Confirmed by footer? | Content |
|---|---|---|
| **30** | **yes** | HUD symbology, first diagram, "Effective range with guns is 1.5 nautical miles" |
| **74** | **yes** | end of the chaff/flares section |
| **85** | **yes** | end of the M61A2 section, "effective range of 0.5 km… fall-off 3 kilometres" |
| 5 | no — TOC | platform spec block |
| 31, 32 | no — TOC | second HUD diagram, ILS, targeting example |
| 35–39 | no — TOC. **No footer survived OCR anywhere in that region** | the seven MFD pages |
| 44–46 | no — TOC | the four campaigns' prose |
| 71–74 | partial (74) | AN/ALR-94, chaff, flares |
| 75–79, 80–83, 84–85, 86–90, 91–92, 93–95, 96, 97 | no — TOC | friendly forces, own weapons, enemy aircraft, enemy AAM, SAM, AAA, ships |

Notation used in the other three files: `p.N` = footer-confirmed, **`p.N*` = TOC only**.

**Verbatim passages relied on** (all read in this run, all quoted in the file named):

| Passage | Used in |
|---|---|
| "2. Campaign One: Gambling on the Mekong… the verdant hills bordering Laos, Myanmar (formerly Burma) and Thailand… the Golden Triangle" | [`terrain.md`](terrain.md) §5.1, [`campaign.md`](campaign.md) §1 |
| "the United Nations Security Council voted 15-0 (two abstaining)" | `campaign.md` §1 |
| "five training and thirty-one combat missions" · "four separate combat campaigns (with 31 missions)" | `campaign.md` §1 |
| the whole "1. HUD Symbology" section and both diagram callout blocks | [`hud.md`](hud.md) §2–§4 |
| "The ILS system is only activated when you are within six miles of the runway and less than 5,000 ft. AGL" | `hud.md` §5 |
| "even at your highest degree of stealth, a SAM radar can still detect your F-22 at a range of 4 miles" | `hud.md` §8 |
| "AIM-1 20s require radar guidance" [sic — OCR of AIM-120s] | `hud.md` §10 |
| "The F-22 carries 100 bundles of chaff" · "Your F-22 only carries 100 flares" | `campaign.md` §7 |
| "comes with a 480 round magazine… rate of fire approaching 6,000 rds. per minute" | `campaign.md` §7 |
| the Stores/Attack/Defense/Navigation/HUD-Repeater display sections | `hud.md` §6–§9 |
| the avionics and wingman keyboard reference | `hud.md` §11, `campaign.md` §10 |
| the platform spec block ("Max speed (sea level): 800 knots" etc.) | `campaign.md` §7 |

### 4. Contradictions — kept, not smoothed

| # | The two statements | Where | Status |
|---|---|---|---|
| 1 | Gun effective range **1.5 nautical miles** vs **0.5 km**, max fall-off 3 km | MAN p.30 (footer) vs MAN p.85 (footer) | **Unresolved.** Factor 5.6 inside one manual, both pages footer-confirmed so it is not a page-number error. Needs a measurement in the game; none made |
| 2 | The same MFD page is the **"Defense Display"** (display chapter) and the **"Threat Display"** (keyboard chapter) | MAN | Naming inconsistency in the manual. `hud.md` uses "Defense Display" and records the other |
| 3 | Boresight key printed as **`L`** in the display chapter and **`'`** in the keyboard reference | MAN | Keyboard reference preferred — it OCR'd cleanly, the display chapter's key labels are visibly garbled throughout. **A judgement, flagged as one** |
| 4 | Attack Display on **keypad 4** (display chapter), **keypad 5** (tutorial text), **keypad 6** (keyboard reference) | MAN | Same cause and same resolution as #3 |
| 5 | Base is **"south of Chiang Rai"** (180°) vs **"Chiang Rai 70NM NW of our base"** (back-bearing 135°) | two briefings, RES | **Resolved by measurement: 155.5°.** Both prose statements are imprecise; neither was choosable. See `terrain.md` §2.2 |
| 6 | **"Objective Talbot"** is an assembly house at 45.6 NM / 39.4° and a command centre at 87.0 NM / 17.6° | RES, missions 1.2 and 1.8 | **Not an error to resolve — the game issues one codename twice**, 88.3 km apart. `campaign.md` §6 |
| 7 | Ship class spelled **"Nanchuka"** | MAN p.97* | Manual error, **corrected in the patch readme** to Nanuchka |
| 8 | Scale concluded as **5 m/unit** but all coordinates computed with **4.989 m/unit** | the source research, §1 vs §4 | **Unresolved and carried.** 0.27 km at anchor distance. `terrain.md` §2.3 |
| 9 | ILS deviation sense given **only for "too far left"**, twice, for both vertical lines | MAN p.31* | Manual defect. Symmetry not inferred. `hud.md` §5 |
| 10 | **EF2000** appears in `EF2000.PAK` and in the briefings but has **no entry** in the manual's enemy aircraft chapter | RES vs MAN p.86–90* | Data wins; the manual is incomplete |
| 11 | "six campaigns, 46 missions", widely repeated | secondary web sources | **Refuted** by the archive index (36 with patch, 31 shipped) *and* by the manual's own "thirty-one combat missions" |

### 5. What was looked for and not found

Carried forward unchanged from the source research, plus this run's additions.

| Item | Status |
|---|---|
| A mission list **in the manual** | **Demonstrably absent.** Its "B. Campaigns" section has five entries with no breakdown; full-text search for `Objective Madison`, `Talbot`, `Storm Squadron`, `Chiang`, `Silkworm` returns nothing in a mission context |
| A Let's Play with a documented mission order | **Not found.** Four candidates checked (<https://www.youtube.com/watch?v=IpiWTaZUltg> · <https://www.youtube.com/watch?v=jcfeBaWoCR8> · <https://www.youtube.com/watch?v=ybJAm8xiGek> · <https://www.youtube.com/watch?v=rL0ZqoTFL10>); the first served neither description nor chapter marks; **the videos were not watched** |
| Contents of the 49 `.REF` files | **Not examined** — each exactly 24,576 bytes, suspected terrain-tile or waypoint data |
| The voxel heightfield | **Not recovered** — it is not in `RESOURCE.RES`; `.PCX` and `.PAK` were not decoded |
| Bytes 12–39 of each object record | **Not decoded** |
| Map rotation against north | **Assumption, not proof** — supported by eight bearings landing in the right octant |
| Real positions of FOB Tyler, Objective Madison, Objective Talbot | fictional; located only by the reconstruction in `terrain.md` §2–3 |
| The second string in each `.ORF` ("and Robbery", "kin'", "live") | **Meaning unclear**, uninterpreted |
| IRST · laser designator · a **named** datalink · IFF | **Absent from the F-22 in this game** — full-text search of the whole manual, no hit. AWACS supplies a "downlink" but no system is named. Helmet sight exists only on the enemy MiG-29 |
| Bangkok coordinate | **not obtained** |
| Nellis AFB coordinates | **not researched** — training area, not campaign one |
| MobyGames page | **HTTP 403** |
| The two HUD diagram pages as **images** | **Not read.** Only OCR text was retrieved in this run; the source research also worked from full text, not the 401 MB PDF. This is the cheapest remaining improvement to `hud.md` |
| A border dataset to place target points by country | **not consulted**, either run |
| A DEM check anywhere inside the campaign box | **not performed**, either run |

### 6. Secondary and reference

| Item | URL | Used for |
|---|---|---|
| F-22 Lightning 3 manual | <https://archive.org/stream/F22_3_MANUAL/F22_3_MANUAL_djvu.txt> | negative result: names neither campaigns nor missions — one reason campaign one of the 1996 title was chosen |
| F-22 Total Air War manual | <https://archive.org/download/F-22_Total_Air_War_Manual/F-22_Total_Air_War_Manual_djvu.txt> | **not this game.** TAW is Digital Image Design / Infogrames UK, not NovaLogic, and has no fixed mission list. Recorded so the confusion is not repeated |
| F-22 ADF / TAW strategy guide | <https://archive.org/details/F-22_Air_Dominance_Fighter_and_Total_Air_War_Strategy_Guide> | **not evaluated** |
| Series identification | <https://en.wikipedia.org/wiki/F-22_Lightning_II> · <https://en.wikipedia.org/wiki/F-22_Raptor_(video_game)> · <https://en.wikipedia.org/wiki/F-22_Total_Air_War> · <https://steamcommunity.com/app/32730> | the NovaLogic line is 1996 / 1997 / 1999; TAW is not in it |
| lilura1 blog | <https://lilura1.blogspot.com/2022/03/F-22-Lightning-2-IBM-PC-MS-DOS-1996-NovaLogic.html> | background only, nothing load-bearing |

**Screenshots** — the HUD cross-check, examined by the source research:

| URL | Use |
|---|---|
| <https://www.myabandonware.com/media/screenshots/f/f-22-lightning-ii-fgu/f-22-lightning-ii_2.png> | **the HUD shot**, 640×480, in-cockpit on the runway. Every manual callout was found again on it: compass band `340 · N · 20`, `WAYPOINT 1 / FOLLOW ROUTE / RANGE: 15.54`, speed band left, altitude band right, pitch ladder `10`, ASE circle, flight path indicator, `THR/G/FUEL/AIRFRAME`, `SHOOT LIST`, `6 AIM-120 AMRAAM`, `GEAR FLAPS BRAKE RADAR`, radar inset upper right. **No contradiction.** Additionally visible and **not labelled in the manual**: black canopy/frame struts left and right, and a blue message line `CLEARED FOR TAKEOFF` |
| <https://www.myabandonware.com/media/screenshots/f/f-22-lightning-ii-fgu/f-22-lightning-ii_1.png> · <https://archive.org/download/msdos_F-22_Lightning_II_1996/screenshot_00.jpg> · <https://www.myabandonware.com/media/screenshots/f/f-22-lightning-ii-fgu/f-22-lightning-ii_3.png> · <https://www.myabandonware.com/media/screenshots/f/f-22-lightning-ii-fgu/f-22-lightning-ii_4.png> · <https://www.myabandonware.com/game/f-22-lightning-ii-a50> | menu and external views |

**Geography** — every coordinate in [`terrain.md`](terrain.md):

| Place | URL |
|---|---|
| Chiang Rai · Chiang Rai International Airport | <https://en.wikipedia.org/wiki/Chiang_Rai> · <https://en.wikipedia.org/wiki/Chiang_Rai_International_Airport> |
| Chiang Mai · Chiang Mai International Airport | <https://en.wikipedia.org/wiki/Chiang_Mai> · <https://en.wikipedia.org/wiki/Chiang_Mai_International_Airport> |
| **Nan Nakhon Airport (VTCN)** — added in this run, `terrain.md` §5.3 | <https://en.wikipedia.org/wiki/Nan_Nakhon_Airport> |
| Golden Triangle · Ban Sop Ruak | <https://en.wikipedia.org/wiki/Golden_Triangle_(Southeast_Asia)> · <https://en.wikipedia.org/wiki/Ban_Sop_Ruak> |
| **Pak Beng** — added in this run, the Mekong reference point in `terrain.md` §6 | <https://latitude.to/articles-by-country/la/lao/132211/pakbeng> |
| Kunming | <https://latitudelongitude.org/cn/kunming/> |

### 7. Provenance rule for these four files

`CLAUDE.md`: *every number carries its origin — derived (with formula), measured (with measurement), or
`[SET]`.* Applied here as four tags, used consistently across `campaign.md`, `terrain.md` and `hud.md`:

| Tag | Origin | Trust |
|---|---|---|
| `[ORF]` / `[TXT]` | measured from `RESOURCE.RES` **by the source research** | **second-hand.** Consistency re-verified (`terrain.md` §2.4); the byte-level read was not |
| `[MAN v]` / `[MAN p.N]` | verbatim from the manual, footer-confirmed page | first-hand, this run |
| `[MAN p.N*]` | manual, **page from the TOC only** | first-hand text, unconfirmed page |
| `[DERIV]` | computed here, formula printed at the point of use | first-hand |
| `[SHOT]` | screenshot, source research | second-hand, non-quantitative |

## State

**Nothing built.** These four documents are the whole of `mods/f22/`.

## Gaps

- **The strongest source was not touched in this run.** `RESOURCE.RES` is the only thing that can settle
  a mission-level question, and every `[ORF]` fact here is relayed. Re-parsing it is the first task of
  any rebuild, and it also re-tests the source research rather than trusting it.
- **Page numbers 35–39 stay unconfirmed** and carry the entire MFD chapter.
- **The HUD diagram pages were never seen as images** — §5. Everything positional in `hud.md` §3 is
  inference from OCR reading order.
- **Two key-binding conflicts were resolved by judgement**, not by evidence (§4 #3, #4).
- **Contradiction #1 (gun range) blocks a real design decision** and cannot be closed from documents at
  all; it needs the game.
- **No source at all describes enemy behaviour** — engagement triggers, CAP logic, scripted friendly
  routes. Neither the manual nor the parsed part of the archive says anything, and the reading rules in
  `campaign.md` §3 depend on it.
