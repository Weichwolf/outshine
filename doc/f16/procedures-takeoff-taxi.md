# F-16C Taxi & Takeoff Procedure

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 5 — Taxi & Takeoff, pp. 119–125.

## Taxi
1. Parking Brake / Anti-Skid — **DISENGAGED (ANTI-SKID)**. (Auto-disengages if RPM > 85% or switch to
   ANTI-SKID/middle.)
2. Taxi Light — DOWN (on).
3. **NWS A/R DISC & MSL STEP** button (stick) — engage nosewheel steering ("NWS" status light on;
   turns via rudder pedals).
4. Set formation/anti-collision/position lights as required.
5. Throttle slightly above IDLE to roll.
6. Taxi turns at **≤ 10 kts**; taxi speed generally **< 25 kts**.

## Takeoff
1. Line up on runway.
2. Taxi light — OFF (middle); Position lights — STEADY; Probe Heat — ON.
3. Verify NWS engaged; speedbrakes CLOSED.
4. Confirm correct FLCS mode on Status panel: **CAT I** (A-A / light) or **CAT III** (A-G / heavy).
5. RADAR ALTIMETER — ON (FWD).
6. Hold wheel brakes; throttle to **90% RPM**; confirm spool-up:
   - HYD/OIL PRESS warning OFF
   - Oil pressure 25–65 psi
   - FTIT ≤ 935 °C
   - Hydraulic pressure (A & B) 2850–3250 psi
7. Throttle to **MIL** (light load) or **Full Afterburner** (heavy load / short runway).
8. Release wheel brakes.
9. At **70 kts**, press NWS button to disengage nosewheel steering (NWS indication out).
10. Rotate to **8–12° pitch** takeoff attitude:
    - MIL power: begin pull ~**10 kts below** takeoff speed.
    - Afterburner: begin pull ~**15 kts below** takeoff speed.
11. Positive rate → gear up. (TEF retract with the gear → aircraft may settle; keep climbing.)
    Retract gear **before 300 kts** (higher speeds risk gear-door structural damage).

## Takeoff speed vs weight

| Aircraft weight (lb) | 20,000 | 24,000 | 28,000 | 32,000 | 36,000 | 40,000 | 44,000 |
|---|---|---|---|---|---|---|---|
| Takeoff speed (KIAS) | 128 | 142 | 156 | 168 | 178 | 188 | 198 |

(Example from guide: 34,000 lb → ~173 kts; rotate ~163 kts MIL / ~158 kts AB.)

Related limits (see `aerodynamics-performance.md`): gear extend/transit ≤ 300 kts / M0.65; takeoff G
window +4/0 symmetric, +2/0 asymmetric.

---

## ED EA Guide addendum — official procedure detail (pp.140–146)

**Takeoff-speed-vs-weight table cross-validated identical** to Chuck's table above — ED's own table
(pp.145) reproduces the exact same seven points (20,000→128 kt … 44,000→198 kt). Rotation technique
also matches exactly: 10 kt below Vr for MIL / 15 kt below Vr for AB, 8–12° takeoff attitude, gear-up
on positive rate, retract before 300 KCAS. **No discrepancy** — strong cross-source confidence on this
table.

### Nosewheel steering — quantitative principle not in Chuck
ED states explicitly: **"Nosewheel steering gain is proportional to ground speed. As speed increases,
the nosewheel steering will become less sensitive for a given pedal input."** (p.140) — this is a
concrete FBW/ground-handling design target: NWS command authority should be a decreasing function of
groundspeed, not a fixed gain. ED also flags the F-16's **narrow main-gear track ("footprint")** as a
rollover/wingtip-strike risk during high-speed ground turns — brake to a lower speed before turning at
high taxi speed. Directly informs a taxi-phase AI's max-commanded-turn-rate-vs-groundspeed schedule.

### CAT I/III determination — exact loadout logic (ED p.143, refines flight-controls-flcs.md's table)
ED gives the precise loadout rule for the STORES CONFIG switch (Chuck/`flight-controls-flcs.md` only
summarize CAT I="A-A/light", CAT III="A-G/heavy"):
- **CAT I**: air-to-air loadouts *without* external wing tanks, **or** a 6-missile air-to-air loadout
  *with* external wing tanks but **no centerline tank and no AIM-120s**.
- **CAT III**: any air-to-ground loadout; any loadout with external wing tanks **and** a centerline
  tank; **or** a 6-missile air-to-air loadout with AIM-120s **and** external wing tanks.

This is the exact boolean rule a rebuild's loadout→CAT-switch auto-detect should encode — the guide's
prose is precise enough to implement directly (not just descriptive).

### Crosswind takeoff technique (ED p.146 — not covered in Chuck's takeoff file at all)
1. Apply stick pressure **against** the wind (opposite roll direction) to keep wings level against the
   crosswind's weathervane-induced roll.
2. Apply rudder **with** the wind to keep the aircraft tracking the runway centerline.
3. During rotation, **smoothly remove** the countering rudder to let the nose weathervane into the wind
   and establish the correct crab angle — at liftoff, the FPM (not the nose) should be aligned down the
   runway.

This is the FBW-relevant sequencing detail: rudder-then-release-during-rotation, not simultaneous
stick+rudder held through liftoff.

---

# Technical depth (researched — shallow pass — deepen when in scope)

## Components (LRUs)
- **Nosewheel Steering (NWS)**: electro-hydraulic steering on the nose gear, engaged via the stick
  NWS button; disengages ~60 kt for the FLCS ARI (`flight-controls-flcs.md`).
- **Anti-skid wheel brakes**: carbon brakes with an anti-skid controller; parking brake auto-releases
  above 85% RPM.

## Functional principle
On the takeoff roll the FLCS is in **takeoff/landing gains** (gear-down, pitch-rate command — see
`flight-controls-flcs.md`); rotation is a pitch-rate command to 8–12°, and gear retraction reverts the FLCS
to cruise gains while the TEF retract with the gear. Directional control transitions from NWS (low speed) to
aerodynamic rudder as airspeed builds — the ~60 kt NWS/ARI handoff reflects that. Takeoff speed scales with
weight (guide table) because rotation is lift-limited at the takeoff AoA.

## Sources
- Wikipedia *General Dynamics F-16* (NWS, gear); DCS guide Part 5 — cross-referenced above.
- FLCS gain modes: `flight-controls-flcs.md`.
- `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — Taxi/Takeoff/Crosswind Takeoff
  p.140–146 (NWS gain-vs-groundspeed, CAT I/III loadout logic, crosswind technique, cross-validated
  takeoff-speed table).
