# F-16C Start-Up Procedure

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 4 — Start-Up Procedure, pp. 84–118.
81 numbered steps in phases A–I. Condensed to the operationally load-bearing steps + parameters.

## Pre-power (battery)
1. Ejection Seat Lever — DOWN & ARMED (usually done just before takeoff).
2. Test FLCS functions on battery power only.
3. MAIN PWR switch — MAIN PWR/FWD.

## A — Provide aircraft power (steps 4–5)
- MAIN PWR readies engine-mounted generator (no electrical power yet until engine running).
- On EPU panel, verify **EPU GEN** and **EPU PMG** indications.

## B — Pre-start setup (steps 6–9)
- Parking Brake / Anti-Skid — **PARKING BRAKE (UP)**.
- Close & lock canopy (CANOPY light extinguished).

## C — Engine start (steps 10–22)
1. Verify throttle at **OFF** detent.
2. JFS (Jet Fuel Starter) switch — **START2**.
3. JFS reaches IDLE RPM within ~30 s → JFS RUN light on, FLCS RLY/PMG lights out, TO FLCS light on.
4. Clutch engages (ADG/PTO shaft drives F110); engine spools to **20–25% RPM**.
5. At **20–25% RPM**, move throttle **OFF → IDLE**. (Too early = hot start or hung start.)
6. Light-off within **10 s**; RPM & FTIT rise.
7. Milestones: SEC caution off @ 20% RPM; standby gen @ ~60% RPM (ENGINE + STBY GEN lights out);
   main gen ~5–10 s later (MAIN GEN light out); JFS auto-off @ ~55% RPM.

**Idle engine parameters (step 22):**
| Parameter | Value |
|---|---|
| HYD/OIL PRESS warning | Off |
| Fuel flow | 700–1700 pph |
| Oil pressure | ≥ 15 psi |
| Nozzle position | > 94% |
| Engine RPM | 62–80% |
| FTIT | ≤ 650 °C |
| Hydraulic pressure (A & B) | 2850–3250 psi |

## D — Set up avionics (steps 23–38)
- Power ON: MMC, ST STA (store stations), MFD, UFC, GPS receiver. (DL/MAP left OFF — no function on F-16C.)
- BITs run as systems power up.
- LEFT/RIGHT HDPT power ON if HTS/targeting pod fitted.
- FCR power — **FCR/ON (FWD)** (enters BIT).
- RDR ALT power — **STBY (MIDDLE)**.
- COMM1 UHF / COMM2 VHF — ON, mode SQL; UHF backup — BOTH/MAIN.
- Turn on HUD via SYM intensity wheel.
- C&I switch — **UFC** (enables upfront control of comms).

## E — INS alignment (steps 39–43)
- INS selector — **ALIGN NORM** (normal) or stored-heading.
- Alignment status on DED; enter/confirm aircraft coordinates.
- Complete when status reaches **"10 RDY"**.
- Then INS selector — **NAV**.

## F — Datalink (steps 44–45) — only after INS aligned. G — IFF (steps 46–48) — IFF Master → NORM.

## H — Complete aircraft setup (steps 49–70)
- Uncage SAI (standby attitude indicator).
- Set FBW control mode via **STORES CONFIG** switch (CAT I / CAT III per loadout).
- Oxygen: Supply ON, Emergency NORMAL, Diluter NORMAL; pressure green; flow blinks.
- RWR/TWA power on; CMDS RWR/JMR ON; CMDS CH/FL ON; CM mode + program selected.
- Equip HMCS/NVGs; perform **HMCS alignment** (coarse then fine DX/DY + ROLL) against the HUD
  reference cross (steps 66–68 — see `hud-symbology.md`).
- Load DTC via DTE page; clear avionic faults on TEST page.

## I — Post-start checks (steps 71–81) — all optional
Pitot heater, fire/overheat detection, malfunction lights, SEC engine control, FLCS op check, fuel
quantity, **DBU**, trim, **MPO**, EPU system.

---

# Technical depth (researched — shallow pass — deepen when in scope)

## Components (LRUs)
- **JFS** (Jet Fuel Starter): a small **hydraulic accumulator-driven turbine** in the Accessory Drive
  Gearbox; spins the engine core via the PTO shaft for start.
- **EPU** (Emergency Power Unit): **hydrazine (H-70) monopropellant** turbine — emergency hydraulic +
  electrical power (`engine-fuel.md`).
- **INS/EGI**: ring-laser-gyro INS (LN-39/93 / H-423) or EGI (see `navigation-ils.md`); needs a
  gyrocompass alignment before NAV.

## Functional principle
Start sequence is a dependency chain: battery/MAIN PWR → JFS (accumulator energy spins the core to
~20–25% RPM) → introduce fuel at IDLE → light-off → generators come online as RPM rises → avionics BIT →
INS alignment (RLG gyrocompassing to "10 RDY") → sensors/EW/HMCS. Each numbered step in the guide gates the
next on a measured parameter (RPM, FTIT, gen lights, align status), which is exactly what a start-state
machine in the sim would encode. The EPU and DBU checks at the end validate the emergency-reversion paths.

## Sources
- Wikipedia *General Dynamics F-16* (JFS, EPU hydrazine); airforce-technology.com F-16 — INS types.
- DCS guide Part 4 (81-step sequence) — cross-referenced above.
