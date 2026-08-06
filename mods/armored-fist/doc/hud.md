# Armored Fist — displays, element by element

> Form per [`doc/mods.md`](../../../doc/mods.md) §3. Companion to [`campaign.md`](campaign.md);
> sources in [`sources.md`](sources.md).
> The original has **no HUD in the aircraft sense** — there is no overlay on a world view. Every
> instrument is a *panel widget* below a gunsight window, and every one of them is also a *control*:
> the same rectangle displays a value and accepts a click. That is the defining property of this
> title's interface and the reason this file is structured by panel, not by symbol.

Provenance tags as in [`campaign.md`](campaign.md). Additionally:

| Tag | Means |
|---|---|
| `[MAN p.N]` | German print manual (Softgold), 86 pp. |
| `[MAN p.N fig]` | read **visually** off a 400–600 dpi render of that German manual page in this run — a labelled diagram, not OCR |
| **`[EN p.N]`** | **the English electronic manual**, 149 pp. — the authoritative source for element *names* ([`sources.md`](sources.md) §4b) |
| `[SHOT]` | read off the retail T‑80 console screenshot, myabandonware `armored-fist_12.png`, 320 × 200 (German build) |

> **Correction to an earlier state of this file.** It said *"no English manual was found"* and
> translated every element name out of the German. **That was wrong** — the English electronic manual
> exists, is 63 pages longer than the German print manual, and names every widget itself. All element
> names below are now the game's own English, and the German is kept only where it carries a
> statement the English does not. Where the two disagree, the disagreement is shown.

---

## Spec

### 1. Four consoles and an external view

`[MAN p.78–82 fig]` — Appendix A carries one labelled diagram per view.

| View | Manual page | Sight | Night device |
|---|---|---|---|
| **M1A2 Abrams** | 78 | 120 mm gunner's sight | **TTS** — Thermal Targeting System |
| **T‑80** | 79 | 125 mm gunner's sight | **IIT** — Image Intensified Targeting |
| **M3 Bradley** | 80 | 25 mm / TOW sight | TTS |
| **BMP‑2** | 81 | 30 mm / Spandrel sight | IIT |
| **External view** | 82 | chase camera | — |

**The Western and Eastern consoles are mirror images of each other.** M1A2: tactical map bottom
**left**, weapon control bottom **right**, threat display top **right**. T‑80: weapon control bottom
**left**, tactical map bottom **right**, threat display **left** of the turret control. Confirmed
independently against `[SHOT]`, where the T‑80's map is on the right and its ammunition block on the
left. `[MAN p.78–79 fig]`

---

### 2. M1A2 console — callouts verbatim

`[MAN p.78 fig]`. German callout, English gloss, position read off the diagram.

Element names are the game's own English `[EN]`; the German callout is kept so the diagram can be
followed, and the position is read off it.

| # | Element `[EN]` | German callout | Position `[MAN p.78 fig]` |
|---|---|---|---|
| 1 | **CCV** — cursor turns into the word `CCV` in the corner `[EN p.50]` | `Ausgang zum CCV` | extreme **upper-left corner** of the screen; also `Esc` |
| 2 | **Viewport Magnify** — **1× · 3× · 10×** `[EN p.37]` | `Sichtvergrößerung` | left, immediately **below** the viewport |
| 3 | **Night Vision** (`TTS`) and **Target Lock** (`TRGT`) `[EN p.38, 44]` | `Zieleinrichtungen` | right, immediately below the viewport |
| 4 | **Viewport Slew Left / Right** — proportional slew bar `[EN p.36]` | `Turmkontrolle` | **centre**, below the viewport |
| 5 | **Viewport / Turret Angle** — 3-digit degrees, `000` = turret over the centreline `[EN p.37]` | (readout) | **bottom centre of the viewport display** |
| 6 | **Threat Indicator Display** `[EN p.40]` | `Bedrohungsanzeige` | **upper right** |
| 7 | **Weapon Status Indicator** — `LOADED` / `READY` `[EN p.41]` | `Waffenstatus` | **at the top of the Weapon Select buttons** `[EN p.41]` |
| 8 | Auto / Manual Turret Control — `USER CONTROL` `[EN p.48]` | `Auto/Manuelle Kontrolle` | **far left**, below the viewport |
| 9 | **Speedometer**, MPH `[EN p.34]` | `Geschwindigkeit` | centre-left |
| 10 | **Fuel Gauge** + temperature `[EN p.34]` | `Temperatur/Treibstoff` | centre-left, paired dials (`F`, `T`) |
| 11 | **Air Support** / **Artillery Support** `[EN p.51–52]` | `Unterstützung` | right — `AIR`, `ART` |
| 12 | **Tactical Map** (a.k.a. **Spin Map**) `[EN p.39]` | `Taktische Karte` | **bottom left** |
| 13 | Tactical Map magnification buttons — **3 levels** `[EN p.40]` | `Kartenkontrolle` | **top left of the map** `[EN p.40]` |
| 14 | steer left / steer right (tiller levers) | `Linksdrehung` / `Rechtsdrehung` | bottom, flanking the centre |
| 15 | **Gear Shift** — `D L N R` `[EN p.33]` | `Getriebe` | bottom centre |
| 16 | fire button | `Feuerknopf` | bottom centre, on the tillers |
| 17 | brake pedal / throttle pedal | `Bremse` / `Gaspedal` | bottom — *always* brake left, throttle right in every vehicle `[MAN p.26]` |
| 18 | **Weapon Select** (and ammunition counter) `[EN p.41]` | `Waffenkontrolle` | **bottom right** |
| 19 | **Smoke Grenades** `[EN p.47]` | `Nebelgranaten` | bottom right |

**One placement disagreement between the manuals.** The German diagram puts the turret-angle readout
inside the slew bar below the viewport; the English text says it is *"located at the bottom center of
the Viewport display"* `[EN p.37]`, and the German body text agrees it is *"unterhalb des
Sichtgeräts"* `[MAN p.29]`. Same region, different side of the frame edge; not resolved.

Key bindings `[EN p.37–52]`:

| Key | Action |
|---|---|
| `+` / `−` | viewport magnify up / down |
| `F5` | night vision on / off |
| `F7` | engine smoke on / off |
| `F1` | pause / resume |
| `7` | request **artillery** support |
| `8` | request **air** support |
| `9` | launch a **smoke grenade** |
| `*` or `/` | centre turret to 000° |
| `Alt`+`*` or `Alt`+`/` | turret to 180° |
| `D` `L` `U` `R` | gear Drive / Low / Neutral / Reverse `[MAN p.27]` |
| `Alt`+`←` / `Alt`+`→` | slew turret left / right `[MAN p.28]` |
| `Alt`+`S` | Settings, from inside a vehicle `[EN p.49]` |
| `Alt`+`Q` | exit mission `[GUIDE p.19]` |
| `Alt`+`B` | mission briefing `[GUIDE p.26]` |
| `1` | show this vehicle's company identification (platoon no., unit no.) `[GUIDE p.14]` |

Two callouts appear on other vehicles only:

| Element | Detail | Vehicles |
|---|---|---|
| **Engine Smoke** — `ENG SM` | sprays fuel into the exhaust; makes the vehicle **harder to see at night and *more* visible by day**, and is **ineffective against night-vision-equipped vehicles** `[EN p.49]`. Clickable icon on the T‑80 only; `F7` everywhere `[EN p.49]` | T‑80 `[MAN p.79 fig]`, M3 `[MAN p.80 fig]`, `[SHOT]` |
| **SAM** | must be *readied* by clicking the safety cover, then fired after lock `[EN p.44]`. An IFV left on **Auto** locks and kills enemy air by itself `[EN p.44]` | M3 (Stinger), BMP‑2 (SA‑14, `Luftabwehr` `[MAN p.81 fig]`) |

---

### 3. The viewport display

| Element | Behaviour | Provenance |
|---|---|---|
| What it shows | the view **along the turret**, not along the hull | `[EN p.35]`, `[MAN p.28]` |
| Why it is one window | *"Um den Spielablauf zu vereinfachen, sind hier, im Gegensatz zu einem echten Panzer, alle Sicht- und Visiereinrichtungen vereint"* — unlike a real tank, all vision and sighting devices are merged into one | `[MAN p.28]` |
| Magnification | **1× / 3× / 10×**, clicked or `+` / `−` | `[EN p.37]` |
| Reticle | cross with a graduated horizontal ladder below it and two curved range wings | `[SHOT]` |
| **`GOALS REMAINING: n`** | live objective counter, printed **inside the sight, lower left** | `[MAN p.78–82 fig]`, `[SHOT]` (German: `VERBLEIBENDE ZIELE: 12`) |
| Range readout | small red numeral at the **right edge** of the sight — `03.9` on `[SHOT]`; unit not stated by any source | `[SHOT]` |
| Night device | `F5` or the `TTS`/`IIT` button | `[EN p.38]` |

**The two night devices behave differently, and the manual says the simulation models the
difference** — *"ARMORED FIST faithfully reproduces the particular visual 'quirks' of each system"*
`[EN p.38]`:

| | TTS (US) | IIT (Soviet) |
|---|---|---|
| Principle | infrared / thermal | ambient-light amplification |
| Total darkness | **works** | fails |
| Through smoke | **sees through smoke grenades, and can still take target locks through them, day and night** `[EN p.38]` | **defeated by smoke** |
| Image quality | motion blur from latent heat in the imaging optics — a moving target looks larger, hence nearer than it is `[GUIDE p.58]` | sharper, but **noisy** and **cannot separate warm targets (tanks) from cool ones (boulders)** `[EN p.38]` |

The guide adds the tactical consequence: turn on **engine smoke** at night to protect the rear
against Soviet optics, since your own thermals are unaffected `[GUIDE p.58]`.

---

### 4. Tactical Map — "Spin Map" `[EN p.39]`

| Property | Value |
|---|---|
| Orientation | **hull-up** — the map moves with the vehicle, own vehicle always centred, front always toward the top of the display |
| Compass | across the **top** of the map |
| Own vehicle | **green dot**, centre |
| Magnification | **three** levels; buttons at the **top left of the map**; the current level is illuminated |
| Steering cue | a small **triangle**; line it up with the **`^`** marker on the compass |

Steering-cue colours `[EN p.39]`:

| Triangle | Points at |
|---|---|
| **red** | the next **waypoint** |
| **yellow** | the nearest **mission goal** — *replaces* the red one when no waypoint exists |

**Both manuals are explicit that this cue is not a route planner:** *"Neither of the '^' indicators
assures that you are following the best path to your destination. There may be obstacles such as
mines or enemy patrols that can be avoided by selecting a different route"* `[EN p.39]`; identically
in German `[MAN p.31]`.

Contact colours — verbatim, `[EN p.39–40]`, identical to the German list `[MAN p.31]`:

| Object | Colour |
|---|---|
| enemy land vehicles & artillery | **red** |
| friendly land vehicles & artillery | **green** |
| mines | **yellow** |
| **goals** | **red blinking** |
| friendly air support | **green blinking** |
| enemy air support | **white blinking** |
| exploding artillery | **fading grey** |

`[GUIDE p.65]` calls minefields *"flashing yellow dots on the tactical display"* where **both**
manuals list plain yellow, and reserve blinking for goals. Both are printed; neither is corrected
here.

Two uses the guide relies on and the manual does not mention: **artillery fall of shot** is walked
onto a target by watching the grey puffs appear on this map relative to the red dots `[GUIDE p.55]`;
and **objective clusters** are recognisable as a pattern of red dots behind a ridge before anything
is visible `[GUIDE p.55]`.

---

### 5. Threat Indicator Display `[EN p.40]`

| Property | Value | Provenance |
|---|---|---|
| Difference from the Tactical Map | shows **enemy units only** | `[EN p.40]` |
| Orientation | own unit centred, front toward the top — same as the map | `[EN p.40]` |
| Extra element | a **"pie slice"** giving turret facing **relative to the hull** | `[EN p.40]`; visible on `[SHOT]` as a green sector |
| Mission goals | appear here too, as blinking dots | `[MAN p.55]` |

---

### 6. Weapon Select and Weapon Status Indicator

**Weapon Select is also the ammunition counter** `[EN p.41]`: the illuminated button is the ordnance
currently selected for firing *and loading*, and displays its remaining rounds. On `[SHOT]` (T‑80):
`AP 0015` · `HEAT 0016` · `HE 0005` · `MG 2000`, with `SG` below.

| Rule | Statement | Provenance |
|---|---|---|
| Reload is not instantaneous | *"Ordnance changes are not instantaneous!"* — watched on the **Weapon Status Indicator**, which reads `LOADED` then `READY` | `[EN p.41]` |
| Non-main-gun weapons | selecting anything other than the main weapon uses that weapon's standard ordnance; the machine gun needs no selection or loading because it does not fire out of the main weapon — **so it can be fired while the main gun loads, without disturbing the process** | `[EN p.41, 43]` |
| Ordnance roles | Sabot/APDS → MBTs, poor against large soft targets · HEAT → IFVs, some effect on MBTs, poor against large targets such as satellite dishes · **HEP** (M1) / **HE** (T‑80), functionally similar → soft targets, bunkers, satellite dishes, finishing a disabled T‑80 · MG → fuel and LPG tanks, **and enemy air** · TOW / AT‑6 → any ground target, few carried · HEI → everything but MBTs, devastating against air · SAM → air, near-certain kill on lock | `[EN p.42–44]` |
| Missile reload | Bradley and BMP reload TOW / Spandrel from reserve **automatically**, but only after **~2 min** (guide, ch. 1) or **1½ min** (guide, ch. 2) with no weapon fired — *the same book gives two figures* | `[GUIDE p.9, 23]` |

Loadouts `[GUIDE p.4–9]`:

| Vehicle | Main | Rounds | Secondary | Defensive |
|---|---|---|---|---|
| **M1A2** | 120 mm smoothbore, laser rangefinder + target tracker | **18 SABOT · 22 HEAT** (+ HEP in the ordnance table) | .50 cal, **2000** rds | **20 smoke grenades** |
| **T‑80** | 125 mm, laser rangefinder + stabilised sight that superelevates automatically | **10 AP · 13 HEAT · 13 HE** | 12.7 mm, **300** rds | **1 smoke grenade launcher** |
| **M3 Bradley** | 25 mm Bushmaster | **900**, split APDS / HEI | .30 cal, **2000** rds; **TOW‑2**: 2 ready + 10 reserve; **Stinger**: 2 ready + 10 reserve | — |
| **BMP‑2** | 30 mm | **300 AP + 400 HE** | 7.62 mm, **2000** rds; **Spandrel AT‑5**: 1 tube + 10 reloads; **SA‑14**: 4 ready + 12 reserve | — |

The guide's chapter 1 says the Bradley carries *"Stinger … with two available rounds"* and its
chapter 2 says *"Bradleys begin with two missiles loaded and have 10 in reserve"* — the second is
taken as the fuller statement, and both are recorded.

**Smoke grenades are the single most load-bearing mechanic in the guide** `[GUIDE p.15]`: 20 per
tank, launched **ahead** of the vehicle, spreading into a wide ground-hugging cloud. Inside the
cloud's perimeter **the tank can lock others but cannot be locked** — including by helicopters. The
faster the tank is moving when they fire, the farther they fly `[GUIDE p.56]`. They **need no target
lock**, and are *"generally more effective \[than engine smoke] and work better against TTS"*
`[EN p.47]`.

---

### 7. Target Lock, and what it is not

| Element | Behaviour | Provenance |
|---|---|---|
| `TRGT` button | activates the vehicle's **laser range finder**; if a target is found, locks it in the viewport. The computer holds rotation and elevation **as long as line of sight is unobstructed**. Clicking again breaks lock | `[EN p.44]` |
| Once broken | the target **must be re-acquired** to deliver ordnance accurately | `[EN p.44]` |
| Manual designation | click the target inside the viewport crosshair; artillery without a lock is directed the same way | `[EN p.45]`, `[MAN p.35]` |
| **Auto Turret Control** (Settings, default **on**) | the turret and main gun **automatically track the locked target**, and re-align with the direction of travel as soon as lock breaks — manually, or when the target leaves range | `[EN p.48]` |
| With it off | automatic repositioning is disabled; the player tracks turret direction and hull direction separately. Suited to *"on-the-fly"* targeting with many contacts, harder to manage | `[EN p.48]` |
| Turret centring shortcuts | **left**-click the Viewport Angle readout → slew to `000°`; **right**-click → `180°` | `[EN p.48]` |
| Gunner call-outs | *"Target destroyed"*, *"Missile! Missile!"*, *"Hind, Hind, Hind…"* — audio, not a display element | `[GUIDE p.55, 56]`, `[EN p.52]` |

**The guide and the English manual describe Auto Turret Control differently.** The guide says only
that the turret *re-centres when a lock is lost* `[GUIDE p.13]`; the manual says the turret
*actively tracks the locked target* and re-centres on break `[EN p.48]`. The manual is the fuller
statement and the guide is not wrong, only partial — but the tracking behaviour is the load-bearing
half and the guide omits it.

The gunnery loop the guide teaches is built entirely out of these widgets: hold the fire button
down, keep clicking `TRGT`, and judge from the gunner's call whether to re-lock or stay
`[GUIDE p.55]`.

---

### 7a. Air and artillery — as controls

| | Air Support (`8`) | Artillery Support (`7`) |
|---|---|---|
| Availability | **every battle has air bases**, but they *may or may not* have helicopters available; there may be a delay, and a refused call may succeed later | **up to three artillery bases** per battle; same delay behaviour |
| With a target lock | the helicopter flies from its base and attacks the designated location, then engages other nearby enemies until out of ammunition | shells fall on the locked point |
| Without a target lock | the helicopter flies to **your** location and attacks enemies in your vicinity | shells are directed by manual aiming |
| Transit time | set by the **distance from the air base to the target** — an explicit mission-design parameter | — |
| Attrition | a helicopter shot down **never flies again**; survivors return to base and reload | artillery rides on **trucks**; destroying an enemy artillery unit removes its calls-for-fire, and your own must be protected the same way |
| Friendly fire | — | **do not call it close to yourself** |

`[EN p.51–53]`. This answers most of what [`campaign.md`](campaign.md) §5.5 left open; the one thing
still unquantified is how many sorties an air base holds.

---

### 8. CCV — the command screen

`[MAN p.83 fig]`, `[GUIDE p.18–33]`. This is a **second seat**, not a map overlay: a full-screen
strategic display with its own menu bar, status bar and floating tool kit.

#### 8.1 Menu bar, left to right `[GUIDE p.19–27]`

| Button | Function | Key |
|---|---|---|
| **Exit Mission** | leave and end the mission; confirmation dialogue, then a statistics screen | `Alt`+`Q` |
| **Company Status** | platoon roster and the four standing orders per platoon ([`campaign.md`](campaign.md) §5.3) | — |
| **Advance** | issue the advance order | — |
| **Take Command** | drop into the currently selected vehicle | `Space` |
| **Mission Briefing** | re-read the standing orders | `Alt`+`B` |
| **Map Zoom In/Out** | **three** zoom levels; left-click in, right-click out | `+` / `=` |
| **Time Acceleration** | 1× … **8×**; drops back to 1× automatically the moment anyone fires or the player takes command | — |
| **Unit / Platoon switch** | change selection | — |
| **`EDIT`** | enter editing mode; the simulation stops and every battle parameter becomes editable | — |

#### 8.2 Status bar `[MAN p.83 fig]`

| Field | Example |
|---|---|
| current message window | `ADVANCING TO WAYPOINT` |
| unit information window | `P:1 U:1 M3` — platoon, unit, type |
| platoon buttons 1–4 | `[1] [2] [3] [4]` |
| pause / play indicator | `IN ACTION` |
| time acceleration | `1X` |
| **game limits**, lower right of the map | remaining **time** and remaining **`GOALS`** |
| **map scale legend**, lower left of the map | a dashed bar labelled **`150 M`** (glyph ambiguous — possibly `160`) |

#### 8.3 Map symbology `[MAN p.83 fig]`

| Symbol | Meaning |
|---|---|
| **blue** icons | own side — always blue, whether the player is Western or Eastern |
| **red** icons | enemy — always |
| diamond | waypoint |
| square marked `AIR` | air base |
| square marked `ART` | artillery base |
| filled square | mine |
| small squares | targets |
| unit label | `PL:1 UN:M3 SP:go` — platoon, unit type, speed order |

**And the map is a reconnaissance picture, not ground truth** — see [`campaign.md`](campaign.md) §5.2.
Contacts decay back to *unlocated* when nothing observes them.

#### 8.4 Floating tool kit

| CCV mode `[MAN p.60–62]` | Edit mode `[GUIDE p.30–33]`, `[MAN p.64–66]` |
|---|---|
| select · pan map · set waypoints · cut waypoints | select **mission goal** · set **enemy** waypoints · cut · **air base** placement · **artillery base** placement · place **minefield** · place **vehicle** · place **trees** · place **target** |

A mission goal is set by `Ctrl`-clicking any vehicle, artillery base or placed target; a **flashing
cross** over the object confirms it; `Ctrl`-clicking again clears it `[MAN p.55]`.

**The edit tool list is the game's own object vocabulary** and therefore the closest thing the
original has to a mission schema: vehicles, air base, artillery base, minefields, trees, targets,
waypoints per platoon, mission goals, mission time. Every one of those has a slot in the `.FSG`
([`campaign.md`](campaign.md) §7).

---

### 9. Main menu `[MAN p.42]`, cross-checked against a retail screenshot

```
SELECT PLAYER · CAMPAIGNS · BATTLES · REVIEW · SETTINGS · ABOUT FIST · QUIT
```

with `CURRENT PLAYER: <name>` printed across the bottom. Verified against myabandonware
`armored-fist_2.png`, read in this run: identical, no additional item.

`SETTINGS` carries `Controls` (joystick, external driver, calibrate), **`Auto Turret`**,
**`Prompts`**, `Display` (sky, **Low / Medium / High detail**, smoke effects) and `Sound`
`[MAN p.45–47]`. The three detail levels are the files `LOW.DTL`, `MEDIUM.DTL`, `HIGH.DTL`,
**each exactly 2052 bytes** `[FSG]`.

---

## State

**Declared and drawn: `../src/hud/armored-fist.fbh`, 12 rows** — exactly §3, what is printed INSIDE the
sight, plus the console readouts of §2 that have a source. Format:
[`doc/render/hud-declaration.md`](../../../doc/render/hud-declaration.md). Proof: a real Chromium on
the deployed WASM app, `sim/build/shots-hud/armored-fist/c01m01-slaughterzone-s40.png`, and the native
frame oracle, `sim/build/shots-hud/armored-fist-native/mission_0001.png`
(`mission hud deck=armored-fist elements=12`) — reticle with its lead solution, range readout at the
right edge of the sight, speedometer, fuel bar, `MAIN`/weapon counter, threat count, spin map.

**§2's nineteen callouts are console FURNITURE** — buttons, tillers, dials, two screens — around the
viewport. A HUD declaration draws over a rendered scene, not over a bitmap console, so they are out of
scope here rather than missing.

**What has no source:** the turret angle `000` (the flown airframe has no turret and nothing publishes
a mount bearing); `GOALS REMAINING: n` (`FBMissionMonitor` judges from its own plan copy and publishes
no count to any display block — deliberately, so a cockpit cannot read the verdict); the 1×/3×/10×
magnification (a camera state, not symbology); the TTS/IIT night device (an MFD page,
`render/stages/FBNvisStage`); `LOADED`/`READY` (the stores block publishes a station and an arm state,
not a breech state).

## Gaps

- **The range readout's unit is unknown.** `03.9` appears at the right edge of the viewport on
  `[SHOT]`; **neither** manual names it — the English manual documents the laser range finder as the
  mechanism behind Target Lock but never describes a numeric range display. Metres, hundreds of
  metres and kilometres are all consistent with the picture, and the world scale is undetermined
  ([`terrain.md`](terrain.md) §6).
- ~~No English manual was found.~~ **Refuted within this run** — see the note at the top of this
  file. What remains open is narrower: the German print manual (86 pp.) and the English electronic
  manual (149 pp.) are **different documents**, not translations of one another, and only the
  differences that surfaced while writing this file have been compared.
- **Pixel geometry is not captured.** Element positions above are relative ("bottom right"), read off
  printed diagrams. No element's screen rectangle in the 320 × 200 frame was measured.
- **Missile reload timing is contradictory** in the guide (2 min vs 1½ min), §6.
- **Minefield dot: steady yellow or blinking yellow** — both manuals vs guide, §4.
- **Auto Turret Control: tracking or only re-centring** — manual vs guide, §7. Untested.
- **Turret-angle readout placement** differs between the two manuals, §2.
- **Only the T‑80 console was verified against a retail screenshot.** M1A2, M3 and BMP‑2 rest on the
  manual diagrams alone; the mirroring claim in §1 is inferred from the callout ordering of pp. 78–79
  plus the one T‑80 screenshot.
- **`REVIEW` is undocumented here** — the menu item exists `[EN p.57]`, `[MAN p.44]`, contents not
  examined.
- **The English manual's `ARMORED COMBAT STRATEGIES` appendix (pp. 104–137) was not read**, beyond
  noting that it defines *"Overwatch/support by fire"* as a **mission type** `[EN p.115]` — i.e. the
  first campaign is named after a doctrinal term the game itself teaches.
- **The 149-page English manual was only mined for the sections this file needed.** Pages 1–103 were
  read selectively; pages 104–149 (strategies appendix, key reference chart p. 138, quick-reference
  screens pp. 150–154) were not.
- **The `.MRL` panel bitmaps were not decoded**, so nothing here is measured from the game's own art;
  every layout statement comes from a printed reproduction or a third-party screenshot.
