# F-16C Navigation & ILS

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 16 — Navigation & ILS Landing, pp. 673–777.

## Navigation sources
Navigation via **HSD** (Horizontal Situation Display), **EHSI**, **HUD**, and **ADI** localizer/glideslope
bars. Standby magnetic compass = backup. DED/ICP consult & edit nav data; FCR page also shows steerpoints.
**TACAN and ILS supported; NDB/ADF navigation is not.**

## Navigation point types
| Type | Purpose |
|---|---|
| **Steerpoints** (waypoints) | Pre-planned route reference points; create/edit/build flight plans |
| **Markpoints** | Mark a point of interest (overflown area, enemy sighting) |
| **Anchor Point / Bullseye** | Common geographic reference shared by friendly forces |
| **Reference points** | VIP, VRP, PUP, OAP (offset targeting) |

## Steerpoint database (99 total)
| # | Function |
|---|---|
| 1–24 | Navigation route / flight planning |
| 25 | Bullseye (auto-assigned) |
| 26–30 | Ownship markpoints |
| 31–54 | HSD lines (4 lines × up to 6 points) |
| 56–70 | Pre-planned threats |
| 71–80 | Datalink markpoints |
| 81–89 | Open (pilot use) |
| 90–99 | AGM-84 HARPOON (some blocks); open on Block 50 |

DED pages: **STPT** (ICP STPT/4, edits steerpoint, affects HSD) · **DEST** (LIST→1, edit without affecting
HSD; UTM→DIR→OA1/2 via Dobber SEQ) · **BULLSEYE** (LIST→0 MISC→8 BULL).

### Steerpoint navigation (HUD)
- Align the **Steerpoint Tadpole with the FPM**; tadpole UP = ahead, DOWN = behind.
- **Steerpoint Diamond** points at steerpoint; crossed-out = out of HUD FoV.
- HUD shows distance (nm), TTG, direction. See `hud-symbology.md`.

## EHSI (Electronic Horizontal Situation Indicator)
Primary gauge for steerpoint + TACAN navigation (extra data not on HUD/DED; battle-damage backup).

Elements: current heading (lubber line), aircraft symbol, course pointer + setting, course deviation
scale/line (CDI), bearing pointer, heading bug, range indicator (nm), MRK BCN light.

**Mode Selector ("M" button)** cycles: **NAV** · **PLS/NAV** (ILS + nav) · **TCN** (TACAN) · **PLS/TCN**
(ILS + TACAN). Course knob OUT = set course; pressed IN = brightness.

## HSD (Horizontal Situation Display)
Plan-view tactical picture: ownship, steerpoints, flight plan, range rings, datalink contacts, threats.
- **Range rings**: outer = display range, middle = ⅔, inner = ⅓.
- **SOI** via DMS DOWN → cursor bearing/range from selected steerpoint (or bullseye if active) to cursor.
- **Expand/FOV**: NORM → EXP1 (2:1) → EXP2 (4:1); press-hold >½ s = Zoom (auto-scale to flight members,
  down to 5 nm).
- **CPL/DCPL**: couples HSD range to FCR range (one range change scales both).
- CNTL pages toggle overlays: AIFF, PRE, FCR ghost cursor, NAV1–3 routes, LINE1–4 map lines, RINGS,
  A/G TGTS, A SURV, G FRND, SAM (+ threat rings), LAR, SHIP, PDLT RNG. Datalink XMT: OFF / TNDL.

## TACAN
Directional + distance (airdromes, tankers, carriers; VORTAC = collocated VOR+TACAN).
1. MIDS LVT knob ON (TACAN is part of MIDS). 2. CNI page (Dobber LEFT/RTN). 3. ICP **T-ILS(1)** →
TACAN-ILS DED page. 4. Dobber DOWN to CHAN, keypad channel (e.g. 44), ENTR. 5. M-SEL(0)+ENTR toggles
band X/Y. 6. Dobber RIGHT (SEQ) to **TCN T/R**. EHSI mode → TCN.

## Bullseye (anchor point, default steerpoint 25)
- Bearing + range from bullseye to aircraft shown on HUD, FCR, HSD.
- Activate: LIST → 0 (MISC) → 8 (BULLS) → M-SEL(0) to toggle active.
- Reassign: BULL DED page → Dobber DOWN to BULL field → inc/dec or steerpoint number + ENTR.

## Reference points
- **VIP** (Visual Initial Point), **VRP** (Visual Reference Point), **PUP** (Pull-Up Point) — A-G attack geometry.
- **OAP** (Offset Aimpoint): a steerpoint offset by true bearing + range (ft) + separate elevation; up to
  two per steerpoint (OA1/OA2), stored on DEST OA1/OA2 pages, move with their parent steerpoint. Shown as
  a HUD triangle in A-G mode with CCRP + OA1 sighting on the TGP.

## INS drift & navigation fix
INS drifts over time. A **nav fix** re-aligns it by designating a known steerpoint's true location:
1. Master Mode NAV; CNI page; select steerpoint via DED inc/dec.
2. Designate with a sensor (TGP/FCR/HUD/OFLY). TGP method: SOI the TGP (DMS DOWN), slew reticle to the
   expected steerpoint location, TMS UP (point track). Laser ARM + trigger for ranging ("L" flashes on HUD).
3. ICP **8 (FIX)** → FIX DED page; Dobber RIGHT (SEQ) selects fix method. DELTA field = position drift.
4. Within **10 nm** of the steerpoint, TMS UP freezes DELTA → ENTR performs the fix (returns to CNI).

## ILS approach tutorial (example: Batumi RWY 13)
Example data: ILS freq **110.30**, runway heading **120 mag / 126 true**.

### Tune
1. RADAR ALTIMETER — ON (FWD); adjust ILS audio.
2. CNI page (Dobber LEFT/RTN).
3. ICP **T-ILS(1)** → TACAN-ILS DED page.
4. Dobber DOWN to **ILS FRQ**, keypad "11030" → ENTR.
5. **CMD STRG** highlights when ILS signal received.
6. **CRS** field auto-selects → set course to runway heading (120) → ENTR.
7. EHSI mode "M" → **PLS/NAV** (slaves EHSI to ILS).
8. Verify **NAV** Master Mode on HUD (A-A/A-G ICP button reverts to NAV).

### Fly
9. Align with runway using EHSI bearing pointer, CDI, ADI localizer bar, and **HUD localizer steering bar**.
10. Close enough → **Glide Slope Fail Flag** disappears; vertical guidance for a **3° glideslope**.
11. Fly to glideslope: center the **Glide Slope Steering Bar** + **Localizer Steering Bar** into a perfect
    cross on the FPM ("center the bars").
    - GS bar **above** FPM center = below glideslope → climb.
    - Localizer bar **right** of FPM = fly right to center.
12. Valid localizer → **Command Steering Symbol** (circle) on HUD; **tic mark** = pitch steering valid.
13. Localizer + glideslope captured → deploy landing gear → **"E" AoA bracket** appears.
14. LANDING light UP. 15. Deploy speedbrake. Then fly the approach at 11° AoA (see `procedures-landing.md`).

---

# Technical depth (researched — for rebuild)

ILS geometry and deviation scaling for a faithful localizer/glideslope model. Sources cited inline.

## ILS beam geometry & deviation scaling
| Element | Value |
|---|---|
| Standard glidepath angle | **3°** (varies with terrain) |
| Localizer full-scale CDI deflection | **±2.5°** from runway centerline |
| Glideslope full-scale deflection | **±0.7°** (beam width 1.4°) |
| Glideslope "dot" | **0.14°** per dot |
| Localizer course width at threshold | **~700 ft** (angular width set so full width = 700 ft at threshold) |

Sources: pilotscafe / code7700 / PPRuNe tech-log — standard HSI/CDI ILS scaling.

## Modeling implications
- The **HUD localizer/glideslope steering bars** and the **EHSI CDI** should both be driven from the same
  angular deviations: lateral = angle off the localizer centerline (saturating at ±2.5°), vertical = angle
  off the 3° glidepath (saturating at ±0.7°). This gives the correct "bar sensitivity increases as you
  approach the runway" behavior (constant angular width → shrinking linear width).
- **Command Steering symbol** = flight-director law combining localizer + glideslope error into a single
  steer-to cue; the **tic mark** appears when pitch (glideslope) data is valid (guide). A rebuild computes
  a director command from the two deviations, not just displays raw needles.
- **Glideslope capture** at ~0.7° (one full-scale) is where the guide's "Glide Slope Fail Flag" clears.
- On the F-16, ILS is tuned as **frequency** (e.g. 110.30) via the DED **T-ILS** page and the EHSI is
  slaved with **PLS/NAV** — the receiver provides the angular deviations above to both HUD and EHSI/ADI.

## Navigation system context
- Steerpoint navigation is **INS + GPS** based (the FLCS/FCC use INS for sideslip and steering); TACAN
  provides bearing/range to a ground station; ILS provides the precision-approach angular guidance. NDB/ADF
  is not supported (guide). A rebuild needs: INS position (drifting, correctable via nav-fix), GPS
  correction, TACAN ρ/θ, and ILS localizer+glideslope deviation.
- **INS drift + nav-fix** (guide §12): the INS accumulates position error; a sensor designation (TGP/FCR/
  HUD) of a known steerpoint computes a DELTA and re-aligns — model INS as slowly-drifting truth with
  discrete fix corrections.

## Hardware (LRUs, for context)
- **Radar altimeter (CARA)**: **AN/APN-232** Combined Altitude Radar Altimeter (Gould) — feeds the ALOW
  system and HUD radar altitude; must be ON for ALOW (guide). Typical usable range to ~5000 ft AGL.
- **INS**: ring-laser-gyro INS — **Litton LN-39 / LN-93**, or **Honeywell H-423**; later blocks use an
  **EGI** (Embedded GPS/INS, e.g. **LN-260**) fusing GPS with the RLG INS. The F-16 was the first
  operational US aircraft with GPS. INS drift + nav-fix (guide §12) reflects the RLG-INS error growth.
- **TACAN/ILS/MIDS**: TACAN is part of the **MIDS** radio (guide); ILS via a separate marker-beacon/LOC/GS
  receiver feeding EHSI (PLS mode) and HUD/ADI steering bars.
- **FCR**: **AN/APG-68** mechanically-scanned radar (4 LRUs) also displays steerpoints (see
  `radar-sensors.md`).

## Sources
- pilotscafe.com *Understanding ILS*; code7700.com *ILS*; PPRuNe tech-log *Full-scale deflection on CDI* —
  localizer ±2.5°, glideslope ±0.7°/1.4° beam, 0.14°/dot, 3° glidepath, 700 ft course width.
- airforce-technology.com F-16; Wikipedia *AN/APG-68* — AN/APN-232 CARA, LN-39/93 / H-423 INS, EGI/GPS, APG-68.
