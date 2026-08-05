# Asset inventory — the four mods, deduplicated

> What the four researched titles need in 3D, as **one list, built once each**, ordered easy → hard so
> the list is a work plan and not a sorting. Sources: `mods/{f22,comanche,armored-fist,delta-force}/doc/`
> (`campaign.md`, `terrain.md`, `hud.md`). Rules under which each is built:
> [`assets.md`](assets.md) — **version the recipe, never the cake**; the F-16
> (`mods/f16/src/models/`) is the one worked example.

Nothing here is invented. Where a title places an object whose type its own data never names, the entry
says so in the source column and the modeller is told it must ask, not guess.

## Spec

### 0. How to read the columns

| Column | |
|---|---|
| **Name** | kebab-case, the directory name under `mods/f16/src/models/<name>/` |
| **Kind** | aircraft · rotorcraft · vehicle · tracked · ship · SAM · AAA · building · person · ordnance · scatter — plus **crew station** and **hand weapon**, two classes the four titles need that the aircraft taxonomy has no word for |
| **Used by** | `F22` · `CMN` (Comanche) · `AF` (Armored Fist) · `DF` (Delta Force) |
| **D** | difficulty, below |
| **Moving** | named joints. Node names are shared with physics ([`body-format.md`](body-format.md) §2) |
| **Sources** | `good` = dimensioned drawings or three-views obtainable · `thin` = photos and prose, no dimensioned drawing, or the *identity* is documented but the variant is not · `none` = the title's own data does not name the object; a modeller would have to invent it |

Difficulty, fixed meaning:

| D | |
|---|---|
| 1 | a box or a cylinder, no joint |
| 2 | a static structure, or a body with one joint |
| 3 | a wheeled vehicle, or a launcher with traverse and elevation |
| 4 | tracked vehicle, aircraft or rotorcraft — running gear, control surfaces, turret or rotor |
| 5 | an **inhabited** station: instruments that move under a camera that sits inside them, or a rigged human |

**One entry = one build.** Where two titles want the same object in different dress, the entry says which
parameter differs; it does not become two entries.

---

### 1. The list

#### 1.1 D1 — boxes and cylinders

| # | Name | Kind | Used by | D | Moving | Sources |
|---|---|---|---|---|---|---|
| 1 | `fuel-tank-cylindrical` | building | **F22 · CMN · AF · DF** | 1 | — | good. **The only object all four titles place.** F22 `FUELTANK` (base set, every mission) · CMN type 3, a mission goal in 4 of 10 · AF "fuel and propane tanks" at the refuel depot · DF `oil tank` 3008 |
| 2 | `crate-supply` | scatter | DF | 1 | — | none — Weatherman's goal is *"destroy the enemy crates"*, type unnamed |
| 3 | `barrel-oil` | scatter | DF | 1 | — | good — hostiles are placed behind them (`campaign.md` Gaps) |
| 4 | `tank-propane-bullet` | building | AF | 1 | — | good; distinct silhouette from #1, not a variant |
| 5 | `mine-anti-tank` | scatter | AF | 1 | — | good on the object, **none on placement** — `STMP` is a mine hypothesis with one counterexample (`terrain.md` §6.2) |
| 6 | `runway-strip` | building | F22 · DF · AF | 1 | — | good. F22 `RUNWAY` (1 780 m measured) · DF Flood needs a C-130 strip · AF every battle has air bases. **Probably a terrain decal, not a mesh** — decide before building |
| 7 | `tent-small` | building | DF | 1 | — | thin — 3011, no model name |

#### 1.2 D2 — one joint, or none and larger

| # | Name | Kind | Used by | D | Moving | Sources |
|---|---|---|---|---|---|---|
| 8 | `tent-long` | building | DF | 2 | — | thin — 3010 |
| 9 | `tent-netting` | building | DF | 2 | cloth sag | thin — 3009; also part of the Flood airfield group |
| 10 | `tree-palm` | scatter | DF | 2 | wind | good — two variants (`tree, palm1` 73×, `tree, palm2` 120× in Bad Habit alone). Build as L-system: [`assets.md`](assets.md) §3.1 tests the vegetation pipeline against closed-form values |
| 11 | `hut-village` | building | DF | 2 | door | none — types 3016/3017/3047 carry no names (`ITEMS.DEF` encrypted). Kit: Weatherman's 9-building village, Flood's 6, Insurrection's 3+4, Masquerade's 7+4 are all instances of it |
| 12 | `tower-guard` | building | DF | 2 | ladder | thin — 3014; load-bearing for Insurrection (*"the watch station on the ridge"*) |
| 13 | `hangar-aircraft` | building | F22 · DF | 2 | door | good. **One asset, two dressings**: F22 `HNG_03` at a military FOB, DF 3021 ×2 at a jungle airstrip. Differs in size and cladding, not in shape |
| 14 | `tower-control` | building | F22 | 2 | — | good — `TWR_02`; `TWR_02N` is the **night variant, a material state, not a second mesh** (3 of 8 missions are night or dusk) |
| 15 | `radar-search` | building | F22 | 2 | antenna rotates | thin — `RDAR_03`, mission 1.7; the campaign data never names the type |
| 16 | `satellite-dish` | building | AF | 2 | dish traverses | good — Night's Quest objective area |
| 17 | `comms-mast` | building | AF | 2 | — | thin — Corrosion's *"communications links"*, never described further |
| 18 | `bunker-reinforced` | building | AF | 2 | embrasure | thin — debriefing prose only; the `DCBS` object table is undecoded, so nothing in the data confirms it |
| 19 | `pillbox` | building | AF | 2 | — | thin — same status as #18 (`INDIA3`: *"saturated with mine fields and pillboxes"*) |
| 20 | `emplacement-artillery` | AAA | AF | 2 | barrel elevates | thin — a mission goal in 2 of 7 Overwatch missions; **the gun type is never named** |
| 21 | `assembly-house` | building | F22 | 2 | — | none — Objective Talbot, mission 1.2. Fictional |
| 22 | `command-centre` | building | F22 | 2 | — | none — Objective Talbot, mission 1.8. **The same codename as #21 at a point 88.3 km away** (`campaign.md` §6); a rebuild must decide per mission |
| 23 | `pyramid-stepped` | building | CMN | 2 | — | none — **painted into map 1's colour map, no geometry in the original**, and the game contradicts itself on it (Mayan architecture, Peruvian briefing). Needed only if the rebuild gives it geometry |
| 24 | `villa-compound` | building | DF | 2 | gates | none — Headhunter, **32 parts** (types 3048–3062), 113.5 × 114.1 m, walled with courtyard and car drive. A kit, not one mesh |
| 25 | `bridge-multispan` | building | F22 | 2 | **per-span damage** | good on bridges, none on this one. Mission 1.3's verdict is *every span* destroyed; a bridge that dies to one hit is a damage-model defect |

#### 1.3 D1–D2 — ordnance

| # | Name | Kind | Used by | D | Moving | Sources |
|---|---|---|---|---|---|---|
| 26 | `aim-120c` | ordnance | F22 | 2 | fins | good |
| 27 | `aim-9x` | ordnance | F22 | 2 | fins, rollerons | good — **variant of the existing `mods/f16/src/models/aim9.glb`**, not a new build from zero |
| 28 | `gbu-30-jdam` | ordnance | F22 | 2 | tail control fins | good — Mk 83 body + nose cap + tail guidance section, 2 per ventral bay |
| 29 | `r-27re` | ordnance | F22 | 2 | fins | good |
| 30 | `r-73` | ordnance | F22 | 2 | fins, vanes | **already built** — `mods/f16/src/models/r73.glb`. Zero work |
| 31 | `v-750-sa2-round` | ordnance | F22 | 2 | booster separates | good |
| 32 | `sa-9-round` | ordnance | F22 | 1 | — | thin — 2.6 kg warhead is all the manual gives |
| 33 | `sa-8-round` | ordnance | CMN | 1 | — | thin |
| 34 | `agm-114-hellfire` | ordnance | CMN | 2 | fins | good |
| 35 | `fim-92-stinger` | ordnance | CMN | 1 | — | good |
| 36 | `hydra-70` | ordnance | CMN | 1 | — | good — fired in spreads of 1 or 2, flechette |
| 37 | `9k121-vikhr` | ordnance | CMN | 2 | — | thin — the manual says *"Vikhr or Spiral"* and does not choose |
| 38 | `sa-19-aam` | ordnance | CMN | 2 | — | **thin, and the name is suspect** — the manual arms the Ka-50 with an "SA-19 AAM"; SA-19 is a surface-to-air system. Recorded as the manual states it |
| 39 | `at-8-songster` | ordnance | CMN | 1 | — | thin — fired through the T-80's 125 mm barrel; *the enemy tank shoots at helicopters* |
| 40 | `bgm-71-tow` | ordnance | AF | 2 | launcher box elevates | good |
| 41 | `at-5-spandrel` | ordnance | AF | 2 | — | good |
| 42 | `ss-n-9-siren` | ordnance | F22 | 2 | — | good — sits in the Nanuchka's two triple launchers; never fired at the player in campaign one |
| 43 | `silkworm` | ordnance | F22 | 2 | — | good — **cargo** in the An-225 of mission 1.4, never launched |

#### 1.4 D2–D3 — hand weapons and carried gear (Delta Force only)

The one title with a man on foot needs these; no other title places a human.

| # | Name | Kind | Used by | D | Moving | Sources |
|---|---|---|---|---|---|---|
| 44 | `satchel-charge` | hand weapon | DF | 1 | — | good — **Weatherman is unwinnable without it**; the loadout constraint is a radio line |
| 45 | `frag-grenade` | hand weapon | DF | 1 | pin, spoon | good — 6 always carried |
| 46 | `ka-bar` | hand weapon | DF | 1 | — | good |
| 47 | `m18-claymore` | hand weapon | DF | 1 | legs | good |
| 48 | `code-book` | scatter | DF | 1 | — | none — Masquerade's mission item; `Carrying: Code Book` |
| 49 | `laser-designator` | hand weapon | DF | 2 | — | thin |
| 50 | `law-m72` | hand weapon | DF | 2 | tube extends | good |
| 51 | `m40a1` | hand weapon | DF | 2 | bolt, 8× scope | good |
| 52 | `m4-m203` | hand weapon | DF | 3 | bolt, magazine, M203 tube, 4× scope | good |
| 53 | `m249-saw` | hand weapon | DF | 3 | bolt, belt, 200-round box | good |
| 54 | `mp5-sd` | hand weapon | DF | 3 | bolt, magazine | good |
| 55 | `barret-50` | hand weapon | DF | 3 | bolt, muzzle brake | good (the misspelling is the game's) |

#### 1.5 D3 — wheels, traverse, elevation

| # | Name | Kind | Used by | D | Moving | Sources |
|---|---|---|---|---|---|---|
| 56 | `truck-cargo` | vehicle | AF · DF | 3 | wheels, steering, tilt | thin in both, and **the merge is a decision, not a measurement**: AF says only *"artillery rides on trucks"*; DF's convoy is 6× type 2010 + 3× type 2015 with no model names. One generic 6×6 serves both only if the owner accepts it |
| 57 | `car-light-utility` | vehicle | DF | 3 | wheels | none — types 2009 / 2012, 2 per mission in Insurrection and Headhunter |
| 58 | `car-limousine` | vehicle | DF | 3 | wheels, doors | thin — visible in `[MAN p.14]`, no type name |
| 59 | `sa-2-launcher` | SAM | F22 | 3 | traverse, elevation, round leaves the rail | good — the single most-used SAM of the campaign (missions 1.2, 1.7, 1.8) |
| 60 | `sa-9-gaskin` | SAM | F22 | 3 | wheels, 4 canisters elevate | good — BRDM-2 hull; mission 1.8 only |
| 61 | `sa-8-gecko` | SAM | CMN | 3 | wheels, turret traverse, radar dish, 6 canisters | good on the vehicle. **Which of the game's `turr` / `radr` sprites draws it is unestablished** — one of the two is a second static object nobody has identified |
| 62 | `aaa-gun` | AAA | F22 | 3 | traverse, elevation, barrels | **none** — AAA appears in the briefings of 1.2 and 1.3 but **no AAA model is loaded by any `.ORF` of campaign one**. The manual names ZPU-4, S-60 and ZSU-23/4 without assigning any of them |

#### 1.6 D4 — tracked, winged, rotary, crewed by AI

| # | Name | Kind | Used by | D | Moving | Sources |
|---|---|---|---|---|---|---|
| 63 | `t-80` | tracked | **CMN · AF** | 4 | turret, gun elevation, tracks, hatches | good. **Two titles, one build.** CMN needs the exterior only (a goal object that shoots back); AF needs it drivable and adds a crew station (#89) |
| 64 | `m1a2-abrams` | tracked | AF | 4 | turret, gun, tracks, hatches | good |
| 65 | `bmp-2` | tracked | AF | 4 | turret, 30 mm, Spandrel rail, tracks | good |
| 66 | `m3-bradley` | tracked | AF | 4 | turret, 25 mm, TOW box folds, tracks | good |
| 67 | `mig-27` | aircraft | F22 | 4 | **swing wing**, gear, control surfaces | good — present in all 8 missions |
| 68 | `mig-29` | aircraft | F22 | 4 | gear, control surfaces, LERX doors | good — **`doc/` already carries two MiG-29A manuals**, the deepest reference in the tree after the F-16 |
| 69 | `ef2000` | aircraft | F22 | 4 | gear, canards, control surfaces | good on the real aircraft, **none in the game's own manual** — `EF2000.PAK` exists and the briefings name it; the enemy chapter has no entry |
| 70 | `f-15c` | aircraft | F22 | 4 | gear, control surfaces | good — base set, every mission |
| 71 | `f-22` | aircraft | F22 | 4 | gear, control surfaces, **weapon-bay doors** (opening raises the radar signature and is visible in real time), vectoring nozzles | good |
| 72 | `c-130` | aircraft | DF | 4 | props, ramp, gear | good — one, static, at the Flood airfield |
| 73 | `c-5-galaxy` | aircraft | F22 | 4 | gear (28 wheels), nose door, control surfaces | good — 4 of them, and mission 1.6's verdict is their survival |
| 74 | `b-1b` | aircraft | F22 | 4 | **swing wing**, gear, bay doors | good — mission 1.8's verdict hangs on it |
| 75 | `an-225` | aircraft | F22 | 4 | 32-wheel gear, 6 engines, control surfaces | good — 3 of them, the whole task of mission 1.4 |
| 76 | `awacs-unidentified` | aircraft | F22 | 4 | rotodome, gear | **thin, and the identity is the problem**: `AWACS.PAK` loads in 1.6; the manual lists a friendly E-767 *and* a hostile A-50 Mainstay, and the data does not decide |
| 77 | `aircraft-extraction` | aircraft | DF | 4 | unknown | **none** — type 2021, present in exactly the 3 missions with an extraction goal and no others. The identification is a 3-of-3 / 0-of-3 inference; "Black Widow" is a NovaLogic name with no real counterpart |
| 78 | `ka-50-hokum` | rotorcraft | CMN | 4 | **coaxial 2×3 blades, no tail rotor**, gear, 30 mm mount | good — 8–14 per mission, the campaign's central threat |
| 79 | `ah-64-apache` | rotorcraft | AF | 4 | 4-blade rotor, tail rotor, chin turret | good |
| 80 | `mi-24-hind` | rotorcraft | AF | 4 | 5-blade rotor, tail rotor, gear, nose turret | good |
| 81 | `rah-66-comanche` | rotorcraft | CMN | 4 | **5-blade bearingless rotor, 8-blade FANTAIL**, retractable gear, weapon bay, 20 mm turret | **thin** — two prototypes, cancelled 2004. The manual gives rotor Ø 11.90 m, FANTAIL Ø 1.37 m, fuselage 13.22 m; dimensioned three-views are scarce |
| 82 | `nanuchka-iii` | ship | F22 | 4 | 2× triple SS-N-9 launchers, radar, gun turret | good — the manual's spelling *"Nanchuka"* is its own error, corrected in the patch readme. **No ship model exists anywhere in this tree** |
| 83 | `soldier-hostile` | person | DF | 4 | rig: stand / crouch / prone, weapon attach | good on anatomy, **none on the four types** (5005 / 5006 / 5041 / 5007 have classes but no model names). 33–96 instances per mission |
| 84 | `soldier-friendly` | person | DF | 4 | as #83 | none — types 5002 `Squad Member` and 5048; 0–4 per mission |
| 85 | `person-druglord` | person | DF | 4 | as #83, plus a surrender state | none — type 5025, **exactly one instance in the whole campaign**, and killing him fails Headhunter |
| 86 | `soldier-dismounted` | person | AF | 4 | as #83 | thin — the *"command troop dug in on the encampment periphery"* is a mission goal in `INDIA5`, named in prose only. Whether it is infantry or a vehicle is not in any decoded data |

#### 1.7 D5 — inhabited

The camera sits inside these. Each is a separate build, and each is where the difficulty actually is.

| # | Name | Kind | Used by | D | Moving | Sources |
|---|---|---|---|---|---|---|
| 87 | `cockpit-f-22` | crew station | F22 | 5 | canopy frame and struts (the title flies a **virtual cockpit**, not a full-screen HUD), MFDs, stick, throttles | good |
| 88 | `crewstation-m1a2` | crew station | AF | 5 | gunner's sight 1× / 3× / 10×, TTS thermal state, turret handwheel | good |
| 89 | `crewstation-t-80` | crew station | AF | 5 | as #88, IIT instead of TTS, **mirrored layout** (map right, weapons left) | thin |
| 90 | `crewstation-m3-bradley` | crew station | AF | 5 | 25 mm / TOW sight, TTS | good |
| 91 | `crewstation-bmp-2` | crew station | AF | 5 | 30 mm / Spandrel sight, IIT | thin |
| 92 | `viewmodel-soldier-fp` | crew station | DF | 5 | hands, weapon swap, reload, **3 postures with 3 eye heights and 3 contact sets** | good |
| 93 | `cockpit-rah-66` | crew station | CMN | 5 | HID helmet sight, **2 TAC monitors**, cyclic, collective, night = palette swap on the whole panel; 3 view directions (forward / left / right) | **thin** — prototype cockpit. The game's own art (`CONSOL1S.DTA`) is decoded and measured, which is a reference for the *layout* and not for the aircraft |

---

### 2. The ten first

Start here. All are D1–D2, none needs a joint the tree has never built, and the first one pays for
itself four times over.

| Order | Asset | Why first |
|---|---|---|
| 1 | `fuel-tank-cylindrical` | the only asset all four titles place, and a mission goal in Comanche |
| 2 | `crate-supply` | Weatherman's entire objective |
| 3 | `barrel-oil` | the same lathe profile as #1 at another scale |
| 4 | `tank-propane-bullet` | closes Armored Fist's refuel depot |
| 5 | `tent-small` | first of the three-tent kit |
| 6 | `tent-long` | second |
| 7 | `tent-netting` | third; completes Objective Calm and the Flood airfield group |
| 8 | `tower-guard` | the first asset with load-bearing *placement* — Insurrection's ridge |
| 9 | `hangar-aircraft` | first shared build (F22 + DF), first door joint |
| 10 | `tree-palm` | first parametric/L-system asset, and the one that tests instancing, LOD, impostor and wind against closed-form values ([`assets.md`](assets.md) §3.1) |

The ordering inside 1–7 is deliberate: seven objects that are lathes, boxes and cloth, done before any
joint exists, so the pipeline itself is proven before difficulty arrives.

**Two D1 entries are deliberately not in the ten.** `runway-strip` (#6) is probably a terrain decal and
the decision has to be made before the mesh; `mine-anti-tank` (#5) is trivial as an object and blocked as
a placement (`STMP` is a hypothesis with a counterexample). Neither teaches the pipeline anything.

### 3. The five hardest

| Asset | Why |
|---|---|
| `cockpit-rah-66` | D5 **and** thin sources. A prototype cockpit, three view directions, two TAC monitors, a helmet display, and a night mode that is a palette swap over the whole panel. Worst combination in the list |
| `viewmodel-soldier-fp` | D5, and the only asset whose geometry is coupled to physics the tree does not have — three postures mean three contact sets and three eye heights ([`body-format.md`](body-format.md)) |
| `rah-66-comanche` | D4 with a bearingless 5-blade head, an 8-blade shrouded FANTAIL, a retracting gear and a weapon bay — on two prototypes' worth of published drawings |
| `an-225` | D4, six engines, a 32-wheel gear, and the largest aircraft ever built; every dimension is public but nothing about it is small |
| `nanuchka-iii` | D4, and **the tree has no ship of any kind** — hull form, waterline, buoyancy attachment and wake are all first-of-kind here, not just the mesh |

Runners-up, named because they will be argued about: the four Armored Fist crew stations (#88–91) are
four D5 builds that differ mostly in layout, and are the largest single block of D5 work in the list.

### 4. Thin or absent sources — the risk list

**This is the section that decides whether the plan is honest.** Every row here is a place where the
modeller has no measurement and would otherwise guess.

| Where | What is missing | Consequence |
|---|---|---|
| **Delta Force, all of it** | `ITEMS.DEF` is encrypted (entropy 7.994/8). PERU uses **84 distinct type IDs; the shipped name table covers 16**. So the convoy's two vehicle types, the villa's 32 part types and **all four hostile soldier types have classes but no model names** | **Nine** entries above are `none` for this one reason: #2, #11, #24, #48, #57, #77, #83, #84, #85. The largest single source hole in the inventory |
| **F-22, eight loaded objects** | `ARTEMIS` · `VTNM_04` · `RBB_H` · `RBA_H` · `CTR` · `CTRMP` · `DOT` · `THAP2` are `.PAK` names the campaign loads and **no source in this tree glosses any of them**. `ARTEMIS` alone appears in 4 of 8 missions | **The F-22 inventory above is not complete.** These 8 are not counted in §6 because nothing can be said about their kind, size or difficulty. Resolve by re-parsing `RESOURCE.RES` before building |
| **F-22 AAA** | briefings name AAA in 1.2 and 1.3; **no AAA model is loaded by any `.ORF`**; the manual lists three candidates and assigns none | #62 has no reference at all. Either pick one and record the choice as a deviation, or leave the AAA out and say so per mission |
| **F-22 AWACS** | `AWACS.PAK`, mission 1.6; friendly E-767 and hostile A-50 both documented, neither confirmed | #76 cannot begin until the identity is decided. A 4-engine jet and a turboprop Il-76 derivative share nothing |
| **Armored Fist objects** | `DCBS` — the object table carrying unit types, positions and goal flags — is **undecoded**. Records are variable length (measured strides 17–257 B) and no record header was found | #18, #19, #20, #86 rest on debriefing prose alone. Five of seven Overwatch goal counts are also unrecovered |
| **Comanche `turr` / `radr`** | seven sprites, six object types; which of `turr` and `radr` draws the SA-8 is unestablished | One static object of the 1992 release is unaccounted for. #61 covers one of them; the other has no entry |
| **Comanche RAH-66** | two prototypes, cancelled 2004 | #81 and #93. Manual dimensions exist and are used; a dimensioned three-view is the gap |
| **Fictional structures** | `assembly-house`, `command-centre`, `villa-compound`, `code-book` are inventions of their games | Free design, but the acceptance record ([`assets.md`](assets.md) §4) has nothing to measure against. Record the reference used, or the bar drifts |
| **The merge in #56** | `truck-cargo` is one entry serving two titles neither of which names its truck | **Flagged as a decision.** If the owner rejects it, the entry splits and the shared count in §6 drops by one |

### 5. What the dedup actually bought — and what it did not

| Shared asset | Titles | Note |
|---|---|---|
| `fuel-tank-cylindrical` | F22 · CMN · AF · DF | 4 |
| `runway-strip` | F22 · AF · DF | 3 |
| `t-80` | CMN · AF | 2 — Armored Fist additionally needs #89, the crew station |
| `hangar-aircraft` | F22 · DF | 2 — differs in size and cladding |
| `truck-cargo` | AF · DF | 2 — **the merge is a decision, not a measurement** (§4) |

**Five of ninety-three. The dedup bought almost nothing, and that is the finding.** The four titles are
1992–1998, four theatres, four scales — an air-superiority campaign over Laos, a NoE helicopter war in a
10 km box, a tank company in Sindh, and six men on foot in the Huallaga. They share infrastructure and
one Soviet tank. Nothing else recurs.

Three near-misses, recorded so nobody re-derives them:

- **Delta Force's engine knows `BMP-2` (2001), `m1a2` (2008), `humvee`, `BRDM`, `havoc`, `LCAC` — and PERU
  places zero of each.** A *later* Delta Force campaign would share #64 and #65 with Armored Fist. Campaign
  one does not.
- **Comanche's manual documents Hind, Scud, OSA II, BRDM, Hughes 500MD and M1A1** — their sprites exist
  only in the 1994 CD file set, not in the 1992 release under study. So Comanche does **not** share the
  Mi-24 with Armored Fist.
- **`tower-guard` (DF) and `tower-control` (F22) are not the same object** and were not merged.

### 6. The two numbers

| | |
|---|---|
| **Assets total** | **93** |
| **Assets used by more than one title** | **5** (§5) — one of which (#56) is a merge by decision |
| *not counted* | **8** F-22 objects that the data loads and no source names (§4); one unattributed Comanche static sprite |

By difficulty: D1 **17** · D2 **34** · D3 **11** · D4 **24** · D5 **7** = 93.
By title: F22 **31** · CMN **14** · AF **23** · DF **33** = 101, i.e. 8 more than 93 — the §5 overlap.

## State

Nothing built. `mods/f16/src/models/` holds the F-16 (four LODs, 107 706 / 41 342 / 14 366 / 9 916
triangles) plus `aim9.glb`, `mk82.glb`, `r73.glb`. Of the 93 entries above, **#30 `r-73` is already
built** and **#27 `aim-9x` is a variant of an existing build**; the other 91 do not exist.

## Gaps

- **No entry carries a triangle budget.** The F-16's ladder is the only measured precedent and it is a
  player aircraft; nothing here says what an L0 tank or an L2 tent should cost. Until it does, the LOD
  ladder of every entry is unspecified.
- **Difficulty is a judgement, not a measurement.** The 1–5 column was assigned by reading the moving-parts
  column, not by building anything. The first ten will show whether the scale is calibrated; if #1–#7 take
  wildly unequal effort, the scale is wrong and this file is to be corrected, not the plan.
- **Source ratings are unverified.** `good` / `thin` / `none` were assigned from what the mod docs state
  and from general availability of drawings. **No search for a blueprint was run for any entry.** The
  `none` ratings are firm (they follow from the docs); the `good` ratings are a claim awaiting a fetch.
- **`DEFECTS.md` still does not exist** for any asset, including the F-16 ([`assets.md`](assets.md) Gaps).
  Building 91 assets against a rule whose recording mechanism is unbuilt will lose the critic's findings
  exactly as it does today.
- **Kind ⇒ physics is not established.** This file names a `ship`, a `person` and a `crew station`; none
  of the three has a body in [`body-format.md`](body-format.md) that is implemented. A mesh whose joints
  have no physical counterpart is a mesh whose node names cannot be checked.
- **Nothing here says which title is built first.** The order in §1 is by difficulty across all four
  titles at once. If the owner wants one *title* playable before the others, that is a different ordering
  and this file does not carry it.
