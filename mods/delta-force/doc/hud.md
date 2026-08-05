# Delta Force (1998) — the original's HUD, element by element

> **Source documents.**
> 1. **Delta Force Field Manual FM 365-7**, *Game Screen*, printed pp. **14–15** (PDF pp. 19–20);
>    *Game Controls* pp. 10–11 (PDF 16–17); *Keyboard Layout* pp. 12–13 (PDF 18);
>    *Gear* pp. 8–9 (PDF 14–15); *Settings* pp. 5–7 (PDF 11–13).
>    **Manual Addendum FM 365-7A**, *Game Screen* printed pp. 3–4 (PDF 3), *Additional Keyboard
>    Commands* p. 5 (PDF 4). Both HUD diagrams were **read as page images** in this run at 400–700 dpi,
>    not from OCR text.
> 2. **The shipped game data** — `DFGAME.BIN` group *Overlays* / *Misc* / *Canned Msg* / *Fire
>    Missions*, and `DFDLG02.BIN`, read in this run: these are the literal strings the HUD prints.
> 3. Two publisher-hosted screenshots of the original, read as images in this run.
>
> Form per [`doc/mods.md`](../../../doc/mods.md) §3.

Tags as in [`campaign.md`](campaign.md); `[MAN p.N]` = printed page, `[SHOT]` = screenshot read here,
`[GAME]` = verbatim string from `DFGAME.BIN`.

## Spec

### 1. Shape of the display

**Everything numeric is in one bar at the TOP. The bottom bar is text only. There is nothing at the
sides.** (The successor DF2 moves the same set to the bottom and replaces the compass rose with a
minimap — do not copy that.)

```
┌──────────────────────────────────────────────┬───────────────────────────────────────────┐
│ ▌ 🯅  M4 Assault Rifle, Burst                 │        N     ┌──────────┬────────────────┐│
│ H    ┌──────────┐┌──────────┐▌  ▬▬weapon▬▬   │  W ( · )·E   │   I13    │ Carrying: …    ││  ← status bar, TOP
│ P    │ 🯄  23   ││    30    │                │        S     ├──────────┴────────────────┤│
│      └──────────┘└──────────┘                │              │ WP3: CP BRAVO (117m)      ││
└──────────────────────────────────────────────┴──────────────┴───────────────────────────┘
│                                                                                          │
│   ┌───────────────┐                3-D view                    ┌──────────────┐          │
│   │   GPS map     │             crosshair at centre            │   Forward    │          │
│   │  (F10 / F11)  │             weapon lower right             │   Observer   │          │
│   └───────────────┘                                            │     (F9)     │          │
│                                                                └──────────────┘          │
┌──────────────────────────────────────────────────────────┬───────────────────────────────┐
│ Proceed west and eliminate all hostiles in the villa.    │ COL. AUSTIN                   │  ← text bar, BOTTOM
│ Do not shoot the target, he must be taken alive.         │ Headhunter                    │
└──────────────────────────────────────────────────────────┴───────────────────────────────┘
```

Layout read from the two labelled diagrams and both screenshots. The GPS map and the Forward Observer
window are **pop-ups over the 3-D view**, not permanent frames.

### 2. The ten named elements

The manual numbers them twice and differently; the names are identical in both. Field-Manual order is
used here, with the Addendum's number in brackets.

| # | Element | Position | Content | Source |
|---|---|---|---|---|
| 1 [1] | **Health Bar** | far left of the top bar, vertical | *"If you have the Player Health Setting on 'Easy', your current health will be represented by this bar."* — **absent otherwise**, measurably: the Insurrection screenshot has no bar `[SHOT]`. Red/orange fill | `[MAN p.14]` |
| 2 [5] | **Situational Icon** | top bar, right of the health bar | a green soldier profile, **three states: Stand / Crouch / Lie Prone** | `[MAN p.14]`, image read |
| 3 [2] | **Current Weapon Selected** | top bar, centre-left | one text line **`<weapon>, <fire mode>`**; two boxed numbers — **magazines** (with a magazine glyph) and **rounds left in the current magazine**; a narrow box right of them **filled when a round is in the chamber**; a weapon silhouette right of that | `[MAN p.14]`; values seen: 11/21 and 22/11 `[MAN]`, 23/30 `[SHOT]` |
| 4 [3] | **GPS Map** | pop-up window, left of centre | overhead satellite map, colour or contour; symbology in §4 | `[MAN p.14]` |
| 5 [4] | **Information Link** | bottom bar, left, multi-line, **white text** | *"Important information, such as current mission orders and status of your squad, will be transmitted over your radio. Text sent from other players in a multiplayer game will appear here as well."* | `[MAN p.15]`, colour from `[SHOT]` |
| 6 [7] | **Grid Coordinates / Items** | top bar, right, **two boxes side by side** | left = current grid square as on the Command Map (`I13` `[SHOT]`, `M13` `[MAN p.14]`); right = the mission item being carried; **D** drops it | `[MAN p.15]` |
| 7 [8] | **Waypoint Indicator** | top bar, right, full width **under** element 6 | *"code name and distance in meters to your next waypoint"* — format **`WP3: CP BRAVO (117m)`** `[SHOT]`, `WP1: IP (520m)` `[MAN p.14]` | `[MAN p.15]` |
| 8 [6] | **Compass Heading** | top bar, between weapon block and grid box | **a round rose** with N/E/S/W, not a tape. *"The color of the center dot represents your team side. The direction of your next waypoint is connoted by a green dot."* In CTF the flag bay appears as a team-coloured dot | `[MAN p.15]`, Addendum p.3; blue centre dot + green waypoint dot on the ring `[SHOT]` |
| 9 [9] | **Forward Observer Camera** | pop-up window, right of centre | *"Press F9 … You can cycle through your view and that of your teammates with the TAB key."* | `[MAN p.15]` |
| 10 [10] | **Mission Information** | bottom bar, right, two lines, **green text** | operative name / current mission / a mission timer *"if appropriate to the mission type"*. Seen: `BLACKHAWK` + `Insurrection` `[SHOT]`, `COL. AUSTIN` + `Headhunter` `[MAN p.14]` | `[MAN p.15]` |

Not numbered by the manual but present:

| Element | Behaviour | Source |
|---|---|---|
| **Crosshair** | small, green, screen centre; **F3** toggles it, **F4** hides the weapon | `[MAN p.11]` |
| **Friendly tag** | teammate's name over the man, green (`ALPHA ONE`); **F** toggles | Addendum p.5, p.3 |
| **Friendly Fire Warning** | *"the Red safety X that appears when you target your team members"*; can be switched off | `[MAN p.7]`, Addendum p.7 |

### 3. Literal strings the HUD prints `[GAME]`

Measured, not paraphrased — these are the exact byte strings in `DFGAME.BIN`.

| Group | Strings |
|---|---|
| **Carried item** (element 6) | `Carrying: Nothing` · `Carrying: Black Box` · `Carrying: Laptop` · **`Carrying: Code Book`** · `Carrying: Case` · `Carrying: Metal Case` · `Carrying: Bomb!` · `Carrying: Red Flag` · `Carrying: Blue Flag` · `Carrying: Unknown` |
| **Waypoint prefix** | `WP` |
| **Binocular mode** (B) | `Heading:` and `Distance:` with format `%s %3im`, plus **`Distance: 1km+`** — the readout **saturates at 1 km** |
| **Laser designator** (0) | `Range:` / **`Range: 1km+`** · `No Target` · `Laser Active` |
| **Fire mission** | `Fire Mission request, over.` → `Roger, Copperhead is on the way.` or `Fire Mission: request denied.` — the artillery round the designator calls is named **Copperhead** |
| **Player state** | `You are dead.` · `Respawn allowed in:` · `Ammo reloaded.` |
| **End of mission** | `Congratulations!  Mission Completed Successfully.` · `Game Over.  Mission Incomplete.` · `Hit Enter to end mission.` · `Hit Space Bar to replay mission` · `End Mission? (Y/N)` |
| **Kill messages** (Information Link) | `$A killed $B.` · `$A shot $B in the head.` · `$A knifed $B.` · `$A slit $B open.` · `$B fell on $A's blade.` · `$A ended $B's misery.` · `$A murdered $B.` · `$A killed himself.` · `$A got tired of life.` · `$A decided it was all too much.` · `$A is dead.` |
| **Loading screen**, in order | `Loading Mission...` · `Establishing Data Link...` · `Loading Mission From Server...` · `Analyzing Mission Topography...` · `Preparing Equipment...` · `GPS Coordinates Locked...` · `Satellite Uplink Confirmed...` · `Drop Zone Confirmed...` · `Stand by for Insertion...` |

**The binocular question is settled by these strings.** A publisher screenshot shows a green-tinted
observation view with `Heading: 302` and `Distance:` overlaid; the game has no night-vision key
(the whole key list is in §6, and `N` is unbound in DF1 — the successor DF2 adds `N Night Goggles`).
`B Binocular Mode` `[MAN p.10]` plus the two format strings identify that view as the **binocular**,
with a heading and a range readout that saturates at 1 km.

### 4. GPS map symbology — read from the legend image `[MAN p.15]`

Shapes **and colours** taken from the drawing, not from the caption text:

| Symbol | Colour | Meaning |
|---|---|---|
| circle | green | Initial / Final waypoint |
| diamond | green | waypoint — **the next one blinks** |
| circle with a radial tick | **blue** | teammate, tick = facing |
| circle with a radial tick | **red** | enemy, tick = facing |
| rectangle | green | building |
| plus / cross | green | vehicle |
| triangle | red | red team flag (CTF) |
| triangle | blue | blue team flag (CTF) |

The contour map (F11) additionally draws a **north arrow labelled `N`** at the top right of the window
and **connects the waypoint diamonds with straight lines** (Addendum p.3, image read). Dead enemies
persist as **X** marks — visible in the Headhunter diagram `[MAN p.14]`, where the objective area shows
both red circles and red X's.

Which enemies appear at all is a setting: *"By 'Default', each mission shows icons for certains enemies
on the GPS Map. You can select to 'Show Friendly' … or 'Show Everything' to see all enemies"*
`[MAN p.7]`. **So part of the opposition is on the map from the start and part is not** — a mission
declaration has to carry that per unit.

### 5. Command Map (C) — full screen

*"Grid coordinates overlay the terrain, shows way points, lines to waypoints, and all other pertinent
information known to the player."* Addendum p.5. Zoom `–` / `=` `[MAN p.11]`.
The grid square shown in HUD element 6 is *"your current grid location as seen on your Command Map"*.

### 6. Keys `[MAN pp.10–13]` + Addendum p.5

| Weapons and gear | | Movement / posture | |
|---|---|---|---|
| 1 | Knife | Delete | Stand |
| 2 | Sidearm | End | Crouch |
| 3 / 4 / 5 | Primary weapon mode 1 / 2 / 3 | Page Down | Lie Prone |
| 6 / 7 / 8 | Secondary weapon mode 1 / 2 / 3 | arrows | forward / back / strafe |
| 9 | Fragmentation grenade | Shift+arrow | walk |
| 0 | Laser designator | Space | jump |
| Backspace | cycle weapons | | |
| M | change magazine (**discards the remaining rounds**) | | |
| S / RMB | toggle scope | | |
| B | binocular mode | | |

| Views | | Information | |
|---|---|---|---|
| F1 | Help | C | Command Map |
| F2 | First person POV | G | Mission Goals — line by line, **lines change colour as goals are met** |
| F3 | Crosshair on/off | O | Mission Orders — the full briefing |
| F4 | First-person weapon on/off | R | recent messages |
| F5 | External view (3rd person) | F | friendly tags on/off |
| F6 | Fixed angle view | K | kill / player list (multiplayer) |
| F7 | Fixed location view | W | cycle waypoints |
| F8 | Picture in picture | Q | cycle enemy flags (CTF) |
| F9 | **Forward Observer view** | T / Y | talk / team talk |
| F10 | **GPS colour map** | A | play audio command |
| F11 | **GPS contour map** | H | toggle turbo |
| F12 | Letter box on/off | D | drop items |

**Contradiction inside the manual, kept.** The *Game Screen* text says *"Press F9 (color map) or F10
(contour map) to bring up your … GPS overhead map"* — in **both** the Field Manual (p.14) and the
Addendum (p.3) — while the same Addendum page also says *"Press F9 to bring up your Forward Observer
view"*, i.e. F9 has two jobs on one page. The key reference `[MAN p.11]` and the keyboard-layout
drawing `[MAN pp.12–13]`, two independent listings, agree on **F9 = Forward Observer, F10 = GPS colour,
F11 = GPS contour, F12 = letter box**. That assignment is preferred; the conflict is not resolved by
measurement.

### 7. Scope and weapon display

| Weapon `[MAN p.8]` | Magazine | Scope | Fire modes on keys 3/4/5 |
|---|---|---|---|
| M4 5.56 mm carbine + M203 40 mm | 30 rounds, 18 grenades | **4×** | Burst M4 / M203 / Single shot |
| H&K 9 mm MP5, integral suppressor | 30 rounds | none | Full auto / Burst / Single shot |
| M249 SAW | 200 rounds per box | none | Semi auto / Full auto |
| Remington 7.62 mm M40A1 | 5 rounds | **8×** | — |
| Barrett .50 cal | 10 rounds | **8×** | — |

The weapon-name field of HUD element 3 prints the **weapon plus its mode**, and the shipped name list
`[GAME]` gives every string it can show, e.g. `M4 Assault Rifle, Burst`, `H&K MP5 SD, Single Shot`,
`SAW Auto. Weap., Full Auto`, `Barret .50 Caliber Sniper Rifle` (the misspelling is the game's).
That list also contains weapons **not offered in the gear screen** — `Garrote`, `Shotgun`, `Ingram M11`,
`Stinger`, `RPG`, `AK 47`, `Smoke Grenade`, `M16`, `M89`, `Camera` — evidence that the HUD's weapon
field is generic, not campaign-specific.

Secondary weapons and standard gear `[MAN p.9]`, because they drive HUD element 3's mode line:
2 satchel charges with radio detonator (place = 6, detonate = 7) · 2 M18 claymores (6 = motion sensor,
7 = radio, 8 = fire) · 2 LAWs · or a double ammo load. Always carried: **6 fragmentation grenades, the
laser designator, the Ka-Bar knife**. There are no smoke or flash grenades in the gear screen.

### 8. What the HUD does not have

Checked against the whole manual and the string tables — each of these is an **absence**, not an
omission of research: no compass tape · no minimap in the permanent HUD · no ammunition count for
grenades in element 3 · no stamina or sprint indicator · no zeroing or range dial on the sniper scope
(the range readout exists only in binocular and designator mode, §3) · no night-vision mode · no wind
indicator.

## State

**Nothing.** The FlightBox HUD is C++ and F-16-shaped; there is no declaration format a per-title HUD
could be expressed in ([`doc/mods.md`](../../../doc/mods.md) `## Gaps`).

## Gaps

- **No pixel geometry.** §1 is a position map read off two diagrams and two screenshots; no element's
  size or offset in pixels is recorded, and the original ran 320×240 to 800×600 `[MAN p.6]`, so the
  layout is presumably resolution-relative. Not measured.
- **Colours are named, not specified.** Green, red, blue, white — no values. The HUD green could be
  sampled from the screenshots; it was not.
- **Element 3's mode-line grammar is inferred** from the string table: it is `<name>, <mode>` for
  weapons that have modes and `<name>` alone for those that do not. No statement in the manual.
- **The mission timer of element 10 is never shown.** *"if appropriate to the mission type"* — which
  types, and its format, are unknown; no Peru mission is known to use it.
- **Team-side colour of the compass centre dot** is stated but the single-player value is only observed
  once (blue, `[SHOT]`). `DFGAME.BIN` names eight teams (`Blue, Red, Green, Yellow, Orange, Indigo,
  Violet` + `Null Team`), so the mapping to single player is unproven.
- **F9/F10 conflict unresolved** (§6) — decided by majority of listings, not by measurement.
- **Fire missions are a HUD feature with no mission behind them.** The `Copperhead` strings exist and
  the laser designator is standard gear, but no Peru mission is known to enable artillery. Whether the
  request is always denied in campaign one is untested.
