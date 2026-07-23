# F-16C Systems Reference — Index

Distilled from `doc/DCS F-16C Viper Guide.pdf` (Charles "Chuck" Ouellet, 794 pp). Each file cites its
source part + page range. This is a reference guide (facts/tables/steps), not the primary source — for
FlightBox flight-model fidelity the reference remains the vanilla JSBSim F-16 itself.

## Files

| File | Content | Source part / pages |
|---|---|---|
| [flight-controls-flcs.md](flight-controls-flcs.md) | **FLCS architecture, gains, CAT I/III limiters, AoA/G envelope, ARI, MPO, autopilot modes** | Part 15, 662–672 |
| [aerodynamics-performance.md](aerodynamics-performance.md) | Airspeed/G/weight limits, envelope, ALOW, VMS warnings | Part 8, 153–157 |
| [hud-symbology.md](hud-symbology.md) | Every HUD element (position + meaning), control switches, AOA bracket, steering/ILS symbology | Parts 3/6/8/16 |
| [cockpit-displays.md](cockpit-displays.md) | ICP/UFC, DED pages, MFD/OSB, instruments, caution/warning lights | Part 3, 13–83 |
| [procedures-startup.md](procedures-startup.md) | 81-step start-up (phases A–I) + engine idle params + INS align | Part 4, 84–118 |
| [procedures-takeoff-taxi.md](procedures-takeoff-taxi.md) | Taxi + takeoff steps, **takeoff-speed vs weight table** | Part 5, 119–125 |
| [procedures-landing.md](procedures-landing.md) | Overhead pattern (7 phases), speeds, AoA, flare, roll-out | Part 6, 126–133 |
| [engine-fuel.md](engine-fuel.md) | F110 engine, limits, throttle, PRI/SEC, EPU, relight, fuel system, BINGO/JOKER | Part 7, 134–152 |
| [navigation-ils.md](navigation-ils.md) | EHSI, HSD, steerpoints, TACAN, bullseye, INS drift/fix, **ILS approach** | Part 16, 673–777 |
| [hotas.md](hotas.md) | Full stick (SSC) + throttle (TQS) control mapping | Part 9, 158–160 |
| [radar-sensors.md](radar-sensors.md) | FCR A-A/A-G/A-Sea modes, TGP, HMCS (mode taxonomy + concepts) | Part 10, 161–312 |
| [weapons.md](weapons.md) | SMS, arsenal, bomb delivery modes (CCIP/CCRP/DTOS), guided-weapon concepts, gun sights | Part 11, 313–573 |
| [defence-rwr-cm.md](defence-rwr-cm.md) | RWR symbology, CMDS modes/programs, chaff/flare | Part 12, 574–599 |
| [datalink-iff.md](datalink-iff.md) | MIDS/Link-16/TNDL, IFF modes (1/2/3/4/5/C/S) | Part 13, 600–648 |
| [air-refueling.md](air-refueling.md) | Boom refueling procedure, PDI lights, disconnect | Part 17, 778–791 |

## Priority (per lead) — highest first
FLCS/autopilot → aero/performance → HUD → procedures+speeds → engine/fuel → navigation → rest.
The top files (FLCS, aero, HUD, procedures, engine, navigation) are full distillations. Radar and weapons
(413 pp combined) are structural references — mode taxonomy + key display/HOTAS concepts, not every
per-weapon tutorial.

## Not separately distilled
- Part 1 (Introduction — history), Part 2 (DCS controls setup), Part 18 (Other Resources).
- Part 14 (Radio Tutorial): comms basics folded where relevant (COM1/COM2 in `cockpit-displays.md`,
  tanker/ATC contact in `air-refueling.md`).

## FlightBox relevance
- `flight-controls-flcs.md` is the **highest-value** file: our FBW (`fcs/fbw-override`) commands this FLCS;
  the limiter/gain/autopilot spec is what FBW must reproduce or cleanly bypass.
- `hud-symbology.md` is the reference our MIL-STD-1787 HUD is built against.
- `aerodynamics-performance.md` + `procedures-*` give speeds/limits for envelope/mission checks.
