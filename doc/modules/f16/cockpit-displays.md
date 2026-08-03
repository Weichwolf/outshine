# F-16C Cockpit Displays & Controls

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 3 — Cockpit & Equipment, pp. 13–83.
HUD covered separately in `hud-symbology.md`; EHSI/HSD detail in `navigation-ils.md`; engine gauges in
`engine-fuel.md`; AOA indexer in `hud-symbology.md`.

Plus `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — **Cockpit Overview /
Instrument Panel** chapter, p.43–81 (**partial pass**: Instrument Panel analog gauges + Caution Light
Panel extracted below; Left/Right Auxiliary Console, Left/Right Console **still not processed**, see
`PROGRESS.md`) — **plus, this round, the Upfront Controls (UFC/ICP/DED) chapter, p.97–120, and the
Multi-Function Displays (MFD) chapter, p.121–127, both now FULL** (closes the biggest gap flagged at
the end of Pass 3). Cite tags `Chuck p.NN` vs `ED EA Guide p.NN`.

Plus, since this round, `doc/TO 1F-16CMAM-34-1-1 BMS.pdf` (BMS Dash-34, change 4.38) — cite tag
**`[BMS-34 p.NNN]`** — **pp.63–94** (MFDS: rockers, format set, master menu, interaction idioms, SOI,
symbol plate, the whole HSD page) and **pp.127–159** (UFC hardware, ICP, DCS, entry idioms and their
limits, LIST/MISC map, DED page inventory, CNI/STPT/CRUS/TIME/T-ILS/BULL/MAN, fuel warnings). See the
two *"BMS Dash-34 addendum"* sections below. **That manual documents a SIMULATOR, not the factory
jet** — full caveat at the head of `hud-symbology.md`; its "not implemented" markers are carried
through verbatim, and where it contradicts the ED guide **both readings are kept**.

**Pilot-KI relevance (why this pass exists):** FlightBox's upcoming avionics command-block model has
the pilot AI drive every system through the **same command path a human uses in the cockpit** — no
magic state jumps. The ICP/DED and MFD sections below are the field-by-field reference for what those
commands actually are (which button, which precondition, which DED/MFD field changes, what feedback the
pilot gets, whether/how input can be rejected). The consolidated, deduplicated **command list** derived
from this material (plus `hotas.md`) lives in **[`controls-commands.md`](controls-commands.md)** — see

## Spec

that file's header for why it's separate.

### ICP / UFC (Integrated Control Panel = Upfront Control)

The ICP drives the DED. Keypad + priority-function buttons, master-mode buttons, override buttons.

#### Key controls
- **A-A / A-G Master Mode** buttons (also NAV = neither selected).
- **COM1 / COM2 / IFF / LIST** override buttons → their DED pages.
- **DCS (Data Control Switch / "Dobber")**: UP/DOWN, **RTN (left)** = return to CNI, **SEQ (right)** =
  sequence fields. Moves the asterisk on DED pages, toggles wind on CNI.
- **DED Increment/Decrement** switch: changes the selected field's value (fields with up/down arrows).
- **RCL (Recall)**: backspace one digit; press again restores original value.
- **ENTR**: commits typed value.
- **HUD Symbology Intensity** wheel (turns HUD on), Reticle Depression wheel (backup bombing), FLIR
  polarity/gain/level, HUD raster contrast/brightness wheels.
- **Drift Cutout/Warning Reset** switch: **DRIFT C/O** (cages FPM to HUD center regardless of wind) /
  **NORM** (used before landing) / **WARN RESET** (clears HUD WARN message).

### DED pages

Priority functions = ICP keypad buttons (button N → page):
| Btn | Page |
|---|---|
| 1 | T-ILS (TACAN & ILS) |
| 2 | ALOW (altitude-low) |
| 3 | (see LIST) |
| 4 | STPT (steerpoint) |
| 5 | CRUS (cruise: TOS/RNG/HOME/EDR) |
| 6 | TIME (HACK timer, DELTA TOS/ROLEX) |
| 7 | MARK (markpoints) |
| 8 | FIX (navigation fix) |
| 9 | ACAL (altitude calibration) |
| 0 | M-SEL |

**Override pages** (COM1/COM2/IFF/LIST buttons): CNI, COM1 (UHF), COM2 (VHF), IFF, **LIST**.

**LIST sub-pages**: DEST, BNGO, VIP, INTG, NAV, MAN (EEGS funnel width), INS, DLNK, CMDS, **MISC** (0).
**MISC sub-pages**: CORR, MAGV, OFP, INSM, LASR (laser codes), BULL (bullseye), HMCS, GPS, DRNG (n/s),
HTS (if HTS pod), HARM (A-G mode), VRP.
- **MODE** page = alternate master-mode change without the A-A/A-G buttons.

### MFDs (Multifunction Displays, left + right)

- 20 **OSBs** (Option Select Buttons) ring each MFD; gain/brightness/contrast/symbology rocker switches.
- 3 lower OSBs = **Direct Access (DA)** buttons: up to 3 saved pages per master mode. Cycle via HOTAS:
  **DMS LEFT** = left MFD, **DMS RIGHT** = right MFD.
- Access a page: press a DA OSB, then pick the page from the Main Menu.
- **Same page cannot display on both MFDs simultaneously** — selecting it on one removes it from the other.
- Available pages: FCR, SMS/WPN, HSD, TGP, FLCS, ENG, TEST, DTE, etc.

---

### ED EA Guide addendum — Integrated Control Panel (ICP) & Data Entry Display (DED), FULL (official, p.97–120)

The **Upfront Controls (UFC)** = ICP (keypad/knobs/switches) + DED (the readout). Active when the
**C&I knob** (IFF control panel) is set to **UFC**; **BACK UP** position hands UHF radio + IFF to
dedicated backup panels instead (ICP/DED failure reversion — the MASTER knob on the IFF panel still
gates the APX-113 IFF system regardless of C&I knob position). *ED EA Guide p.97.*

#### ICP physical controls (14 numbered items, ED p.98–99)
| # | Control | Function |
|---|---|---|
| 1 | **Override Buttons** (COM1/COM2/IFF/LIST) | Jump directly to that DED page from ANY page; **2nd press returns to the previous page** (a one-level undo stack, not RTN-to-CNI) |
| 2 | **Master Mode Buttons** (A-A / A-G) | Select master mode; pressing the button of the **already-active** mode returns to **NAV** |
| 3 | **SYM knob** | HUD symbology brightness, rotary |
| 4 | **RET DEPR knob** | Depressible-reticle angle for MAN bombing, **0–260 mrad** |
| 5 | **Priority Function / Data-Entry Keypad** | From CNI page: jumps to one of 8 priority DED pages (below); on any other page: numeric/alpha data entry for the highlighted field |
| 6 | **RCL button** | **1st press**: backspace one digit of newly-typed data. **2nd press** (with nothing new typed since): rejects all new input, restores the field's original value |
| 7 | **ENTR button** | Commits the highlighted field's new value |
| 8 | **BRT knob** | HUD raster intensity — **no function on Block 50** |
| 9 | **CONT knob** | HUD raster contrast — **no function on Block 50** |
| 10 | **0/M-SEL button** | Cycles modes/settings on the current DED page; also used to key a **negative value** or a **zero** on numeric fields (page-specific — see below) |
| 11 | **DED Increment/Decrement rocker** | Inc/dec the value of the field currently under the DCS asterisks (only fields with up/down-arrow glyphs are adjustable this way) |
| 12 | **DCS / "Dobber" switch** | 5-way (U/D/L/R + depress-equivalents RTN/SEQ) — moves the field-select asterisks, sequences between sub-pages, **RTN = jump straight back to CNI** |
| 13 | **DRIFT C/O & WARN RESET switch** | 3-position: **DRIFT C/O** (cages FPM to HUD center + Attitude Bars to boresight — for crosswind/INS-attitude-mode drift) · **NORM** (spring-loaded return, free drift) · **WARN RESET** (clears HUD warning text/voice, resets Max-G indicator to 1.0) |
| 14 | **TFR/FLIR controls** (TFR WX button, FLIR Inc/Dec, FLIR GAIN/LVL/AUTO) | **No function on Block 50** |

#### DED interaction model (generic — applies to every page below)
1. **DCS (Dobber) LEFT = RTN**: return to CNI page. **Any unaccepted edit on the current page is
   discarded** on RTN (ED explicit, p.100).
2. **DCS UP/DOWN**: move the asterisks (field-select cursor) to the previous/next data field.
3. **DCS RIGHT = SEQ**: sequence to the next sub-page within a multi-page group (e.g. CRUS TOS → RNG →
   HOME → EDR → back to TOS).
4. **Modify a field**: put the asterisks on it (DCS U/D), type digits on the keypad (or use the
   Inc/Dec rocker for arrow-marked fields) — the field **highlights** to show "typed but not yet
   committed."
5. **ENTR**: commits the highlighted value; field returns to normal (non-highlighted) display.
6. **RCL**: 1st press = backspace one digit; 2nd press = reject the whole edit, restore prior value,
   field returns to normal display.
7. **0/M-SEL**: page-specific — usually toggles/enables a boolean field (e.g. CRUS mode ON/OFF) or
   enters a negative/zero numeric value.
8. **Override buttons (COM1/COM2/IFF/LIST) always work** regardless of what page is showing; 2nd press
   of the *same* override button undoes to the prior page (not necessarily CNI).

This is the **exact state machine** a command-block model needs to reproduce: every DED "write" is
(a) select field → (b) type/toggle → (c) **ENTR to commit or RCL/RTN to discard** — a real
propose/commit/reject cycle, never an instant field-level set.

#### DED page map (button → page)
**Priority Function pages** (keypad 1–9, 0, visible only from CNI):

| Key | Page | Detail location |
|---|---|---|
| 1 | **T-ILS** (TACAN & ILS) | `navigation-ils.md` |
| 2 | **ALOW** (altitude-low warnings) | below |
| 3 | *(unused — no priority page)* | — |
| 4 | **STPT** (steerpoint) | `navigation-ils.md` |
| 5 | **CRUS** (cruise: TOS/RNG/HOME/EDR, 4 sub-pages via SEQ) | below |
| 6 | **TIME** (system time, Hack, Delta-TOS) | below |
| 7 | **MARK** (markpoints) | not extracted this pass — TODO, likely in ED's Navigation chapter |
| 8 | **FIX** (navigation fix) | `navigation-ils.md` |
| 9 | **A-CAL** (altitude calibration) | `navigation-ils.md` |
| 0 | **M-SEL** — *not a page*, the 0/M-SEL keypad key itself | — |

**Override pages** (dedicated buttons, work from any page): **COM1** (UHF), **COM2** (VHF), **IFF**
(page **not implemented** in DCS), **LIST**.

**LIST sub-pages** (keypad 1–9,0 while LIST shown): 1 DEST, 2 BNGO, 3 VIP, R(=RCL) INTG **(N/I)**, 4
NAV, 5 MAN, 6 INS, E(=ENTR) DLNK, 7 CMDS, 8 MODE, 9 VRP, 0 → **MISC** sub-menu.

**MISC sub-pages** (from LIST→0): 1 CORR **(N/I)**, 2 MAGV, 3 OFP **(N/I)**, R HMCS, 4 INSM **(N/I)**,
5 LASR, 6 GPS **(N/I)**, E HTS, 7 DRNG **(N/I)**, 8 BULL, 0 HARM.

Pages documented elsewhere in this doc set (not repeated here): **DEST/NAV/BULL** →
`navigation-ils.md`; **INS** (alignment) → `procedures-startup.md`; **DLNK** →
`datalink-iff.md`; **CMDS** → `defence-rwr-cm.md`; **VIP/VRP** → `weapons.md` / `navigation-ils.md`;
**HMCS/LASR/HTS** → `radar-sensors.md`; **HARM** → `weapons.md`. **N/I** = "not implemented" per ED
itself (DCS module limitation, not a doc gap).

#### CNI DED page (the "home" page — default at power-up, reached via DCS RTN from anywhere)
| Field | Content | DCS-adjacent action |
|---|---|---|
| Active UHF ch/freq | ARC-164 tuned channel/frequency | Inc/Dec cycles preset channel (if on a preset) |
| UHF status | blank=UFC-controlled · **GRD**=on GUARD 243.0 · **BUP**=UHF Backup panel · **OFF** | — |
| Active VHF ch/freq | ARC-222 tuned channel/frequency | Inc/Dec cycles preset channel |
| VHF status | blank=UFC-controlled · **GRD**=on GUARD 121.5 · **BUP** (view-only in backup) · **OFF** | — |
| IFF/transponder modes | **N/I** | — |
| Mode 3 code | **N/I** | — |
| IFF status | blank=UFC-controlled · **BUP**=IFF control panel | — |
| Selected steerpoint | current STPT number | Inc/Dec cycles steerpoint **only** when asterisks are on this field |
| Wind dir/speed | CADC-computed magnetic wind; **DFLT** prefix if CADC can't compute | DCS depress-equivalent toggles display on/off |
| System time | 24 h Zulu, auto from GPS | edit on TIME page |
| Hack time | from TIME page, hidden if zeroized | — |
| TACAN channel/band | REC/T/R mode: channel+X/Y band; A-A T/R mode: 00.1–99.9 NM or blank if no lock | — |

#### ALOW DED page (keypad 2, from CNI)
| Field | Meaning | Edit |
|---|---|---|
| **CARA ALOW** | AGL altitude (ft) at which the radar altimeter triggers low-altitude warning | Asterisks on field → type altitude (ft) → **ENTR** |
| **MSL FLOOR** | MSL altitude (ft) at which the baro altimeter triggers low-altitude warning | Asterisks → type altitude → ENTR |
| Selected steerpoint | for reference/cycling only, Inc/Dec | — |

**System effect / feedback**: below CARA ALOW → HUD "Altitude…altitude" **voice message** +
flashing altitude-low text (**inhibited if gear is down**); below MSL FLOOR → "Altitude…altitude"
voice only (no gear inhibit stated). Requires radar altimeter **powered and transmitting** for the
CARA ALOW warning to function at all — a documented **failure precondition**, not just a threshold.

#### CRUS DED pages (keypad 5, from CNI; 4 sub-pages, DCS SEQ cycles TOS→RNG→HOME→EDR→TOS)
Mutually exclusive: **enabling any one CRUS mode auto-disables whichever other CRUS mode was active.**
Enable/disable = asterisks on the "Mode Select" field + **0/M-SEL** (toggles highlight = enabled).

| Sub-page | Fields | Effect when enabled | HUD feedback |
|---|---|---|---|
| **TOS** | Desired Time-Over-Steerpoint (editable, HHMMSS), ETA (computed, read-only), Required GS (computed) | Aircraft should reach selected steerpoint at the DES TOS | HUD Velocity Scale gets a **speed caret**; HUD Time-To-Go field replaced by ETA; gear-down removes caret |
| **RNG** | Fuel-remaining-at-steerpoint (computed), wind (computed) | none commanded — informational | Speed caret = **max-range speed**; gear-down removes caret |
| **HOME** | Home Point (editable — Inc/Dec **or** keypad+ENTR to set the steerpoint number), Fuel-at-home (computed), Optimum Cruise Altitude (computed) | Home steerpoint becomes selected steerpoint | Speed **and altitude** carets = min-fuel-return profile (climb at MIL, cruise-climb as fuel burns, idle descent to 5,000 ft over home); fuel-at-home also shown on HUD below Master Mode; gear-down removes carets |
| **EDR** | Time-to-Bingo (computed, uses BNGO page's Bingo value), Optimum Mach (computed) | none commanded — informational | Speed caret = **max-endurance Mach** |
| — | Selected steerpoint (all 4 pages) | Inc/Dec cycles it | — |

Gear-down freezes all computed (non-editable) CRUS fields at their **last calculated value** rather
than updating live — a modeled staleness behavior, not just "hidden."

#### TIME DED page (keypad 6, from CNI)
| Field | Meaning | Edit |
|---|---|---|
| System Time | 24 h Zulu; auto from GPS ("GPS SYSTEM" label) or manual (HHMMSS+ENTR, "SYSTEM" label) | keypad + ENTR |
| **Hack Time** | independent stopwatch reference | keypad HHMMSS+ENTR to set; **Inc-rocker** = start-from-zero / freeze-unfreeze display (continues counting in background while frozen); **Dec-rocker** = zeroize to 0 |
| **Delta TOS** | applies one delta (±23:59:59, HHMMSS) to **every** steerpoint's TOS simultaneously — **0/M-SEL enters the minus sign** for a negative delta | keypad + ENTR; **cumulative while this page stays open**, but the DELTA field itself zeroes if you navigate away (the TOS *adjustments already applied* persist) |
| System Date | auto from GPS, read-only | — |

#### BNGO DED page (LIST → 2)
| Field | Meaning | Edit |
|---|---|---|
| **Bingo Setting** | fuel quantity (lb) that triggers the FUEL warning | keypad + ENTR |
| Total Fuel | current total onboard incl. external tanks, read-only | — |
| Selected steerpoint | reference/cycling only | Inc/Dec |

**Feedback / trigger condition**: fuel below Bingo → **"FUEL" advisory** (HUD lower-left) + **"Bingo…
bingo" voice** + flashing **"FUEL" warning** center-HUD, acknowledged via **DRIFT C/O switch → WARN
RESET**. Trigger fires on **combined fuselage tanks OR total fuel**, whichever crosses Bingo first.
**Ceiling caveat** (documented failure-adjacent quirk): if Bingo is set above **~6,070 lb** (FUEL QTY
SEL knob = NORM) or **6,667 lb** (any other knob position), the warning fires anyway at that ceiling —
an implicit clamp on how high Bingo can usefully be set.

#### MAN DED page (LIST → 5)
Manual ballistics backup for weapons without an SMS profile / EEGS passive-ranging input.
| Field | Meaning | Edit |
|---|---|---|
| **Wingspan** | target aircraft wingspan (ft) fed to the **EEGS Level II funnel** (passive-ranging gunnery, no FCR solution) | keypad + ENTR |
| Range | free-fall weapon range | **N/I** |
| Time-of-Fall | free-fall weapon impact time | **N/I** |
| Selected steerpoint | reference/cycling only | Inc/Dec |

#### MODE DED page (LIST → 8) — backup master-mode selector
Backs up the ICP's physical A-A/A-G buttons if they fail.
| Field | Behavior |
|---|---|
| **Mode Select** | shows the mode ("A-A" or "A-G") that **would** be entered if 0/M-SEL is pressed now; **DCS SEQ or any keypad button toggles which mode is shown** (A-A ↔ A-G); if the shown mode == the actual current master mode, the text is **highlighted** |
| 0/M-SEL | commits the shown mode; **pressing it while the field is already highlighted (i.e. shown mode == current mode) sets NAV instead** — a "press again to cancel back to NAV" semantic, same as the physical buttons |

**Precondition / failure mode**: **inoperative** whenever the throttle **DOG FIGHT switch** is out of
center (DOGFIGHT or MISSILE OVERRIDE) — those hardware overrides take priority and this software path
is locked out. A concrete example of a **documented command-rejection precondition**.

---

### BMS Dash-34 addendum — ICP and DED

*Source: `TO 1F-16CM/AM-34-1-1 BMS` pp.127–159. Cite tag `[BMS-34 p.NNN]`. **Simulator manual, not
factory manual** — caveat block at the head of `hud-symbology.md`; the manual's own "non-functional"
markers are carried verbatim. This runs alongside the ED EA Guide distillation above, which it
**refines and contradicts in places** (both readings are kept; where they differ it is a divergence
between two reference implementations, not an error to be resolved here).*

#### I1 — Hardware `[BMS-34 pp.127–128]`

- **UFC = ICP + DED + PFLD.** LRUs: data entry display, pilot's fault list display, DED/PFLD power
  supply, Integrated Control Panel, **CDEEU** (Common Data Entry Electronics Unit).
- The **ICP sits directly below the HUD**, reachable with either hand. The CDEEU takes **separate**
  inputs from the ICP and the caution panel.
- The CDEEU is a microprocessor that drives the CNI equipment, the DED/PFLD data stream, and control
  and entry data across the mux buses. **It accepts CNI data even while that equipment is powered off**,
  holding it in non-volatile memory — **but if UFC power is cycled while the backup battery is below
  2.4 V the data is lost and reverts to defaults.**
- **Backup chain on failure**: DED/PFLD dead → view DED/PFLD data **on the HUD** via the HUD control
  panel · CDEEU dead → **IFF control panel** provides transponder modes 1, 3/A, 4 and S · **Have Quick
  panel** provides backup UHF.
- DED/PFLD geometry: **5 rows × 24 characters**, dot matrix **64 px per linear inch**, surface
  **192 × 64 pixels**, three **1 MHz** serial-digital mux buses `[p.127]`.

#### I2 — ICP layout, the 16 numbered controls `[BMS-34 p.129]`

Four functional groups: **master mode**, **override**, **priority**, **other UFC switches**. The
labelled figure names these 16 items (figure without coordinates):

| # | Control | # | Control |
|---|---|---|---|
| 1 | Priority function buttons 1–9 / numeric keypad 1–9 | 9 | **M-SEL** (mode select) + numeric `0` |
| 2 | Override buttons | 10 | Raster contrast knob |
| 3 | **RCL** (recall) | 11 | FLIR gain/level switch |
| 4 | Master mode buttons | 12 | **DRIFT C/O · WARN RESET** switch |
| 5 | Weather button | 13 | **DCS** (Data Control Switch) |
| 6 | Reticle depression control — **N/I** | 14 | **DED increment/decrement** switch |
| 7 | **ENTR** (enter) | 15 | Raster **BRT** knob |
| 8 | FLIR gain rocker | 16 | HUD symbology intensity knob |

*(Note the conflict with `[p.98]`, which describes **RET DEPR as a working 0–260 mR control**; item 6
here is marked N/I. Recorded, not resolved.)*

**Master mode buttons** `[p.129]` — two buttons, **A-A** and **A-G**. Pressing one selects that master
mode; **pressing it again deselects**; with none selected the system defaults to **NAV**. They are
inoperative while an override master mode is engaged from the manual controls.

**Override buttons** `[p.130]` — **COM1 · COM2 · IFF · LIST**. An override replaces the current DED page
from **anywhere**; pressing the same button again restores the overridden page. **If one override page
replaced another, deselecting shows the CNI page** rather than the earlier override.

**Priority function buttons** `[p.131]` — keys **1–9**, active **only while the CNI base page is
displayed**: `1 T-ILS · 2 ALOW · 3 DTS · 4 STPT · 5 CRUS · 6 TIME · 7 MARK · 8 FIX · 9 ACAL`.

#### I3 — Data Control Switch ("the Dobber") `[BMS-34 p.131]`

| Position | Function |
|---|---|
| **Up / Down** | move the DED **asterisks** between enterable/selectable items; **hold** to run through them; **the asterisks wrap around** end-to-end (fastest way to the bottom item is DCS up from the top) |
| **SEQ** (right) | sequence through page options; on a page whose title has subordinate pages, SEQ shows the next subordinate page |
| **RTN** (left) | **always returns to the CNI page** |

Asterisk placement is **exclusive to the DCS** and is limited to enterable data or selectable options.

#### I4 — The five entry idioms `[BMS-34 pp.132–135]`

| Idiom | Rule |
|---|---|
| **Rotary** | non-numerical option sets. When the rotary holds high-priority page titles, **DCS SEQ** cycles the view; to *select*, put the asterisks around the rotary mnemonic and press **any ICP key 1–9** |
| **Mode select (M-SEL)** | for options that are *not* auto-selected when shown. Asterisks around the mnemonic, press **M-SEL**; the mnemonic **highlights**. Same action deselects. **A mnemonic that fails to highlight means the controlling subsystem REJECTED the request** — the DED's own rejection channel. DTE mission load may pre-select options |
| **INC/DEC rocker** | marked by an up-triangle above a down-triangle; press repeatedly or hold; **values wrap** (COM presets run past 20 back to 1). Usually one item per page is inc/dec-able — **the CNI page is the exception**: COM1 preset, COM2 preset and steerpoint number are all inc/dec-able and the DCS up/down moves the inc/dec symbol between them. If presets are not selectable (manual frequency, radio off) the symbol is **shown but the switch does nothing** |
| **ENTR** | commits keyed data to the avionics |
| **RCL** | before ENTR: **1st press erases the last digit**; **2nd press clears the whole field** (non-alphanumeric) **or moves the highlight one position left** (alphanumeric); **3rd press deletes the highlighted character** (alphanumeric only). After ENTR, RCL clears the field for a fresh entry |

**Entry sequence** `[p.133]`: asterisks around the field → key the value (**the first keystroke wipes
the old value and highlights the field**) → **ENTR**.
- If the page is an override page that replaced another override page, **or** the entered data is
  verifiable from the CNI page, **ENTR jumps automatically to the CNI page**.
- Selecting/deselecting an override page after keying but before ENTR **recalls the previous data** and
  drops the highlight.
- **After a data-entry error the asterisks cannot be moved** until RCL is pressed; the offending field
  **and all DED data on the HUD flash** `[p.135]`.
- Fields have fixed digit counts; **excess keystrokes are ignored** (elevation = 5 digits + sign).
- COM1/COM2 frequencies need **≥3 digits**; a **trailing zero is assumed**; the DED **auto-fills the
  sixth digit as 5 or 0 depending on the fifth**. **Leading zeros are valid.**
- Signed values need the sign key **first**: **`−` = M-SEL**, **`N` = 2**, **`S` = 8**, **`E` = 6**,
  **`W` = 4**. Unsigned ⇒ positive assumed.
- **A field is blank and refuses entry when its supporting subsystem is failed or unpowered.**

**Alphanumeric entry** `[p.135]` — phone-keypad mechanization: each ICP key 0–9 carries a number
followed by letters; repeated presses cycle and wrap. Used **only** on: **Mode S page (Aircraft ID)**,
**A-G DL page (transmit and ownship address)**, **STPT/DEST pages (nav point ID)**, **Markpoint page
(nav point ID)**.

#### I5 — UFC entry parameter ranges `[BMS-34 pp.133–134]`

*The complete published list; these are the validation limits a DED implementation must enforce.*

| Item | Range |
|---|---|
| UHF (ARC-164) frequency | 225.000 – 399.975 MHz, **0.025 MHz** steps |
| UHF presets | 1 – 20 |
| Have Quick net numbers | 000 – 999 |
| VHF (ARC-186) FM | 30.00 – 87.97 MHz, 0.025 MHz steps |
| VHF (ARC-186) AM | 108.00 – 151.97 MHz, 0.025 MHz steps; ARC-186R supports **8.33 kHz spacing (0.005 MHz steps)** |
| VHF presets | 1 – 20 |
| ARC-210 | 30.000 – 87.975 MHz FM; 108.000 – 117.975 MHz AM **receive only** |
| ILS frequency | 108.10 – 111.95 MHz, **alternating 0.05 / 0.15 MHz** increments |
| ILS course | 0° – 359° |
| TACAN channel | 1 – 126 |
| IFF mode 1 | 00 – 73, last digit 0–3 |
| IFF modes 2 and 3 | 0000 – 7777 **octal** |
| Mode S address | 1 – 16,777,214 (octal 1–77777776, hex 1–FFFFFE, or all-nines decimal) |
| Mode S aircraft ID | 8 alphanumeric chars, trailing blanks allowed, **no embedded blanks** |
| Steerpoint number | 1 – 99 |
| Destination number | 1–25 **and** 31–99 |
| **Markpoint number** | **26 – 30** |
| Visual initial point number | 1 – 25 |
| Visual release point target number | 1 – 25 |
| OA / IP / RP range | 0 – 486,090 ft · 0 – 80.000 NM · 0 – 148.16 km |
| OA / IP / RP elevation | ±30,480 m · ±99,999 ft |
| Bullseye point number | 1 – 99 |
| Latitude | 90°00.0000′ N … 90°00.0000′ S, **4 decimal places**, minutes ≤ 59.999 |
| Longitude | 180°00.0000′ E … 180°00.0000′ W, 4 decimals, minutes ≤ 59.999 |
| System altitude | **−1500 … 80,000 ft** |
| Bearings | 0.0° – 359.0° |
| TGP / LST laser codes | **1111 – 2888**, first digit 1 or 2, last three digits 1–8 |
| Laser start time | 0 – 150 s |
| Range estimate overlay | 0–99,999 ft · 0–30,479 m · 0–30.47 km · 0–16.45 NM |
| **Altitude low (ALOW) limit** | **0 – 50,000 ft** |
| **Wingspan (MAN page)** | **20 – 120 ft** |
| Date | MM/DD/YY, MM 01–12, DD 01–31, YY 87–99 or 00–47 |
| System time | 00:00:00 – 23:59:59 |
| TOS | −23:59:59 … +23:59:59; **a negative TOS entry is blanked** |
| Hack time | 00:00:00 – 23:59:59 |
| Groundspeed | 0 – 1700 kt |
| Heading | 0° – 359° |
| EGI manual magnetic variation | 90.0° E … 90.0° W |
| Datalink ownship address | 01 – 99, **excluding multiples of 10** |
| MIDS TOD | 00:00:00 – 23:59:59 |
| Link 16 Fighter Channel | 0 – 126 |
| Link 16 Mission Channel | 0 – 127 |
| Link 16 Special Channel | 0 – 126 |
| Link 16 voice callsign number / label | 01–99 / `AA…ZZ` |
| Link 16 STN (team member, donor) | 00000 – 77776, excluding 00077, 00176, 00177, 07777 |
| Track number (5-char) | first two chars alphanumeric **excluding I, O, 8, 9**; last three digits **0–7** |
| Link 16 ownship number | 1 – 4 |
| Auto PDLT | 0 – 8 |
| IDM team address | 00 – 99, excluding multiples of 10 |
| IDM team size | 2 – 8 |
| Datalink point number | 71 – 80 |
| Datalink transmit address | 0 – 99 |
| HARM threat table entries | 0 – 4095 |
| CMDS chaff/flare/other-1/other-2 bingo | 0 – 99 each |
| Burst quantity | 0 – 20 |
| Burst interval | **0.005 – 10.000 s in 0.001 s steps** |

#### I6 — LIST page map `[BMS-34 p.130]`

Pressing **LIST**, then the shown key:

| Key | Page | Key | Page |
|---|---|---|---|
| 1 | `DEST` | 0-1 | `CORR` |
| 2 | `BINGO` | 0-2 | `MAGV` |
| 3 | `VIP` | 0-3 | `OFP` |
| 4 | `NAV` | 0-4 | `INSM` |
| 5 | `MAN` | 0-5 | `LASR` |
| 6 | `INS` | 0-6 | `GPS` |
| 7 | `EWS` | 0-7 | `DRNG` |
| 8 | `MODE` | 0-8 | `BULL` |
| 9 | `VRP` | 0-9 | `WPT` |
| RCL | `INTG` | 0-RCL | `HMCS` |
| ENTR | `DLNK` | 0-ENTR | — |
| 0 | `MISC` | 0-0 | `HARM` |

The `0-` prefixed entries are the **MISC** sub-tree, reached by `LIST → 0 (MISC)` then the second key.

#### I7 — DED page inventory with BMS status `[BMS-34 pp.136–157]`

| Page | Access | Status / content |
|---|---|---|
| **CNI (base)** | default; **DCS RTN** from anywhere | see below |
| COM1 / COM2 / IFF | override buttons | radio + IFF chapters |
| `DEST` | LIST 1 | destination page |
| `BINGO` | LIST 2 | bingo fuel state + total fuel state |
| `VIP` | LIST 3 | visual initial point |
| `NAV` | LIST 4 | **non-functional** |
| `MAN` | LIST 5 | **EEGS funnel wingspan**, in feet; **default 35 ft**, DTC-settable; wingspan table below |
| `INS` | LIST 6 | EGI/INS |
| `EWS` | LIST 7 | countermeasures |
| `MODE` | LIST 8 | **backup master-mode selector**: DCS **SEQ** toggles the mode, **M-SEL (0)** activates it; neither A-A nor A-G ⇒ NAV |
| `VRP` | LIST 9 | visual reference point |
| `INTG` | LIST RCL | IFF interrogation |
| `DLNK` | LIST ENTR | IDM |
| `MISC` | LIST 0 | sub-tree, below |
| `CORR` | MISC 1 | **non-functional** |
| `MAGV` | MISC 2 | current magnetic variation at ownship; **the correction is applied automatically in code**, the page is read-only information |
| `OFP` | MISC 3 | **non-functional** |
| `INSM` | MISC 4 | **non-functional** |
| `LASR` | MISC 5 | laser designator/ranger codes |
| `GPS` | MISC 6 | **limited**: Zulu time, date, groundspeed, magnetic heading; **only shown when GPS is on at the Avionics Power Panel** |
| `DRNG` | MISC 7 | Delta Bomb Range manual impact-point correction — **non-functional** |
| `BULL` | MISC 8 | bullseye management, below |
| `WPT` | MISC 9 | TGT-TO-WPT for Harpoon RBL; **settings cannot be changed when reached from LIST** |
| `HMCS` | MISC 0-RCL | helmet display config |
| `HARM` | MISC 0-0 | HARM threat tables |
| `T-ILS` | priority 1 | below |
| `ALOW` | priority 2 | ALOW + MSL FLOOR (see `hud-symbology.md` §A5) |
| `DTS` | priority 3 | Digital Terrain System — **non-functional** |
| `STPT` | priority 4 | below |
| `CRUS` | priority 5 | below |
| `TIME` | priority 6 | below |
| `MARK` | priority 7 | markpoints (numbers **26–30**) |
| `FIX` | priority 8 | **non-functional** |
| `ACAL` | priority 9 | **non-functional** |

**MAN page wingspan table** `[BMS-34 p.141]` — feet:

| A-10 58 | F-111 48 | F-14 51 | F-15C/E 43 | F-16 31 | F/A-18C 38, E/F/G 42 | F-4 39 | F-5 27 |
|---|---|---|---|---|---|---|---|
| MiG-21 24 | MiG-23 37 | MiG-25 46 | MiG-29 36 | MiG-31 46 | Su-24 44 | Su-25 51 | Su-27/30/33/34/35 42 |

#### I8 — The pages that carry flight-relevant fields

**CNI base page** `[BMS-34 p.136]` — field inventory from the labelled figure (figure without
coordinates): UHF mode · UHF preset channel or frequency/net · UHF status (`OFF`/`GRD`/blank) ·
VHF status (`OFF`/`GRD`/blank) · VHF preset or manual frequency · steerpoint (`STPT`/`TGT`/`IP`) with
**INC/DEC arrows** · system time (**ZULU, always displayed**) · hack time (below system time, when
running) · IFF modes enabled · IFF mode 3 code · IFF status (`OFF`/`STBY`/`POS`/`P/T`/`TIM`) ·
TACAN channel and band · **wind direction and wind speed**.
- Available **only when the AUX COMMS `CNI` switch is at UFC**; in `BACKUP` the UFC is dead and
  everything moves to the side consoles.
- **DCS SEQ on the CNI page shows wind speed and direction** — and BMS states plainly that **there is
  no wind indication on the ground** (the probes need airflow), so wind must come from ATC/ATIS.
- Auto steerpoint sequencing shows as **an `A` next to the current steerpoint**.

**STPT page (4)** `[p.150]` — asterisks start at the top; keying a number + ENTR selects that
steerpoint and **all steering cues update immediately**. Dobber down to edit **latitude · longitude ·
elevation · TOS**; edits to the *current* steerpoint feed back live into the tadpole, STPT diamond,
ETE/ETA and bearing/distance. **Elevation is the MSL elevation of the steerpoint at ground level.**
Dobber **right (SEQ)** toggles **AUTO ↔ MAN** steerpoint sequencing; **AUTO increments the steerpoint
when within 2 NM and range is increasing**.

**CRUS page (5)** `[pp.151–154]` — rotary `TOS · RNG · HOME · EDR`. Available in **any** master mode,
but the **steering cues** only in **NAV, emergency jettison and selective jettison**; fuel warnings and
TOS cues work in all modes. Valid for **all 99 steerpoints**; accuracy degrades for a moving-target
steerpoint as its speed rises. **The three fuel warnings (home / trapped / normal) are computed
independently of the selected option.**

| Sub-page | HUD cue | DED fields |
|---|---|---|
| **TOS** | **caret on the HUD speed tape** = the speed needed to make the time; **ETA** on the HUD (not selected ⇒ no caret and **ETE** instead) | system time · **DES TOS** · ETA at steerpoint · required groundspeed |
| **RNG** | speed-tape caret = **best-range airspeed at the current altitude** | current steerpoint · fuel remaining at the active steerpoint · wind direction and speed |
| **HOME** | **two carets — speed tape AND altitude tape**; technique is MIL power, capture the speed caret, then pitch to the altitude mark. The **altitude caret disappears once descent may start**; the optimum altitude is given in **radar altitude**, which may not match the HUD baro scale | home point (any INS steerpoint) · fuel at active steerpoint · optimum altitude · wind |
| **EDR** | speed-tape caret = **best-endurance speed at the current altitude** | selected steerpoint · **time to bingo** · optimal Mach for max endurance · wind |

**Trap named by the manual**: switching between CRUS sub-modes **requires mode-selecting the new one**;
otherwise the caret still belongs to the previous sub-mode `[p.154]`.

**TIME page (6)** `[p.155]` — system time · **hack clock** (started/stopped with the **▲▼** switch) ·
**DELTA TOS**, which shifts the TOS of **all** destinations by one entry (prefix **`0-`** for a negative
delta = arrive earlier, then ENTR). **With GPS operational the system time and date are initialized
from GPS and the TIME page will not accept a system time or date entry.** On MMC power loss time/date
blank; on restore the clock resumes **from the last known time and date**.

**T-ILS page (1)** `[p.149]` — line 1 = TCN and ILS status (ILS power from the **ILS knob on the AUDIO 2
panel**); scratchpad on the left accepts **either** a TACAN channel (0–126) **or** an ILS frequency
(VHF, 4 or 5 digits) — the system discriminates by the value. Line 2 = active TACAN and ILS
frequencies. Line 3 = TACAN band (X/Y) and **CRS** for the ILS approach.
- **Entering `0` in the scratchpad + ENTR toggles the TACAN band X↔Y.**
- **DCS SEQ** switches TACAN mode `T/R` (ground) ↔ `A/A TR` (air).
- **ILS `CMD STRG`** is enabled/inhibited by putting the scratchpad over it and pressing **M-SEL (0)**;
  the CMD STRG line **highlights** while active.
- The ILS **course** is edited by dobbering to `CRS` and entering the runway heading. (Cross-check the
  three-step enable chain in `hud-symbology.md` §A8.)

**BULL page (MISC 8)** `[p.147]` — **bullseye defaults to STPT #25**, changeable to any steerpoint up to
#25 via PREV/NEXT or by typing the number. **M-SEL (0)** with the asterisks around `BULLSEYE` toggles
bullseye mode; **default is enabled**. Effects:

| Bullseye mode | HUD | FCR / HSD |
|---|---|---|
| selected | bearing and range to bullseye at the **bottom left** | cursor bearing/range **relative to bullseye** |
| not selected | no bullseye bearing/range | cursor bearing/range **relative to the active steerpoint**; the bullseye symbol and circle are **not drawn** and a **waterline flight-director symbol relative to the active steerpoint** replaces them |

**Beyond 99 NM the range is not shown inside the MFD bullseye circle** — the field is only two digits.
Newer blocks are noted to display the flight-director symbol even with bullseye mode selected.

#### I9 — Fuel warning function `[BMS-34 pp.158–159]`

| Warning | Trigger | Indication | Reset |
|---|---|---|---|
| **Bingo** | FUEL QTY SEL **NORM** ⇒ the **lesser of fuselage fuel and total fuel** below the bingo value. **Out of NORM ⇒ total fuel only** (with trapped external fuel this risks starvation before the warning, unless the `FUEL SW` caution catches it) | flashing **`FUEL`** in the **centre** of the HUD, **`FUEL`** at the **lower left**, VMU **"BINGO, BINGO"** | **DRIFT C/O → WARN RESET**, or enter a lower bingo value |
| **Home bingo** | predicted fuel remaining at the home point **< 800 lb** | flashing **`FUEL`** centre + estimated fuel over the home point at the lower left **in hundreds of pounds** (if both fire, both the bingo `FUEL` mnemonic and the home estimate appear at the lower left) | as above |
| **Trapped fuel** | **all five**: FUEL QTY SEL in NORM · no aerial refuelling in the past **30–90 s** · fuselage fuel ≥ **500 lb below capacity for 30 s** · total fuel ≥ **500 lb above fuselage fuel for 30 s** · fuel flow **< 18,000 pph for 30 s** | flashing **`FUEL`** centre + flashing **`TRP FUEL`** lower left. **`TRP FUEL` takes precedence over other fuel warnings** | `FUEL` by **DRIFT C/O → WARN RESET**; `TRP FUEL` clears when the condition ends **or after taking on ≥500 lb**. An **MMC power cycle re-raises it as a new condition** |

Trapped-fuel computation uses **fixed fuselage capacities: 5900 lb single-seat, 4600 lb two-seat**.
*(Note: the `-1` manual gives the aerial-refuelling exclusion window as **60 s** `[BMS-1 p.56]` where
the `-34` gives **30–90 s** `[BMS-34 p.159]` — a source discrepancy, recorded not resolved.)*

---

### ED EA Guide addendum — Multi-Function Displays (MFD), FULL (official, p.121–127)

Two **4×4 inch color LCD MFDs**. Each is ringed by **20 OSBs (Option Select Buttons)** — pressing an
OSB "selects" whatever text is currently printed next to it; **highlighted OSB text = that option is
active / that command is in progress.** Plus 4 rockers (GAIN/SYM/BRT/CON) for display appearance.

#### OSB numbering (non-obvious — asymmetric between top/bottom rows)
| Edge | Numbering |
|---|---|
| **Top row** | OSB **1→5**, left to right |
| **Right column** | OSB **6→10**, top to bottom |
| **Bottom row** | OSB **11→15**, starting at the **far right**, ending at the **far left** (i.e. reversed vs. the top row — OSB 11 is bottom-right, OSB 15 is bottom-left) |
| **Left column** | OSB **16→20**, bottom to top |

This asymmetry is a real HUD/MFD-rendering-layout hazard if a FlightBox MFD is ever built naively by
mirroring the top row's L→R convention onto the bottom row.

#### Rockers
| Rocker | Effect | Mode-dependent variants |
|---|---|---|
| **GAIN** | sensor-video intensity, independent of symbology/brightness/contrast; holds continuously ramp to min/max | FCR format + GM/SEA mode → radar map-underlay intensity; FCR format + GMT mode → **Moving Target Indicator gain**, independently of the map; TGP format + FLIR Gain Control = **MGC** (Manual Gain Control, OSB 18 on TGP CNTL page) → FLIR thermal gain |
| **SYM** | MFD symbology intensity only | — |
| **BRT** | overall display brightness | — |
| **CON** | overall display contrast | — |

#### Format Selection Master Menu (reached via the bottom-row **Format Select buttons** = OSB 13/14/15
— these are the "3 lower OSBs" `hotas.md`/Chuck call the **Direct Access (DA)** buttons; same physical
buttons, ED's official term is "Format Select buttons")
14 assignable formats + BLANK + a RESET-MENU meta-page:

| Format | Purpose | Detail location / status |
|---|---|---|
| BLANK | removes the format from that FS button's slot **and** from the DMS-cycle rotation | — |
| **HAD** | HARM Attack Display — operate the ASQ-213 HTS pod | `radar-sensors.md` |
| RCCE | Reconnaissance | **not functional in DCS F-16C** |
| RESET MENU | reset MFD symbology/brightness/contrast to defaults | **N/I** |
| **FCR** | operate the APG-68 radar (A-A/A-G/Sea) | `radar-sensors.md` |
| **TGP** | operate the AAQ-33 targeting pod | `radar-sensors.md` |
| **WPN** | relay AGM-65/AGM-88 seeker video + targeting control | `weapons.md` |
| TFR | Terrain-Following Radar | **not functional in DCS F-16C** |
| FLIR | (separate from TGP) | **not functional in DCS F-16C** |
| **SMS** | select munitions, release profiles, fuzing, terminal params | `weapons.md` |
| **HSD** | top-down tactical picture, nav + threats + datalink fusion | `navigation-ils.md` / `datalink-iff.md` |
| **DTE** | upload mission data from the Data Transfer Cartridge | below |
| TEST | Maintenance Fault List / BIT | **N/I** |
| FLCS | FLCC diagnostic data display | **N/I** |

**Reassignment procedure** (per master mode — each of the **7 avionics master modes**: Navigation,
Air-to-Air, Air-to-Ground, Missile Override, Dogfight, Selective Jettison, Emergency Jettison — has its
**own** pre-configured FS-button↔format map, independently re-editable):
1. Press the target Format-Select OSB **once** to highlight its current label (if not already
   highlighted); press it a **2nd time** to open the Master Menu. (If already highlighted, one press
   opens the Menu directly.)
2. Press the OSB next to the desired format in the Menu → MFD exits the Menu and shows that format.
   Selecting the **same** format that was already assigned, or pressing any Format-Select button
   itself, **exits without changing anything**.
3. **Constraint**: an MFD format (other than BLANK) can be assigned to **only one** FS button across
   **both** MFDs at a time, per master mode. Assigning a format that's already in use elsewhere
   **auto-evicts** it from its old slot (which becomes BLANK) and moves it to the new one. BLANK itself
   has no such uniqueness limit.

**Swap button** (OSB 15 label context — Chuck's "DMS LEFT/RIGHT cycles format"): swaps the *currently
displayed* formats between left and right MFD, **and** swaps their underlying FS-button assignments
too (not just a visual swap).

**Declutter button**: removes OSB label *text* only; the underlying commands remain live if pressed
(**N/I** in DCS — flagged as such by ED itself).

#### DTE (Data Transfer Equipment) format — MFD page detail (p.126–127)
Interfaces the cockpit **DTU** (Data Transfer Unit) to a removable **DTC** (Data Transfer Cartridge)
for bulk-loading mission data into the MMC.

| OSB | Field | Action |
|---|---|---|
| — | DTE Power status | display only |
| — | CLSD (Classified Data) | **N/I** |
| **LOAD** | Load All Data | sequentially uploads **every** partition below in one command |
| **FCR** | Fire Control Radar settings | default FCR config + FCR CNTL-page settings |
| — | DTC file identification | display only (loaded file's name) |
| **MPD** | Mission Planning Data | steerpoints, nav route, VIP/VRP, ATDT ROE, CMDS programs, TACAN/ILS/BINGO settings — **precondition: CMDS MODE knob must be STBY** before uploading MPD, else erroneous CMDS data entry (a real documented failure mode for this command) |
| **COMM** | Communications | ARC-164 UHF + ARC-222 VHF preset frequencies |
| **INV** | Stores Inventory | external stores config → SMS |
| **PROF** | Weapon Profiles | default weapon-delivery settings → SMS |
| **MSMD** | Master Mode Initialization | default MFD formats, FCR/TGP modes, HSD/HAD CNTL-page settings, per master mode |
| **ELINT** | Electronic Intelligence | ALR-56 RWR threat tables, HARM threat tables, HTS threat classes |
| SMDL | Secure Modem Datalink | **N/I** |
| **TNDL** | Tactical Net Datalink | network config, ownship settings, Flight/Team member + Donor STN data |
| NCTR | Non-Cooperative Target Recognition data | **N/I** |
| **GPS** | GPS receiver data | — |
| **COLR** | Color Symbology | MFD symbology/text/OSB-label colors |
| — | Page Sequence | cycles DTE Page 1 ↔ Page 2 |
| — | Aircraft Reference Symbol | steering line vs. steerpoint/SPI/release-solution course, left/right of watermark = turn direction needed |
| — | DTE Advisory Messages | upload-error/status text |

---

### BMS Dash-34 addendum — MFDS operation, format set and HSD page

*Source: `TO 1F-16CM/AM-34-1-1 BMS` pp.63–94 (MFDS chapter, HSD chapter) plus p.127 (DED/PFLD
hardware). Cite tag `[BMS-34 p.NNN]`. **Simulator manual, not factory manual** — the caveat block is at
the head of `hud-symbology.md`; the manual's own "not implemented" markers are carried through
verbatim, and BMS deviates from the ED reference on several points flagged below.*

#### M1 — Hardware and rockers `[BMS-34 pp.63–64]`

- **4 in × 4 in colour MFD**, 20 OSBs on the bezel, 4 rocker switches. Two hardware variants named:
  **CMFD** (Color Multifunction Display) and **CCMFD** (Common Color Multifunction Display).
- Three display types are distinguished: **text**, **video**, **MFD-generated symbols**, plus
  **video-generated symbols**.
- Rockers: **SYM**, **CON**, **BRT**, **GAIN**. Each is an increase/decrease control (upper half /
  lower half); holding ramps continuously; **full-off to full-on takes ≈5 s**.
- **BMS deviation, explicit**: `[p.64]` marks **SYM (symbology) and CON (contrast) as NOT IMPLEMENTED**
  — only **BRT** and **GAIN** do anything. The rocker-effect table on p.64 lists BRT as the only
  affecting rocker for text, video, MFD-generated symbols and video-generated symbols alike.
  *(ED's guide describes all four as functional — this is a documented divergence between the two
  reference implementations, not a contradiction to resolve.)*
- **GAIN** additionally controls: certain ATP (targeting-pod) functions, the **FCR ground map** gain, and
  the **air-to-air Moving Target Reject value** `[p.63]`.
- Per-format persistence: symbology / contrast / brightness are stored **per format** and restored when
  that format is reselected; gain is stored **in the sensor**. `[p.64]` states that on a **SWAP (OSB 15)**
  the brightness and contrast settings are **not** retained through the transition — note that `[p.63]`
  states the opposite ("are preserved"); **the manual contradicts itself between these two pages.
  Recorded as-is, not resolved.**
- **SBC DFLT RESET** on the RESET MENU page restores symbology/brightness/contrast defaults `[p.63]` —
  but see M2: the RESET menu is itself **not implemented** in BMS.

#### M2 — Format set, format selection and the master menu `[BMS-34 pp.63, 65, 73–75]`

**OSB roles**: top row = sensor modes · bottom row = display formats · sides = format/mode-specific
functions. Pressing an OSB **briefly illuminates the display area next to it** as feedback `[p.63]`.

**Format-select mechanics** `[p.65]`:
- The **three centre OSBs of the bottom row** carry the primary format plus two secondary formats; the
  **primary is highlighted**.
- Pressing the OSB next to a **secondary** name promotes it to primary. **DMS left** does this for the
  left MFD, **DMS right** for the right MFD; **selection runs from inside to outside**.
- Pressing the OSB next to the **primary (highlighted)** name opens the **master menu**.
- **Uniqueness rule**: of the six visible slots (three per MFD) **no two may be the same format**,
  except BLANK, and except TEST during MFD BIT. Selecting a format from the menu that already occupies
  another slot causes **the blank format to be used in place of the old one**.

**The 13 named formats + BLANK** `[p.65]` — *this is the full list the earlier "6 generic pages"
summary was measured against:*

| Format | Mnemonic | Display type |
|---|---|---|
| FLIR Navigation Pod | `FLIR` | video + text |
| Digital Flight Control System | `FLCS` | text |
| HARM Attack Display | `HAD` | text |
| Horizontal Situation Display | `HSD` | text |
| Test | `TEST` | text |
| Data Transfer Equipment | `DTE` | text |
| Stores Management System | `SMS` | text |
| Weapon (AGM-65, AGM-88, …) | `WPN` | video + text |
| Fire Control Radar | `FCR` | video + text |
| Targeting Pod | `TGP` | video + text |
| TACAN | `TCN` | text |
| Navigation Pod Terrain Following Radar | `TFR` | video + text |
| Blank | *(none)* | text |

**Master menu entries** `[p.73]` — the menu is reached by pressing a **DA (Direct Access) button while
that page is already displayed** (e.g. press FCR when FCR is up). Entries: `BLANK`, `HAD`, `RCCE`,
`RESET MENU`, `SMS`, `HSD`, `DTE`, `TEST`, `TCN`, `TGP`, `FLIR`, `TFR`, `WPN`, `FCR`.
That is **14 menu entries** = the 13 formats above (FLCS is reachable but is listed on the menu figure
too) plus BLANK, RCCE and RESET MENU. **BMS status**: `RCCE` **cannot be reached via OSB 4 of the menu
page and none of its functionality is implemented** `[p.75]`; `RESET MENU` is reachable via OSB 5 **but
none of its functionality is implemented** `[p.75]`.

**OFF format** `[p.74]` — selecting an unavailable/unused format shows an **OFF page** (e.g. `FLIR OFF`)
until the format is deselected.
**BLANK format** `[p.74]` — a blank secondary slot shows no mnemonic; selecting it displays the blank
format and highlights the primary label position. **Blank formats are skipped by DMS left/right.**
**SWAP** `[p.74]` — pressing the SWAP OSB on either MFD exchanges left and right MFD content,
**video and text**.

#### M3 — The four interaction idioms `[BMS-34 pp.67–72]`

| Idiom | Rule |
|---|---|
| **Rotary** | used when there are **≤4 options**. Press the OSB next to the current mnemonic; each press steps to the next choice |
| **Menu** | used when there are **>4 options**. Pressing the OSB next to the current choice opens a list with the current choice **highlighted**; pressing the OSB next to a choice selects it and **returns to the normal format page** |
| **Increment/decrement** | marked by **up/down triangles or arrowheads**, the up symbol next to the upper OSB, the down symbol next to the lower one. **Triangles = one step per press** (no effect at the limit). **Arrowheads = press-and-hold for continuous change** until release or limit |
| **Data entry** | press the OSB next to the field → a **data-entry page** appears with digits **1,2,3,…,0 next to the side OSBs**, format controls on the bottom OSBs, a prompt in the centre and the field below it, the old value framed by **highlighted asterisks** (same idiom as the DED). Type, then press **ENTR at the top edge**. ENTR on a non-final field steps the asterisks to the next field; ENTR on the last field returns to the previous page. **RTN** leaves without entering |

**Rejection and recall** `[pp.71–72]`
- Data is **not accepted until ENTR**; selecting another page **restores the old value**.
- Each field has a **fixed digit count**; further key presses are **ignored** once full.
- **RCL** at any point before ENTR restores the previous valid value and clears the entry highlight.
- On a **detected error** the system **refuses the entry, flashes the highlight around the bad data
  (alternating highlight/de-highlight) and FREEZES the display**. **RCL** recalls the last valid value,
  restarts entry and unfreezes.

#### M4 — Sensor of Interest on the MFD `[BMS-34 p.66]`

- SOI on an MFD = **lines drawn on the outer edges of the display, forming a box**.
- SOI on the HUD or HMCS = **asterisk at the upper left, above the airspeed scale**.
- A sensor format that is **not** SOI prints **`NOT SOI` in the centre** of the display.
- **Only one MFD can be SOI at a time**; some pages cannot be SOI at all.
- **DMS up** designates the HUD as SOI when the HUD's mode allows it; **DMS down** toggles SOI between
  the two MFDs, or from the HUD back to an MFD.

#### M5 — MFDS symbol legend (FCR + HSD) `[BMS-34 p.82]`

The manual publishes a **symbol plate**. It is a drawing without dimensions — shapes and colours only,
no sizes. Entries, as labelled:

| Group | Symbols |
|---|---|
| Reference | **Aircraft reference** (cyan flying-W); a second **aircraft reference** (vertical bar form); **horizon line** (magenta bar) |
| DLZ | **DLZ target range cue** (right-pointing caret in a box); **dynamic launch zone** (staple); **TWS expand cue** (open rectangle) |
| Scan | **A-A range marks**; **antenna elevation tics**; **antenna azimuth marker** (T); **antenna azimuth tics**; **elevation marker**; **increment/decrement** (double triangle) |
| Cursors | **acquisition cursors** (two vertical bars) |
| Steering | **attack steering cue** (circle with centre dot) |
| Bullseye | **bullseye** (concentric circles); **bullseye LOS** (circle with directional tic and 2-digit range) |
| IFF | **AIFF friend** = **green circle**; **AIFF unknown** = **yellow square** |
| HSD | **HSD ownship / range ring**; **HSD steerpoint**; **HSD route**; **HSD line 1–4** |
| Markpoints | **markpoint received** · **markpoint received and selected** · **markpoint ownship** · **markpoint ownship and selected** (cross forms, colour-differentiated) |
| Track files | **search target (high aspect)** and **(low aspect)**; **system track file**; **extrapolated track file**; **bugged/priority track file**; **ECM/jamming detected** (chevron) |
| AMRAAM overlays | **BTF / TF with (inactive) AMRAAM in flight** · **… with active MPRF (HPRF if no `\`) AMRAAM in flight** · **… at AMRAAM predicted time of impact** · **… with LOSE AMRAAM in flight** (the last labelled `LOSE` in place of the altitude digits) |

**Occlusion and masking model** `[BMS-34 p.81]` — this is the MFD's drawing-order contract:
- **All MFD text and symbology exhibit occlusion**: a small area of background video around each symbol
  is blanked.
- Symbols carry **priority levels, 1 = highest**.
- **Masking** = blanking a lower-priority symbol that intersects a higher-priority one. Masking symbols
  own a **rectangular mask zone**; some symbols have **no** mask zone and simply merge.
- Symbols of **equal priority always merge**, mask flag irrelevant. Symbols of different priority merge
  only if the higher-priority symbol has masking **off**.
- **Symbol colours are DTC-loadable** (BMS Avionics Configurator → `COLR` OSB on **page 2 of the MFDS
  DTE format**) and **survive MFDS power cycles**. The manual publishes **no palette**.

#### M6 — HSD page `[BMS-34 pp.83–94]`

Reached by **OSB 7 on the master format menu**. Two pages: **HSD Base** and **HSD Control (CNTL)**.

**Base page fields, by OSB (from the labelled figure `[p.83]`, figure without coordinates):**

| OSB | Mnemonic | Function |
|---|---|---|
| 1 | `CEN` / `DEP` | ownship centre vs. depressed |
| 2 | `CPL` / `DCPL` | couple HSD range to FCR range |
| 3 | `NORM`/`EXP1`/`EXP2` | expand (only when HSD is SOI) |
| 5 | `CNTL` | control page |
| 6 | `XMT IDM` / `XMT L16` | transmit current SPI/steerpoint; **default XMT IDM**; **in BMS XMT L16 does nothing different from XMT IDM** |
| 7 | `FZ` | freeze ground-stabilized symbols |
| 10 | `CZ` | cursor zero |
| 16 | `DNR` | Link 16 donor page |
| 19 / 20 | ▽ / △ | range decrement / increment |
| — | — | range rings · magnetic north pointer · sensor volume · A-A and A-G ghost cursors · bullseye bearing/range · ownship bullseye bearing/range symbol · ownship symbol |

**Ownship placement** `[p.85]` — **cyan aircraft symbol**; **CEN** = display centre, **DEP** = **three
quarters down from the top**. OSB 1 alternates.

**Range scales** `[p.84]`

| Format | Selectable ranges (NM) | Minimum |
|---|---|---|
| **DEP** | 7.5, 15, 30, 60, 120, 240 | 7.5 (displayed as **"8"**) |
| **CEN** | 5, 10, 20, 40, 80, 160 | 5 |

Range **does not wrap** at either limit; the decrement triangle is **removed** at the minimum.
**Range rings** `[p.83]`: **DEP divides the scale into thirds**, **CEN divides it by two** (60 DEP →
20 NM per ring; 80 CEN → 40 NM per ring).

**Couple/decouple** `[p.85]` — coupled, HSD range follows FCR range and **OSB 19/20 are inhibited**;
**CEN coupled: HSD range = FCR range**, **DEP coupled: HSD range = 1.5 × FCR range**. With the SOI on
the HSD the pilot may **bump range without dropping out of CPL**; removing SOI reverts to the CPL range.
On any range change the cursor **keeps its distance from ownship**; on a range **decrease** the cursor
**resets to ownship**.

**Cursor bumping** `[p.84]` — applies to FCR, HSD and HAD: driving the cursor to the top/bottom edge
steps the range up/down. **In CPL mode the first edge contact switches to DCPL**; later contacts change
range.

**Cursors and ghost cursors** `[p.84]` — when the HSD becomes SOI its cursor initializes at the **FCR
ghost cursor** (A-A or A-G); with no ghost cursor, at **ownship**. **Not SOI ⇒ no HSD cursor drawn.**
With bullseye mode on and HSD as SOI, **bearing/range from bullseye to the HSD cursor** is displayed.
The A-A and A-G ghost cursors show where the sensors point and are **fixed — they cannot be slewed**
while the HSD is SOI.

**Sensor volume** `[p.84]` — drawn while the radar is **searching** in A-A or A-G **and no track exists**.

**Expand** `[p.86]` — **EXP1 = 2:1**, **EXP2 = 4:1**, rotary `NORM → EXP1 → EXP2 → NORM` via **OSB 3** or
a **short (<0.5 s) EXPAND/FOV press**, **only when the HSD is SOI**. The display centres on the cursor
and fills with the expanded patch; **the display stays live (no freeze)**; **the cursor may move only
inside the patch and the patch itself cannot be moved**. The label **flashes at 5 Hz** while expanded.
Decluttered while expanded: range scale + INC/DEC · sensor volume · A-A ghost cursor · A-G ghost
cursor · range rings and magnetic north pointer · CEN/DEP · CPL/DCPL · freeze.

**Freeze** `[p.91]` — with HSD as SOI and valid nav data, **OSB 7** freezes the display at the **cursor**
position; not SOI, it freezes at **ownship**. Either way the display switches to **CEN** format around
the ground-stabilized point (and reverts to the previous format on exit). Range rings and the north
pointer are shown during freeze **unless manually disabled** on the CNTL page. **OSB 7 is disabled when
nav data is invalid.** Steerpoints **1–89** are frozen; **ownship and the radar FOV keep moving** across
the frozen map and may fly off it. Range bump during freeze places the cursor at **display centre**; in
NORM a range **increase** keeps cursor range/bearing from ownship, a **decrease** puts it on ownship.

**Cursor zero** `[p.92]` — **OSB 10**; clears system SPI slews. `CZ` is **highlighted whenever system
slews exist** (e.g. JDAM relative targeting) and de-highlights after the press. **Works regardless of
the current SOI.** Displayed under the same conditions as on the A-G FCR format (A-G preplanned and
manual, S-J, E-J).

**Aircraft reference symbol** `[p.92]` — lower left; wings aligned with the aircraft's wings, working
with the azimuth and pitch steering bars. **If neither steering bar is present (invalid steering) the
symbol is not drawn.** In A-G it is specified to **flash on weapon release** like the HUD FPM —
**flashing is not implemented in BMS**. With bullseye selected it is **replaced by the bullseye
bearing/range symbol**.

**HSD CNTL page (non-L16)** `[pp.87–89]` — accessed by OSB 5; **all base-page graphics remain visible**,
but range, CEN/DEP, CPL/DCPL and freeze **cannot be changed there**. Changes take effect immediately so
the pilot can verify before exiting; settings are **saved per master mode** and restored on re-entry.
Options: `NAV` (1–3), `LINE` (1–4), `RINGS`, `ADLNK`, `GDLNK`, plus declutter for `FCR`, `PRE`, `AIFF`
and `HPN` (**Harpoon — not implemented**).

| Option | Behaviour |
|---|---|
| `FCR` (OSB 1) | shows the **radar search volume in cyan** and the **white ghost cursor** |
| `PRE` | preplanned threats from the DTC: **three-character code** at the threat location + a **lethal-range ring**. Threat IDs live in **steerpoints 56–70**, are **not editable in the cockpit**, and are **cleared on WOW during power cycles**; only the threat **lat/long** is editable in the cockpit, never its range. **Yellow** symbol and ring normally, changing **to the default colour (red) when ownship enters the ring**. **Horizontal range only — entered threat altitude is ignored.** If the DTC carries no preplanned threats the `PRE` mnemonic is absent |
| `AIFF` | in A-A master mode an interrogation draws **green circles (friendly)** and **yellow squares (unknown)**; deselect to remove |
| `NAV 1-3` | up to **three routes over steerpoints 1–24**, solid line through the route. Steerpoints outside a route are not drawn unless they are the system steerpoint. Types: **steerpoint = circle, initial point = square, target = triangle**. **BMS: only one route can actually be programmed**, and route/type configuration is **DTC-only, not cockpit-editable** |
| `LINE 1-4` | four geographic **dashed** lines through steerpoints (FEBA, free-fire zones, restricted/operating areas), authored in the BMS UI 2-D map; clipped to the visible area; **not editable or navigable in the cockpit** |
| `RINGS` | range rings **and** the magnetic north pointer, together |
| `ADLNK` / `GDLNK` | A-A / A-G datalink symbology |

**HSD CNTL page (L16 aircraft)** `[p.89]` — page 1 differs: `RINGS` moves to **OSB 17**, `ADLNK`/`GDLNK`
are removed to page 2, and **`RPT` (Report, BDA responses to C2) sits at OSB 16 — not implemented**.
Page 2 declutter switches, with BMS status:

| Switch | Declutters | BMS |
|---|---|---|
| `L16 ENG` | engagement diamonds on HSD **and** FCR A-A pages | **N/I** |
| `REF PT` | L16 reference points | **N/I** |
| `T-R` | L16/IDM SEAD and preplanned threat rings | SEAD part **N/I** |
| `PDLT` | HSD **auto-ranges** to keep the PDLT on display | implemented |
| `RNG` | L16 air surveillance tracks | — |
| `A SURV` / `G FRND` | air surveillance / ground friendly positions | `G FRND` **N/I** |
| `LAR` | JASSM MPPRE LAR displays | **N/I** |
| `A IDM` | air tracks from other fighters via IDM | — |
| `SHIP` | friendly and hostile ships | **N/I** |
| `SAM` | friendly/hostile SAM sites via L16 or IDM | **N/I** |
| `G TGTS` | non-SAM ground targeting data via L16/IDM (incl. IDM markpoints) | **N/I** |
| `A TGTS` | air targets from other fighters via L16 | — |

#### M7 — The remaining text formats `[BMS-34 pp.76–80]`

**DTE page** — two pages, loaded typically **at ramp start via OSB 3**, right around switching CNI to
UFC. Loading runs **counter-clockwise from the LOAD button**, highlighting each subsystem in turn.
Partitions: `ON` (**not functional**), `CLSD` (classified coefficient data), `LOAD`, `FCR`, `DLNK`,
`LINK 16` (file A or B), `NCTR`, `MSMD`, `PROF`, `INV`, `COMM`, `MPD`, `COLR`, `GPS`. Link 16 success =
the L16 mnemonic **de-highlights**, then **`LINK 16 INIT CHECK`** appears at the **bottom centre**; an
`A` or `B` under the mnemonic if two NDL files were transferred (**short press <0.5 s** switches A↔B;
**long press** on OSB 8 changes file A to B). If only one NDL was on the DTC the field stays blank.
*(This partition list differs from ED's — ED lists ELINT/SMDL/TNDL, BMS lists DLNK/LINK 16/NCTR. Both
are recorded; they are two different implementations of the same panel.)*

**TEST pages** — `BIT 1`/`BIT 2`/`BIT 3` via **OSB 1**, showing the **Maintenance Fault List**:
subsystem, failed test number, failure count, and time since FCC power-up of the first fault.
Two **pseudo-faults are always recorded**: **`TOF`** at **120 kt with gear up**, and **`LAND`** at
**gear down (WOW) below 80 kt**. **`CLR` (OSB 3)** clears the list and launches a fault survey. The MFL
holds a **maximum of 17 faults including the two pseudo-faults**; overflow replaces the oldest. The
post-flight copy is written to `dtc_last_flight_faults.txt`, **overwritten every flight**.

| BIT page | Entries (BMS status in brackets) |
|---|---|
| 1 | `MFDS` (N/I) · `RALT` (only if RALT powered) · `TGP` (N/I) · `FLIR` (N/I) · `TFR` (N/I) · `INS` · `SMS` (N/I) · `FCR` · `DTE` |
| 2 | `IFF` · `CMDS` · `MIDS` · `BLKR` · `IDM` (N/I) · `MMC` (N/I) · `FDR` (N/I) · `GPS` · `UFC` |
| 3 | `EHSIS` · `EHSIM` · `HMCS` (N/I) · `RECCE` (N/I) |

**FLCS page** — backup display for FLCS faults; shows up to **five faults at once**; two downward
arrows mark the last shown and **`MORE` next to OSB 20** opens the extended list. On the ground it also
exposes maintenance access to the **four FLCCs**: **OSB 7** = memory-location data entry, **OSB 6/8** =
decrement/increment the location by one, **OSB 9** = `HEX`/`PRCNT` display form. Cycling the **DBU
switch with WOW** reinitializes the FLCS and **suppresses FLCS MFL reporting for 20 s**, with `DBU`
displayed while running; **if the MFLs stay clear for those 20 s, no maintenance action is required.**

**TCN page** — backup TACAN: **channel 1–126** (**OSB 8** data entry, **OSB 7/9** decrement/increment,
integers 1–126 only), **band X/Y** (OSB 10, two-position rotary), **mode `REC`/`T/R`/`A/A TR`**
(OSB 6, three-position rotary). Displays the values **actually reported by the TACAN system**. Backup
TACAN control is available **only for a single failure (the UFC)** and **not** when the MFDS is the
backup bus controller (MMC degraded) and the UFC has failed.

**DED/PFLD hardware** `[BMS-34 p.127]` — DED on the **upper right glareshield**, PFLD on the forward
right console above the caution panel. Both driven by the **CDEEU**. **5 rows × 24 characters**, dot
matrix at **64 pixels per linear inch**, total display surface **192 columns × 64 rows of pixels**.
Communication over **three 1 MHz serial-digital multiplex buses**. Each crew station has its own DED and
PFLD power supply.

---

### Primary flight instruments (analog standby)
- **ADI** (Attitude Director Indicator): attitude + ILS localizer/glideslope steering bars.
- **SAI** (Standby Attitude Indicator): uncage during setup (startup step 49).
- Standby magnetic compass, altimeter (baro knob), airspeed, VVI, AoA indicator (deg), AoA indexer.
- Cabin pressure altitude indicator (×1000 ft), clock.

### Warning / caution / status
- **Master Caution** light (push to reset); eyebrow warning lights (ENGINE, HYD/OIL PRESS, FIRE, etc.).
- **TF FAIL** (terrain-following radar fail), **F-ACK** (fault acknowledge — clears PFL faults),
  **IFF IDENT** button/light.
- **Caution Advisory** panel (many lights); **PFLD** (Pilot Fault List Display).
- Landing gear indicator lights (down-and-locked / transition / up-and-locked).
- **RDY** light (e.g. refuel door open), fuel flow/quantity indicators (×100 lb), EPU fuel %, oxygen
  pressure/flow.

### HUD-related switches (panel) — full list in `hud-symbology.md`
Depressible Reticle, DED Data (DED/PFL/OFF), HUD Scales (VV/VAH · VAH · OFF), FPM (ATT/FPM · FPM · OFF),
Velocity (CAS/TAS/GND), Altitude (RADAR/BARO/AUTO), Brightness (Day/Auto/Night), RADALT power (ON/STBY/OFF).

---

### ED EA Guide addendum — analog instrument panel detail (official, p.43–81, partial pass)

The instrument-panel analog gauges are the F-16C's **battle-damage/avionics-independent backup path**
(EHSI/SAI/altimeter/ASI/AoA-indicator/VVI/ADI all work without the MMC — `flight-controls-flcs.md`'s
DBU concept extended to displays). ED gives exact scale numbers Chuck's guide doesn't quantify.

#### AoA Indicator (center instrument panel) — precise band edges (ED p.83, refines the AOA-indexer
table in `hud-symbology.md`)
- **Scale**: **−5° to +32°**, major tick = 5°, minor tick = 1°.
- **Low AoA region**: 8.5°–11° ("energy gaining, less than optimal").
- **Optimal AoA region**: **11.1°–13.9°** ("on-speed").
- **High AoA region**: 14°–16.5° ("energy depleting, greater than optimal").

**This refines `hud-symbology.md`'s AOA Indexer table** (which gave the coarser bands "Top red >14° /
Center green 11–14° / Bottom yellow <11°", sourced from Chuck): ED's exact edges are **11.1°/13.9°**,
not a flat 11°/14° — a small but real precision gain for a pilot AI's AoA-hold deadband. The Indexer
(glareshield 3-light version) duplicates this exactly and is **always powered** regardless of gear
position (ED explicit — Chuck's guide implies but doesn't state the indexer is gear-independent).
ED also states explicitly for landing: **"the pilot should maintain between 11° and 13° AoA"** — this
is the same 11–13° range already folded into `procedures-landing.md`'s ED addendum, now cross-confirmed
from the instrument-panel chapter independently of the procedures chapter.

#### Vertical Velocity Indicator (VVI) — analog backup to the HUD's VV scale
- Scale: **±6,000 fpm**, major tick = 500 fpm, minor tick = 100 fpm (finer resolution than the HUD's
  own Vertical Velocity Scale, which uses 1000/500 fpm ticks per `hud-symbology.md`'s ED addendum —
  the analog backup instrument is actually the more precise of the two).

#### Attitude Director Indicator (ADI) — full instrument, backup to HUD attitude + ILS steering
- **Attitude sphere**: light-blue (climb/sky) vs dark-brown (dive/ground) hemispheres, rotates around a
  fixed aircraft-symbol waterline.
- **Pitch scale**: major tick 10°, minor tick 5°.
- **Bank angle** (two redundant indicators — upper index + lower scale): lower-scale major ticks at
  **30°/60°/90°**, minor ticks at **10°/20°**.
- **Slip indicator (ball)**: centered = coordinated turn (matches `flight-controls-flcs.md`'s ARI
  coordination goal); slides same direction as bank = slipping turn; opposite = skidding turn.
- **Rate-of-turn indicator**: 1 bar-width ≈ **1–1.2°/s**; aligned with outer bar = **standard-rate 3°/s
  turn**; halfway = half-standard-rate. A concrete, instrument-derived turn-rate reference distinct from
  the FLCS's own roll-rate command (`flight-controls-flcs.md`'s Ps command) — useful if a FlightBox
  autopilot ever wants a "standard rate turn" mode (3°/s, a common IFR/procedure-turn convention).
  **T3 general-aviation convention, not itself F-16-specific, but explicitly the F-16 ADI's calibration
  reference per ED.**
- **Glideslope Deviation Scale**: **2.5°/dot**, ±5° top/bottom — identical number already flagged as a
  discrepancy against the generic-ILS-standard figure in `navigation-ils.md`'s ED addendum (not
  repeated in full here, cross-reference only).
- **OFF flag** = no INS attitude data (INS off/failed); **AUX flag** = degraded INS attitude data
  (INS malfunction or failed to reach even coarse alignment) — two distinct failure-severity flags, a
  useful state-machine detail for an ADI-fidelity implementation.
- **Pitch trim knob**: ±0.5° per click, purely a display-zero adjustment (does not affect the real
  aircraft attitude, only where "zero" is drawn on the sphere relative to the waterline).

#### Caution Light Panel — full trigger-condition list (ED p.58–59, official; Chuck's guide only lists
"many lights" without conditions)
| Light | Trigger condition |
|---|---|
| FLCS FAULT | Dual FLCC electronics malfunction, LEF locked, or FLCS BIT failed |
| ENGINE FAULT | Engine-related fault (clears on acknowledge) |
| AVIONICS FAULT | Avionics fault, or MUX bus lost comms with engine/FLCC |
| SEAT NOT ARMED | Ejection-seat arming lever up (disarmed) |
| ELEC SYS | Electrical fault (see ELEC Control Panel for which) |
| SEC | Engine in Secondary control mode |
| EQUIP HOT | Avionics-bay cooling insufficient — **auto-cuts FCR power** |
| NWS FAIL | Nosewheel-steering system failure |
| PROBE HEAT | Reduced airflow to pitot/air-data/AoA probes (icing) or heater/monitor failure |
| FUEL/OIL HOT | Engine fuel or oil overtemp |
| RADAR ALT | Radar altimeter malfunction |
| ANTI SKID | Anti-Skid switch OFF, **or** braking-system fault while ground speed **>5 kt** |
| CADC | Central Air Data Computer malfunction |
| INLET ICING | Engine inlet ice detected, or ice detector failed |
| IFF | Mode-4 interrogation received but reply inhibited (RF switch / MODE 4 REPLY switch / zeroized) |
| HOOK | Emergency arresting hook not up/locked |
| STORES CONFIG | STORES CONFIG (CAT I/III) switch mismatched to loadout — `flight-controls-flcs.md` |
| OVERHEAT | Engine bay, main-gear wheel wells, ECS bay, or EPU bay overheat |
| OBOGS | ECS air pressure **<10 PSI** |
| CABIN PRESS | Cockpit pressure altitude **>27,000 ft** |
| FWD FUEL LOW | Forward reservoir **<400 lb** |
| AFT FUEL LOW | Aft reservoir **<250 lb** |
| NUCLEAR / ATF NOT ENGAGED / EEC / BUC | Not implemented / no function in DCS |

This is directly useful as a FlightBox **flight-monitor cross-check list**: several of these conditions
(ANTI SKID groundspeed threshold, CABIN PRESS altitude threshold, fuel-low thresholds) are quantified
physical triggers a `FBFlightMonitor`-style system could reproduce if FlightBox ever adds a
caution/warning-light simulation layer — currently out of scope (CLAUDE.md's `FBFlightMonitor` is a
K.O./crash judge, not a caution-light simulator), noted here as a ready-made trigger table if that scope
ever expands.

#### Other gauges (quantified)
- **HYD PRESS A/B**: 0–4,000 psi gauge, 500 psi increments; **normal 2,850–3,250 psi** (matches
  `engine-fuel.md`'s and `procedures-startup.md`'s existing hydraulic-pressure figures — cross-validated).
- **EPU Fuel Quantity**: percentage, 5% increments; **at 100%, EPU runs ~10–15 minutes** — a concrete
  endurance figure for the EPU (`engine-fuel.md`'s hydrazine emergency-power unit) not previously
  quantified in this doc set.
- **Cabin Pressure gauge**: pressure altitude, 0–50,000 ft, 1,000 ft increments (feeds the CABIN PRESS
  caution light above).
- **Mechanical clock**: 8-day wind-up backup timepiece — no functional relevance to FlightBox beyond
  completeness.

## Spec — the MFD bank (this round)

The owner's four sentences are the contract:

> *"Bitte in den unteren drei Bildschirm-Quadranten (3x3) zwei Multifunktionsdisplays und ein
> Heads-Down-Display bauen. Die oberen 6 Quadranten nur fuer Zielerfassung."* ·
> *"du kannst auch drei Multifunktionsdisplays machen und Heads-Down ist einfach ein Modus.
> **Die Piloten-KI soll aber schalten, was sie grad ansieht.**"* ·
> *"und HUD vereinfachen. HUD hat nur zielerfassung/wegpunkte und alles andere im heads down"* ·
> *"du kannst f16 und mig29 prinzipiell gleich gestalten nur die verfuegbaren ansichten unterscheiden
> sich"* · *"je nach beladung unterscheiden sie sich ja auch"*

| Contract | Acceptance / measurement anchor |
|---|---|
| The screen is a 3x3 grid: the upper six quadrants are out-the-window + HUD, the lower three are the MFD bank | one rendered frame; the scene pass carries a viewport of the top two rows and the bank is drawn below it |
| The bank costs no render pass | `render passcount` is 6 (7 with a cloud deck, 5 with the HUD off) before and after — the bank appends into the HUD stage's own geometry |
| **The pilot AI switches the page, and it does so over the COMMAND BUS** | `FBCommandTarget::MfdPageSelect`, value = this module's page ordinal, HOTAS class. A `CMD_ISSUE`/`CMD_ACK` pair per switch in `events.log`, and no other write path exists |
| A display shows only PUBLISHED blocks | every page reads `FBState` and nothing else; which page a bay carries is itself a published block (`FBMfdBlock`), not renderer-side state |
| The FRAME is generic, the CATALOGUE belongs to the module | `systems/FBMfdSystem` has no virtual at all; each module calls `DeclarePages` once. The F-16 has HSD (cooperative datalink) and NO IRST page; the MiG-29 has IRST and no HSD |
| The catalogue is a function of **(module, current stores)** — read from `FBStoresBlock`, never from the mission text | a jet whose racks and drum are empty loses its SMS page mid-sortie; the page then becomes unselectable and the pilot's next tick moves off it |
| The HUD carries only what one aims and navigates with | everything that is STATE moved down: master arm, station/store inventory, rounds, fuel, systems, warnings, radar/RWR/datalink — **and this round the last four**: bank angle (→ SYS), bullseye, time-to-go and slant range (→ HSD). The element-by-element ruling is in [`hud-symbology.md`](hud-symbology.md) |
| **The bays are TRANSLUCENT over the out-the-window picture, not opaque over black** | the world covers the whole frame (`doc/render/renderer.md` §2.4) and each bay is drawn over one veil quad. Opacity is a contrast budget, not taste: `kMfdVeil = 0.87` is the smallest value that keeps HUD green at WCAG AA 4.5:1 against the brightest measured bay background (white SVS ground, 99.5th percentile `L = 0.93`). **Measured back on `payerne-full`: green 4.67–5.91:1, amber 3.45–4.37:1, per bay, ink excluded** |
| The switch is READABLE, not merely correct | the bay carries its page label, the attention bay a second frame and the box's own `SEL <t>` stamp; the browser strip shows the pilot phase and the `mfd_page` CMD_ISSUE/CMD_ACK pair |

## State

**Built: three MFDs across the bottom row of a 3x3 screen, switched by the pilot AI over the command
bus — and since this round TRANSLUCENT, with the out-the-window picture running behind them.** What is
still missing is every *panel* — ICP, DED, OSB bezels, DTE, analog instruments. The HUD was cut again
this round and now fills the whole windscreen ([`hud-symbology.md`](hud-symbology.md)).

| Piece | Status | Anchor |
|---|---|---|
| `core/FBMfdPage.h` — the page ROLE vocabulary (Fcr/Sms/Hsd/Rwr/Irst/Sys), three bays, the middle one the attention bay | built | this round |
| `core/FBMfdBlock` — catalogue + `Available` mask + the ordinal per bay + the last select's page and time | built, **not in `FBStateBusTelemetry`'s column list** (no telemetry.csv column moved) | this round |
| `FBCommandTarget::MfdPageSelect` — HOTAS class, Avionics group, value = the module's page ordinal | built | this round |
| `systems/FBMfdSystem` — declares/cuts/places/publishes; no virtual | built | this round |
| `systems/FBDisplaySystem::BuildMfd` — the generic six pages, the third override point | built | `95c2e8e` |
| The bays' veil — one `FBHudGeometry::Fill()` quad per bay at `kMfdVeil = 0.86` before its symbology | built | this round |
| The three numbers the HUD handed down: `HSD` gained `BULL bbb/rr`, `TTGmmm:ss` and the `B` slant range; `SYS` gained `PIT/BNK` | built | this round |
| F-16 catalogue `{FCR, SMS, HSD, RWR, SYS}`; MiG-29 catalogue `{FCR, IRST, SMS, RWR, SYS}` | built | this round |
| `FBPilot::SelectCockpitPage` — one action per decision tick on its OWN spacing timer, rank: someone's round in the air > a warning > the phase's own job | built | this round |
| **Measured, `mig29-intercept`:** the MiG spawns with `n019_emission off`, so there IS no FCR page and the pilot takes RWR at t = 0.0; the emission command acks at **t = 27.9** and the FCR page appears; the pilot posts `mfd_page 0` **in that same tick** and it acks at t = 28.4 | measured | browser + `gpu_native` |
| **Measured, `payerne-full`:** three selects in 734 s — SYS at t = 0.0 (nothing else published yet), HSD at t = 16.0 (the Nav block came up), SYS at t = 663.2 (the ALOW warning went active on the approach, and the SYS page shows ALOW in amber) | measured | `events.log` |

What this file *did* shape is not a display but an architecture: the DED's edit protocol is the
reference pattern for FlightBox's avionics command bus, and the CRUS "gear down freezes the computed
fields" behaviour is the documented precedent for the bus's third validity state.

| Item of this reference | FlightBox | Where |
|---|---|---|
| ICP/DED as a rendered panel, MFD formats, OSB layout, master menu, DTE | **not implemented** | [`../flightbox/clients/clients.md`](../../clients/clients.md) |
| The DED **propose → commit/reject** protocol | **built as the command bus** — every avionics input is a command with an acknowledgement `{result, reason}`, two latency classes (HOTAS vs. head-down DED) and a manoeuvre lock on head-down entries | [`../flightbox/sim/core.md`](../../core.md) |
| "Gear down **freezes** the CRUS computed fields" | **built as the `Held` block state** — deliberately frozen, last values plus the timestamp of the last real update; the reason this state exists at all | same |
| ALOW floor + BNGO threshold + selected steerpoint number as DED-entered values | **built as data**, published by `FBF16Ufc` and read by the HUD — entered by the pilot over the command bus as his brief | [`module.md`](module.md) §9 |
| Caution/warning light panel | **built as a bitmask, not a panel** — `FBWarningSystem`, with the rule that a warning whose source block is invalid reports **INHIBITED** rather than "no warning" | [`../flightbox/sim/systems.md`](../../systems.md) §6 |
| Analog instruments (AoA indicator, VVI, ADI, gauges) | **not implemented** — the quantities exist in the air-data and platform blocks, nothing draws them | — |
| Master mode selection (MODE page, backup selector) | **partially** — a master mode exists as a value (`FBMasterMode`), there is no panel or DED page that sets it | [`../flightbox/sim/core.md`](../../core.md) |

## Gaps

**Source gaps** (this file vs. its sources)

#### Remaining gap (this file, cumulative)

*Moved here verbatim from the end of the guide distillation.*
Left/Right Auxiliary Console (Landing Gear Panel, CMDS Control Panel, HMCS Control Panel, Magnetic
Compass, Fuel Quantity Indicator, PFLD), Left/Right Console (FLT CONTROL panel, EPU Control Panel, ELEC
Control Panel, ENG CONT switch, MANUAL PITCH Override switch — cross-referenced already from
`flight-controls-flcs.md`/`procedures-startup.md` but not independently re-extracted from this chapter)
remain **not processed**. The **Upfront Controls (UFC/ICP/DED) p.97–120** and **Multi-Function
Displays (MFD) p.121–127** chapters — previously the biggest gap — are now **FULL** (see the two new
sections above this one). One small residual gap within that new material: the **MARK DED page**
(priority key 7) was not found within p.97–120 despite the chapter's own cross-reference table
implying it should be there — likely lives in ED's Navigation chapter (p.163–246) instead; not
independently re-extracted this pass, marked TODO above.

**BMS Dash-34 pass (this round)** — `[BMS-34]` pp.63–94 (MFDS chapter incl. the full format list, the
interaction idioms, the symbol plate and the whole HSD page) and pp.127–159 (UFC hardware, ICP layout,
DCS, the five entry idioms, the full entry-parameter range list, the LIST/MISC page map, the DED page
inventory with BMS status, CNI/STPT/CRUS/TIME/T-ILS/BULL/MAN detail, and the fuel warning function)
are **processed** into the two addenda above. What that pass did **not** cover and is explicitly left
open:
- **Pages that the DED chapter delegates to other chapters are NOT re-extracted here**: `COM1`/`COM2`
  (radio chapter, `[BMS-34 pp.160–161]`), `IFF`/`INTG` (`[pp.170–177]`), `DEST`/`INS`
  (`[pp.162–169]`), `EWS` (`[p.360]`), `VIP`/`VRP` (`[pp.424–426]`), `LASR`, `HMCS` (`[pp.296–300]`),
  `DLNK`/Link 16 (`[pp.256–278]`), `HARM`, `WPT`. Each belongs to `datalink-iff.md`,
  `navigation-ils.md`, `defence-rwr-cm.md` or `weapons.md`. **TODO — flagged, not guessed.**
- **SMS page formats** (`[BMS-34]` SMS chapter from p.391) and **HAD / TGP page formats**
  (`[BMS-34]` pp.316+, 483+) are not distilled here — they belong to `weapons.md` and
  `radar-sensors.md` respectively and are flagged there.
- **Three source discrepancies recorded, not resolved**: RET DEPR functional `[p.98]` vs. N/I
  `[p.129]`; the trapped-fuel refuelling window 60 s `[BMS-1 p.56]` vs. 30–90 s `[BMS-34 p.159]`;
  and the SWAP brightness/contrast retention contradiction listed under M1.
- Two **internal contradictions in the source** are recorded rather than resolved: whether BRT/CON
  survive a SWAP (`[p.63]` vs `[p.64]`), and the SYM/CON "not implemented" marker on `[p.64]` against
  ED's description of all four rockers as functional.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* the interaction *semantics* — command/acknowledge/reject, latency classes, held-vs-invalid
  data — plus the handful of DED-entered values the HUD needs (ALOW, BNGO, steerpoint number) and the
  caution set as warning bits.
- *Built this round:* three MFD bays, six generic page formats, the module-owned catalogue and the
  pilot's page selection over the bus.
- *Partially:* master mode (a value without a selector), warnings (a bitmask without a panel).
- *Not at all:* ICP, DED, the OSB bezels and the master menu, DTE, analog instruments, the physical
  panels and their switches.

**Gaps this round opened, each named rather than papered over**

| # | Gap | Why it is a gap and not a decision to hide |
|---|---|---|
| D1 | **A page is not chosen at a BEZEL.** The real jet has an OSB per format per display; here ONE command target names a PAGE and the cockpit's placement rule puts it on the middle bay while the flanking two carry the remaining catalogue pages in declared order. A bay index packed into the same scalar would be a second field pretending to be a value; a second target was explicitly out of scope this round. **Source side now fully specified** (BMS Dash-34 addendum §M2): the three-centre-OSB primary/secondary scheme, DMS left/right promotion inside→outside, the master menu reached by pressing the highlighted primary, the 13-format table, the six-slot uniqueness rule with BLANK substitution, OFF/BLANK/SWAP behaviour. **This is still an IMPLEMENTATION gap** — distilling the spec does not build the bezel — but it is no longer blocked on knowing what a bezel does | the pilot's DECISION (which picture) is the thing that travels; where it lands is the cockpit's |
| D2 | **A command carries no AUTHOR.** `FBAvionicsCommand` has `{Seq, Target, Value, IssuedS, DueS}` and nothing that says whether a hand or the AI posted it, so no display can show "who". The cockpit strip shows the pilot's PHASE beside the select instead, which is a different fact honestly labelled. Adding an author field touches every command ever logged and is a round of its own. | the alternative was inventing the field in the frontend |
| D3 | **The pilot's PHASE and ENGAGEMENT STATE are not published blocks.** They exist as telemetry columns (`eng_state`) and as an `FBLog` line, which is why the browser strip can read them at all; a WGSL-drawn MFD cannot, because it reads `FBState` only. Publishing the AI's own state onto the avionics bus would put a brain on an instrument bus and make it readable by every system — not done, and named here instead. | doc/player-layer.md §1's one-way rule |
| D4 | **The page labels are the generic ROLE names**, not each jet's own nomenclature (the MiG-29 has no box called "FCR"). The role vocabulary is generic on purpose; per-airframe legends would be a second module table. | cosmetic, but it IS a claim about the aircraft |
| D5 | **No page has a range knob, a cursor or a declutter level.** The FCR/HSD scope scale is auto-chosen as the smallest of {5,10,20,40,80,160} nm that contains everything published, and the number is printed so nobody has to guess it. The radar block publishes no selected range scale to read instead. **Source side now fully specified**: the A-A FCR range rotary is exactly {5,10,20,40,80,160} NM on OSB 19/20 `[BMS-34 p.200]` — FlightBox's invented auto-scale set turns out to be the documented set — the A-G FCR rotary is {10,20,40,80} NM `[BMS-34 p.232]`, the HSD rotary is {7.5,15,30,60,120,240} DEP / {5,10,20,40,80,160} CEN `[BMS-34 p.84]`, cursor bumping thresholds are 95 % / 42.5 % (A-G FCR) and <5 % / >95 % (A-A FCR) `[BMS-34 pp.200, 233]`, and declutter is a short-press toggle plus a long-press options page `[BMS-34 pp.195, 237]`. **Still an IMPLEMENTATION gap** — nothing was built — but the "no source for a knob" half of the excuse is gone; what remains is that the radar block publishes no selected range scale | the honest alternative to inventing a knob |
| D6 | **The IRST page only exists while the head is powered.** In `mig29-intercept` the KOLS is never switched on (`blk_irst = 0` for all 782 rows), so the MiG shows no IRST page at all there; `o2-05-late-radar` (`set kols_mode …`) does show it with a contact. Correct by the availability rule, and worth stating because it makes the F-16/MiG asymmetry invisible in exactly the missions where one would look for it. | — |
| D8 | **The veil is FIXED, so a bay over dark terrain reads nearly black.** Legibility was the requirement and it is met against the BRIGHTEST background; the cost is that the promised see-through is small exactly where the world is dark. The honest fix — sampling the background per pixel — needs the frame as a texture inside the HUD pass, i.e. a second pass, and the pass count is a contract. | the number is published (0.86) instead of the effect being called "transparent" |
| D10 | **The owner's HUD cut is F-16-only.** The MiG-29 composes no display override, so it flies the GENERIC `FBDisplaySystem::BuildHud`, which still carries groundspeed, ASL, AGL and vertical speed — the very "state, not aiming" class the cut removed from the F-16. Two of those cannot simply move down: **AGL lives in `FBHudEnv`, renderer-side, not on a published block**, so an MFD page may not read it (a display shows only published blocks). Cutting the generic HUD therefore needs an AGL block first, and inventing one to win a layout round would be the wrong order. | naming the data gap beats a half-cut cockpit |
| D9 | **The bays' 5 px gaps and the 4 px margin under them are UNVEILED**: bright terrain shines between the displays. It is the world behind the panel and it is honest, but nobody designed it — no source says what is between two MFDs. | naming it beats inventing a bezel |
| D7 | **The MiG-29 gets no situation page** although it composes a datalink system. Its Lazur-M is a ground COMMAND channel, not a flight's shared picture, and no source for a MiG situation display was read. Its `FBDatalinkBlock` is therefore published and undisplayed. | the fourth addendum's own rule: do not lend it the F-16's page |

## Knowledge

**Technical depth (researched — for rebuild)**

*Researched engineering depth (public engineering sources, cited at the end). Kept separate from the
guide distillation in `## Spec` — every fact here is researched, not taken from the DCS guides.*

Avionics computer/display architecture (LRUs) behind the panel. Sources cited inline.

### Central computer — MMC (Modular Mission Computer)
- The **MMC** is the F-16C's central mission computer. On Block 50+ it **replaces three earlier LRUs**:
  the **Expanded Fire Control Computer (XFCC)**, the **HUD Electronics Unit**, and the Stores Management
  System's **Expanded Central Interface Unit (XCIU)** (airforce-technology.com).
- The MMC generates HUD symbology, drives the MFDs, runs fire-control/SMS, and hosts the master-mode logic
  (NAV/A-A/A-G). Retrofit MMC is **~42% less volume, 55% less weight, 37% less power** than the boxes it
  replaced — indicates the functional consolidation a sim's "avionics" module should mirror (one computer,
  many display surfaces).

### Display LRUs
- **UFC/ICP + DED**: the Up-Front Controls (ICP keypad) drive the **Data Entry Display** (DED) — a small
  monochrome alphanumeric display for data entry/readout, separate from the MFDs.
- **MFDs**: two multifunction displays, each with 20 OSBs, fed by the MMC over a data bus (MIL-STD-1553).
  Same page cannot show on both simultaneously (guide) — a display-manager constraint.
- **EHSI**: electromechanical/electronic HSI for nav (steerpoint/TACAN/ILS) — see `navigation-ils.md`.
- **Standby instruments**: SAI (standby attitude), altimeter, ASI, magnetic compass — independent of the
  MMC for battle-damage reversion (the reason the guide stresses the EHSI as an FCC-independent backup).
- **Data bus**: avionics interconnect is **MIL-STD-1553** multiplex — the design rationale for the
  master-mode/SOI (Sensor of Interest) arbitration and why sensors/displays are loosely coupled LRUs.

### Sources
- airforce-technology.com F-16 — MMC replaces XFCC + HUD EU + XCIU; volume/weight/power figures.
- Wikipedia *AN/APG-68*; general F-16 avionics references — 1553 bus, LRU display architecture.
- `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — Cockpit Overview/Instrument Panel
  p.43–81 (**partial**: AoA Indicator p.83, VVI p.85, ADI p.87–88, Caution Light Panel p.58–59, misc
  gauges — extracted; Aux Consoles/Left-Right Console still NOT processed) — **plus, this round:
  Upfront Controls (UFC/ICP/DED) p.97–120 and Multi-Function Displays (MFD) p.121–127, both now FULL**,
  see `PROGRESS.md`.
