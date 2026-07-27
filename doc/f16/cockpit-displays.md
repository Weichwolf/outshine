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

## State

**FlightBox has no cockpit displays.** The values are on the bus; the presentation is missing — that is
the state, stated as such in the project journal ("Cockpit-Displays: nicht begonnen"). Only the HUD is
drawn ([`hud-symbology.md`](hud-symbology.md)).

What this file *did* shape is not a display but an architecture: the DED's edit protocol is the
reference pattern for FlightBox's avionics command bus, and the CRUS "gear down freezes the computed
fields" behaviour is the documented precedent for the bus's third validity state.

| Item of this reference | FlightBox | Where |
|---|---|---|
| ICP/DED as a rendered panel, MFD formats, OSB layout, master menu, DTE | **not implemented** | [`../flightbox/clients/clients.md`](../flightbox/clients/clients.md) |
| The DED **propose → commit/reject** protocol | **built as the command bus** — every avionics input is a command with an acknowledgement `{result, reason}`, two latency classes (HOTAS vs. head-down DED) and a manoeuvre lock on head-down entries | [`../flightbox/sim/core.md`](../flightbox/sim/core.md) |
| "Gear down **freezes** the CRUS computed fields" | **built as the `Held` block state** — deliberately frozen, last values plus the timestamp of the last real update; the reason this state exists at all | same |
| ALOW floor + BNGO threshold + selected steerpoint number as DED-entered values | **built as data**, published by `FBF16Ufc` and read by the HUD — entered by the pilot over the command bus as his brief | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) §9 |
| Caution/warning light panel | **built as a bitmask, not a panel** — `FBWarningSystem`, with the rule that a warning whose source block is invalid reports **INHIBITED** rather than "no warning" | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §6 |
| Analog instruments (AoA indicator, VVI, ADI, gauges) | **not implemented** — the quantities exist in the air-data and platform blocks, nothing draws them | — |
| Master mode selection (MODE page, backup selector) | **partially** — a master mode exists as a value (`FBMasterMode`), there is no panel or DED page that sets it | [`../flightbox/sim/core.md`](../flightbox/sim/core.md) |

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

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* the interaction *semantics* — command/acknowledge/reject, latency classes, held-vs-invalid
  data — plus the handful of DED-entered values the HUD needs (ALOW, BNGO, steerpoint number) and the
  caution set as warning bits.
- *Partially:* master mode (a value without a selector), warnings (a bitmask without a panel).
- *Not at all:* every rendered display — ICP, DED, both MFDs and all their formats, DTE, analog
  instruments, the physical panels and their switches.

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
