# Comanche: Maximum Overkill — cockpit and Helmet Integrated Display, element by element

> **Sources:** the NovaLogic `USER'S MANUAL` (labelled cockpit diagram on **p. 30**, definitions on
> **p. 30–58** and **p. 70–74**) and the game's own cockpit art `CONSOL1S.DTA`, decoded and measured
> here. Method in [`sources.md`](sources.md) §2.
> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has no `test/`.

| Tag | Means |
|---|---|
| `[MAN p.N]` | NovaLogic Comanche `USER'S MANUAL`, page N (printed numbers = PDF page indices) |
| `[ART]` | **measured here** from the decoded `CONSOL1S.DTA`, 328 × 290, 8-bit + appended palette |
| `[MIS]` | measured here from the mission data |

**Comanche has no HUD.** It has a **Helmet Integrated Display (HID)** projected on the helmet optics,
and the manual is explicit about the difference and its consequence:

> *"Unlike traditional Heads Up Displays (HUD), the HID is not limited to the view directly in front of
> the windshield. In fact, you can target some weapons out the side and rear views."* `[MAN p.35]`
> *"This can account for the targeting box sometimes traveling over the interior of the cockpit."* `[MAN p.35]`

Symbology follows the head, not the airframe. That is a structural difference from every fixed-wing
HUD in this tree, not a cosmetic one.

## Spec

### 1. Screen layout

Two independent sources, cross-checked. The manual's p. 30 diagram names the elements; the decoded art
gives their positions.

Measured `[ART]`, in pixels of the 328 × 290 console image:

| Element | x | y | Note |
|---|---|---|---|
| Outside view (window) | full width | **0 – 142** | upper 49 % of the image; canopy struts cut in diagonally left and right |
| Instrument panel | full width | **143 – 289** | |
| **IR detection lamp** (left of pair) | ≈ 139 – 146 | **148 – 151** | red |
| **Radar detection lamp** (right of pair) | ≈ 179 – 186 | **148 – 151** | red; the pair is symmetric about the centreline (x = 164) |
| **Collective / Thrust bars** `C` `T` | 48 – 57 | 170 – 188 | two red vertical bars, letter beneath each |
| Weapon-quantity readout | ≈ 33 – 55 | 178 – 194 | green seven-segment |
| **Weapons Display** (screen) | 5 – 60 | 154 – 250 | |
| **Left TAC monitor** | 60 – 160 | 143 – 250 | bezel labels `F1 F2 F3` above, `F4 F5 F6` below |
| **Right TAC monitor** | 170 – 265 | 143 – 250 | bezel labels `F7 F8 F9` above, `F10 F11 F12` below |
| **Fuel bar** `F` | 272 – 273 | 174 – 188 | single orange vertical bar, letter beneath |
| `CHAFF:nn` / `FLARE:nn` | ≈ 278 – 310 | 172 – 192 | green text, two lines |
| **Threat Indicator** (screen) | 268 – 325 | 154 – 250 | |
| Pilot's legs / centre pedestal | centre | 250 – 289 | |

**The function-key labels are painted on the bezel**, which is why the manual can call the assignment
fixed: F1–F6 drive the left monitor, F7–F12 the right `[MAN p.45]`, and the pilot reads that off the
panel rather than off a menu.

The relationship between this 328 × 290 artwork and the 320 × 200 VGA screen is **not established** —
the file header carries both 328 × 290 and 320 × 200. Positions above are given in artwork pixels.

Callouts on the manual's p. 30 diagram, verbatim, with where they point:

| Callout | Points at |
|---|---|
| `I.R. & RADAR DETECTION LIGHTS` | the red pair, top centre of the panel |
| `COMPASS` | top centre, **inside the window** |
| `ARTIFICIAL HORIZON` | centre of the window |
| `RADAR ALTIMETER` | right edge of the window |
| `RATE OF CLIMB` | right, below the altimeter |
| `LASER TARGET LOCK` | upper left of the window |
| `SPEED GAUGE` | lower left of the window |
| `COLLECTIVE & THRUST GAUGES` | the `C` `T` bars |
| `FUEL GAUGE` | the `F` bar |
| `CHAFF & FLARE DISPLAY` | the two green lines |
| `WEAPONS DISPLAY` | left screen |
| `LCD TACTICAL DISPLAY MONITORS` | the two centre screens |
| `THREAT INDICATOR` | right screen |

**The compass, artificial horizon, radar altimeter, rate of climb, speed and laser-lock symbols sit
inside the window, not on the panel** — they are the HID, drawn over the world.

### 2. HID elements

| Element | Position | Definition | Source |
|---|---|---|---|
| **Compass** | top centre of the field of view | analogue compass ribbon; *"shows the aircraft's heading via a projected compass"* | `[MAN p.37, 73]` |
| **Artificial Horizon** | centre | shows true horizon even when *"obscured by darkness or objects such as mountains"*; carries **pitch and roll** | `[MAN p.35, 71]` |
| **Radar Altimeter** | **right** side | bar graph **plus digital**, **feet above the ground**, radar-sensed — *not* above sea level | `[MAN p.36, 72]` |
| **Rate of Climb** | right of the altimeter | climb/descent **relative to sea level** | `[MAN p.36, 73]` |
| **Heading Velocity Display** | centre | a **top-down vector**: a cross at the present position, a circle at the position **one second from now**, joined by a line. Angle = direction, **length = speed** | `[MAN p.36–37, 73]` |
| **Digital Speed Indicator** | **lower left** | true airspeed in **knots**; `1 kt = 1.15 mph` | `[MAN p.37, 73]` |
| **Laser Target Lock** | upper left | TAS lock state | `[MAN p.30]` diagram |
| **Target box** | wherever the target is | *"a green rectangle will mark your target"* on lock; may travel over cockpit interior | `[MAN p.35, 48]` |

The velocity display is the one element with no fixed-wing counterpart in this tree: it is a **map-frame
one-second position prediction drawn on the windscreen**, which is a hover/NoE instrument, not a
flight-path marker.

### 3. HID clutter control — the pilot may switch symbology off `[MAN p.70–73]`

Four independent switches in the Options menu. **All are on by default.**

| Switch | Turns off |
|---|---|
| `Artificial Horizon` | the horizon line alone |
| `HID Indicators` | **four together** — Radar Altimeter, Rate of Climb, Velocity Display, Speed Indicator |
| `HID Compass` | the compass alone |
| — | the manual recommends switching all three off on slow machines `[MAN p.65]` |

Three further switches change the flight model rather than the display, and belong here because they
are on the same menu:

| Switch | Default | Effect when **disengaged** |
|---|---|---|
| `Rotor Mixed with Cyclic` | engaged | tail rotor and cyclic become **independent** axes — *"more precise and realistic movement"* `[MAN p.71]` |
| `Altitude Stabilizer` | engaged | *"the Comanche's pitch is no longer held stable by the computer"* `[MAN p.72]` |
| `Stealth Mode` | engaged | *"enemies will be able to detect your presence from much farther"* `[MAN p.72]` |
| `Auto Chaff` / `Auto Flare` | engaged | countermeasures no longer dispensed automatically `[MAN p.72]` |
| `Missile Cam` | — | TAS display switches to a **missile-eye closing view** for Stinger and Hellfire `[MAN p.71]` |

**A declared HUD must carry all of this**, because two of the switches change what the aircraft does,
not what it shows.

### 4. Tactical Display Monitors — two identical screens, six pages each `[MAN p.38–45]`

*"Access to these TAC Displays is INDEPENDENTLY AVAILABLE on either Tactical Monitor"* — both may show
the same page, either may show any page, and if one is destroyed the other keeps all six `[MAN p.38, 58]`.

| Page | Left | Right | Content |
|---|---|---|---|
| **Digital Map** | F1 | F7 | contoured terrain from the "Optical Mission Disk", overlaid with detected units. Three zoom steps, `<` out and `>` in |
| **Threat Indicator** | F2 | F8 | incoming missile/rocket warning; also permanently present as the dedicated right-hand panel |
| **TAS Camera / Target** | F3 | F9 | gyro-stabilised close-up of the locked target; holds until the target dies, lock is lost, or a new lock is taken |
| **Mission Status** | F4 | F10 | **two numbers: assigned goals, and goals remaining** |
| **Damage Status** | F5 | F11 | an RAH-66 icon with per-system state |
| *(reserved)* | F6 | F12 | *"reserved for future sensor expansion"* — never used in the 1992 release |

`?` throws a help overlay on **both** monitors. `Q`/`W` step the left monitor back/forward through the
pages, `A`/`S` the right `[MAN p.45]`.

#### 4.1 Digital Map marker colours `[MAN p.40]`

| Colour | Means |
|---|---|
| **RED** | air threat |
| **YELLOW** | ground threat (T-80, Gecko) |
| **WHITE** | neutral object (fuel tanks) |
| **GREEN** | friendly aircraft (the wingman) |
| **BLUE** | friendly ground vehicle |
| **flashing border** | **this object is a mission completion goal** |

The flashing border is the display of the `*` flag in the mission file — see
[`campaign.md`](campaign.md) §5. Manual: *"Not all enemies are mission goals. Only those with flashing
markers count towards mission completion."* `[MAN p.42]`

The map is explicitly **not** a truth feed: *"Not all of your threats can be picked up by your automated
systems. Your direct view through the HID will be the only way to see some hidden ones."* `[MAN p.40]`
Sourcing is named — *"AWACS, surveillance satellites and other information gathering resources"*
`[MAN p.38, 40]` — and own position comes from *"a Ring Laser Gyro-based Inertial Navigation System with
the Global Positioning Satellite System … within a few meters"* `[MAN p.39]`.

#### 4.2 Mission Status is the mission's own verdict counter

Two integers: goals assigned, goals remaining `[MAN p.42–43]`. That is exactly the starred object count
of [`campaign.md`](campaign.md) §2 — **the game ships a countdown of its own success condition**, and a
rebuild that judges a mission by anything else has invented a second truth.

### 5. Weapon Select Display `[MAN p.47–53]`

Far-left panel. Shows the selected weapon **and its remaining count**.

| Key | Weapon | Behaviour |
|---|---|---|
| `Z` | 20 mm Gatling | auto-aimed by TAS when a target is selected; **500 rounds at 1 500 rpm — under one minute of fire** |
| `X` | 70 mm rockets | salvo of 1 or 2; **unguided**, fire along the nose; TAS aims azimuth only |
| `C` | AGM-114 Hellfire | **lock must be held to impact**; may be re-targeted in flight |
| `V` | AIM-92 Stinger | **fire-and-forget**; keeps its own lock even if the TAS re-locks |
| `B` | Artillery (155 mm / MLRS) | off-board; TAS coordinates → C2 net; **travel time**, so poor against movers |
| `N` | Wingman | the wingman moves into a firing position and puts **his** Hellfire on **your** TAS lock |
| `M` | — | **always live**: fires a 2-rocket salvo regardless of the selected weapon |
| `[` `]` | — | previous / next weapon |

Firing sequence `[MAN p.48]`: select weapon → `Enter` (or joystick #2) to lock with the TAS → green
rectangle appears and the TAS page shows a close-up → `Space` (or joystick #1) to fire.

**The counts are per mission and come from the mission file**, six fields in the same order as this
table — derivation and per-mission values in [`campaign.md`](campaign.md) §5.1.

Two consequences a declared HUD must carry:

1. **Selecting `N` is not selecting a weapon**, it is commanding another aircraft into an exposed
   position: *"Be careful not to keep your wingman in a precarious position for too long. The enemy could
   concentrate their fire on him"*, and if he dies the option dies with him `[MAN p.52]`.
2. **Artillery availability varies per mission** and is zero in five of ten `[MIS]`.

### 6. Panel gauges `[MAN p.54–56]`, `[ART]`

| Gauge | Marking | Reads |
|---|---|---|
| **Collective** | `C`, red vertical bar | the **computer's chosen** collective pitch, not the pilot's demand — fly-by-wire. *"Keeping the gauge marked 'C' near the bottom of its range will allow for easy Nap-of-the-Earth (NoE) flying"* |
| **Thrust** | `T`, red vertical bar | the computer's chosen throttle setting |
| **Fuel** | `F`, orange vertical bar | fuel remaining; initial quantity is per mission |
| **Chaff / Flare** | `CHAFF:nn` `FLARE:nn`, green | counts remaining; **automatic release by default**, manual `;` and `'` |
| **IR lamp** | left red lamp | own IR sensors have seen an approaching heat signature |
| **Radar lamp** | right red lamp | own passive gear has seen a nearby radar — *"very similar to a radar detector in your car"* |

The two bar gauges are a **fly-by-wire read-out, not a control position**. A rebuild that draws them
from stick position has drawn the wrong thing.

### 7. Damage display `[MAN p.57–58]`

Six areas, each with a **named flight consequence**:

| Area | Consequence |
|---|---|
| **Tail rotor** | the airship *"wants to spin"*; caused by flying **backwards** |
| **Engine** | limits **altitude and speed** |
| **TAS** | lock becomes hard to hold → **removes Hellfire**, which needs lock to impact; further damage removes it entirely |
| **Weapon mount** | **no Hellfire, Stinger or rockets** — the external mounts are gone |
| **20 mm cannon** | the computer **disables** the gun to prevent a misfire |
| **TAC display** | one monitor dies; the other retains all six pages, but only one at a time |

Cause named: *"Slamming into objects nose first will usually result in Cannon or TAS damage."*

### 8. External views `[MAN p.30–34]`

| Key | View | HID active? |
|---|---|---|
| `1` | Forward cockpit — panel, both TAC monitors, full HID | **yes** |
| `2` | Left cockpit | yes |
| `3` | Right cockpit | yes |
| `4` | Rear — shows the co-pilot/gunner and anything on your six | yes |
| `5` | Panoramic front — **no struts, no head-down gauges**, HID and targeting still live | **HID only** |
| `6` | Chase | **none** |
| `7` | **Drop camera** — deposits a ground camera at the present position; it then tracks the Comanche | none |
| `8` | Reactivate the last drop camera | none |

Only **one** drop camera exists at a time; dropping a new one supersedes the old. Views 1–4 are also on
the joystick hat.

### 9. Control bindings `[MAN p.16–22, 45, 53, 63–69]`

| Input | Function |
|---|---|
| `E` / `D`, or `−` / `=`, or throttle axis | **collective** up / down |
| arrows or numeric keypad, or stick | **cyclic** — pitch, roll, yaw |
| `Insert` / `Delete` | **fantail rotor**: turn in place left / right |
| keypad `*` | **auto-hover** — stabilises into a hover *"at low altitude … an ideal height for NoE flying"* |
| `Enter` / joystick #2 | TAS lock |
| `Space` / joystick #1 | fire |
| `;` / `'` | manual chaff / flare |
| `F1`–`F12`, `Q W A S`, `?`, `<` `>` | TAC monitors |
| `Z X C V B N`, `[` `]`, `M` | weapons |
| `1`–`8` | views |
| `Esc` | menu bar, in flight as well as out |
| `Alt-A` / `Alt-Q` | abort mission / quit |
| `J` | re-calibrate joystick, in flight |

**Ground effect is a modelled control aid**, not a physics footnote: *"This effect combined with the
fly-by-wire system monitoring your distance to the ground, makes flying low very easy. If you set your
collective low or press the numeric keypad '✱' you can then for the most part track the ground without
any further collective adjustment."* `[MAN p.18–19]`

### 10. Night `[MAN p.22]`

> *"During all of your night missions … the Cockpit Main Screen Display will appear in tones of green
> and black. This means that your Image Intensifiers and Thermal Imagers are on-line … Your two Tactical
> Monitor Displays will also be operating in this mode."*

Image intensifiers *"amplify moon and starlight over 30,000 times"*; FLIR *"looks for temperature
differences"*.

Implementation, measured: **a palette swap**, not a lighting model. The night missions load an
`n`-prefixed 941-byte file whose 768-byte palette is green-dominant where the day palette is
red-dominant — numbers in [`terrain.md`](terrain.md) §1. **Both TAC monitors change too**, so the swap
is global, not per-viewport.

## State

**Nothing.** The HUD in this tree is C++ and fixed-wing. Nothing here is declared, drawn or measured
against.

## Gaps

- **The HUD is C++, so "a HUD per title, declared" has no surface to be declared into**
  ([`doc/mods.md`](../../../doc/mods.md) `## Gaps`). Every element above is a specification without a
  target format.
- **A helmet-slaved display does not exist in this tree.** Symbology that follows head aim and can
  target out of the side and rear views has no counterpart in a fixed-wing HUD, and it is not a
  rendering detail — it decides where a shot may be taken from.
- **Two independent multi-function displays with six interchangeable pages each** is not in the tree.
- **The velocity display's one-second position prediction** is defined but no numeric example exists in
  the manual, so the scale of the vector on screen is unknown.
- **The 328 × 290 artwork versus 320 × 200 screen relationship is unresolved.** Absolute pixel positions
  in §1 cannot be converted to screen coordinates until it is.
- **No in-game screenshot was cross-checked in this run.** §1 rests on the game's own console art and
  the manual diagram, which agree — but neither shows the HID *drawn over terrain*, and the HID is the
  half that matters. The F-22 sibling document did have such a cross-check; this one does not.
- **The Threat Indicator's symbology is undocumented.** The manual describes what it warns about and how
  reliably (*"very effective in spotting radar-guided missiles and somewhat less effective in spotting
  laser- and IR-guided threats"*, `[MAN p.41, 46]`) but never says what is drawn.
- **The Damage Status icon's states are undocumented** — six areas are named, the display is described
  as *"an icon representing your RAH-66"*, and nothing says how a damaged area is shown.
- **The digital map's contour rendering is undescribed.** *"downward-looking contoured Terrain Map"* is
  all there is; the maps themselves are decoded ([`terrain.md`](terrain.md) §1) but how they were drawn
  on a TAC monitor is not known.
- **`HUDS.RLE` (2 940 B) and `CONS.RLE` (17 023 B) were not decoded.** They use a different container
  (`KRL1`) from the `Kyle DTA` images and almost certainly hold the HID symbol set and the console
  overlay — i.e. the exact pixel truth this document is missing.
