# MiG-29A (9-12) — JSBSim Flight-Model Build Specification

**What this file is.** The **build order** for a JSBSim `<fdm_config>` for the MiG-29 9-12. It is
deliberately *not* written like `doc/modules/f16/flight-model.md`, because that file **describes a model
that exists** (every number has an `[XML]` tag, read out of the pinned vendored deck). Here **no
model exists yet**, so the same section structure is filled with three columns instead of one:

| Column | Meaning |
|---|---|
| **Documented** | a value with a citation and a confidence tag. Copy it into the XML. |
| **Derivation path** | no direct source, but a *stated procedure* produces the number from documented facts. Write the procedure into the XML comment next to the value. |
| **Open / `[SET]`** | nobody publishes it. It will be a declared FlightBox setting, and it says so in the file that carries it. |

**The honesty rule for this file:** a value may move from column 3 to column 2 to column 1, never
backwards, and never silently. A `[SET]` that acquires a source keeps its history in the changelog.

**Gliederungsvorlage:** `doc/modules/f16/flight-model.md` **§11 — Übergabe-Checkliste**, row by row. Row
numbers below are that checklist's row numbers.

---

> **Schema note — this file carries a THIN four-section frame, on purpose.** The rest of
> `doc/modules/mig29/` is organised as `Spec` / `State` / `Gaps` / `Knowledge`. Here the **three-column
> format above *is already* a Spec+Gaps hybrid**: every row states what is documented, what is
> derivable and what is open, side by side. Exploding those rows into separate sections would
> destroy the one structure that makes the file usable. So the frame is applied *around* the
> existing structure and not *through* it:
> - **`## Spec`** holds §0–§11 unchanged, including the **envelope-anchor tables (§8)** and the
>   **build order (§11)** — the two parts that are pure specification.
> - The **derivation-path column stays inside each row**, where it belongs; it is the file's
>   `Knowledge` content, distributed rather than collected.
> - **`## State`** is new and says the honest thing: nothing is built.
> - **`## Gaps`** holds §12 (open points, already split into the three required kinds).
> - **`## Knowledge`** holds the sources and points back at the in-row derivations.

## Spec

### 0. Header — sources and tags

**Primary sources** (the same two PDFs as `doc/modules/mig29/weapons.md`; see that file's source caveat,
which applies here identically):

- **`DCS-FM p.NN`** — `doc/DCS MIG-29 Flight Manual EN.pdf` (ED, 2018, 116 pp). **`NN` is the
  PRINTED page number** (the one in the page footer) — this manual has six roman-numbered
  front-matter pages, so **printed = PDF − 6**. Pages used:
  **9–11** (airframe geometry, control surfaces, landing gear, intakes), **12–13** (avionics),
  **14** (the TTD table — the single densest page in either manual), **81** (powerplant intro),
  **110** (its own sources).
- **`DCS-EA p.NN`** — `doc/DCS MiG-29A Early Access Manual EN.pdf` (ED, 2025, 115 pp). Pages used:
  **19** (AoA/g meter markings), **55, 57–58** (feel unit, flaps/slats/speedbrake, emergency panel),
  **60** (drag chute), **64** (SAU AFCS panel), **75–80** (taxi/takeoff/landing procedures — the
  richest source of speed anchors in either document).

**Confidence tiers for researched material:** **T1** official/declassified · **T2** manufacturer ·
**T3** established literature (Jane's-derived, engine-maker data, TsAGI-adjacent) · **T4**
community/wiki, cross-check only.
**Other tags:** `[DER]` derived by a stated formula · `[ABL]` inference from two cited facts ·
`[ANALOGY]` taken from a *different aircraft's* public data, declared as such · `[SET]` FlightBox
setting without source · `[GAP]` unknown, not guessed · `[ED-MODEL]` more likely an Eagle Dynamics
modelling decision than an aircraft property.

**File layout** (checklist row 0 — BUILT, see `## State`):

| File | Contents | Licence |
|---|---|---|
| `sim/assets/aircraft/mig29/mig29.xml` | `<metrics> <mass_balance> <ground_reactions> <propulsion> <flight_control> <aerodynamics>` | FlightBox-own, GPL-2.0-or-later — **the pinned JSBSim submodule has no MiG-29 and is read-only (Principle 1)**, so it is declared as having no upstream in `sim/assets/MODEL-DELTAS.md` and there is nothing for `verify-models` to diff, exactly like the AIM-120. (This row originally said "reached via `FBModule::FdmModelVendored() == false`"; that mechanism is gone — the tree now flies from ONE model root, `missions/FBModelRoots.h`.) |
| `sim/assets/aircraft/mig29/engine/RD-33.xml` | `<turbine_engine>` | FlightBox-own |
| `sim/assets/aircraft/mig29/engine/RD-33-nozzle.xml` | `<direct>` thruster | FlightBox-own |
| `sim/assets/aircraft/mig29/reset00.xml` | default IC, unused by FlightBox (as the F-16's is) | FlightBox-own |
| `release=` | **`ALPHA`** — see the promotion assessment in `## State` | — |

**Directory name:** `engine/`, not the `Engines/` this table originally planned. Both are searched by
`FGPropulsion::FindEngineFullPathname`; `engine/` is what the F-16 and AIM-120 already use, and one
convention per tree is worth more than matching a plan written before the tree was looked at.

---

### 1.1 Provenance chain (checklist row 1.1) — and why it is the weakest part

The F-16 model has **NASA TP-1538**: a published, full-envelope wind-tunnel-derived aerodynamic
dataset. **There is no MiG-29 equivalent in the public domain, at any tier.** That single fact
determines the shape of this entire document.

| Candidate source | What it would give | Status |
|---|---|---|
| **GAF T.O. 1F-MIG29-1** (German AF MiG-29G/GT Flight Manual, 30 Sep 1994 / rev. 20 Sep 2001, ~454 pp, English, USAF format) | **T1**: performance charts (takeoff/landing distance vs weight/OAT/pressure altitude, climb schedules, drag indices, fuel flow), limits, control-system description | **Exists; not consulted this pass.** `DCS-EA` reads like a distillation of it (knots/feet, TLP panel, "German manual" at `DCS-EA p.40`). **The highest-value single acquisition for this model.** |
| TsAGI / MiG OKB aerodynamic reports | coefficient decks | **`[GAP]`** — not public |
| `DCS-FM p.14` TTD table | envelope endpoints | **have it** (T3-grade, see §8) |
| `DCS-EA p.75–80` procedures | speed anchors by phase | **have it** (T3-grade) |
| Jane's-derived compendia (SirViper, aerospaceweb) | dimensions, weights, takeoff/landing distances | **have it** (T3) |
| Klimov / ММП им. Чернышева RD-33 data | engine scalars | **have it** (T2/T3) |
| **NASA TM-110216** — *Simulation model of the F/A-18 high angle-of-attack research vehicle*, Strickland et al., May 1996; aero database **α −10…+90°, β ±20°, M 0…2.0, 0…60,000 ft** | a **complete, public, high-α coefficient deck for a twin-engine, twin-tail, LEX-equipped fighter** | **the declared analogy**, §6.4 |

**The consequence, stated once so it does not have to be repeated:** for the MiG-29 the *envelope*
is documented and the *coefficients* are not. Every aerodynamic number below is therefore either
**inverted out of an envelope anchor**, **taken by declared analogy**, or **`[SET]`**. There is no
fourth category, and any file that presents a MiG-29 aero coefficient as a *cited* value is
misrepresenting its source.

---

### 1.2 Model delta (checklist row 1.2)

Not applicable in the F-16 sense (there is no upstream deck to diff against). The equivalent
discipline here: **the model is FlightBox-own from the first line**, so the "delta" is the full
content, and every value carries its tag in an XML comment. `diff` against the pinned submodule is
meaningless; the audit is `grep -c '\[SET\]' mig29.xml` — **that count is the model's honesty
metric and belongs in `PROGRESS.md`.**

---

### 2. Geometry — `<metrics>` (checklist row 2)

#### 2.1 Reference quantities (what JSBSim actually reads)

| `<metrics>` element | Documented | Derivation path | Open / `[SET]` |
|---|---|---|---|
| `wingarea` | **38.056 m² = 409.6 ft²** `DCS-FM p.14`; T3 rounds to 38.0 m² | — | **the *convention* is open**: for an integral-layout aircraft it is unclear whether 38.056 m² is the exposed wing, the wing + carry-through, or wing + carry-through + LERX. Every coefficient in §6 is scaled by this number, so **the convention must be declared and then never changed** `[SET]` |
| `wingspan` | **11.36 m = 37.27 ft** `DCS-FM p.14` | — | — |
| `chord` (c̄) | `[GAP]` | `[DER]` mean geometric chord `= S/b = 38.056/11.36 = 3.35 m = 10.99 ft`. True MAC differs because of the taper + LERX; a planform reconstruction from a scaled 3-view refines it | expect ±10 % until a 3-view is digitised |
| `htailarea` | **7.05 m² = 75.9 ft²** (stabilator, `DCS-FM p.10`) | assumed to be **both halves**, per Soviet convention `[ABL]` | if it is per-half the pitch derivatives double — **verify before trusting §6.5** |
| `htailarm` | `[GAP]` | measure stabilator quarter-chord to CG on a scaled 3-view; expect **≈ 5.0–5.5 m** for a 17.32 m airframe `[SET]` pending measurement | |
| `vtailarea` | **rudder 1.25 m² each** `DCS-FM p.10` (that is the *control surface*, not the fin) | total fin area from a 3-view; `[ABL]` twin fins of a 38 m² fighter are typically 2 × 5–6 m² | `[SET]` |
| `vtailarm` | `[GAP]` | same method as `htailarm` | note `doc/modules/f16/flight-model.md` §12.3 flags `vtailarm = 0` in the F-16 deck as an unfilled field — **do not repeat that** |
| `AERORP` | `[GAP]` | place at the c̄ quarter-chord once c̄ and the CG station are fixed | `[SET]` |
| `EYEPOINT` | `[GAP]` | from the cockpit sill / seat position in a 3-view; `DCS-EA p.75` gives a taxi-geometry drawing (nose wheel 14'10" behind the pivot reference, 24'12" total) that constrains the longitudinal layout | `[SET]` |
| `VRP` | — | set to the nose reference used for all other stations | `[SET]` |

#### 2.2 Planform and surfaces (all `DCS-FM p.9–10`, i.e. T3)

| Item | Value |
|---|---|
| Layout | **integral** — one continuous lifting surface, LERX blended into the fuselage |
| Wing LE sweep | **42°** (`DCS-FM p.14` confirms) |
| Wing TE sweep | **≈ 9°** |
| Wing dihedral | **−3°** (anhedral) |
| Aspect ratio | **3.4** (T3) — consistent with `b²/S = 11.36²/38.056 = 3.39` `[DER]` ✔ |
| **LERX** | area **4.71 m²**, LE sweep **73°30′**, **monolithic with the fuselage**; the auxiliary upper intakes are in the LERX |
| Leading-edge flaps | **three-section**, area **2.35 m²**, deflection **20°** |
| Trailing-edge flaps | **single-slotted**, area **2.84 m²**, deflection **25°** |
| Ailerons | area **1.45 m²**, **up 25° / down 15°**, **neutral position = 5° up** |
| Stabilator | **fully moving, differential**, area **7.05 m²**, LE sweep **50°**, anhedral **3°30′** |
| Rudders | **1.25 m² each**; *"first series aircraft had 20 % less rudder area"* |
| Airbrake, upper | area **0.75 m²**, deflection **+56°** |
| Airbrake, lower | area **0.55 m²**, deflection **−60°** |
| Fuselage | semi-monocoque, 10 main frames; nose tilted down relative to the horizontal reference line for over-the-nose view |
| Intakes | main: **supersonic, external compression, adjustable horizontal ramp**, with boundary-layer bleed. **Upper (LERX) intakes used on the ground up to 200 km/h** |
| Airfoil | `[GAP]` — T4 (Jay Miller *Aerofax*) says **NACA 64A-series biconvex, ~6 % t/c root and tip**. Low confidence; affects only §6's zero-lift and thickness-drag assumptions |

`[ABL]` **The LERX is 12.4 % of the reference wing area** (4.71/38.056) and sweeps at 73.5°. That is
the single most consequential geometric fact for the aerodynamics: it is the vortex generator that
produces the MiG-29's documented high-α behaviour, and **it is exactly the feature the F/A-18 HARV
database (§6.4) also has** — which is why that analogy is the right one and an F-16 analogy would
not be.

#### 2.3 Landing gear geometry (`DCS-FM p.10–11`)

| Item | Value |
|---|---|
| Wheel base | **3.645 m** |
| Wheel track | **3.09 m** |
| Nose gear | **2 × KT-100**, 570 × 140 mm, mudguard fitted |
| Main gear | **single KT-150**, 840 × 290 mm; retract **forward with a 90° rotation** into bays above the intake ducts |
| Nose-wheel steering | **±30° taxi**, **±8° takeoff** |
| Surface capability | concrete, asphalt-concrete, metal, **ground and snow** runways |

---

### 3. Mass, inertia, tanks — `<mass_balance>` + `<propulsion>` tanks (checklist row 3)

#### 3.1 Masses

| Item | Documented | Derivation path | Open |
|---|---|---|---|
| **Empty weight** | **10,900 kg** `DCS-FM p.14`; T3 agrees (24,030 lb) | — | high confidence |
| **Normal takeoff** | **15,300 kg** `DCS-FM p.14`; T3 says 15,240 kg | — | — |
| **Max takeoff** | **18,100 kg** `DCS-FM p.14`; T3 says 18,500 kg | — | spread ≈ 2 % |
| **Internal fuel** | **3,200 kg / 4,300–4,365 L** (T3); T4 gives 3,375–3,500 kg | — | ±10 % |
| Max external ordnance | 3,000 kg (T3); one T4 says 3,500 | — | — |
| Pilot + kit | `[GAP]` | `[SET]` 100 kg as a `<pointmass>`, mirroring the F-16 deck's single "Pilot" point mass | — |
| **Ammunition** | 150 rds × ≈ 0.832 kg ≈ **125 kg** | `[DER]`, T4 round mass | consumable — model as a tank or a shrinking point mass |

`[DER]` **The closure check that validates all four numbers at once** (repeated from
`doc/modules/mig29/weapons.md` §6 because it belongs in both files):
`10,900 (empty) + 3,200 (fuel) + 100 (pilot) + 926 (2×R-27R + 4×R-73) + 125 (ammo) = 15,251 kg`
against the documented **normal takeoff weight 15,300 kg**. **Agreement 0.3 %.** The four independent
figures — empty mass, internal fuel, the standard loadout, and normal TOW — are mutually consistent,
which is much stronger evidence than any of them alone. **Adopt all four.**

#### 3.2 Inertia tensor — the largest single `[SET]` in the model

**Nothing is published.** The derivation path, stated explicitly so it can be criticised:

1. Take the pinned JSBSim F-16's tensor as the reference fighter `[XML, doc/modules/f16/flight-model.md §3]`:
   empty 7,893 kg; `Ixx 9,496`, `Iyy 55,814`, `Izz 63,100`, `Ixz −982` slug·ft²
   (= 12,875 / 75,673 / 85,553 / −1,332 kg·m²).
2. Convert to **radii of gyration normalised by the natural length of each axis**
   (`kx/b`, `ky/L`, `kz/((b+L)/2)`) — this removes both mass and size:
   F-16: `kx = 1.277 m` → `kx/b = 0.128`; `ky = 3.096 m` → `ky/L = 0.206`;
   `kz = 3.292 m` → `kz/((b+L)/2) = 0.263`.
3. Apply to the MiG-29 (`b = 11.36 m`, `L = 17.32 m`, `m = 10,900 kg`):

| Axis | `k` | Inertia (kg·m²) | Inertia (slug·ft²) |
|---|---|---|---|
| `Ixx` | 1.455 m | 23,080 | **17,020** |
| `Iyy` | 3.568 m | 138,750 | **102,340** |
| `Izz` | 3.770 m | 155,000 | **114,320** |
| `Ixz` | — | ≈ −2,410 | **≈ −1,780** (scaled by the F-16's `Ixz/Izz = −0.0156`) |

4. **Then apply the twin-engine correction, and say that you did.** The F-16 carries one engine on
   the centreline; the MiG-29 carries two, separated laterally by roughly 1.5 m `[GAP]`, each
   ~1,055–1,156 kg (§5.1). Two masses of ~1.1 t at ±0.75 m add `2 × 1100 × 0.75² ≈ 1,240 kg·m²` to
   `Ixx` over a centreline placement `[DER]` — about **+5 %**. Widely spaced fuel (wing tanks) adds
   more. **Raise `Ixx` by 10 % to 25,400 kg·m² (18,730 slug·ft²) `[SET]`** and record the reasoning.

`[SET]` **All four inertias are declared settings.** They are the first thing to re-derive if the
measured roll response (§8) misses its anchor, because `Ixx` and the aileron/differential-stabilator
power are the *only* two knobs that set roll acceleration, and one of them (§6.6) is also a `[SET]`.

#### 3.3 Fuel tanks

**Documented structure** `DCS-FM p.9` (fore → aft): tank **#1**, tank **#2**, then the
**integral tank #3** *("the main fuel source of the aircraft")*, then the engine bays, flanked by
**two tanks #3a**.
T3 adds **one tank in each wing**, for **six internal tanks total (four fuselage + two wing)**;
`DCS-FM`'s five named compartments plus two wings is seven, so **the two accounts do not reconcile**
`[GAP]`.

| Tank | Documented | Derivation path | Open |
|---|---|---|---|
| #1, #2 | exist, forward of #3 | split the 4,300 L total by compartment volume from a 3-view | capacities `[GAP]` |
| **#3** | integral, **the main tank** | it is by far the largest; `[SET]` allocate ≈ 50 % of internal volume to it | — |
| #3a L/R | flank the engine bays | `[SET]` | — |
| Wing L/R | T3 | `[SET]` | conflicts with `DCS-FM`'s enumeration |
| **PTB-1500 centreline** | **1,500 L (T3: 1,500–1,520)**, ferry only; **9-12 has no wet wing pylons** (`doc/modules/mig29/weapons.md` §2.2) | — | — |
| **Feed order** | **`[GAP]`** | JSBSim's default consumes all priority-1 tanks simultaneously (`[JSB FGPropulsion::ConsumeFuel]`, as documented for the F-16). The real jet certainly sequences (external → forward → aft) to manage CG. `[SET]` model it with tank priorities and **state that the sequence is invented** | this is a **CG-travel** issue, i.e. it changes trim and stick force through the flight — not cosmetic |

**Fuel-quantity instrumentation** exists in the cockpit (ISTR4 flow meter, `DCS-EA p.26`) but no
manual gives per-tank capacities. **`[GAP]` remains.**

---

### 4. Ground reactions — `<ground_reactions>` (checklist row 4)

| Strut | Documented | Derivation path | Open |
|---|---|---|---|
| **NOSE (BOGEY)** | position from wheel base 3.645 m; **steering ±30° taxi / ±8° takeoff**; 2 × KT-100 570 × 140 mm `DCS-FM p.10–11` | longitudinal position: nose gear is **well behind the pilot's seat** — `DCS-EA p.75` gives a turning diagram with 24'12" and 14'10" dimensions that fixes it relative to the pivot point | spring/damping `[SET]` |
| **LEFT / RIGHT MAIN (BOGEY)** | track 3.09 m; single KT-150 840 × 290 mm; **retract forward with 90° rotation** `DCS-FM p.11` | — | spring/damping `[SET]` |
| Brakes | wheel brakes + separate **nose-wheel brake handle** `DCS-EA p.23`; *"the aircraft brakes with a slight delay"*, *"hard braking … leads to a smooth but significant lowering of the nose"* `DCS-EA p.76` | brake gain `[SET]`; the documented **delay** is a real behaviour worth an actuator lag rather than an instant `[SET]` | — |
| **Drag chute** | **jettisons/separates above 175 kts** `DCS-EA p.60`; mandatory for wet runway, short field, landing immediately after takeoff, no-slat landing, aborted takeoff after nose-wheel lift, heavy feel-unit setting | JSBSim has no chute primitive → implement as an `<external_reactions>` force, `F = q·CdA_chute`, aft along body X, at the tail attachment `[SET]` | `CdA` `[GAP]`; calibrate against the **600 m landing roll with chute** anchor (§8) |
| **Structure contacts** | `[GAP]` | place at nose, tail, wingtips, fin tips, **and the nozzles** — `DCS-EA p.78` warns *"rapid full aft movement of the stick may result in the exhaust nozzles hitting the runway"*, which is a documented ground-strike point and belongs in the deck | `[SET]` |
| **Structural failure load** | **JSBSim has none** (same as the F-16, `doc/modules/f16/flight-model.md` §4.3) | — | the physics verdict stays with `core/FBFlightMonitor`; **do not invent a break load in the XML** |

---

### 5. Propulsion — `<turbine_engine>` × 2 (checklist row 5)

#### 5.1 RD-33 scalars

| Parameter | Documented | Tier | JSBSim field |
|---|---|---|---|
| **Static thrust, max afterburner** | **8,300 kgf = 81.4 kN = 18,298 lbf** | `DCS-FM p.14` (kgf) + T3 (kN) — **two independent, agreeing** | `maxthrust` |
| **Static thrust, military** | **5,040 kgf = 49.4 kN = 11,111 lbf** | `DCS-FM p.14` + T3 | `milthrust` |
| **SFC, military** | **0.77 kg/(kgf·h)** = **0.77 lbm/(lbf·h)** (numerically identical) | T3 (Klimov/ММП data, RU sources) | `tsfc` = **0.77** |
| **SFC, full afterburner** | **2.05 kg/(kgf·h)** = **2.05 lbm/(lbf·h)** | T3 | `atsfc` = **2.05** |
| **Bypass ratio** | **0.47–0.49** (sources: 0.47 / 0.48 / 0.49) | T3 | `bypassratio` = **0.48** |
| Overall pressure ratio | **21–21.5** | T3 | — |
| Air mass flow | **76–77 kg/s** at max | T3 | — |
| Turbine inlet temperature | **≈ 1,536 K** (one source); **1,530 K takeoff / 1,680 K in flight** (another) | T3, contested | — |
| Dry weight | **1,055 kg** (T3) / 1,156 kg (T3, installed?) | T3, contested | mass budget only |
| **Idle → full AB response** | **≈ 4 s** | T3 | see §5.3 |
| Compressor | 9 stages (4 LP + 5 HP typical for this engine family) | T3/T4 | — |

`[ABL]` **Unit trap, flagged because it is in circulation.** Several web sources quote RD-33 SFC as
"7.5 / 20.1 kg/(kN·h)". That is a **factor-of-10 slip** (kgf→daN mistaken for kgf→kN): the correct
conversion of 0.77 kg/(kgf·h) is **78.5 kg/(kN·h)**, not 7.5. Use the kgf figures, which map onto
JSBSim's `lbm/(lbf·hr)` **one-to-one with no conversion at all**.

#### 5.2 Two engines — placement and the moment problem

| Item | Documented | Derivation path | Open |
|---|---|---|---|
| Count | **2 × RD-33** `DCS-FM p.14, p.81` | two `<engine>` + two `<thruster>` blocks, one per nacelle | — |
| Lateral separation | `[GAP]` | measure nacelle centrelines on a scaled 3-view; **expect ≈ 1.4–1.6 m** for an 11.36 m span with the wide fuselage tunnel `[SET]` | this number **is** the asymmetric-thrust yaw moment: at full AB one-engine-out gives `81.4 kN × 0.75 m ≈ 61 kN·m` `[DER]`, which the rudders must trim — a direct check on §6.7 |
| Cant / toe-in | `[GAP]` | assume **0°**, thrust along body X `[SET]` | — |
| Vertical offset from CG | `[GAP]` | 3-view; small, but sets the thrust pitch couple | `[SET]` |
| Intake | adjustable ramp, external compression, boundary-layer bleed; **ramp position is instrumented in the cockpit** (`DCS-EA p.30`) and has an **emergency retraction switch** (`DCS-EA p.58`); `DCS-EA p.78` notes a **nose-lowering tendency during intake duct opening** on the takeoff roll and *"observe increase of thrust when intake system opens at about 108 kts"* | JSBSim's turbine model has no ramp schedule. `[SET]` Either fold ram recovery into the thrust tables (simplest, recommended) **or** add a scheduled thrust multiplier on Mach. **Do not model the ramp as a separate system on a first pass.** | the documented 108 kts step-change in thrust is a **real, measurable takeoff-roll feature** — it belongs in §8's acceptance list, not in the aero |

#### 5.3 Spool dynamics

| Parameter | Documented | Derivation path |
|---|---|---|
| Idle → max AB | **≈ 4 s** (T3) | JSBSim `FGTurbine` uses `<bleed>`/N2 seek constants; **the F-16 deck's default spool-down is 3× faster than spool-up, which `doc/modules/f16/flight-model.md` §12.3 flags as physically backwards** — set both explicitly here rather than inheriting the default `[SET]` |
| Taxi RPM | **72–75 %** for comfortable taxi speed `DCS-EA p.76` | a direct idle-thrust calibration point: at 72–75 % N the aircraft must roll but stay controllable with light braking |
| Brake-check RPM | **80 %** — aircraft must remain stationary on brakes `DCS-EA p.75` | **a thrust-vs-N anchor**: thrust at 80 % N < total brake friction at ~12–15 t. `[DER]` With μ ≈ 0.8 and 90 % weight on the mains, brake force ≈ 0.8 × 0.9 × 15,300 × 9.81 ≈ 108 kN, so **thrust at 80 % N must be < 54 kN per engine** — nearly the whole military rating, so this is a weak but free constraint |
| Takeoff hold RPM | **90 % RPM** on brakes before release `DCS-EA p.78` | — |
| Climb RPM | **83–85 %** giving **985–1,480 ft/min at 270 kts** `DCS-EA p.77` | **a genuine part-power thrust anchor** — see §8 |
| Max RPM difference (limit) | **4 %** between engines `DCS-EA p.77` | operational limit, not a model parameter |

#### 5.4 Thrust tables — the honest statement

JSBSim's `<turbine_engine>` needs `IdleThrust`, `MilThrust` and `AugThrust` as **tables of
(Mach, density altitude)**. **No public RD-33 thrust deck exists** `[GAP]`.

**Derivation path** — and note carefully what it can and cannot do:

1. Anchor the **static sea-level** values from §5.1 (documented, high confidence).
2. Take the **shape** of the (Mach, altitude) surfaces from a comparable low-bypass afterburning
   turbofan whose deck *is* public — the F100-PW-229 deck in the pinned F-16 model
   (`doc/modules/f16/flight-model.md` §5.2) is the obvious candidate `[ANALOGY]`. Bypass ratios are close
   (F100 ≈ 0.36, RD-33 ≈ 0.48), both are two-spool augmented turbofans of the same generation.
3. **Scale** the borrowed surface so it reproduces the RD-33's documented statics.
4. **Then stop, and be explicit about the limit of this procedure:** the envelope anchors in §8
   (Vmax at sea level, Vmax at altitude, Ps) constrain the **product** `T(M,h) / D(M,h)`, **not `T`
   and `D` separately**. Fitting the thrust deck and the drag polar against the same three anchors
   is under-determined. **The rule adopted here: the thrust deck is fixed by the analogy + statics
   and is then FROZEN; all residual error is absorbed by the drag polar (§6.3).** If that later
   forces an implausible `CD0`, the *thrust* analogy is what is wrong — and that failure mode is
   only diagnosable because the rule was written down first.
5. **`[XML]` clamp behaviour matters.** `doc/modules/f16/flight-model.md` §12.3 flags the F-16 deck's
   "thrust exactly 0 above 60,000 ft density altitude" as *"a wall, not a decay"*. **Do not copy the
   zero column.** Extend the RD-33 table with a small non-zero value past the ceiling so the
   18,000 m service-ceiling anchor is approached asymptotically.

---

### 6. Aerodynamics — the derivation plan, per coefficient (checklist row 6)

**This section deliberately contains almost no numbers.** It contains, for every coefficient the
model needs, **which of the three categories it falls into**. That classification *is* the
deliverable.

#### 6.1 Category legend

| Category | Meaning |
|---|---|
| **INV** | *inverted from a documented envelope anchor* — a documented flight condition plus the equations of motion produce the coefficient |
| **GEO** | *computed from geometry* by a standard method (DATCOM / Polhamus / lifting-line) |
| **ANA** | *taken by declared analogy* from the F/A-18 HARV public database (§6.4) |
| **SET** | *declared setting* — no source, no derivation, written as such in the XML |

#### 6.2 Lift

| Coefficient | Cat. | Basis |
|---|---|---|
| `CLalpha` (linear range) | **GEO** | `AR = 3.39`, LE sweep 42°: DATCOM/lifting-line gives `CLα ≈ 3.0–3.3 /rad` for the wing alone; the **LERX adds a nonlinear vortex-lift increment**, not a slope increment (Polhamus leading-edge-suction analogy) |
| **Vortex-lift increment (LERX)** | **ANA** | this is the coefficient that *defines* the MiG-29 and it is `[GAP]` in every public MiG source. Take the HARV database's LEX contribution, scaled by `S_LEX/S_ref` (MiG-29 4.71/38.056 = **0.124**; F/A-18 LEX/S is comparable) |
| `CLmax` / stall shape | **INV + ANA** | **INV anchor**: `[DER]` touchdown at **140 kts and 11° AoA** `DCS-EA p.79`, flaps 25° + slats 20°. At an assumed landing weight of 12,000 kg: `CL = 2W/(ρV²S) = 2·117,720/(1.225·72.02²·38.056) = 0.97`. **So `CL(α=11°, full high-lift) ≈ 0.97` — the single hardest aerodynamic number in this document.** Sensitivity: at 13,000 kg it becomes 1.05. The high-α continuation beyond the limiter comes from **ANA** |
| Second anchor | **INV** | base leg **≥ 180 kts, 15° AoA max** `DCS-EA p.80`; final **≥ 175 kts** `DCS-EA p.80`. Weaker (bank angle unstated) but it must not be *violated* by the fitted curve |
| Ground effect | **SET** | the F-16 deck models it on lift only (`doc/modules/f16/flight-model.md` §6.3); do the same, and record that drag/pitch ground effect is absent |
| `CLde` (stabilator) | **GEO** | tail area 7.05 m², arm from §2.1, tail efficiency `[SET]` |

#### 6.3 Drag

| Coefficient | Cat. | Basis |
|---|---|---|
| `CD0` subsonic | **INV** | from the **climb-rate/Ps anchor**, §8 |
| `CD0` supersonic | **INV** | `[DER]` **Vmax sea level 1,500 km/h** `DCS-FM p.14` → `M = 1.224`, `q = 106,350 Pa`, `qS = 4.047 MN`. With both engines at AB and a ram-recovery factor of 1.30 at M1.2/SL, `T ≈ 212 kN` → **`CD ≈ 0.052`**. And **Vmax 2,450 km/h at 11 km** → `M = 2.30`, `q = 83,800 Pa`, `qS = 3.19 MN`; with `T ≈ 135 kN` (density ratio 0.297 × ram ≈ 2.8) → **`CD ≈ 0.042`**. Both are plausible for a fighter with wave drag, and they bracket the supersonic level |
| **Caveat on the above** | — | **the ram-recovery factors are `[SET]`, not measured.** This is exactly the under-determination named in §5.4 step 4. Publish both the CD and the assumed ram factor next to each other so a later reader can re-solve |
| Transonic drag rise | **ANA + SET** | shape from the HARV/F-16 decks' `*_M` correction structure; the *magnitude* is fitted to hit both Vmax anchors |
| Induced drag / `e` | **INV** | from **sustained** turn performance — **and this is a weak anchor**: the only public numbers are T4 (28 °/s instantaneous, 9 g at corner). §8 lists what must be measured instead |
| Store/tank drag | **not in the deck** | as for the F-16, FlightBox adds it externally through `fdm/FBFdm::SetStoresDrag` (`<external_reactions>` `fb-stores`) — the deck stays clean |
| Speedbrake drag | **INV** | areas documented (0.75 + 0.55 m², §2.2); `[DER]` at full deflection the projected area ≈ `0.75·sin56° + 0.55·sin60° ≈ 1.10 m²`, `Cd ≈ 1.2` flat plate → `ΔCdA ≈ 1.3 m²` → `ΔCD ≈ 0.035` on 38.056 m². Calibrate against the pattern-deceleration behaviour |
| Gear drag | **SET** | calibrate against the **220 kts gear-check speed** and pattern speeds |

#### 6.4 The high-α regime — the declared analogy

**Statement, to be reproduced verbatim in the model's XML header comment:**

> The MiG-29's aerodynamic behaviour above the AoA limiter (26°) is **not publicly documented at any
> confidence tier**. This model's high-α coefficients are taken by **declared analogy** from the
> **NASA F/A-18 HARV simulation database** (Strickland, Bundick, Messina, Hoffler, Carzoo, Yeager,
> Beissner: *Simulation model of the F/A-18 high angle-of-attack research vehicle utilized for the
> design of advanced control laws*, **NASA TM-110216**, May 1996 — aerodynamic database covering
> **α −10° … +90°, β ±20°, M 0 … 2.0, 0 … 60,000 ft**). The F/A-18 is chosen because it shares the
> three features that dominate this regime: **leading-edge extensions**, **twin canted vertical
> tails**, and **twin podded engines with a wide fuselage tunnel**. It is **not** a MiG-29, its LEX
> is a different size and shape, and its wing sweep and loading differ. Every coefficient sourced
> this way is tagged `[ANALOGY]` in this deck and **must not be quoted as a MiG-29 property.**

`[ABL]` Why this analogy and not the F-16's TP-1538 deck (which FlightBox already ships): the F-16
is a **single-engine, single-tail, strake-not-LEX, blended-body** aircraft whose high-α behaviour is
dominated by its own very different forebody. The two candidate analogies are not equally good, and
picking the convenient one would be the exact failure this file exists to prevent.

#### 6.5 Pitch

| Coefficient | Cat. | Basis |
|---|---|---|
| `Cmalpha` / static margin | **SET** | **the MiG-29 is statically stable** — it has a *mechanical* control system with an ARU gearing changer and a damper, not a relaxed-stability FBW (§7). That is a qualitative certainty from the control architecture `[ABL]`, but the *margin* is `[GAP]`. `[SET]` start at **−5 % c̄** and adjust against the trim-speed and stick-force behaviour |
| `Cmq` | **GEO/SET** | tail volume from §2.1 × standard estimate |
| `Cmadot` | **SET** | note `doc/modules/f16/flight-model.md` §12.1 records that the F-16 deck **omits** `Cmadot` while the Mk-82 deck has one. Include it here or state its absence |
| `Cmde` | **GEO** | stabilator area × arm × efficiency; **must reproduce the documented deflection limits** (§7.2) as usable pitch authority at both ends of the ARU schedule |
| Mach tuck | **ANA** | shape from a comparable deck; magnitude `[SET]` |

#### 6.6 Roll

| Coefficient | Cat. | Basis |
|---|---|---|
| `Clp` (roll damping) | **GEO** | `AR`, taper, sweep → standard estimate |
| `Clda` (aileron power) | **GEO + INV** | geometry documented (1.45 m² each, +25/−15°, **5° up neutral**) `DCS-FM p.10`; **INV** against a measured roll rate anchor — which is `[GAP]`, see §8 |
| Differential stabilator roll contribution | **SET** | the stabilator is documented as *"fully rotary, differential"* `DCS-FM p.10`, so it contributes roll, but the **mixing law and the roll/pitch authority split are `[GAP]`** |
| `Clbeta` (dihedral effect) | **GEO** | wing anhedral −3°, high LERX vortex contribution `[ANALOGY]` at high α |

#### 6.7 Yaw

| Coefficient | Cat. | Basis |
|---|---|---|
| `Cnbeta` | **GEO** | twin fins, areas `[SET]` (§2.1) |
| `Cnr` | **GEO/SET** | fin volume × standard estimate |
| `Cndr` | **GEO + INV** | rudder area 1.25 m² each documented; **INV** against the **asymmetric-thrust trim requirement** from §5.2 (≈ 61 kN·m at full AB single-engine) and against the documented crosswind technique: **1° of crab per 3 kts of crosswind**, wings-level crab above 15 kts `DCS-EA p.79` |
| Adverse yaw / ARI | **SET** | `[GAP]` whether the aircraft has aileron-rudder interconnect |

#### 6.8 What the deck will NOT contain (row 6.x)

| Absent | Consequence |
|---|---|
| Store aerodynamics | supplied externally by `fdm/FBFdm::SetStoresDrag`, as for the F-16 |
| Intake-ramp thrust scheduling | folded into the thrust tables (§5.2) |
| Post-limiter departure/spin behaviour | `[ANALOGY]` only; **the model must not be claimed to predict MiG-29 spin behaviour** |
| Aeroelasticity, fin-buffet | `[GAP]` — and it is a real phenomenon: T4 records that Luftwaffe MiG-29s developed **cracks at the base of the vertical tails** from aggressive flying, i.e. LEX-vortex buffet is structurally significant on this airframe. Not modelled |
| Damage aerodynamics | supplied by `fdm/FBFdm::SetDamageDrag` (`fb-damage` external force), per `CLAUDE.md` |

---

### 7. Flight control — `<flight_control>` sketch (checklist row 7)

**The headline, because it changes everything relative to the F-16 module:**

> **The MiG-29 9-12 has a conventional, mechanically-signalled flight control system with hydraulic
> boost, a variable-gearing unit (ARU), a limiter unit (SOS) and a three-axis damper (part of the
> SAU AFCS). It is NOT fly-by-wire. There is no FLCS to bypass, and therefore
> `fcs/fbw-override` has no analogue here.**
>
> FlightBox's module FBW (`systems/FBFlightControl`) wraps this deck **from the outside**, exactly as
> it does for every other module — it writes `fcs/*-cmd-norm` and the deck turns those into surface
> deflections. The F-16's peculiarity (a real FLCS *inside* the deck that the FBW has to override) is
> the exception in FlightBox, not the rule, and this airframe is the rule. **No FBW reimplementation
> is required.**

#### 7.1 Channels

| Channel | Surface(s) | Documented behaviour | Source |
|---|---|---|---|
| **Pitch** | fully-moving **differential stabilator** | travel **takeoff: ≈ 15° up / 35° down**; **in flight: 5°45′ / 17°45′** | `DCS-FM p.10` |
| **Roll** | ailerons (+ differential stabilator) | **up 25° / down 15°**, **neutral = 5° up** | `DCS-FM p.10` |
| **Yaw** | twin rudders | 1.25 m² each; **rudder trim is a 3-position switch** | `DCS-FM p.10`, `DCS-EA p.64` |
| **LEF (slats)** | 3-section LE flaps | **20°**, **auto-extend above α = 8.7°**; after gear retraction *"the slats become fully automatic and operate depending on the angle of attack and the Mach number"*; retract with the flaps or manually on the ground | `DCS-FM p.10`, `DCS-EA p.57` |
| **TEF (flaps)** | single-slotted | **25°**; three buttons: **RETRACTED / TAKEOFF / LANDING**; *"the flaps and slats extract both"* on either DOWN button | `DCS-FM p.10`, `DCS-EA p.57` |
| **Speedbrakes** | upper 0.75 m² +56°, lower 0.55 m² −60° | spring-loaded switch on the right throttle, auto-returns to IN; **full extension ≈ 3 s**; **blow-back retraction above 540 kts**; **inhibited with centreline tank or gear down**; **auto-retract on total electrical failure** | `DCS-FM p.10`, `DCS-EA p.57` |
| **Gear** | tricycle | retract **32–50 ft AGL**; if not fully up: 80 % RPM, 220 kts, recheck | `DCS-EA p.77` |
| **NWS** | nose wheel | ±30° taxi / ±8° takeoff; a **gain-increase button** is held for sharp turns | `DCS-FM p.10`, `DCS-EA p.76` |
| **Drag chute** | — | separates above **175 kts** | `DCS-EA p.60` |

#### 7.2 ARU — the variable gearing unit

**What it does, from the one documented fact:** the stabilator's available travel is **roughly
halved between the takeoff/landing configuration and the in-flight configuration**
(15°/35° → 5°45′/17°45′) `DCS-FM p.10`. `[ABL]` That is the classic Soviet **АРУ (автомат
регулирования управления)**: a q-scheduled gearing changer that reduces stick-to-stabilator gearing
as dynamic pressure rises, so that a given stick displacement produces a roughly constant *g*
response instead of a constant *deflection*.

| Aspect | Documented | Derivation path | Open |
|---|---|---|---|
| End points of the schedule | the two deflection pairs above | — | — |
| **Scheduling variable** | `[GAP]` | `[SET]` schedule on **q** (or IAS), transitioning across the approach/cruise band; JSBSim implements this as a `<scheduled_gain>` on the pitch channel | whether the real unit schedules on q, IAS or Mach is unknown |
| Transition rate | `[GAP]` | `[SET]` first-order lag | — |
| **Cockpit tie-in** | there is a **"FEEL UNIT TAKEOFF – LANDING" lamp** checked before takeoff `DCS-EA p.77`, a **feel-unit control** (authority of the AFCS in handling, *"not implemented yet"* in DCS) `DCS-EA p.55`, and a **throttle-tightening handle** `DCS-EA p.55` | model the feel unit as **stick-force gradient**, which JSBSim does not simulate — so it becomes a `systems/FBFlightControl` gain, not a deck element `[SET]` | the "Heavy" feel-unit position is one of the **mandatory drag-chute conditions** `DCS-EA p.60`, i.e. it is a real configuration state |

#### 7.3 SOS — the AoA/g limiter

| Aspect | Value | Tier |
|---|---|---|
| Limit AoA (9-12A / 9-13) | **26°** | T4 |
| Limit AoA (early 9-12) | 24°, raised incrementally after the rudder-chord extension | T4 |
| **Mechanism** | pistons at the base of the stick **push the stick forward**, reducing AoA by about **5°** | T4 |
| **Overridable** | yes — **≈ 17 kgf of additional aft stick force** defeats it; the aircraft *"can still fly far beyond 28°"* | T4 |
| Coupled to | **disengaging the stability augmentation also disengages the AoA limiter** | T4 |
| Cockpit indication | AoA/g gauge carries **15° and 25° markers**, a **red sector**, and a **red mark at 7 g** | `DCS-EA p.19` |

`[ABL]` **The 25° gauge marker and the 26° T4 limiter figure corroborate each other** — an
instrument marked at 25° on an aircraft limited at 26° is exactly what one expects; that raises the
26° figure above bare T4.

**Modelling decision `[SET]`:** implement SOS as a **soft limiter in `systems/FBFlightControl`
(FlightBox side), not in the deck.** Reasons: (a) it is a *force* effect on the stick, and JSBSim's
deck sees normalised commands, not forces; (b) it must be **overridable**, which is a pilot-model
decision, not an airframe one; (c) putting it in the deck would make it invisible to
`core/FBFlightMonitor`, and the whole point of that monitor is that the limiter is not the judge.

#### 7.4 SAU-451 AFCS — the outer loops

`DCS-EA p.64` gives the panel; each of these is a **`systems/FBAutopilot` mode**, not a deck element:

| Switch | Function | FlightBox mapping |
|---|---|---|
| **DAMPER** | *"significantly improves the controllability and stability … in most cases it must be enabled"* | three-axis rate damper — this **is** `FBFlightControl`'s inner rate loop |
| **AUTO RECOVER** | automatic pull-up from a dangerous altitude, restores level flight | a ground-collision recovery mode; no current FlightBox analogue |
| **ALT HOLD** | altitude hold | `FBAutopilot::Direct` altitude channel |
| **ATT HOLD** | attitude hold | `FBAutopilot` attitude channel |
| **APPROACH** | director following on the landing approach | `FBPilot` Approach phase |
| **MISSED APPROACH** | *"not implemented yet"* in DCS | `FBPilot` phase, later |
| Cold-start | AFCS runs a **3-minute BIT** | a startup-procedure fact, not a deck one |

`DCS-FM p.34–36` covers the same system at FC3 level, including radar-altimeter minimum-altitude
setting — cross-reference the parallel agent's systems files rather than duplicating.

---

### 8. The documented envelope anchors — the future gym acceptance test

**This table is the deliverable that outlives the rest of the file.** Every row becomes a `.fbm`
mission plus a telemetry assertion; the model is `release=BETA` when all "must" rows pass.

#### 8.1 Performance envelope

| # | Anchor | Value | Source / tier | Gym measurement |
|---|---|---|---|---|
| A1 | **Vmax, altitude** | **2,450 km/h ≈ M 2.3** (T3 says 2,445 at 11,000 m) | `DCS-FM p.14` | level accel at 11 km, AB, clean, until `dV/dt → 0`; read `mach` |
| A2 | **Vmax, sea level** | **1,500 km/h ≈ M 1.22** | `DCS-FM p.14` | **conflict:** one T3 source says 1,200 km/h / M1.06. Test against 1,500; record the miss |
| A3 | **Service ceiling** | **18,000 m** | `DCS-FM p.14`; T3 spread **17,000–18,500 m** | zoom-free steady climb to `ROC < 0.5 m/s` |
| A4 | **Max climb rate** | **330 m/s** (= 19,800 m/min = 65,000 ft/min, T3 states the same number in both units) | `DCS-FM p.14` + T3 | **read the derivation note below — do not test this as a steady climb** |
| A5 | **Max operational g** | **+9** | `DCS-FM p.14` | limiter/monitor check |
| A5b | g limit above M 0.85 | **+7** | T4 | corroborated by the **red 7 g mark on the cockpit gauge** `DCS-EA p.19` `[ABL]` |
| A5c | g limit with centreline tank | **+4 until empty** | T4 | a carriage-dependent limit |
| A6 | **AoA limiter** | **26°** (early 9-12: 24°) | T4 + the 25° gauge marker `DCS-EA p.19` | measure the α at which the modelled limiter arrests the pull |
| A7 | Range, internal fuel | **1,430 km** | `DCS-FM p.14` | cruise mission, fuel-to-zero |
| A8 | Ferry range | **2,100 km** (9-12, centreline tank only) | `DCS-FM p.14` | see `weapons.md` §2.2 |
| A9 | Instantaneous turn rate | **28 °/s** | **T4 only** — weakest anchor in the table | measure anyway; it is the number the community will compare against |

`[DER]` **A4 is not a rate of climb, and testing it as one will fail.** Check: a steady climb at
M0.9 and 5 km with 13,000 kg needs `T−D = Ps·W/V = 330·127,530/290 ≈ 145 kN`, while both engines at
that condition give ≈ 122 kN — **impossible**. At **sea level, M 0.9, 13,000 kg**, however:
`T ≈ 195 kN`, `D ≈ 48 kN` (`q = 57,350 Pa`, `qS = 2.18 MN`, `CD₀ ≈ 0.022`), so
`Ps = (T−D)·V/W = 147,000·306/127,530 ≈ 353 m/s`. **The 330 m/s figure is specific excess power near
sea level at a light weight, not a sustained climb rate at altitude.** Test A4 as **`Ps` at SL,
M 0.9, light** — and record this reinterpretation in the mission file, because the raw number in the
manual invites exactly the wrong test.

#### 8.2 Takeoff and landing — the densest and best-sourced anchors

All from `DCS-EA p.75–80` unless marked; these are procedure numbers from a German-manual descendant
and are the most trustworthy operational figures available.

| # | Anchor | Value | Gym measurement |
|---|---|---|---|
| B1 | **Rotation speed** | **125–135 kts** | takeoff mission; `airspeed` at pitch-up |
| B2 | **Liftoff speed** | **140–150 kts** (weight-dependent) | WOW → false |
| B3 | Takeoff pitch attitude | **8–10°**, horizon just above the IR sensor | `theta` after liftoff |
| B4 | Intake-system thrust step | *"observe increase of thrust when intake system opens at about **108 kts**"* | if §5.2's simplification is adopted, this will **not** reproduce — record the deliberate miss |
| B5 | Gear retraction | **32–50 ft AGL**; recheck at **220 kts / 80 % RPM** if unsafe | procedure |
| B6 | Flap retraction | **350 ft** | procedure |
| B7 | **Climb schedule** | **270 kts at 83–85 % RPM → 985–1,480 ft/min** | **a part-power thrust + drag anchor**; measure `ROC` at that exact state |
| B8 | **Takeoff run** | **250 m at normal weight** | T3 (Jane's-derived) |
| B9 | **Landing roll** | **600 m with brake chute** | T3 |
| B10 | **Touchdown** | **≈ 140 kts at 11° AoA**; **do not exceed 13° AoA**; **+5–10 kts at max landing weight** | the §6.2 `CLmax` anchor, measured |
| B11 | Final approach | **≥ 175 kts** | |
| B12 | Base leg | **≥ 180 kts, ≤ 15° AoA** | |
| B13 | Downwind | **200–220 kts**; pattern **≥ 300 kts** | |
| B14 | Flare initiation | **20–30 ft AGL**, *"nearly full aft stick"* to arrest the sink rate | a **stick-authority** check at approach speed |
| B15 | Crosswind | up to 15 kts: 5–10° low wing + crab; above 15 kts: wings-level crab; **≈ 1° of crab per 3 kts** | yaw-authority check |
| B16 | Brake application after landing | **115 kts** | |
| B17 | Drag-chute separation | **> 175 kts** | `DCS-EA p.60` |
| B18 | Speedbrake blow-back | **> 540 kts** | `DCS-EA p.57` |
| B19 | Upper-intake operating limit | ground speed **≤ 200 km/h** | `DCS-FM p.11` |

`[DER]` **B8 is self-consistent with §5.1 and B2, which is a strong sign both are right.**
At 15,300 kg with both engines in AB, `T/W = 162.8/150.1 = 1.084`; with rolling friction μ ≈ 0.03
and mean drag ≈ 0.05 W, mean acceleration `a ≈ 9.81·(1.084 − 0.08) ≈ 9.8 m/s²`; liftoff at 145 kts
= 74.6 m/s gives `s = V²/2a = 74.6²/19.6 = 284 m` — against a documented **250 m**. The **13 %
residual** is exactly what a lighter "normal weight", a headwind, or ground effect would supply.
**Three independently-sourced numbers (static thrust, liftoff speed, takeoff run) closing to 13 % is
the second-strongest consistency check in this document** (after the mass closure, §3.1).

#### 8.3 What is NOT anchored, and therefore cannot be accepted yet

| Missing anchor | Why it matters |
|---|---|
| **Roll rate** | `[GAP]` at every tier. Roll is the axis with **two** `[SET]`s stacked (`Ixx` §3.2 and `Clda` §6.6) and **no** measurement to separate them. The model's roll behaviour will be an assumption until a T1 figure appears |
| **Sustained turn rate at a stated altitude/weight** | `[GAP]`; only T4 anecdote. Without it, the induced-drag factor `e` is unconstrained |
| **Corner speed** | `[GAP]`. `make -C sim test-corner` (which exists for the F-16) will *measure* the model's corner speed — but there is nothing to compare it against |
| **Drag index per store** | `[GAP]` — handled outside the deck, but unvalidated |
| **Fuel flow vs. throttle/altitude** | only the two SFC scalars (§5.1); no cruise/loiter validation |

---

### 9. Accepted-model-property policy (checklist row 9)

`CLAUDE.md` Principle 5 says the **pinned vanilla F-16 model** is truth and that FlightBox is judged
on flying it faithfully, not on model-vs-real-jet. **That principle does not transfer unchanged**,
because this model is FlightBox's own: there is no upstream author whose choices are simply
accepted. The adapted rule:

1. **Documented anchors (§8) are truth.** A model that misses one has a defect, and the defect is
   recorded in `PROGRESS.md` with its measured value.
2. **`[SET]` values are not defects — they are declared ignorance**, and they may be tuned freely to
   satisfy rule 1, **provided the tuning is written next to the value**.
3. **`[ANALOGY]` values may not be presented as MiG-29 properties**, in code comments, in docs, or in
   telemetry column descriptions.
4. **Where the two DCS manuals disagree**, both figures are recorded and the model follows the one
   named in §8; the other is listed as a known miss. (`doc/modules/mig29/weapons.md` §7.1 lists the
   disagreements found so far.)
5. **`core/FBFlightMonitor` remains the judge.** It is model-agnostic by construction and must not be
   taught anything about the MiG-29 — including the AoA limiter (§7.3).

---

### 10. Weapons and stores (checklist row 10)

Fully covered in **`doc/modules/mig29/weapons.md`**. Interface points that belong to *this* file:

| Interface | Value | Note |
|---|---|---|
| Store carriage mechanism | `<pointmass>` per occupied station + one `<external_reactions>` force `fb-stores` | identical to the F-16 path; **no model XML is patched at runtime** |
| Station coordinates | `[GAP]` | must be derived from the same 3-view that fixes §2.1's arms; `modules/mig29/…Sms` anchors them, as `FBF16Sms` does |
| Gun position | port side of the nose ahead of the cockpit `DCS-FM p.64` | exact offsets `[GAP]`; `[SET]` from a 3-view |
| **Gun recoil** | `[DER]` **8.39 kN** steady during fire (`weapons.md` §4.4) = **0.056 g** at 15,300 kg | worth an `<external_reactions>` channel; the F-16 model has no gun-recoil force either, so this would be a FlightBox extension, declared as such |
| Ammunition mass | ≈ 125 kg full | model as a depleting mass, otherwise a 6-second burst is mass-neutral |
| Centreline tank | 1,500 L; **inhibits speedbrakes** (`DCS-EA p.57`) and caps g at +4 until empty (T4) | a carriage-dependent *flight-control* and *limit* effect, i.e. it belongs to this file, not to `weapons.md` |

---

### 11. Build order — what is built when, and what it is measured against

**The rule: each step is only "done" when a named §8 anchor is measured in `fb-gym` telemetry.
No step may be started before its predecessor's anchor passes**, because every later step's
calibration would otherwise absorb the earlier step's error.

| Step | Build | Anchor(s) | Gym mission | Failure diagnosis |
|---|---|---|---|---|
| **1** | `<metrics>` + `<mass_balance>` + `<ground_reactions>`; **no aero, no engine beyond idle** | aircraft spawns on a runway, sits at the documented ground clearance, does not sink or bounce; taxi at **72–75 % RPM** (§5.3) | `mig29-ground.fbm` | gear spring/damping `[SET]` |
| **2** | `<propulsion>`: two RD-33, statics from §5.1, thrust surfaces per §5.4 | **A2** (Vmax SL), **A1** (Vmax alt), **A4** (Ps at SL, per the §8.1 reinterpretation) — **with a first-cut `CD0` only** | `mig29-accel-sl.fbm`, `mig29-accel-alt.fbm` | if A1 and A2 cannot both be hit, the **transonic drag-rise shape** is wrong, not the statics |
| **3** | Drag polar: `CD0(M)`, wave drag, induced term | A1, A2, A4 all simultaneously; **A3** (ceiling) | + `mig29-ceiling.fbm` | if the ceiling is reached but Vmax is not, revisit §5.4 step 4 — the thrust analogy is the suspect |
| **4** | Lift: `CLalpha`, high-lift devices (LEF 20° auto at α > 8.7°, TEF 25°) | **B1, B2, B8** (rotation, liftoff, takeoff run); **B10** (touchdown 140 kts @ 11° AoA); **B9** (landing roll with chute) | `mig29-takeoff.fbm`, `mig29-landing.fbm` | takeoff run high + liftoff speed right → drag; liftoff speed high → `CLmax`; both wrong → mass |
| **5** | Chute + speedbrake + gear drag | **B9** (600 m), B17, B18, B19 | `mig29-landing.fbm` | chute `CdA` is the free parameter, calibrated **last** so it does not mask airframe drag |
| **6** | Moments + control power: `Cm*`, `Cl*`, `Cn*`, stabilator/aileron/rudder authority | **B3** (takeoff attitude), **B14** (near-full aft stick in the flare), **B15** (crosswind crab), the §5.2 asymmetric-thrust trim, **A9** (instantaneous turn, T4) | `mig29-pattern.fbm`, `mig29-turn.fbm` | B14 failing = insufficient stabilator power at approach q, i.e. §7.2's ARU low-speed end is wrong |
| **7** | ARU gearing schedule (§7.2) | stick-per-g roughly constant from approach to high-q cruise `[SET]` — **no documented anchor**, this is a *shape* requirement | `mig29-gearing.fbm` (sweep of q, measure g per unit `elevator-cmd-norm`) | — |
| **8** | Limits: SOS AoA limiter (§7.3, in `FBFlightControl`), g limits, carriage-dependent limits | **A5** (+9), **A5b** (+7 above M0.85), **A6** (26° α), **A5c** (+4 with tank) | `mig29-limits.fbm` | the limiter must be *overridable* (T4) — verify the override path exists |
| **9** | Corner-speed and roll-rate **measurement** (not calibration — there is nothing to calibrate against) | `make -C sim test-corner` equivalent; record the numbers as **accepted model properties** per §9 rule 2 | — | these become the module's own hooks (`FBPilot::CornerTurnRateDegS` etc.), exactly as the F-16's 380 KCAS / 16.2 °/s were measured rather than assumed |
| **10** | Stores integration: point masses, `fb-stores` drag, gun recoil | mass closure (§3.1) reproduced in telemetry `fuelLbs` + weight; §10's carriage-dependent limits | `mig29-loadout.fbm` | — |

**Promotion gates:** `release=ALPHA` while steps 1–3 are open · **`BETA`** when steps 1–6 pass ·
**`PRODUCTION`** only after GAF T.O. 1F-MIG29-1 (or an equivalent T1) has replaced the §8.3 gaps —
i.e. **the model cannot honestly reach PRODUCTION on the sources consulted in this pass**, and
saying so here is the point of the file.

---

## State

**Stage 1 of R8 is built: the JSBSim deck exists and every §8 anchor is measured.** What does NOT
exist is anything above the airframe — no `sim/src/modules/mig29/`, no registry name, no `.fbm`
mission that flies one. Nothing in the tree loads this model except its own harness, so no existing
measurement moved.

| Artefact | State |
|---|---|
| `sim/assets/aircraft/mig29/mig29.xml` + `engine/RD-33.xml` + `engine/RD-33-nozzle.xml` + `reset00.xml` | **built**, `release="ALPHA"` |
| `sim/assets/MODEL-DELTAS.md` | declares `mig29` as FlightBox-own (no upstream, so no delta block); `make -C sim verify-models` green |
| `sim/test/modules/mig29/FBTestMig29Envelope.cpp` + `make -C sim test-mig29` | **built** — loads the deck through `FBFdmBoot` and measures 22 anchors; two identical runs are byte-identical |
| module / registry / missions | **not built** — stage 2/3 |

### The measured anchor table (`make -C sim test-mig29`, 2026-07-28)

Deviation is `(measured − documented) / documented`. "must" rows are the ones §11 gates on.

| # | Anchor | Documented | Measured | Dev. | Verdict |
|---|---|---|---|---|---|
| A1 | Vmax at 11 km | 2,450 km/h | **2,500 km/h** (M 2.35) | **+2.1 %** | pass |
| A2 | Vmax at sea level | 1,500 km/h | **1,503 km/h** (M 1.23) | **+0.2 %** | pass |
| A3 | Service ceiling | 18,000 m | **19,566 m** | **+8.7 %** | miss — see §12.5 |
| A4 | Ps at SL, M 0.90, light | 330 m/s | **248 m/s** | **−24.8 %** | miss — see §12.5 |
| A5 | g reached under a 9 g limiter | +9 | **+8.26** | −8.3 % | pass |
| A6 | α reached under a 26° limiter | 26° | **25.8°** | −0.8 % | pass |
| A9 | Instantaneous turn rate, 1,000 m | 28 °/s (T4) | **26.5 °/s** | −5.2 % | pass (weakest anchor) |
| A9′ | the same at 5,000 m | — | 22.0 °/s | — | recorded, no anchor |
| B1 | Rotation speed | 125–135 kt | **130.1 kt** | in band | pass |
| B2 | Liftoff speed | 140–150 kt | **146.1 kt** | in band | pass |
| B3 | Takeoff pitch attitude | 8–10° | **8.87°** | in band | pass |
| B7 | ROC at 270 kt, 84 % RPM | 985–1,480 ft/min | **1,141 ft/min** | in band | pass |
| B8 | Takeoff ground run | 250 m | **324 m** | **+29.8 %** | miss — see §12.5 |
| B9 | Landing roll with chute | 600 m | **659 m** | +9.9 % | miss (marginal) |
| B10 | α in landing config at 140 kt / 12 t | 11° | **10.29°** | −6.5 % | pass |
| B10′ | Touchdown speed in a flown landing | ~140 kt | **145.5 kt** | +4.0 % | pass |
| — | Deck mass (empty + internal fuel + pilot) | 14,200 kg | **14,199.7 kg** | −0.002 % | pass |
| — | Mass closure vs normal TOW (deck + 926 + 125) | 15,300 kg | **15,250.7 kg** | −0.32 % | pass |
| — | Internal fuel | 3,200 kg | **3,200.1 kg** | +0.003 % | pass |
| — | Static gear travel over 18 s on the brakes | 0 | **0.13 mm** | — | pass |
| — | Creep at 80 % RPM on full brakes | 0 | **2.2 µm/s** | — | pass |
| — | Taxi speed after 30 s at 73 % RPM | "comfortable" | **26.0 kt** | — | pass |
| GAP | Peak roll rate at 450 kt / 10,000 ft | **no anchor at any tier** | **241 °/s** | — | declared model property |

### Consistency probes (§3.1 and §8.2, re-run against the built deck)

- **Mass closure:** the deck reproduces 14,199.7 kg against the 14,200 kg the four documented figures
  predict, i.e. the closure that validates empty mass, internal fuel and pilot mass simultaneously
  holds *in the model* to 0.002 %, and to 0.32 % against normal TOW once the standard loadout is added
  arithmetically. The stores half is untested until the module carries point masses (stage 2).
- **Thrust against takeoff run:** §8.2 predicts 284 m from static thrust, liftoff speed and a mean
  drag of 0.05 W; the deck gives 324 m at a *measured* liftoff of 146.1 kt. The 14 % gap is the
  configuration drag §8.2 lumped into one coefficient: this deck carries flaps (ΔCD 0.060) and gear
  (0.030) explicitly, which is ~8 kN of the ~150 kN accelerating force through the second half of the
  roll. Both predictions bracket the documented 250 m from above, which §12.3 already expected.

### The honesty metric (§1.2)

§1.2 asks for `grep -c '\[SET\]' mig29.xml` as the model's honesty metric and files it under
`PROGRESS.md`. It is recorded **here** instead, because `PROGRESS.md` is a *source-coverage* ledger for
the twelve knowledge files and this is a *build* fact; the build state belongs next to the build.

Raw tag counts — `mig29.xml` / `engine/RD-33.xml`:

| `[SET]` | `[GEO]` | `[ANALOGY]` | `[INV]` | `[DERIVED]` | `[GAP]` | `[ABL]` |
|---|---|---|---|---|---|---|
| **35 / 2** | 16 / 0 | 15 / 2 | 4 / 0 | 9 / 5 | 14 / 0 | 5 / 0 |

The more useful cut is **per coefficient**. The deck holds **35 aerodynamic functions** (33
coefficients + 2 shared factors) over 38 tables, plus 9 flight-control channels in 25 components.
Classifying each function by the category that *sets its magnitude*:

| Category | Count | Which |
|---|---|---|
| **GEO** | 17 | `CLde CLq kCLmach CYb CYdr Clb Clp Clr Cldr Cm_mach Cmde Cmq Cmadot Cnb Cnr Cnp Cndr` |
| **SET** | 11 | `CDde CDgear CDflaps CDlef CDchute Clda Cm Cmflaps Cmlef Cmgear` + the split inside `CLflaps`/`CLlef` |
| **INV** | 4 | `CD0` (both Vmax anchors), `CLflaps`+`CLlef` (touchdown anchor), `Cndr` (asymmetric-thrust case) |
| **ANALOGY** | 2 | `kCLge`, `Cnda` |
| **DERIVED** | 1 | `CDsb` (from the documented brake areas and deflections) |

**But the analogy reaches further than those two rows suggest, and that is the point of §6.4.** Ten
tables are GEO- or SET-led through the linear range and **`[ANALOGY]`-shaped above it**: `CLalpha`
(vortex-lift increment and its breakdown above 32°), `CDalpha` (the leading-edge-suction loss
schedule), `Cm` (nose-up departure above 38°), `Cnb` (loss of directional stability above 33°), plus
`Clb Clp Clr Clda Cnr Cnda`. **That is what the F/A-18 HARV database concretely supplies here: the
shape of the post-linear regime in ten curves, and nothing below α 20°.**

### Build-order status (§11), gate by gate

| Step | State | Gate |
|---|---|---|
| 1 metrics / mass / ground | **done** | sits, does not bounce (0.13 mm over 18 s), holds on the brakes at 80 % RPM, taxis at 26 kt at 73 % |
| 2 propulsion | **done** | A2 within 0.8 %, A1 within 10 % on a first-cut polar |
| 3 drag polar | **done** for A1/A2, **A3 missed** | A1 +2.1 %, A2 +0.2 %, A3 +8.7 % |
| 4 lift and high-lift devices | **done** | B1/B2/B3/B10 all in band; B8 +29.8 % |
| 5 chute / speedbrake / gear drag | **done** | B9 +9.9 % with the chute CdA left at its derived 12 m² — it needed no calibration |
| 6 moments and control power | **done** | B3 in band, A5/A6 reached, rudder sized against the asymmetric-thrust case |
| 7 ARU gearing | **built, unmeasured** | §11 itself states this step has no documented anchor |
| 8 limits | **as specified** — the limiter is NOT in the deck (§7.3); the harness carries the SOS sketch and A5/A6 are measured through it | A5/A6 pass |
| 9 corner speed / roll rate | **measured** | corner 440 kt / 22.0 °/s at 5,000 m; roll 241 °/s at 450 kt — both declared model properties, neither has an anchor |
| 10 stores integration | **not started** — module-level | — |

**Promotion:** §11 puts `BETA` at "steps 1–6 pass". Steps 1–6 are built and four of their anchors are
missed (A3, A4, B8, B9), so the model stays **`ALPHA`**. Two of those four (A4, A3) are traced below to
the *thrust* side, which §5.4 step 4 predicted would be the diagnosable failure mode, and that is the
mechanism working rather than a surprise.

| Roadmap stage | What it will take from this file |
|---|---|
| **R3** — knowledge base (`doc/modules/mig29/`) | *running*: this file is the R3 deliverable for the flight model |
| **R6** — asymmetric weapons + RCS | §10 (weapons and stores) is the hook: store masses and drag areas become point masses and one external force, exactly as on the F-16 |
| **R7** — enemy units, MiG-29 at **BVR scale** | §8's anchors are the acceptance criteria that decide whether a BVR-scale opponent is good enough; turn-fight fidelity is explicitly **not** one of them |
| **R8** — JSBSim MiG-29 model | the primary consumer: §11's 10-step build order with its promotion gates, measured in the gym against §8 anchor by anchor, under the `MODEL-DELTAS` discipline |

**The measurement contract**, restated from the module file: every anchor of §8 (max speed by
altitude, sustained/instantaneous turn, climb rate, service ceiling, fuel flow) is measured in the
gym against the documented number and the deviation is stated. A failing knife-fight comparison is
not a defect of this model; a wrong envelope is.

---

## Gaps

**Source gaps** — §12 below is the file's own three-way split (permanently open · closable by the
GAF T.O. 1F-MIG29-1 · closable by measurement) and is left exactly as written, section number
intact. The governing acquisition note: **GAF T.O. 1F-MIG29-1** (German Air Force MiG-29G flight
manual, ~454 pp, English, USAF format) was located but **not available to this pass**; the whole
file is written so its arrival is an edit, not a rewrite (§1.1).

**Implementation gaps** — §12.5 below: the four missed anchors, each with what it hangs on and what it
would cost to close. §12.6: what the deck deliberately does not model.

### 12. Open points (checklist row 12) — split into the three required kinds

#### 12.1 What nobody knows publicly — permanent declared estimates

These will remain `[SET]` or `[ANALOGY]` **even if the German flight manual is obtained**, because a
flight manual contains performance and limits, not coefficient decks:

1. **The entire aerodynamic coefficient deck.** No MiG-29 wind-tunnel or flight-derived database is
   public at any tier. Everything in §6 is INV / GEO / ANA / SET.
2. **High-α behaviour above the limiter (α > 26°).** Departure boundaries, post-stall pitch and yaw
   moments, spin modes, recovery characteristics. **`[ANALOGY]` from NASA TM-110216, permanently.**
   The MiG-29's reputation rests on precisely this regime, so **the model's most famous property
   will be its least sourced one** — this must be said in the module header, not hidden.
3. **Dynamic (rate) derivatives** — `Clp`, `Cmq`, `Cnr`, `Cmadot`, `Cnp`, `Clr`. Estimable by
   standard methods (GEO) but never verifiable.
4. **The exact LERX contribution.** Area (4.71 m²) and sweep (73°30′) are documented; the vortex
   lift it produces, its burst behaviour with α and sideslip, and its interaction with the twin fins
   (the buffet that cracked Luftwaffe fins) are not.
5. **Inertia tensor.** §3.2's scaling is defensible and unverifiable.
6. **Wing airfoil section.** T4 only (NACA 64A-series, ~6 % biconvex).
7. **Static margin / CG limits.** Qualitatively "stable" `[ABL]`; quantitatively `[GAP]`.
8. **Engine lateral separation, cant, and vertical offset.** Measurable from a scaled 3-view, which
   is a *drawing*, not a source.
9. **Per-tank capacities and the fuel feed sequence** — hence CG travel through a sortie.
10. **Control-surface actuator rates and hinge-moment limits.**

#### 12.2 Where the sources contradict each other

| Item | Value A | Value B | Note |
|---|---|---|---|
| Vmax sea level | 1,500 km/h `DCS-FM p.14` | 1,200 km/h / M1.06 (T3 aerospaceweb) | 25 % apart — **the largest single conflict in the envelope** |
| Service ceiling | 18,000 m `DCS-FM p.14` | 17,000 m (T3 Jane's) / 18,500 m (T3 aerospaceweb) | 9 % spread |
| Max takeoff weight | 18,100 kg `DCS-FM p.14` | 18,500 kg (T3) | 2 % |
| Normal takeoff weight | 15,300 kg `DCS-FM p.14` | 15,240 kg (T3) | 0.4 % — negligible |
| Internal fuel | 3,200 kg (T3) | 3,375–3,500 kg (T4) | 10 % — affects range anchors A7/A8 |
| Internal tank count | 5 fuselage compartments named `DCS-FM p.9` | 4 fuselage + 2 wing (T3) | enumeration does not reconcile |
| RD-33 dry weight | 1,055 kg (T3) | 1,156 kg (T3) | probably bare vs. installed |
| Turbine inlet temp | 1,536 K | 1,530 K takeoff / 1,680 K in flight | both T3 |
| RD-33 SFC units | 0.77 / 2.05 kg/(kgf·h) | "7.5 / 20.1 kg/(kN·h)" | **the second is a factor-of-10 unit error**, §5.1 |
| R-27ER/ET on a 9-12 | `DCS-EA p.86` lists them | `DCS-FM p.68–69` assigns them to the Su-27 | `weapons.md` §3.1 |

#### 12.3 Where numbers look implausible

| Finding | Why suspicious |
|---|---|
| **Max climb rate 330 m/s** | Reads as a rate of climb; the §8.1 derivation shows it can only be **specific excess power near sea level**. Any model tested against it as a steady climb at altitude will be "wrong" for the right reason |
| **Takeoff run 250 m** | Very short for a 15.3 t fighter; §8.2's derivation says 284 m at normal weight with AB, so the 250 m figure is credible **only** at a lighter weight or with AB + headwind. It is almost certainly an AB figure at a demonstration weight, and should not be used as a MIL-power anchor |
| **Ferry 2,100 km (9-12) vs 2,900 km (9-13)** | 38 % apart for the same airframe and engines; explained *exactly* by the 9-13's wet inboard pylons (`weapons.md` §2.2), which is a satisfying resolution rather than a contradiction |
| **Wing area 38.056 m² quoted to 5 significant figures** | The precision implies a specific reference-area convention that no source states (§2.1). Treat the digits as a *convention marker*, not as accuracy |
| **"Rudder fins square 1.25 m²"** `DCS-FM p.10` | Almost certainly the **rudder** (control surface), not the fin; if read as total fin area the aircraft would be directionally hopeless. Recorded as an interpretation, `[ABL]` |
| **Landing roll 600 m with chute** vs **takeoff run 250 m** | A 2.4:1 ratio is unusual — most fighters land shorter relative to their takeoff run. Consistent with a 9 t-class landing weight and no thrust reverser, but flagged for re-check against B9 |

#### 12.4 Not investigated this pass

- **GAF T.O. 1F-MIG29-1** — the one document that would move a dozen `[GAP]`s to `[T1]`.
- A **scaled 3-view digitisation**, which would close `htailarm`, `vtailarm`, `AERORP`, `EYEPOINT`,
  engine separation, and station coordinates in a single afternoon. This is the cheapest remaining
  win and needs no new source, only care about the drawing's provenance and scale.
- The **N019 radar's** own parameters beyond detection range and gimbal limits — belongs to the
  parallel agent's sensor file.
- **JSBSim's own trim behaviour** for a twin-engine, statically-stable airframe. The F-16 model's
  `"udot doesn't appear to be trimmable"` failure above 580 KCAS (`doc/modules/f16/flight-model.md` §12.4)
  is a warning that the trim routine has envelope limits, and this airframe has not been tested
  against them.
- **Whether `Ixx` should include a wing-tank correction** for the 9-13 comparison case.

---

#### 12.5 The four missed anchors — what each one hangs on

**A4, Ps at sea level, M 0.90: 248 m/s against 330, −24.8 %. The clearest result in the build, and it
falls exactly where §5.4 step 4 said it would.** The residual is *provably not* absorbable by the drag
polar. At that condition the drag is ≈52 kN; to reach Ps = 330 m/s at 13,000 kg the thrust would have
to be 189 kN, i.e. an augmented-thrust factor of **1.16** at M 0.90 / sea level. The borrowed F100
surface gives **1.02**. Closing A4 by drag instead would need CD ≈ 0.0133, about 40 % below a clean
fighter's zero-lift drag and impossible to reconcile with A2, which is measured 0.2 % from its
documented value at the same altitude. **So the thrust analogy is the suspect, precisely as the rule
predicted, and the specific defect is named: the F100-PW-229's ram recovery at high subsonic speed near
sea level is ~12 % lower than the RD-33's would need to be.** Cost to close: a real RD-33 thrust deck,
or a T1 source giving installed thrust at one (M, h) point off the static line. Nothing else will do
it, and tuning the surface to hit A4 would destroy A2 and A1, which is the whole reason the surface was
frozen first.

**A3, service ceiling: 19,566 m against 18,000, +8.7 %.** Same family, opposite end. A1 and A3
constrain the same product T/D at 11 km and at ~19 km; drag coefficients depend on Mach, not altitude,
so once A1 is hit at 11 km there is no drag term left that can lower the ceiling without breaking A1.
The lever is the **`[DERIVED]` high-altitude thrust extension** — the 60,000 and 70,000 ft columns this
build added because the borrowed deck's own column was exactly zero (a wall, not a decay,
`doc/modules/f16/flight-model.md` §12.3). Those columns continue the surface by the **ISA density ratio**,
which is the standard textbook approximation and is known to be optimistic near an engine's altitude
limit, where component efficiencies fall faster than density. Multiplying both columns by ≈0.78 would
put the ceiling at 18,000 m. **That was not done**, because those columns would then be a number fitted
to an anchor wearing a `[DERIVED]` tag, and the honest version of this row is a stated miss with a
named mechanism. Cost to close: an RD-33 altitude-thrust curve, or a decision to re-tag the extension
as `[SET]` and fit it — a one-line change with a one-line justification, available whenever the owner
prefers the anchor to the derivation.
*Measured at 13,000 kg, on a constant-500 kt-CAS-then-M 1.6 climb. The documented figure carries no
weight and no schedule, which is itself part of the 9 % spread §12.2 records for this row.*

**B8, takeoff ground run: 324 m against 250, +29.8 %.** This one the spec had already called: §12.3
records 250 m as "very short for a 15.3 t fighter … almost certainly an AB figure at a demonstration
weight", and §8.2's own derivation from independent numbers gives **284 m**. The model is 14 % above
that derivation, and the difference is identifiable: §8.2 lumps all configuration drag into "mean drag
≈ 0.05 W", while the deck carries flaps and gear as separate, explicitly `[SET]` coefficients whose sum
(ΔCD 0.090) costs ~8 kN over the second half of the roll. Liftoff speed is measured at 146.1 kt, inside
its documented band, so the run length is a *drag* result and not a *lift* one — which is exactly the
diagnosis §11 step 4 prescribes for this pattern. Cost to close: a documented takeoff-configuration
drag index (GAF T.O. 1F-MIG29-1 would carry one), or a decision to treat 250 m as the anchor and fit
the two `[SET]` coefficients to it, which would then be fitted numbers rather than estimates.

**B9, landing roll with chute: 659 m against 600, +9.9 %.** The smallest miss and the least
interesting: the flown landing touches down at 145.5 kt against a documented ~140, and (145.5/140)² is
+8 % of kinetic energy on its own. The chute's `CdA` — the free parameter §11 step 5 says to calibrate
last — was **left at its derived 12 m² and needed no adjustment**, which is the useful finding here.
Cost to close: a landing controller in the harness that floats another second, i.e. an instrument
change, not a model change. Deliberately not done, because tuning the instrument until the model looks
right is how a measurement stops being one.

#### 12.6 What the built deck deliberately does not model

| Absent | Why, and where the decision is written |
|---|---|
| The SOS α limiter | §7.3: it is a stick *force*, it must be overridable, and putting it in the deck would hide it from `core/FBFlightMonitor`. The deck has **no limiting of any kind**; a full-stick pull tumbles to 180° of α, which is a true statement about a bare mechanically-signalled airframe. The harness carries a throwaway SOS *sketch* so A5/A6 can be measured at all, and `systems/FBFlightControl` carries the real one at module level (stage 2) |
| B4, the 108 kt intake-opening thrust step | §5.2 folds ram recovery into the thrust tables rather than modelling the ramp. A deliberate, documented miss |
| B19, the 200 km/h upper-intake limit | same decision |
| The centreline-tank speedbrake inhibit and g cap | stores state; belongs to the module (§10) |
| Speedbrake auto-retract on electrical failure | there is no electrical system in this deck |
| Drag-chute deployment as a pilot action | `systems/FBAirframeControls` has no chute channel yet, so the deck arms it on the documented *conditions* (WOW, gear down, below the documented 175 kt separation speed). Consequence, stated in the deck: **every rollout in this model is a chute rollout**, and the no-chute landing distance is not measurable until the client channel exists |
| Ammunition mass as a depleting load | §10; module-level, along with the stores point masses |
| Aeroelasticity / fin buffet | §6.8, `[GAP]`, and a real phenomenon on this airframe |

## Knowledge

The researched depth of this file is **distributed, not collected**: it lives in the
**derivation-path column** of every table in Spec (the INV / GEO / ANA / SET procedures of §6, the
`<turbine_engine>` derivation of §5, the inertia estimates of §3) and in the **declared F/A-18 HARV
high-α analogy** of §6.4. That is deliberate — a derivation is only useful next to the number it
produces. The sources those derivations draw on are listed here.

### Sources

- `doc/DCS MIG-29 Flight Manual EN.pdf` — Eagle Dynamics, 2018. Printed pages 9–14, 81, 110
  (= PDF pages 15–20, 87, 116).
- `doc/DCS MiG-29A Early Access Manual EN.pdf` — Eagle Dynamics, 2025. Pages 19, 55, 57–58, 60, 64, 75–80.
- **NASA TM-110216** — Strickland, Bundick, Messina, Hoffler, Carzoo, Yeager, Beissner,
  *Simulation model of the F/A-18 high angle-of-attack research vehicle utilized for the design of
  advanced control laws*, May 1996 — [NTRS 19960027892](https://ntrs.nasa.gov/citations/19960027892)
  (the declared high-α analogy, §6.4).
- [SirViper — MiG-29 'Fulcrum-A'](https://sirviper.com/index.php?page=fighters%2Fmig-29%2Fmig-29a) (T3, Jane's-derived: dimensions, weights, takeoff 250 m / landing 600 m, climb 19,800 m/min, g limits).
- [aerospaceweb.org — MiG-29 'Fulcrum'](https://aerospaceweb.org/aircraft/fighter/mig29/) (T3; the conflicting sea-level Vmax and ceiling figures in §12.2).
- [GlobalSecurity — MiG-29 specifications](https://www.globalsecurity.org/military/world/russia/mig-29-specs.htm) (T3).
- [ru.wikipedia — РД-33](https://ru.wikipedia.org/wiki/%D0%A0%D0%94-33) and [airbase.ru — Основные данные двигателя РД-33](https://www.airbase.ru/hangar/russia/mikoyan/mig/29/zlinek/Articles/engine.htm) (T3, RD-33 scalars; the SFC figures in kg/(kgf·h)).
- [The Aviationist — MiG-29 in close air combat](https://theaviationist.com/2015/04/08/mig-29-in-close-air-combat/) (T4, Luftwaffe-pilot-derived: 28 °/s instantaneous, fuel/radius figures).
- [Secret Projects forum — MiG-29 supermanoeuvrability thread](https://www.secretprojects.co.uk/threads/mig-29-supermanoeuvrability.46663/) (T4, SOS-3M limiter values and override force — the §7.3 figures).
- [Aviation Archives — German Air Force MiG-29 Flight Manual](http://aviationarchives.blogspot.com/2017/08/mig-29-flight-manual.html) and [Avialogs — GAF T.O. 1F-MIG29-1](https://www.avialogs.com/aircraft-m/mikoyan-gurevitch/item/56020-gaf-t-o-1f-mig29-1-flight-manual-mig-29) (the **unconsulted T1**, §1.1).
- Companion file: `doc/modules/mig29/weapons.md`. Structure template: `doc/modules/f16/flight-model.md` §11.
