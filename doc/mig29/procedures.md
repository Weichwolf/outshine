# MiG-29A (9-12) — Procedures: Preflight, Start, Taxi, Takeoff, Landing

**Variant scope**: izdeliye **9-12** (Fulcrum-A). Deltas in §9.

**Sources**: **DCS-EA** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (printed page == PDF page);
pages used: 60, 71–80. **DCS-FM** = `doc/DCS MIG-29 Flight Manual EN.pdf` (**printed page = PDF page
− 6**); pages used: 81–82.
Research in **§8 Technical depth**.

**Depth declaration**: **FULL** on the DCS-EA procedure chapter — it is the single best-documented part
of the whole `doc/mig29/` set and is distilled step-for-step below. **SHALLOW** on performance *tables*
— the manuals give **one** weight qualifier ("+5 to 10 kts at max landing weight") and **no**
weight/temperature/altitude schedule at all (§7).

> **Why this file matters most for FlightBox**: `FBPilot`'s phase machine (Preflight → Takeoff → Climb
> → Route → Approach → Flare → Rollout) needs *numbers per phase*. This file provides them for the
> MiG-29, and the numbers differ substantially from `doc/f16/procedures-*.md` — notably a **much
> higher approach speed** and a **brake chute** as a normal-procedure item.

---

## 1. Quick reference — the numbers a pilot module needs

| Phase | Parameter | Value | Source |
|---|---|---|---|
| Taxi | RPM for comfortable taxi speed | **72…75 %** | `DCS-EA p.76` |
| Taxi | Brake check: hold brakes, advance to | **80 % RPM**, aircraft stationary | `DCS-EA p.75` |
| Taxi | Separation from other taxiing aircraft | **≥300 ft** | `DCS-EA p.75` |
| Takeoff | Run-up brake pressure | **11 ± 1.0 kp/cm²** (normal brake 8 ± 0.5) | `DCS-EA p.72, 36` |
| Takeoff | Pre-release checks | EGT in the **brown/yellow sector**, **RPM difference ≤ 4 %**, ramps fully closed | `DCS-EA p.77–78` |
| Takeoff | Intake system opens (thrust step + nose-down tendency) | **≈108 kts** | `DCS-EA p.78` |
| Takeoff | Nose raise | **125…135 kts** | `DCS-EA p.77` |
| Takeoff | Pitch attitude on the HUD during rotation | **8°…10°** | `DCS-EA p.77–78` |
| Takeoff | **Liftoff** | **140…150 kts**, depending on gross weight | `DCS-EA p.77` |
| Takeoff | Gear retraction | at **32…50 ft** | `DCS-EA p.77` |
| Takeoff | Flap retraction | at **350 ft** | `DCS-EA p.77` |
| Climb | At **270 kts** set RPM | **83–85 %**, climb **985…1,480 ft/min** | `DCS-EA p.77` |
| Climb | Hold until clear of the control zone | **270 kts** | `DCS-EA p.77` |
| Crosswind takeoff | Attitude lower than normal → liftoff speed | **+≈8 kts** | `DCS-EA p.78` |
| Pattern | Pattern airspeed | **≥300 kts** | `DCS-EA p.80` |
| Pattern | **Downwind leg** | **200…220 kts**; gear extended, flaps/LEF down and out, indicators checked | `DCS-EA p.80` |
| Pattern | **Base leg** | **≥180 kts**, **AoA ≤ 15°** | `DCS-EA p.80` |
| Pattern | **Final approach** | **≥175 kts** | `DCS-EA p.80` |
| Landing | Flare initiation | **20…30 ft AGL** | `DCS-EA p.79` |
| Landing | Round-out | requires **nearly full aft stick** | `DCS-EA p.79` |
| Landing | **Touchdown** | **≈140 kts at 11° AoA**; **do not exceed 13° AoA** | `DCS-EA p.79–80` |
| Landing | Max landing weight correction | **+5…10 kts** on the touchdown speed | `DCS-EA p.79` |
| Landing | Brake application (crosswind procedure) | at **115 kts** | `DCS-EA p.79` |
| Landing | **Brake chute separates above** | **175 kts** | `DCS-EA p.60` |
| Crosswind landing | Crosswind ≤ **15 kts** | **5…10° low wing** + crab | `DCS-EA p.79` |
| Crosswind landing | Crosswind > **15 kts** | **wings-level crab** | `DCS-EA p.79` |
| Crosswind landing | Crab required | **≈1° per 3 kts of crosswind** | `DCS-EA p.79` |
| Missed approach | Initial | **600 ft, 270 kts**, turn at **30° bank**, climb to **2,000 ft** | `DCS-EA p.84` |
| Approach set-up | Radar-altimeter danger bug, standard pattern | **200 ft** | `DCS-EA p.71` |
| Approach set-up | Radar-altimeter danger bug, RETURN mode | **2,000 ft** | `DCS-EA p.82` |
| Engine | Idle RPM after start | **58…72 %** | `DCS-EA p.73` |
| Engine | Ramps close during start at | **35 % RPM** | `DCS-EA p.73` |

**Direct comparison with the F-16 baseline** (`doc/f16/procedures-landing.md`): the F-16 flies its
approach at ~11–13° AoA too, but at **~140–150 kts final**; the MiG-29's **≥175 kts final / 140 kts
touchdown** is markedly faster, its **base-leg AoA cap (15°)** is explicit where the F-16's is not, and
its landing roll relies on a **drag chute** that the F-16 does not have. A MiG-29 `FBPilot` approach law
must therefore be a **different schedule**, not a re-tuned F-16 one.

---

## 2. Preflight interior check (**FULL**) — `DCS-EA p.71–72`
Prerequisites: wheel chocks placed, **ground electric power** connected (all starting operations use
ground power).

1. **BAT–GND SUPPLY** switch UP.
2. **Voltmeter ≈ 28 V ± 0.5 V.**
3. All **Electrical Power Panel** switches ON (the "ALL ON" frame moves them together).
4. On the **System Power Panel**: **NAVIGATION, GYRO STBY, GYRO MAIN, ACFT SYS** ON.
5. Start the **clock stopwatch**.
6. **After 30…40 s from gyro power-on**, begin heading alignment:
   - Press **MAG HDG SLAVE** and **COMP ZERO** simultaneously for **10…15 s**; confirm the HSI needle
     aligns with the aircraft's true heading.
   - Move the navigation **OPER / PREPARE** switch to **OPER**.
7. **LAMP TEST** — confirm all warning lights.
8. Reset **MASTER CAUTION**.
9. Check speed indicators read **M 0.2…0.29** and **TAS 110…190**.
   ⚠️ *(A static aircraft showing a non-zero TAS/Mach is an instrument-floor artefact, but it is the
   documented check value.)*
10. Zero the barometric altimeter.
11. **Radar-altimeter bug to 200 ft** (minimum altitude for the standard landing pattern).
12. Radio-altimeter **TEST** → needle settles at the **45 ft** test mark.
13. Hydraulic pressures within the **Pак** region on the IKG-1.
14. **AEKRAN CALL** → "SELFTEST" then "AEKRAN READY".
15. **Fuel quantity 3,100…3,700 kg.**
16. Navigation system active — **WP-AD #1** and **BEACONS #1** lamp-buttons illuminated.
17. **GYRO → STBY**, confirm the HSI reads the same heading.
18. **GYRO → MAIN.**
19. **SET COURSE → MAN.**
20. Set the current airstrip landing course on the HSI course knob.
21. **SET COURSE → AUTO.**
22. Oxygen flow valve open, **MIX–100 % in MIX**.
23. Brake pressures: **8.0 ± 0.5 kp/cm²** on the brake lever; **11.0 ± 1.0 kp/cm²** on the run-up brake
    lever; **zero with both released**.
24. Confirm **FAST PREP** lamp lit (navigation fast-initialisation complete).
25. **RADIO** switch ON.

## 3. Engine start — `DCS-EA p.72–73`
See `engines-fuel.md` §5.1 for the full sequence and the monitored parameters. Preconditions specific to
this phase: **RECORD** on · canopy closed and locked (pin recessed, no AEKRAN signal, LOCK CANOPY out) ·
**ejection handle ARMED** · **Start-Up Mode Switch = START BOTH** · both throttles **IDLE**. Then
**GND START**. Ground power is disconnected once the engines are running.

## 4. Post-engine-start (**FULL**) — `DCS-EA p.73–75`
1. **Trim check**: pitch full forward → aft until **STAB TRIM NEUTRAL** lights; aileron full right →
   left until **AIL TRIM NEUTRAL** lights; rudder right until **RUD TRIM NEUTRAL** extinguishes, then
   back to neutral.
2. **AFCS ON.**
3. Monitor the **AFCS BIT**: DAMPER light blinking, **stick moves by itself**.
   ⚠️ *"Do not move the flight stick until the end of BIT — it may cause fail of AFCS BIT."*
4. Check the **COC** (flight-envelope protection, i.e. SOS-3M) — **"COC FAIL"** and **"NO COC RESERVE"**
   lamps extinguish.
5. **PITOT HEAT ON.**
6. **VOICE WARN SYSTEM test** → answers **"BINGO BINGO"**.
7. **ADF check**: ADF/RSBN → ADF; ADF mode on the radio panel; listen to the **outer beacon** Morse and
   verify the HSI needle; **OUTER/INNER → INNER**; verify the inner beacon; back to **OUTER**; ADF off;
   **ADF/RSBN → RSBN**.
8. **RHAW (SPO) ON** — verify the notch at the nose of the aircraft silhouette lights.
9. **SPO self-test**: hold the TEST switch right in **AUTO**; the function light goes out, all others
   light; after **5…7 s** the function light returns → release.
10. Set SPO brightness.
11. Verify **AFCS BIT success**: TLP "DAMPER OFF" out, AFCS panel **DAMPER lit steadily**.
    *Recovery if BIT failed (lamps blinking): centre the stick with trim until pitch and aileron neutral
    lamps light, then short-press **AFCS MODES OFF** → BIT restarts.*
12. **Flaps/slats check** — press a DOWN button, confirm extension visually and on the IP-52 indicator,
    then UP.
    ⚠️ *"Flaps extension reduces the maximum angle of rotation of the nose wheel strut."*
13. **Ramp check** — push a throttle to **80–90 % RPM**, confirm **LH/RH INLET CHECK** lamps light;
    idle → out.
14. **Reset trim to neutral** (green rudder and aileron neutral lamps lit).
15. Power up **ACS** and **WEAPON** circuits.
16. **Clear the AEKRAN queue** — press AEKRAN CALL until QUEUE is out and MEMORY is on.
17. Request chock removal.

## 5. Taxi — `DCS-EA p.75–76`
- Verify no obstacles; no aircraft crossing; taxiing traffic **≥300 ft** away.
- **Brake check before moving**: hold brakes, advance to **80 % RPM**; the aircraft should remain
  stationary (*may crawl when wet*). Release.
- Once rolling, throttles back to idle and let the aircraft coast; **72…75 % RPM** gives a comfortable
  taxi speed with minimum braking.
- **The nose-wheel strut is well behind the pilot's seat** — the documented pivot geometry is
  **14′10″** and **24′12″** from the pivot point (`DCS-EA p.75` figure).
- **Sharp turns**: press and hold the **Nose Wheel Steering (high-gain)** button, apply pedal gradually,
  release the button after the turn.
- **Braking has a slight delay** — reduce speed with smooth lever movements *with lead*.
- **Hard braking lowers the nose smoothly but significantly.**
- Periodically check the navigation system and HSI during taxi (heading, bank, pitch, presence of
  correction).
- Line up, stop **10…15 m** after crossing the threshold, aligned with the centreline.

## 6. Takeoff — `DCS-EA p.76–78`

**Power choice** (`DCS-EA p.76`): takeoff is normally at **maximum (military) throttle**; afterburner
is used for training or with external payloads. The decision must weigh aircraft mass, OAT, pressure
altitude, wind, runway length and barrier availability; **maximum abort speed and minimum go speed are
the deciding indicators**. AB always improves performance and safety margins, **but aircraft handling is
more difficult if an engine fails during an AB takeoff than during a military-power takeoff.**

### 6.1 Normal takeoff
1. **Flaps down**; confirm visually, in the mirrors and on the IP-52.
2. Check roll, pitch, heading and course indicators; altimeter zeroed.
3. Start the stopwatch and flight timer.
4. Confirm the **FEEL UNIT TAKEOFF–LANDING** lamp is lit and the **three green neutral-trim lamps**.
5. Apply the **RUN-UP BRAKE** trigger (maximum wheel-brake pressure).
6. Advance throttles to **MAX**; the nose visibly lowers on the brakes.
7. **EGT in the yellow sector**, **RPM difference ≤ 4 %** → release brakes; **stick neutral on the
   roll**.
8. Rotate at **125…135 kts**.
9. Place the horizon just above the IR sensor on the nose; **HUD pitch 8°…10°**.
   → **Liftoff at 140…150 kts** depending on gross weight.
10. **Gear up at 32…50 ft.**
11. Confirm gear lamps out and hydraulic pressure normalised.
    *If the gear is not fully retracted (red lamp flashing on the IP-52): reduce RPM to **80 %**, set
    **220 kts**, and check again.*
12. **Flaps up at 350 ft**, confirm lamps out.
13. At **270 kts** set **RPM 83–85 %**, climb at **985…1,480 ft/min**.
14. Hold **270 kts** until clear of the airbase control zone.

### 6.2 Afterburner takeoff (`DCS-EA p.78`)
- **AB detents must be unlocked** before max AB is available.
- Advance smoothly to max AB **after brake release**. **Maintain directional control with the rudder
  pedals — do not use wheel braking for directional control on the roll.**
- **Delay rotation until the intake duct has opened (≈108 kts)** because of the nose-lowering tendency.
- ⚠️ *"**Rapid full aft movement of the stick may result in the exhaust nozzles hitting the runway.**"*
- Illustrated sequence (`DCS-EA p.78`): brakes applied → **throttles 90 % RPM** → instruments within
  limits → warning-system check → brakes released → throttles advanced → **EGT within brown sector** →
  **both RPM, difference < 4 %** → **ramps fully closed** → observe thrust increase at intake opening
  (**≈108 kts**) → **aft stick for rotation to 10° pitch** → **liftoff 140–150 kts, fly off at 10°**.

### 6.3 Crosswind takeoff (`DCS-EA p.78`)
- The aircraft **weather-vanes into the wind**; controllable with **nose-gear steering and rudder**, and
  the tendency **decreases as speed increases**.
- **Takeoff attitude slightly lower than normal → liftoff speed ≈ +8 kts.**
- After liftoff, **crab into the wind, wings level**, to hold runway alignment.

## 7. Landing — `DCS-EA p.79–80`

### 7.1 Normal landing
1. Enter the pattern per local procedure (**pattern speed ≥300 kts**, altitude as directed locally).
2. Adjust power to reach the allowable gear-lowering airspeed.
3. **Extend gear and flaps/LEF on the downwind leg** (**200…220 kts**).
4. Ensure flaps/LEF are down and out **before** turning base.
5. Establish and hold the desired speed on base (**≥180 kts, AoA ≤ 15°**) and final (**≥175 kts**),
   controlling glide slope with pitch attitude and power.
6. **Flare at 20…30 ft AGL.**
7. Round-out to an acceptable sink rate requires **nearly full aft stick**.
8. Reduce power during or after the late round-out; **touch down at ≈140 kts, 11° AoA**;
   **do not exceed 13° AoA at touchdown**. **At maximum landing weight add 5…10 kts.**
9. At touchdown reduce power to idle, **hold the stick position, deploy the chute**.
10. After nosewheel touchdown maintain directional control with **rudder and NWS**.

⚠️ *"At excessive touchdown speeds the aircraft has a tendency to **bounce**. In this case,
maintain/attain landing attitude and **deploy the chute immediately upon touchdown** to prevent further
bouncing."*

### 7.2 Crosswind landing
1. Compensate carefully in the pattern to avoid under/overshooting the final turn.
2. **≤15 kts crosswind**: **5…10° low wing** plus crab.
3. **>15 kts**: **wings-level crab**.
4. **≈1° of crab per 3 kts of crosswind.** Because the chute **intensifies weather-vaning**, consider a
   **no-chute landing near the crosswind limit** — **but chute deployment is mandatory on a wet
   runway.**
5. Wings level before touchdown.
6. **Neutralise the crab with rudder at touchdown.**
7. **Deploy the chute after nosewheel touchdown.**
8. **Brakes at 115 kts.** Counter weather-vaning with **aileron into the wind and rudder**.
9. *If the chute is used and excessive weather-vaning results — **jettison the chute**.*

### 7.3 Brake chute (`DCS-EA p.60`)
*"The landing drag chute enormously reduces the required runway distance at landing. To use it or not
is solely a pilot's decision."* **Mandatory** for:
- Landing **immediately after takeoff**
- Landing on a **wet runway**
- **Short-field** landings
- **Landing without slats**
- **Aborted takeoff after nose-wheel lift-off**
- **AFCS feel unit in the "Heavy" position** (i.e. the ARU gearing state — `flight-controls.md` §3)

**The chute separates from the aircraft above 175 kts.** Release button on the left console
(`DCS-EA p.55, 59`); chute container is behind the engine nozzles (`DCS-FM p.10`).

**Rebuild note**: the chute is a **normal-procedure, decision-gated device with a speed limit** — a
genuinely different rollout model from the F-16's aerobraking. A MiG-29 `FBPilot` Rollout phase needs:
deploy decision (from runway state + weight + slat state), a deploy command over the bus, a drag
increment, and a **jettison** path.

## 8. Technical depth (researched)
Little research value was found beyond the manuals for this file. Two community data points, both
**T4**, both about the *DCS module* rather than the jet, and both **not used as facts** here:
- Community discussion reports MiG-29 manuals quoting **250–260 km/h landing speed** while the common
  practice is **280–290 km/h** (≈135–140 kts vs ≈150–157 kts) — consistent in order of magnitude with
  `DCS-EA`'s **140 kts touchdown**, and with the Russian technical description's **"landing speed
  250–260 km/h"** (T4, `military.wikireading.ru`).
- No T1–T3 performance manual (takeoff/landing distance charts, V-speed schedules) was located in this
  pass.

## 9. Open gaps (honest)
1. **No weight/temperature/pressure-altitude schedules of any kind.** The only weight qualifier in the
   entire source set is *"for landing with maximum landing weight add 5 to 10 kts"* and *"liftoff occurs
   at 140 to 150 kts **depending on aircraft gross weight**"* — with no table behind either. The task's
   request for "approach speeds by weight" **cannot be met from these sources**; anything more granular
   would be invented.
2. **Takeoff and landing ground-roll distances** — absent from both manuals and from research.
3. **Maximum abort speed / minimum go speed** — named as the deciding criteria for the AB-takeoff
   decision (`DCS-EA p.76`) but **never quantified**.
4. **Maximum landing weight** — the +5…10 kts correction references it; the weight itself is not given.
5. **Gear/flap limit speeds** — the FC3 voice warning implies **250 kts** for gear-down
   (`DCS-FM p.104`), and the takeoff troubleshooting step uses **220 kts** with gear travelling
   (`DCS-EA p.77`); **no placarded limits** are stated. ⚠️ Do not treat 250 kts as a limit; it is a
   *warning trigger in the FC3 module*.
6. **Emergency procedures** — engine failure on takeoff, single-engine handling, hydraulic failure,
   flameout/relight envelope, ejection envelope: **none present** in either manual.
7. **Shutdown checklist** — `DCS-FM p.82` gives only the throttle actions; the EA manual has no
   shutdown chapter at all.
8. **Crosswind limits** — the technique is scaled by crosswind but **no limit value** is given.

## 10. Variant notes
No variant-specific procedure differences are documented. The 9-13's higher empty weight and fuel load
would shift the speed schedule, but **no source quantifies it** — see gap 1.
