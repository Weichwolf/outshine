---
name: mig29-systems
description: MiG-29A (izdeliye 9-12, Fulcrum-A) systems knowledge for FlightBox — distilled from the DCS MiG-29A full-fidelity manual and the FC3 flight manual plus web research, structured to build a JSBSim model and an FBModule, not to teach players. Covers mechanical flight controls (ARU/SOS — no FBW), RD-33 engines, N019 radar with quantified Doppler notch, KOLS IRST + helmet sight, SPO-15 RWR failure modes, GCI doctrine, weapons (R-27R/T, R-73, R-60M, GSh-301, A-G stores) and a flight-model build spec with documented envelope anchors. Load when building or judging anything MiG-29: the JSBSim model, the module, its weapons, its sensors, or the AI doctrine of a GCI-guided adversary.
---

# MiG-29A (9-12) Systems Reference

The knowledge base lives in `<repo>/doc/mig29/` — 12 files. It is the MiG-29 counterpart to
`doc/f16/` (skill `f16-systems`) and was built to its template; `doc/f16/flight-model.md` §11 is the
checklist `flight-model-spec.md` follows. Start at `doc/mig29/INDEX.md`.

**Coverage is honestly PARTIAL.** Both DCS manuals are fully distilled (page ledger in
`PROGRESS.md`), but they are far thinner than the F-16 corpus. Trust the FULL/SHALLOW declaration
and the per-number source tags in each file; the gap lists say what nobody has sourced yet.

## Read this first

- **File schema — `## Spec` / `## State` / `## Gaps` / `## Knowledge`**, the same four sections as
  `doc/f16/` and `doc/flightbox/`. **Spec** = what the real 9-12 documentably does (the bulk, variant
  notes included). **State** = what FlightBox implements — today **"nothing built"** in every file,
  linking the spec-first module contract `doc/flightbox/aircraft/mig29.md` and naming which roadmap
  stage (R3/R6/R7/R8) consumes which part. **Gaps** = source gaps, unsourced numbers, the
  GAF T.O. 1F-MIG29-1 acquisition note. **Knowledge** = researched depth, derivations, the
  cross-manual conflict registrations. Original section numbers are unchanged (`§2.4`, `§7.1` …
  still resolve); only the heading level dropped one. `INDEX.md`/`PROGRESS.md` are meta and exempt.
  `flight-model-spec.md` carries a deliberately **thin** frame — its three-column rows
  (documented / derivation path / open+`[SET]`) are themselves a Spec+Gaps hybrid and stay intact;
  the file says so in a schema note at its top.
- **Page citations: printed pages everywhere.** DCS-FM's printed page = PDF page − 6 (six roman
  front-matter pages); DCS-EA has no offset. The former split between the system files and
  `weapons.md`/`flight-model-spec.md` is **resolved** — both were converted citation by citation
  against the PDF text (they were internally mixed, so a blanket shift would have been wrong).
  Details and the verification in `PROGRESS.md`. A `DCS-FM p.N` in this base can now be quoted
  onward without checking which file it came from.
- **DCS numbers can be ED design decisions**, not documented jet behaviour — the files mark the
  suspect ones. The FlightBox ground truth for flight behaviour will be the JSBSim model that
  `flight-model-spec.md` specifies, once built (same Prinzip-5 logic as the F-16).
- **Nothing is implemented.** There is no MiG-29 module, model or mission. Every "State" section says
  so; do not write code against this base assuming a `modules/mig29/` exists.

## Task routing

| Task | Read |
|---|---|
| JSBSim model build (the main event) | `flight-model-spec.md` — §11-checklist layout, three-column rows (documented+tier / derivation path / open+`[SET]`), RD-33 `<turbine_engine>` spec, declared F/A-18 HARV high-α analogy, envelope-anchor table (= the future gym acceptance test), 10-step build order with promotion gates |
| Flight controls / pitch response | `flight-controls.md` — mechanical runs, ARU-29-2 gearing schedule (function known, numbers open), SOS-3M soft stick pusher at 26° AoA, two stabilizer travel sets as the ARU's visible signature. **No FBW — `fcs/fbw-override` has no counterpart here; the gearing schedule IS the model.** |
| Engines / fuel | `engines-fuel.md` — two RD-33, tanks and feed order; spool times and EGT limits are the worst gap for a throttle loop |
| Radar / IRST / helmet sight | `radar-sensors.md` — N019 modes and the **quantified Doppler notch** (closure > 81 kts beyond 8 nm, > 27 kts inside; 6 s inertial coast), KOLS IRST 13.5–5.4 nm with laser range, Shchel-3UM ±60° az designation. The IRST has **no IFF** — radiating costs stealth, not radiating costs range *and* identity |
| RWR / countermeasures | `defence-rwr-cm.md` — SPO-15 as a physics simulation with eleven analogue failure modes (own-radar blanks the forward hemisphere; priority logic assumes ownship at 26–55 kft) |
| Navigation / procedures | `navigation.md`, `procedures.md` — waypoints advance **manually** (a pilot action with latency, belongs on the command bus), no in-cockpit coordinate entry, brake chute |
| GCI doctrine (the AI's guidance model) | `datalink-gci.md` — voice BRAA → pilot enters expected range/relative altitude → radar computes scan elevation. The counter-design to Link-16 |
| Weapons | `weapons.md` — R-27R **SARH support obligation** (~26 s illumination vs the AIM-120's 5–15 s, crank ceiling 67°, Support/Defend mutually exclusive), R-73 + helmet cueing, R-60M, GSh-301 built row-for-row against the M61A1 table, A-G stores, DLZ mapped onto `Raero/Rtr/Rmin` |
| Cockpit / HUD | `cockpit-displays.md` — HUD FOV is a **24° circle** (not the F-16's rectangle); symbol geometry is unsourced, do not invent it |

## Cross-conflicts and acquisitions

Six cross-manual conflicts are registered (SPO-15 threat letters, waypoint sequencing, tachometer
100 %, radar-alt units, N019 gimbals, 7 g vs 9 g) — both values kept, never silently resolved, same
rule as `doc/f16/`.

**The one acquisition that upgrades ~a dozen gaps to top tier: GAF T.O. 1F-MIG29-1** (German Air
Force MiG-29G flight manual, English, USAF format, ~454 pp) — both files are written so its arrival
is an edit, not a rewrite. Also located: a Russian-language MiG-29 systems scan needing OCR, and two
403-blocked research sites listed in `PROGRESS.md`.

## Relation to the other skills

- `f16-systems` — the template and the comparison baseline (gun tables, DLZ vocabulary, HUD depth).
- `flightbox` — how a module/model actually gets built (registry, systems slots, MODEL-DELTAS
  discipline, the gym control loop that will measure every envelope anchor).
