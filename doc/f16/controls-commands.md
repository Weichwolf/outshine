# F-16C Pilot Command List — for the Avionics Command-Block Model

Source: distilled from `cockpit-displays.md` (ICP/UFC/DED §§, MFD §§ — both `ED EA Guide p.97–127`),
`hotas.md` (`Chuck p.158–160` + `ED EA Guide p.82–88`), and cross-referenced against
`flight-controls-flcs.md` (autopilot mode switches). No new source pages were read for this file — it
is a **re-cut** of material already distilled elsewhere, reorganized around one question: *what
discrete command can the pilot actually issue, through what control, under what precondition, with
what feedback and what documented failure mode.*

## Spec

### Why this is a separate file, not folded into `cockpit-displays.md`/`hotas.md`
The two source files are organized **by physical panel** (ICP, DED pages, MFD, SSC, throttle) because
that's how the guides present them and how a pilot learns them. This file is organized **by command**
across all panels, because that's the shape FlightBox's upcoming avionics command-block model needs:
one typed command block per pilot action, independent of which physical control issues it. The
pilot-KI drives every one of these through the *same* command path a human would (per this round's
task mandate) — this list is the enumeration of that path's vocabulary. Every row here traces back to
one of the two source files; this file adds no new facts, only the cross-device regrouping +
explicit precondition/failure columns the source files don't always spell out as such.

### The DED's propose → commit/reject protocol (the canonical command pattern)
This is the single most directly reusable architectural fact in this whole pass. Every DED field edit
follows **the same three-step cycle**, independent of which page (`cockpit-displays.md`'s new ICP/DED
addendum, `ED EA Guide p.100`):
1. **Select** the field (DCS/Dobber U/D moves the asterisks onto it).
2. **Propose** a new value (ICP keypad digits, or the Inc/Dec rocker for arrow-marked fields) — the
   field **highlights** to show "typed but not committed."
3. **Resolve**: **ENTR commits** (field returns to normal display, new value active) — **or** **RCL**
   rejects (1st press = backspace one digit; 2nd press with nothing further typed = full reject,
   restore prior value) — **or** **DCS RTN** silently discards any uncommitted edit by leaving the page.

This maps directly onto "Kommandoblöcke mit Quittierung und Ablehnungsgründen": a DED-field-write
command block is naturally `{field, proposedValue}` → ack is `{committed: bool, reason?}`, where
`reason` distinguishes an explicit pilot reject (RCL/RTN) from a system-side rejection (value out of
range — not spelled out per-field in either guide, a real documented gap, see "Gaps" below).

**Read-only DED fields are not commands at all** — they are periodically-recomputed **output blocks**
(ETA, ground-speed-required, fuel-at-steerpoint, optimum cruise altitude, time-to-Bingo, optimum Mach).
One documented nuance for the output-block "Gültigkeitsflag" design: DCS/ED models gear-down as
**freeze-at-last-value**, not invalidate — several CRUS-page computed fields explicitly stop updating
(not blank out) once the gear is down (`cockpit-displays.md` CRUS table). A command-block architecture
should decide deliberately whether "stale" is a validity-false state or a distinct "held" state; the
real jet does the latter.

---

### 1. ICP / DED commands

#### 1.1 Navigation-within-DED (control-plane, not data-plane — moves the cursor/page, doesn't change
mission state)
| Command | Trigger | Precondition | Effect | Feedback | Failure/rejection |
|---|---|---|---|---|---|
| `SelectOverridePage(COM1\|COM2\|IFF\|LIST)` | ICP override button | any page | jumps to that page | DED shows page | 2nd press of the SAME override button returns to the previous page (not necessarily CNI); **IFF page is N/I** — button press has no visible effect |
| `SelectPriorityPage(1..9,0)` | ICP keypad, **only while CNI shown** | current page = CNI | jumps to the numbered priority page | DED shows page | keypad presses when CNI is NOT shown are data-entry instead — same physical buttons, different semantic depending on page state |
| `ReturnToCni()` | DCS → RTN (left) | any page | jumps to CNI | DED shows CNI | **discards any uncommitted edit** on the page just left |
| `SequencePage()` | DCS → SEQ (right) | on a multi-subpage group (CRUS TOS/RNG/HOME/EDR; MODE A-A/A-G toggle) | advances to next subpage | DED shows next subpage | none documented |
| `MoveFieldCursor(prev\|next)` | DCS → UP/DOWN | any page with ≥1 field | asterisks move to prev/next field | asterisks visibly relocate | none |
| `OpenListSubpage(1..9,0)` | ICP keypad while LIST shown | LIST page displayed | opens named subpage (DEST/BNGO/VIP/NAV/MAN/INS/DLNK/CMDS/MODE/VRP, 0→MISC) | DED shows subpage | `INTG` is **N/I** |
| `OpenMiscSubpage(1..9,0)` | ICP keypad while MISC shown | MISC page displayed | opens named subpage (MAGV/HMCS/LASR/GPS/HTS/BULL/HARM, etc.) | DED shows subpage | `CORR`,`OFP`,`INSM`,`GPS`,`DRNG` **N/I** |

#### 1.2 Field edit (data-plane — the propose/commit/reject cycle above, applied per page)
| Command | Trigger | Precondition | Effect | Feedback | Failure/rejection |
|---|---|---|---|---|---|
| `TypeDigit(0-9)` / `TypeNegative()` | ICP keypad / **0/M-SEL** for the minus sign | asterisks on an editable field | appends to proposed value, field highlights | field text highlighted | none — always accepted into the edit buffer |
| `Backspace()` | RCL, 1st press | field highlighted (mid-edit) | removes last typed digit | field text updates | — |
| `RejectEdit()` | RCL, 2nd press (no further typing since) | field highlighted, nothing typed since last RCL | discards all new input, restores original value | field un-highlights | — |
| `CommitField()` | ENTR | field highlighted | new value becomes active | field un-highlights | **no per-field range-check documented** in either guide — real hardware presumably clamps/rejects out-of-range entries; not specified, flagged as a gap |
| `IncrementField()` / `DecrementField()` | DED Inc/Dec rocker | asterisks on an arrow-marked field | steps value up/down (e.g. steerpoint number, preset channel) | field value changes live, no highlight (not a "proposed" edit — takes effect immediately) | none |
| `ToggleOrEnable()` | 0/M-SEL | asterisks on a boolean/enum field (CRUS mode enable, MODE-page mode commit) | toggles/commits the shown state | field highlights when enabled | MODE page: **inoperative if throttle DOG FIGHT switch is not centered** — a hard precondition failure |

#### 1.3 Page-specific commands (each `{field, value}` pair follows the §1.2 commit cycle; only the
semantic payload is listed here — see `cockpit-displays.md` for full field tables)
| Command | Page | Precondition | Effect | Feedback | Failure/rejection |
|---|---|---|---|---|---|
| `SetCaraAlow(ft)` | ALOW | — | sets AGL low-altitude warning threshold | none until triggered | warning **only fires if radar altimeter is powered+transmitting** — a runtime precondition on the *effect*, not the *command* |
| `SetMslFloor(ft)` | ALOW | — | sets MSL low-altitude warning threshold | none until triggered | — |
| `SetCrusMode(TOS\|RNG\|HOME\|EDR, on\|off)` | CRUS | — | enables one CRUS mode, **auto-disables any other active CRUS mode** | HUD speed/altitude caret appears; TOS mode also replaces HUD Time-To-Go with ETA | gear-down → HUD carets removed, DED computed fields freeze at last value |
| `SetDesiredTos(steerpoint, time)` | CRUS TOS | — | sets target time-over-steerpoint | ETA/required-GS recompute | invalid (negative) TOS → field goes blank instead of erroring |
| `SetHomePoint(steerpoint)` | CRUS HOME | — | changes which steerpoint HOME calculations target; **also becomes the selected steerpoint** | Fuel-at-Home/Optimum-Alt recompute | — |
| `SetSystemTime(HHMMSS)` | TIME | — | sets Zulu system time | — | auto-overridden by GPS when GPS available (manual entry only matters w/o GPS) |
| `SetHackTime(HHMMSS)` / `StartStopHack()` / `ZeroizeHack()` | TIME | — | sets/starts-freezes/clears the Hack stopwatch | Hack field appears/disappears on CNI | — |
| `SetDeltaTos(±HHMMSS)` | TIME | — | shifts **every** steerpoint's TOS by one delta, cumulative while page stays open | all steerpoint TOS values shift | DELTA field itself zeroes on page-leave (applied shift persists) — a "commit is permanent, display resets" quirk |
| `SetBingo(lb)` | BNGO | — | sets fuel-warning threshold | — | effective ceiling ~6,070 lb (FUEL QTY SEL=NORM) / 6,667 lb (other) — warning fires at the ceiling even if Bingo is set higher |
| `SetWingspan(ft)` | MAN | — | sets EEGS Level-II passive-ranging target wingspan | funnel width changes | — |
| `SetMasterModeBackup(A-A\|A-G\|NAV)` | MODE (LIST DED backup path) | throttle DOG FIGHT switch centered | changes master mode (same as ICP buttons) | DED field highlights when shown mode == active mode; pressing again → NAV | **rejected outright** if DOG FIGHT switch is not centered |
| `CycleSteerpoint(prev\|next)` | any page with a Selected-Steerpoint field | Inc/Dec asterisks on that field | changes selected steerpoint | field updates, HSD/HUD steerpoint symbology follows | — |

#### 1.4 ICP hardware commands (not DED-page-mediated)
| Command | Trigger | Effect | Feedback | Failure |
|---|---|---|---|---|
| `SelectMasterMode(A-A\|A-G)` | ICP Master Mode button | sets master mode; pressing the **active** mode's own button returns to NAV | HUD Master Mode Status text changes | none documented (but see §3 DOG FIGHT-switch override, which takes precedence) |
| `SetHudSymBrightness(delta)` | SYM knob | adjusts HUD symbology brightness | visible immediately | — |
| `SetReticleDepression(0–260 mrad)` | RET DEPR knob | moves MAN-bombing depressible reticle | reticle position on HUD | only visible/relevant in MAN bombing sub-mode |
| `SetDriftCutout(DRIFT C/O\|NORM)` | DRIFT C/O & WARN RESET switch | cages FPM to HUD center + Attitude Bars to boresight, or releases | HUD FPM/bars behavior changes | NORM is spring-loaded (momentary, not a persistent select) |
| `WarnReset()` | same switch, WARN RESET position | clears HUD warning text + voice message, resets Max-G indicator to 1.0 | HUD warning text disappears | — |

---

### 2. MFD commands
| Command | Trigger | Precondition | Effect | Feedback | Failure/rejection |
|---|---|---|---|---|---|
| `PressOsb(n)` | any of 20 OSBs | — | activates whatever function is currently labeled at that OSB (context-dependent — format-specific, see `radar-sensors.md`/`weapons.md`/`navigation-ils.md` per format) | OSB label highlights if now-active | — |
| `OpenFormatMenu(fsButton)` | Format-Select OSB (13/14/15), 1st press if not highlighted / opens directly if already highlighted | — | opens Format Selection Master Menu | Menu page displayed | — |
| `AssignFormat(fsButton, format)` | OSB next to format name, inside Master Menu | Master Menu open | assigns format to that FS button; **auto-evicts** the format from any other FS button it was already on (across both MFDs) → that old slot becomes BLANK | MFD exits Menu, shows new format | selecting the already-assigned format, or pressing any FS button, exits the Menu **without change** |
| `SwapMfdFormats()` | Swap button | — | swaps displayed formats **and** their FS-button assignments between left/right MFD | both MFDs' content swaps | — |
| `ToggleDeclutter()` | Declutter button | — | removes OSB label text (commands still live) | labels disappear | **N/I** in DCS |
| `AdjustGain/Sym/Brt/Con(delta)` | respective rocker, hold-to-ramp | — | adjusts video gain / symbology intensity / brightness / contrast | visible immediately | GAIN's meaning is format+mode-dependent (radar-map vs. MTI vs. FLIR gain — see `cockpit-displays.md`) |
| `DteLoadPartition(FCR\|MPD\|COMM\|INV\|PROF\|MSMD\|ELINT\|TNDL\|GPS\|COLR\|ALL)` | DTE format OSB | DTC inserted, DTU powered | uploads that data partition from DTC into MMC | DTE Advisory Messages field shows status/errors | **MPD upload requires CMDS MODE knob = STBY first**, else erroneous CMDS data entry — a documented precondition failure; `CLSD`/`SMDL`/`NCTR` **N/I** |

---

### 3. HOTAS commands
Full SOI/mode-dependent action matrices (TMS/DMS/CMS/EXP-FOV) are large 4-way×2-duration tables kept
in `hotas.md`'s ED addendum, not reproduced here — referenced by row below. Duration convention:
**short <0.5 s, long >0.5 s** (exceptions needing a full 1 s are noted).

#### 3.1 SSC (stick)
| Command | Trigger | Precondition | Effect | Feedback | Failure |
|---|---|---|---|---|---|
| `Trim(pitch\|roll, dir)` | Trim hat, 4-way | — | trims nose up/down, wing left/right down | aircraft response; trim indicator (if modeled) | — |
| `FireWeapon(hold)` | Weapon Release button, press-and-hold | A-A missile selected & armed, or A-G store selected & armed | fires missile / releases store | weapon leaves station, SMS updates | arming/master-arm preconditions — `weapons.md` |
| `TriggerStage1()` | Trigger, 1st detent | TGP powered | fires laser designator/ranger | range readout | no TGP → no effect (not separately flagged as rejection) |
| `TriggerStage2()` | Trigger, 2nd detent | gun selected+armed, **or** CCIP/STRF sub-mode | fires gun, **or** fires laser designator/ranger for 30 s | gun tracer/impact, or laser-on indication | mode-conditional: same physical action, two different systems depending on sub-mode |
| `MslStep(short\|long)` | MSL STEP button | context-dependent (ground/air, AIR REFUEL OPEN, master mode) | NWS toggle **or** boom disconnect **or** missile-station/type step **or** A-G mode cycle — see `hotas.md` §MSL STEP table | varies | — |
| `TmsCommand(dir, short\|long)` | TMS, 4-way | SOI-dependent | designate/track/reject per current SOI — full matrix `hotas.md` | varies by SOI | most cells "No Action" outside their valid SOI — an implicit precondition-gated no-op, not an explicit rejection |
| `DmsCommand(dir, short\|long)` | DMS, 4-way | — | changes SOI, or cycles MFD format, or swaps SOI between MFDs — full matrix `hotas.md` | SOI indicator changes / MFD format changes | — |
| `CmsCommand(dir)` | CMS, 4-way | CMDS mode (MAN/SEMI/AUTO) gates RIGHT/AFT semantics | dispense chaff/flare program, or ECM enable/disable — full matrix `hotas.md`, `defence-rwr-cm.md` | CMDS status display, chaff/flare count decrements | mode-gated: same switch position means different (sometimes opposite) things depending on CMDS mode |
| `PaddleOverride(hold)` | Paddle switch | — | disengages autopilot for duration held; on release, autopilot captures NEW reference at current pitch/roll/alt per active PITCH/ROLL modes | control authority returns to stick immediately on press | **does not disengage** autopilot in HDG SEL/STRG SEL roll modes (`flight-controls-flcs.md`) — a real documented exception to what "paddle" normally does |
| `ExpandFov(short\|long)` | Expand/FOV button | SOI-dependent | cycles FOV/zoom for current SOI's sensor — full matrix `hotas.md` | sensor FOV changes | — |

#### 3.2 Throttle (TQS)
| Command | Trigger | Precondition | Effect | Feedback | Failure |
|---|---|---|---|---|---|
| `SetThrottle(OFF\|IDLE\|MIL\|AB\|MAX AB)` | throttle lever position | — | commands engine thrust (`engine-fuel.md`) | engine spool/N1/EGT response | — |
| `TransmitRadio(UHF\|VHF)` | UHF/VHF Transmit switch, AFT/FWD | radio powered | keys the respective radio | — | — |
| `ToggleDatalinkFcrInfo()` | switch LEFT, short | — | toggles datalink info overlay on FCR | FCR display changes | — |
| `CycleDatalinkFilter()` | switch RIGHT, short | — | cycles FCR datalink filters | FCR display changes | — |
| `TransmitDatalinkContact()` | switch RIGHT, long (>0.5s) | — | transmits selected steerpoint/SPI/SEAD-target over datalink | — | `datalink-iff.md` |
| `SetManRangeZoom(delta)` | MAN RNG/UNCAGE, rotate | — | manual TGP zoom | TGP video zoom | — |
| `Uncage(short)` | MAN RNG/UNCAGE, depress <0.5s | mode-dependent | uncages AIM-9 seeker (A-A/Missile Override/Dogfight), toggles TGP LST (TGP SOI), or (gear down, airborne, NAV mode) declutters HUD (removes Roll Indicator + ILS bars, moves Heading Scale to top) | seeker growl/reticle, or HUD symbology change | — |
| `ToggleGunStrafe(long)` | MAN RNG/UNCAGE, depress >1.0s | A-G mode | toggles Gun STRF sub-mode | HUD symbology changes to strafe reticle | — |
| `SetDogfightOverride(DGFT\|MSL ORIDE\|center)` | DOG FIGHT switch, 3-pos | — | outboard: forces ACM radar (standby until Tx commanded) + gun/missile HUD symbology, tag "DGFT"; inboard: forces RWS radar + A-A missile symbology, gun unavailable, tag "MRM"/"SRM"/"HOB"/"MSL"; center: revert to last master mode | HUD Master Mode Status tag changes | **overrides every mode except Emergency Jettison** — highest-priority mode-select command in the whole system |
| `SetAntennaElevation(deg)` | ANT ELEV knob, rotary w/ center detent | FCR SOI relevant | sets FCR antenna elevation manually | FCR display | — |
| `SlewCursor(dx,dy)` | RDR CURSOR/ENABLE, multi-directional | SOI-dependent (FCR/HSD/HAD cursor, TGP sensor, AGM-65 seeker) | slews the active sensor's cursor/seeker | cursor/seeker moves | — |
| `SwapBoreSlave(hold)` | RDR CURSOR/ENABLE, depress | A-A mode | swaps AIM-9 BORE/SLAVE for duration held | seeker reticle mode changes | — |
| `StepPreVisBore()` | RDR CURSOR/ENABLE, depress | A-G mode, AGM-65 selected | steps PRE→VIS→BORE | WPN format indicator changes | — |
| `SetSpeedbrake(retract\|hold\|extend)` | SPD BRK switch, 3-pos aft-momentary | — | moves speedbrakes; **any intermediate deflection achievable** by releasing mid-travel | speedbrake position indicator (if modeled), drag/deceleration | **max 60°** gear-up/right-gear-not-locked; **capped 43°** with right main gear down-and-locked, **overridable by holding SPD BRK aft**; cap lifts automatically once nose gear compresses post-touchdown |

---

### 4. Autopilot mode commands (cross-ref `flight-controls-flcs.md`)
| Command | Trigger | Precondition | Effect | Feedback | Failure |
|---|---|---|---|---|---|
| `SetPitchMode(ALT HOLD\|OFF\|ATT HOLD)` | Pitch Mode switch, MISC panel | — | engages/disengages pitch autopilot mode | — | **a pitch mode must be active for any roll mode to engage** — roll-mode commands are rejected (or simply inert) without one |
| `SetRollMode(HDG SEL\|OFF\|ATT HOLD\|STRG SEL)` | Roll Mode switch, MISC panel | a pitch mode is active | engages/disengages roll autopilot mode | — | see above |
| `PaddleOverride(hold)` | SSC Paddle switch | — | see §3.1 | — | **exception**: does not disengage in HDG SEL/STRG SEL (only overrides while held, autopilot doesn't drop the mode) |

### 6. Documented rejection/precondition patterns (summary, for the "Ablehnungsgründe" design)
Every concrete case found in the source material where a command can fail or be gated, collected in
one place:
1. **Explicit reject via pilot action**: RCL (2nd press) / DCS RTN discard an uncommitted DED edit —
   the only "pilot changed their mind" case in the material.
2. **Hardware precedence lockout**: MODE DED page master-mode command rejected outright if the throttle
   DOG FIGHT switch is not centered — a physical-switch-state precondition on a software command path.
3. **Sequencing precondition**: roll autopilot mode requires an active pitch mode first — a
   state-machine precondition, not a hardware lockout.
4. **Effect-side precondition (command accepted, effect gated)**: CARA ALOW warning requires the radar
   altimeter powered+transmitting; the DED command to *set* the threshold always succeeds regardless.
5. **Silent no-op outside valid context**: most TMS/DMS/CMS/EXP-FOV matrix cells are "No Action" when
   the SOI/mode doesn't apply — not a rejection with a reason, just an inert command.
6. **N/I (not implemented in DCS)**: IFF DED page, several MISC/LIST subpages, RESET MENU, Declutter,
   TFR/FLIR/RCCE MFD formats, CLSD/SMDL/NCTR DTE partitions — a DCS-module limitation, not an F-16
   system limitation; a FlightBox rebuild is not bound by these.
7. **Soft-failure / data-corruption risk, not outright rejection**: uploading the DTE MPD partition
   while CMDS MODE ≠ STBY causes "erroneous data entry" rather than a clean reject — the only
   documented case of a command succeeding but corrupting state as a side effect.
8. **Implicit value clamp**: BNGO warning fires at a ceiling (~6,070/6,667 lb) even if the pilot enters
   a higher Bingo value — the command is "accepted" (ENTR succeeds, field shows the entered value) but
   the system-level effect silently clamps.

None of the two source guides documents a generic **numeric range-validation** failure (e.g. "CARA ALOW
rejected if > 50,000 ft") for any DED field — every field-edit "failure" case found is either a pilot
self-reject (RCL/RTN) or a state/mode precondition on a *different* control, never a bounds-check
rejection on the field itself. This is a real, flagged gap: a FlightBox command-block model will need
to invent its own range-validation policy for numeric DED fields, since neither guide specifies one.

## State

**This is the file with the highest implementation coverage of the whole set** — FlightBox's avionics
command bus was built from it, pattern for pattern.

| Item of this reference | FlightBox | Where |
|---|---|---|
| propose → commit/reject as the command shape | **built** — `core/FBAvionicsCommand.h`: a command is `{target, proposed value}`, the answer an acknowledgement `{result, reason}` | [`../flightbox/sim/core.md`](../flightbox/sim/core.md) |
| The two latency classes derived in §5 (HOTAS sub-second vs. head-down DED multi-second) | **built** — `FBCommandBus` enforces the latency of the class the command belongs to, plus a manoeuvre lock for head-down entries | same |
| The §6 rejection catalogue | **built as the reject-reason enum**, plus **two reasons FlightBox owns**: `OutOfRange` (the sources document no per-field bounds check — FlightBox rejects rather than silently clamping) and `ChannelBusy` | same |
| Read-only DED fields as periodically recomputed **output blocks** | **built** — `FBState` is exactly that: typed blocks, one writer each, with a `{stamp, status}` head | same |
| "Gear down freezes the CRUS fields" | **built as the `Held` state** — the third validity value exists because of this documented precedent | same |
| Effect-side vs. command-side precondition (ALOW set always succeeds; the *warning* needs a powered CARA) | **built** — the radar altimeter is the reference case: unpowered it publishes no 0 ft, it invalidates its block, and the warning reports INHIBITED | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §5–§6 |
| Hardware-precedence lockout (weight on wheels, master arm) | **built** — `hardware_precedence` on weapon release and gun trigger | [`../flightbox/sim/weapons-and-damage.md`](../flightbox/sim/weapons-and-damage.md) §2–§3 |
| A command to a destroyed box | **built** — the bus answers `rejected / system_failed`; nothing had to be written for damage to reach the command path | same, §8 |
| The pilot as a bus client | **built and exclusive** — the autonomous pilot holds no system pointers; what he enters in flight is his brief (`brief_*` mission lines), one input per decision tick, in its latency class, with the risk of rejection | [`../flightbox/sim/pilot-ai.md`](../flightbox/sim/pilot-ai.md) §2 |
| `WeaponSelect` | **deliberately `NotImplemented`** — the jet has it, FlightBox does not, and the command says so instead of silently succeeding | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) Gaps 4 |
| The actual DED/MFD command *surface* (pages, fields, OSBs) | **not implemented** — there is no display to enter them on; only the handful of values the HUD needs exist | [`cockpit-displays.md`](cockpit-displays.md) |
| SOI, press-duration classes, DOG FIGHT precedence | **not implemented** | [`hotas.md`](hotas.md) |

## Gaps

**Source gaps** (this file vs. its sources — the list that stood under the previous `## Gaps` heading,
kept verbatim)

- Per-field numeric range/validation rules (see §6 closing note) — not documented in either source.
- **MARK DED page** (steerpoint markpoint entry) — not found in ED p.97–120 despite the chapter's own
  cross-reference table implying it belongs there; likely lives in the Navigation chapter (p.163–246),
  not independently re-extracted this pass (see `cockpit-displays.md`).
- FCR/TGP/WPN/SMS/HSD MFD-format-internal OSB commands are **not** repeated here — they're already
  fully enumerated in `radar-sensors.md`/`weapons.md`/`navigation-ils.md`; only the format-management
  layer (assign/swap/declutter) is covered above.
- IFF procedure command detail remains a gap (`datalink-iff.md`).

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* the command protocol itself — propose/acknowledge/reject, latency classes, rejection
  reasons, output blocks with three-state validity, effect-side gating.
- *Partially:* the command *vocabulary* — only the commands whose target systems exist are issuable
  (designate, gun trigger, weapon release, countermeasures, CMDS mode, ALOW/BNGO/master-arm brief
  entries); everything DED- or MFD-page-shaped has no target.
- *Not at all:* the physical control layer (no bound HOTAS, no panels), SOI, press-duration semantics,
  master-mode selection commands, DTE, and any command whose subject is a display.

## Knowledge

### 5. Command-rate ceiling (task priority 3 — "ohne die Hand zu bewegen")
What a pilot can realistically command **without moving a hand off stick/throttle**, and how fast:
- **Every HOTAS switch action above is a single discrete press/hold/direction** — the physical ceiling
  is however fast a human can move a 4-8-way switch and release it, gated by the **short/long
  press-duration discriminators** the avionics itself uses to distinguish two different commands on
  the same switch: **0.5 s** (most TMS/DMS/CMS/EXP-FOV/MAN-RNG-depress cells) and **1.0 s** (a few TMS
  cells, MAN-RNG long-depress for Gun Strafe).
- This means **the practical minimum time between two DIFFERENT commands on the same switch is ~0.5–1
  s** if the pilot needs the long-press variant of either — a real, book-derived upper bound on HOTAS
  command rate, not a guessed one. Commands on **different** switches (e.g. TMS forward + CMS aft) have
  no such coupling and can be issued in the same instant (two hands, many switches).
- The **paddle switch** is the fastest-acting command in the set: instantaneous authority handoff
  (autopilot ↔ stick) on press/release, no debounce/duration semantics documented — it is a level
  signal, not an edge-triggered command, and should probably be modeled as such (state, not event).
- **ICP/DED commands are categorically slower**: every DED field write needs the propose→commit cycle
  (§ above) — realistically several seconds per field (select field, type digits, press ENTR) — this is
  **head-down, not HOTAS**, and is the class of command a pilot-KI would issue between maneuvering
  segments, not during one. A command-block rate model should treat **HOTAS commands** (sub-second,
  usable while maneuvering) and **DED commands** (multi-second, effectively "administrative") as two
  different latency classes.
- **abgeleitet**: no book source states a numeric "commands per minute" figure anywhere in either
  guide; the 0.5 s/1.0 s discriminators above are the only quantitative timing facts documented, and
  the HOTAS-vs-DED latency-class split is the derived, not stated, conclusion drawn from them.

*Kept numbered §5 for cross-reference stability, and filed here rather than in `## Spec` because it is
**derived** (marked `abgeleitet` above), not documented by either guide.*
