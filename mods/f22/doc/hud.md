# F-22 Lightning II — HUD, MFDs and the sensor mode logic behind them

> **Source document:** research distillation
> `scratchpad/novalogic/f22.md`, §5 (l. 304–353), §6 (l. 356–458).
> **The manual's full text was retrieved in this run** and every element below is quoted from it
> directly, not relayed. That closed several items the source left open — see §12.
> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has no `test/`.

Provenance tags:

| Tag | Means |
|---|---|
| `[MAN v]` | **verbatim** from the manual full text, read in this run |
| `[MAN d]` | callout on one of the two labelled HUD **diagrams**; OCR of a drawing, so position is inferred from the callout's reading order |
| `[MAN p.N]` | page number confirmed by a surviving OCR page footer |
| `[MAN p.N*]` | page number from the table of contents (pp. i–ii) only — **no footer survived**, so the page is unconfirmed |
| `[SHOT]` | seen on a screenshot by the source research; **not in the manual** |

The manual is explicit about what its HUD chapter is: *"this section will merely describe the symbology
as it appears on-screen. The actual functionality of the targeting systems and methodologies is covered
in the appropriate sections elsewhere"* `[MAN v]`. §10 below is that other material, gathered back in,
because symbology without its mode logic is not rebuildable.

## Spec

### 1. Frame

The F-22 is flown from a **virtual cockpit**, not a full-screen HUD: canopy and frame struts are
visible left and right, and a blue message line runs along the bottom (`CLEARED FOR TAKEOFF`) `[SHOT]`.
Neither is labelled in the manual.

Two labelled HUD diagrams exist, `[MAN p.30]` (footer confirmed) and `[MAN p.31*]`, plus a third,
targeting-state diagram at `[MAN p.32*]`. Together they name every element.

### 2. Element inventory

| Element | Position | Content / sample | Provenance |
|---|---|---|---|
| **Compass Heading Indicator** | horizontal line across the **top** | periodic heading marks; sample band `315 310 305`, screenshot `340 · N · 20` | `[MAN v]`, `[SHOT]` |
| Current heading | small **rectangular box centred on** the compass line | — | `[MAN v]` |
| **Waypoint steering cue** | caret **on** the compass band | — | `[MAN d]` |
| **Off-HUD waypoint steering cue** | above/outside the band | the caret **becomes** a steering cue when the selected waypoint's heading is not currently shown on the compass band | `[MAN v]` |
| **Waypoint Data**, line 1 | upper left | waypoint ID — `WAYPOINT 2` | `[MAN v]` |
| Waypoint Data, line 2 | upper left | special instruction for that waypoint — `FOLLOW ROUTE` | `[MAN v]` |
| Waypoint Data, line 3 | upper left | range to waypoint in **NM** — `RANGE: 27.91` | `[MAN v]` |
| **Airspeed Indicator** | vertical line down the **left** | knots up to Mach 1; **a Mach number above Mach 1** | `[MAN v]` |
| **Altimeter** | vertical scale down the **right** | feet **above sea level** | `[MAN v]` |
| **Pitch Ladder** | centre, horizontal rungs | pitch angle; **the tick marks on the rung tips always point to the horizon** | `[MAN v]` |
| **Horizon Indicators** | centre | callout on the p.30 diagram; adjacent numerals `280` and `27.0` — **association ambiguous in OCR, TODO** | `[MAN d]` |
| **ASE Circle** (Allowable Steering Error) | large circle, centre | kill probability with a missile is greater with the target inside it before firing | `[MAN v]` |
| **Gunsight Pipper** | centre, **replaces the ASE circle** when guns are selected | computes fall of shot from wind speed, yaw and gravity | `[MAN v]` |
| **Flight Path Indicator** | tiny dot, dead centre | "indicates where the nose of your aircraft is **actually** pointing" | `[MAN v]` |
| **Engine Management Data**, 4 lines | lower left | `THR: 85%` (thrust) · `+01.0G` (G meter) · `FUEL: 14396 LBS` · `AIRFRAME: 100%` | `[MAN v]` |
| — Airframe Integrity | 4th line of that block | *"the remaining damage you can absorb. When this gauge gets to zero, you've had it."* | `[MAN v]` |
| **Shoot List Data**, 4 lines | lower right | see §4 | `[MAN v]` |
| **Target Steering Cue** | lower right on the diagram; wherever the target lies in reality | tiny circle; **replaces the Target Designation Box** when the target leaves the HUD, and points the way back to it | `[MAN v]` |
| **Weapon Indicators** | across the **bottom** | selected weapon + rounds remaining — `6 AIM-120 AMRAAM` | `[MAN v]` |
| **Status fields** | bottom strip | `GEAR` (down) · `FLAPS` (extended) · `BRAKE` (extended) · `RADAR` (on) | `[MAN d]`, `[MAN v]` |
| **Attack Display inset** | **upper right** | miniaturised Attack Display; **aircraft appear as "T" shapes**; toggled on/off | `[MAN v]` |
| **Artificial Horizon inset** | upper right of *other* displays | same toggle, when not on the HUD | `[MAN v]` |

`RADAR` is stated separately: *"The word RADAR appears on your HUD when your radar is turned ON"*
`[MAN v]`.

### 3. Layout

Reconstructed from the callouts' reading order on both diagrams. This is a **position map, not a
pixel layout** — no coordinates exist in any source read.

```
      ┌ waypoint steering cue (caret) ────────────────────────────────┐
   ───┴──── 315 ── 310 ──[ 308 ]── 305 ─────── compass heading band ──┴───
   WAYPOINT 2                                                  ┌───────────┐
   FOLLOW ROUTE                                                │ atk inset │
   RANGE: 27.91                                                │  (T's)    │
                                                               └───────────┘
      ▲                          ,--..--.
      │ airspeed         ── ── ──(   ·   )── ── ──  pitch ladder rungs      ▲ altitude
      │ (kt / Mach)              `--'`--'           · = flight path ind.    │ (ft MSL)
      ▼                        ASE circle                                   ▼
                        (gunsight pipper when guns)

   THR: 85%                                                        SHOOT LIST
   +01.0G                                                       MIG-27 ALPHA 1
   FUEL: 14396 LBS                                                 VC: 1126
   AIRFRAME: 100%                                                RANGE: 17.24
                        6 AIM-120 AMRAAM
                        GEAR  FLAPS  BRAKE  RADAR
```

### 4. Shoot List block and the engagement sequence

Lower right, four lines `[MAN v]`:

| Line | Content | Sample `[MAN p.32*]` |
|---|---|---|
| 1 | target type and ID | `MIG-27 ALPHA 1` |
| 2 | closure velocity | `VC: 1126` |
| 3 | range to target | `RANGE: 17.24` |
| 4 | target's approximate **airframe integrity** | — |

Inactive, the block reads only `SHOOT LIST` `[MAN d]`.

**The firing sequence, verbatim in substance `[MAN v]`:**

| Step | What appears | Colour |
|---|---|---|
| 1 | radar **on**, weapon selected, enemy inside about **25 miles** on the Attack Display | red triangles |
| 2 | create Shootlist — the four nearest eligible targets are taken and prioritised | — |
| 3 | on the HUD, a **Target Designation Box** appears around the current target | **green square** |
| 4 | target enters the **selected weapon's** range: a **target diamond** appears and travels toward the box | **green diamond** |
| 5 | box and diamond meet → both turn **bright red**, with **rapid beeping**. This is the **Shoot Cue** | **red** |
| 6 | cycle to the next Shootlist target; a new green box appears | green again |

*"As long as the box remains green the target is safe"* `[MAN v]` — green means out of range, not
merely un-designated. That is the whole weapon-envelope cue, and it is one symbol.

### 5. Instrument Landing System

**Three red lines, two vertical and one horizontal, centred in the HUD.** Active **only** within
**6 miles of the runway and below 5,000 ft AGL** `[MAN v]`, `[MAN p.31*]`.

| Line | Name | Reading |
|---|---|---|
| horizontal | **Glideslope Indicator** | centred (or slightly below centre) = on profile · **below** centre = you are **too high** · **above** centre = you are **too low** |
| vertical, **dashed** | **Yaw Deviation** | gets the **nose** pointed at the runway; centred = pointing at the centreline |
| vertical, **solid** | **Localizer** | gets you **onto** the runway centreline; centred = in line |

Both vertical lines overlapping into one = lined up. Overlapped verticals plus a centred glideslope
form **a cross** — that is the on-profile picture `[MAN v]`.

**Defect in the manual, recorded not repaired:** the deviation sense is given only for one direction.
Verbatim: *"If you yaw too far left, the line moves to the right in the display. If you yaw too far
left, the deviation line moves to the right."* — and identically for the Localizer, *"If you are flying
left of the runway centerline the Localizer moves to the right… If you are too far left, the Localizer
line moves to the right."* **The right-hand case is missing from both.** Symmetry is the obvious
inference and is exactly what must not be written down as fact here.

### 6. MFD pages — seven, each full screen

There is **no simultaneous multi-MFD arrangement.** Every page is called full-screen from the numeric
keypad; only the Attack Display additionally exists as a HUD inset `[MAN v]`.

| Keypad | Display | Page | Content |
|---|---|---|---|
| **2** | **Stores Management** | `p.35*` | loadout graphic; **Thrust Circles**; **WIP** (Weapon In Priority) text box with selected weapon and status; counters for Guns (20 mm shells) / AIM-9X / AIM-120 / JDAM / Chaff / Flares |
| **4** | **Defense Display** — the keyboard chapter calls the same page **"Threat Display"** | `p.37*` | §8 |
| **5** | **Navigation Display** | `p.38*` | §9 |
| **6** | **Attack Display** | `p.36*` | §7 |
| **7** | **HUD Repeater** | `p.39*` | queue of the **last six** HUD messages; **no scrolling back** beyond them |
| **8** | **Atk/Nav Overlay** | — | toggles the thumbnail Attack Display into the HUD's upper right; inserts the Artificial Horizon into the *other* displays |
| **9** | **Artificial Horizon** | `p.39*` | horizon indicator, pitch ladder, airspeed indicator, **remainder cut off in the retrieved text — TODO** |

Keypad bindings are `[MAN v]` from the keyboard reference, which OCR'd cleanly. The display chapter's
own inline key labels are garbled and contradict it (it prints the Attack Display as both "Numeric
Keypad (4" and, in the tutorial, "Numeric Pad [5]"). **The keyboard reference is taken as authoritative
and the conflict is recorded, not silently resolved.**

### 7. Attack Display

| Symbol | Meaning |
|---|---|
| blue F-22 silhouette | own aircraft, **stays centred** |
| blue circle | wingman |
| green circle | other friendly aircraft |
| white square | **unidentified** air target |
| red triangle | hostile air target |
| red circle | hostile **ground** target |
| white lines | missile tracks |

**Lead lines** protrude from every object; **length ∝ speed** `[MAN v]`.

**Shootlist marking:** all four targets carry a **large open white circle** and a **red ordinal 1–4**;
the **first** target's circle is **solid** `[MAN v]`.

**Side bars** `[MAN v]`:

| Side | Bar | Reading |
|---|---|---|
| left | altitude of **all four** Shootlist targets, thousands of ft | — |
| right | range band **of the currently selected weapon** — top tick = max range, top of the bottom box = min range, the small circle inside the box = own aircraft |

**Target data block, lower left**, six lines `[MAN v]`, with the manual's own sample:

| Line | Meaning | Sample |
|---|---|---|
| 1 | type + wing designation | MiG-27, third aircraft of Alpha Squadron |
| 2 | target altitude, thousands of ft, **MSL not AGL** | 19,900 ft |
| 3 | bearing to target, degrees | 351° |
| 4 | distance, NM | 11 |
| 5 | target speed, **Mach** | 0.9 |
| 6 | velocity of closure, kt — **negative = opening** | 478 |

**Boresight** locks whatever is directly ahead — *"the nearest object within your ASE circle"* `[MAN v]`,
keyboard chapter. The display chapter prints the key as `L`, the keyboard chapter as `'`; conflict
recorded.

### 8. Defense Display — the stealth instrument

This is **not** a classic RWR ring. It is a plan view with a range scale in NM on which **each ground
radar site is drawn as its own red circle whose radius is that site's detection range against the
player's momentary signature** `[MAN v]`.

| The circle expands when | Provenance |
|---|---|
| the **weapons bay opens** to fire a missile or drop a bomb | `[MAN v]` |
| the **radar is turned on** | `[MAN v]` |
| the **landing gear is lowered** | `[MAN v]` |
| the **airframe takes damage** | `[MAN v]` |

Also shown: nearby aircraft icons; **incoming missiles as white lines whose length is proportional to
the distance to the target**; chaff and flare counts lower left `[MAN v]`.

**The one hard number in the whole stealth model** `[MAN v]`:

> *"Note that even at your highest degree of stealth, a SAM radar can still detect your F-22 at a range
> of 4 miles."*

The manual's own instruction for observing the effect is a usable test case: fly Training Mission Four
against the mock-up airbase, toggle the radar, and watch the red circle expand and contract; that SAM
will not fire `[MAN v]`.

### 9. Navigation Display

Route, heading, distance, own and hostile aircraft. Waypoints are **pre-programmed by squadron HQ
during the briefing** and normally routed **around known SAM sites** — with the manual's own caveat
that *"the data does not necessarily represent fully accurate information, only the best possible data
as known at the time"* `[MAN v]`.

| Shape | Meaning |
|---|---|
| **star** | the **first** waypoint, always |
| **square** | a waypoint where a change of direction is indicated |
| **triangle** | a waypoint where **enemy units are assumed to be targeted** |
| **circle** | the **final** waypoint, normally a friendly field — **ILS is to be activated from here** |

The **current** waypoint in the cycle is always drawn **white** `[MAN v]`. The patch readme adds that
waypoints are additionally numbered.

Autopilot: engaging it after takeoff flies the waypoints **in sequence** and heads immediately for the
current one; a cycle key steps through them; **Go Home** turns the autopilot on and flies **directly to
the final waypoint** `[MAN v]`.

### 10. Radar, AWACS and the mode logic the symbology depends on

The rules that decide what the HUD can even show. All `[MAN v]`.

| Rule | Consequence |
|---|---|
| Radar **must be on** to identify aircraft **beyond visual range** | no BVR ID with radar off |
| Radar **must be on** to **create a Shootlist** | no Shootlist → no Target Designation Box → no Shoot Cue |
| **AIM-120 requires radar guidance** | cannot be launched with the radar off |
| **AIM-9X can** be launched with the radar off | "radar off + Sidewinder only" is named by the manual as *ultra-stealthy mode* |
| **AWACS downlink** supplies enough information to "see" enemy aircraft **with own radar off** | the situational picture survives radar silence; the shot does not |
| Radiating **tells everyone where you are** | *"indiscriminate use of your radar effectively tells everyone exactly where you are"* |
| Opening the bay raises the signature | visible on the Defense Display, §8 |
| **Chaff creates a huge return** — it masks position, it does not hide presence | *"If you are trying to keep a low profile, dropping chaff is a decidedly bad idea."* |
| Flares are **not** reliable | effectiveness depends on seeker sensitivity and range/aspect; manual recommends flares **plus** evasive manoeuvring |
| Chaff and flares also release **automatically on a preset pattern** when the onboard computer detects a threat | the player is not the only dispenser |

The AWACS link is described only as a *"downlink"*; **no datalink system is named anywhere in the
manual**, and full-text search for IRST, laser designator and IFF returns nothing. A helmet sight
exists only on the enemy MiG-29 `[MAN p.86–90*]`.

### 11. Controls that change what the HUD shows

`[MAN v]`, keyboard reference. Only the entries that alter symbology or sensor state.

| Key | Effect |
|---|---|
| `R` | radar standby ↔ on; `RADAR` appears on the HUD when on |
| `W` | cycle weapon selection |
| `4` (top row) | ready a single JDAM Mk. 83 — **when JDAM is selected only ground objects are targeted** |
| `C` | release a **chaff/flare combination** |
| `Enter` | create a Shootlist of the four nearest **eligible** targets |
| `Tab` / `Ctrl+Tab` | cycle forward/back through **all objects in the forward arc within 40 NM** |
| `[` / `]` | previous / next target on the Shootlist |
| `'` | boresight — target the nearest object **inside the ASE circle** |
| keypad `2 4 5 6 7 9` | the six full-screen displays, §6 |
| keypad `8` | Attack Display thumbnail on the HUD / Artificial Horizon inset elsewhere |

### 12. What this run's manual retrieval changed against the source research

| Item | Source research said | Manual says `[MAN v]` |
|---|---|---|
| Flare count | "not quantified" | **100** — *"Your F-22 only carries 100 flares"* |
| Target Designation Box, target diamond, Shoot Cue | absent | the complete engagement sequence, §4 |
| Minimum SAM detection range against a clean F-22 | absent | **4 miles** |
| AMRAAM vs radar-off | absent | AMRAAM cannot be launched radar-off; Sidewinder can |
| MFD keypad bindings | not given | six clean bindings, §6 |
| ILS deviation sense | "three red lines" | full per-line reading, **plus a documented one-sided defect**, §5 |
| Automatic countermeasure release | absent | preset pattern on computer-detected threat |
| Wingman command set | absent | five commands ([`campaign.md`](campaign.md) §10) |
| Boresight key | `L` | `'` in the keyboard chapter; conflict recorded |

Nothing the source research asserted was **contradicted**. It was under-complete, not wrong.

## State

**Nothing.** The FlightBox HUD is C++ and F-16-shaped ([`doc/mods.md`](../../../doc/mods.md) `## Gaps`:
*"The HUD is C++, so 'HUD per title, declared' has no surface to be declared into"*). There is no
declaration format for a HUD, so none of the above can be expressed yet.

## Gaps

- **No HUD declaration format exists.** This file describes a HUD that has no target representation.
  Building it is the engine backlog item, not a mod task.
- **No pixel geometry anywhere.** §3 is a position map inferred from callout reading order on an OCR'd
  drawing. The two diagram pages were **not read as images** in this run — only the manual's OCR text
  was retrieved. Reading pp. 30–32 of the PDF visually would settle the layout and the ambiguous
  `280` / `27.0` association in §2.
- **Artificial Horizon page incomplete** — the retrieved text is cut mid-list after "Airspeed Indicator
  (in".
- **Display page numbers 35–39 are TOC-derived only.** No page footer survived OCR in that region; the
  footers that did survive (30, 74, 85) all confirm the source research's numbering elsewhere.
- **Two internal key conflicts unresolved** — Attack Display keypad (chapter 3 vs keyboard reference)
  and boresight key (`L` vs `'`). The keyboard reference is preferred, which is a judgement.
- **ILS right-side deviation sense is missing from the manual** (§5) and is not inferred here.
- **Gun range contradiction** — 1.5 NM `[MAN p.30]` vs 0.5 km `[MAN p.85]`; carried in
  [`campaign.md`](campaign.md) §8, unresolved, affects the gunsight pipper's usable envelope.
- **The signature model behind the Defense Display has exactly one number** (4 miles, clean). Nothing
  says by how much the bay, the radar, the gear or damage each widen the circle. Without those the
  display can be drawn but not driven.
- **Colours are named, not specified.** "Green", "bright red", "white", "blue" — no values.
- **Message set unknown.** The HUD Repeater holds six messages; which messages exist is undocumented
  beyond the one screenshot line `CLEARED FOR TAKEOFF` `[SHOT]`.
