# F-16C Landing Procedure

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 6 — Landing, pp. 126–133.
HUD element details in `hud-symbology.md`.

## Overhead pattern (7 phases)
Initial Approach → Overhead Break → Downwind Leg → Base Turn → Final Turn → Short Final → Roll-Out.

### 1. Initial Approach
- RADAR ALTIMETER — ON (FWD).
- Align with runway at **1500 ft AGL**, maintain **300 kts**.

### 2. Overhead Break
- Break left/right over desired touchdown point.
- Throttle **80% RPM**; deploy **speedbrakes**.
- Break at ~**70° bank**, ~**3–4 G**.
- Align **FPM with the Horizon Line** to hold a level turn.

### 3. Downwind Leg
- Roll out opposite landing heading, **200–220 kts**, **1500 ft AGL**.
- Extend landing gear; LANDING light UP.
- Control AoA with **throttle, not pitch trim** (FBW sets AoA). Target **11° AoA**.
- Monitor AoA via: AOA Indicator, AOA Indexer, HUD AOA Bracket (with FPM).

### 4. Base Turn
- Begin abeam rollout point (estimate: wingtip at runway end).
- Lower nose to **8–10° pitch**, fly turn at **11° AoA**.

### 5. Final Turn
- Throttle controls airspeed; stick maintains 8–10° nose-low + 11° AoA through the turn.
- Roll out on final, raise nose to hold glidepath: **300 ft AGL, 1 nm** from touchdown.
- Align **FPM + 2.5° pitch-ladder lines with the runway threshold** for glidepath; hold **11° AoA**.

### 6. Short Final
- Over the overrun, shift FPM forward to a point **300–500 ft down the runway**.
- Gently flare to reduce descent rate — **do NOT level off**.
- Throttle to IDLE; touchdown at **≤ 13° AoA** (green circle).
- **> 15° AoA on rollout** risks speedbrake/nozzle striking the runway.

### 7. Roll-Out
- Hold **13° nose-up** two-point aerodynamic braking until ~**100 kts** (F-16 wheel brakes are weak — this
  step matters).
- Reduce back stick, lower nosewheel.
- Speedbrakes fully open, full aft stick for max braking; apply moderate–heavy wheel braking.
- Engage nosewheel steering **below 30 kts**, taxi clear.

## ILS approach
See `navigation-ils.md` (Part 16 ILS tutorial): center localizer + glideslope bars on the FPM, capture,
gear down → "E" AoA bracket appears, LANDING light, speedbrake.

---

# Technical depth (researched — shallow pass — deepen when in scope)

## Components (LRUs)
- **Speedbrakes**: split panels on the aft fuselage (open ~60° each), used through the approach and
  rollout.
- **Wheel brakes**: carbon anti-skid brakes — **deliberately weak**, hence the two-point aerodynamic
  braking technique in the guide.
- **AoA sensing**: AoA probes drive the indexer/bracket and the FLCS approach law (`flight-controls-flcs.md`).

## Functional principle
The approach is flown to a constant **AoA (~11°, on-speed 13°)** rather than a fixed speed — the FLCS is in
takeoff/landing (pitch-rate) gains, and the pilot sets AoA with throttle while the FLCS holds the commanded
pitch response (`flight-controls-flcs.md`). The flare bleeds descent rate, touchdown at ≤13° AoA, then
**aerodynamic braking** (hold 13° nose-up) does most of the deceleration because the carbon wheel brakes
are ineffective until slow. Exceeding 15° AoA on rollout risks a speedbrake/nozzle strike (geometry limit).

## Sources
- Wikipedia *General Dynamics F-16* (speedbrake, brakes); DCS guide Part 6 — cross-referenced above.
- Approach AoA/FLCS gains: `flight-controls-flcs.md`, `hud-symbology.md`.
