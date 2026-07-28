# MiG-29A (9-12) — Weapons & Armament

**Sources** (cite the tag, never a bare number):

- **`DCS-FM p.NN`** = `doc/DCS MIG-29 Flight Manual EN.pdf` (Eagle Dynamics, *DCS: MiG-29 Fulcrum*
  Flight Manual, FC3-level, 2018, 116 pp). **`NN` is the PRINTED page number** (the one in the page
  footer) — this manual has six roman-numbered front-matter pages, so **printed = PDF − 6**.
  Pages distilled here: **9–14** (general design, TTD
  table), **63–79** (MiG-29 Weapons: A-A missiles, A-S weapons, rockets), **83–92** (weapons
  delivery), **110** (the manual's own source list — it is itself a distillation of Russian
  monograph literature, see below).
- **`DCS-EA p.NN`** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (Eagle Dynamics, *DCS: MiG-29
  Fulcrum* Early Access Flight Manual v.09, 2025, 115 pp). Pages distilled here: **11–13**
  (PSR-31 weapon control panel), **59–60** (PU-S31 weapon control panel, external-stores selector,
  drag chute), **63** (PUR-31 radar panel), **86–105** (Combat Employment: A-A modes, gun, A-G).
  This is the **9-12 clickable-cockpit** manual — the primary source for *employment logic*.
- Researched material is confined to **§7 Technical depth**, tiered:
  **T1** official/declassified military documents · **T2** manufacturer datasheets ·
  **T3** established literature/databases (Jane's-derived, GlobalSecurity, weaponsystems.net) ·
  **T4** community/wiki, cross-check only, always flagged.

**Other tags:** `[DER]` = computed from stated inputs by a stated formula, not itself a cited fact ·
`[ABL]` = inference from two cited facts, marked as inference · `[SET]` = a FlightBox setting with
no source · `[GAP]` = not known publicly, not guessed · `[ED-MODEL]` = a statement that is more
plausibly an Eagle Dynamics modelling decision than a documented aircraft property.

> **Source caveat that applies to this whole file.** `DCS-FM p.110` names its own sources: the
> monograph *Aviation and Time* №5/2001, S. Moroz *Frontline fighter MiG-29* (Exprint, 2004),
> Pavlov/Voylokov 2009, Markovskiy in *M-Hobby* №2(24)/2000, plus airwar.ru. So `DCS-FM` is a
> **secondary distillation of Russian open literature**, not a flight manual in the T.O. sense. It
> is treated here as a good T3, not as T1. The T1 document that would settle most open numbers —
> **GAF T.O. 1F-MIG29-1** (German Air Force MiG-29G/GT Flight Manual, 30 Sep 1994, rev. 20 Sep
> 2001, ~454 pp, English, USAF format) — exists and is exactly what `DCS-EA` reads like
> (knots/feet, TLP telelight panel, "German manual" mentioned at `DCS-EA p.40`), but was **not
> available to this pass**. Every `[GAP]` below is a candidate to close from it.

---

## Spec

### 1. Inventory — what the 9-12 carries

| Class | Weapons | Source |
|---|---|---|
| Gun | 1 × **GSh-301** (GRAU **9A-4071K**) 30 mm, **150 rounds**, **1,500 rd/min**, in the port LERX/fuselage side ahead of the cockpit | `DCS-FM p.64`, `DCS-EA p.86` |
| A-A medium (SARH) | **R-27R** (+ **R-27ER** per `DCS-EA p.86`, see §3.1 caveat) | `DCS-EA p.86` |
| A-A medium (IR) | **R-27T** (+ **R-27ET** per `DCS-EA p.86`) | `DCS-EA p.86` |
| A-A short (IR) | **R-73**, **R-60**, **R-60M** | `DCS-EA p.86` |
| Bombs GP | FAB-100 / **FAB-250** / **FAB-500** (FAB-1500 named in the family text but not as a 9-12 store) | `DCS-FM p.75` |
| Bombs penetrator | BetAB-500ShP (retarded + rocket-boosted) | `DCS-FM p.75–76` |
| Bombs cluster | **RBK-250**, **RBK-500** | `DCS-FM p.77` |
| Dispenser | **KMGU-2** (8 × BKF cartridges) | `DCS-FM p.77` |
| Rockets | **S-5** (UB-32 class), **S-8** (B-8M1, 20 rds), **S-24** | `DCS-EA p.98`, `DCS-FM p.78–79` |
| Fuel | 1 × **PTB-1500** centreline (1,500 L) — ferry only | §2.2 |

**Standard air-defence loadout: 2 × R-27R + 4 × R-73** `DCS-FM p.64`.
**Guided missiles are carried on six wing hardpoints** `DCS-EA p.86`; the ground-attack role is
explicitly **secondary** `DCS-FM p.64`.

**Not carried by the 9-12:** any guided A-G weapon. *"Originally, MiG-29А and MiG-29С can use only
unguided bombs and rockets"* `DCS-FM p.74`. There is no laser/TV/ARM store in this airframe's
inventory — the laser in the nose (KOLS) is a **rangefinder for the sight**, not a designator
(`DCS-EA p.99, p.101`: the LRF auto-starts at dive angles > 10° inside a range gate).

---

### 2. Stations, racks and the release path

#### 2.1 Station map

| FlightBox index | Position | Documented stores | Source / confidence |
|---|---|---|---|
| 1 / 6 | wing **outboard** (one per wing) | R-60M, R-73 | T3 (Jane's-derived: *"four R-60/R-73 short-range AAMs outboard"*) |
| 2 / 5 | wing **middle** | R-60M, R-73; rockets/bombs per T3 "unguided bombs or rockets" on outer group | T3/T4 — **contested**, see below |
| 3 / 4 | wing **inboard** (nearest fuselage) | **R-27R/T** (APU-470 rail), FAB-500/RBK-500 class, B-8M1, S-24, and — **on the 9-13 series 351 onward only** — the PTB-1150 wing tank | T3 |
| C | fuselage **centreline** | **PTB-1500** fuel tank, **ferry only** | T3 |

- **Six underwing pylons + one centreline station** is certain and agreed by every source
  (`DCS-FM p.64` "6 different external stations"; `DCS-EA p.86` "six wing hardpoints").
- **`[GAP]` The official station NUMBERING is not established by any source consulted.** The 1…6
  ordering above is a FlightBox convention (`[SET]`, left-outboard → right-outboard) chosen to match
  `modules/f16/FBF16Sms`'s left-to-right convention. Do not present it as a MiG-29 fact.
- **`[GAP]` Per-station structural weight limits are not published.** T3 gives only the airframe
  total: **max ordnance 3,000 kg** (Jane's-derived; one T4 source says 3,500 kg). The
  F-16 file's warning applies verbatim: real carriage clearance is per store+rack combination,
  not a single kg number per station.
- **`[T4]` Early aircraft could not fire the gun with the centreline tank fitted** — the tank blocks
  the case/link ejection port. Relevant to FlightBox only as a `hardware_precedence` rejection
  reason if ever modelled; **not** confirmed for the 9-12 by a T1–T3 source.

#### 2.2 Fuel-tank carriage — a real 9-12 restriction

| Variant | Centreline PTB-1500 | Wing PTB-1150 | Ferry range |
|---|---|---|---|
| **9-12** | yes | **no** (inboard pylons not wet) | **2,100 km** `DCS-FM p.14` |
| 9-13 (series 351+) | yes | yes (2×) | **2,900 km** `DCS-FM p.14` |

`[ABL]` The 800 km ferry-range delta between the two variants in `DCS-FM p.14` is exactly explained
by the T3/T4 statement that the wet inboard pylons arrived with the 9-13 series 351 — two
independent sources agreeing on a *difference* is stronger than either alone. **A 9-12 loadout in a
`.fbm` mission must not offer wing tanks.**

`[DCS-EA p.57]`: **speedbrake operation is inhibited with the centreline tank installed**, and with
gear down. That is a carriage-dependent *flight-control* interlock — model it in the airframe
controls layer, not in the stores layer.

#### 2.3 Racks / launchers

| Store | Launcher | Source |
|---|---|---|
| R-27R/T | **APU-470** rail (same rail for the standard and extended bodies) | `DCS-FM p.67` ("the same rail and ejector launchers are used for both size variants"), T4 for the designation |
| R-73 | **P-72 / P-72D**, i.e. **APU-73-1 / APU-73-1D** | `DCS-FM p.72` (verbatim) |
| R-60M | APU-60-1(M) | T4 |
| S-24 | APU-68 | T4 |
| S-8 | **B-8M1** pod, **20 rounds** | `DCS-FM p.78`, `DCS-EA p.98` |
| KMGU-2 | BDZ-U beam rack, singly | `DCS-FM p.77` |

#### 2.4 The release path — pilot controls that gate every shot

This is the part FlightBox has to reproduce faithfully, because **the MiG-29's store selection is
structurally different from the F-16's**: there is no per-station stepping and no SMS inventory page.

| Control | Panel | Positions | Effect | Source |
|---|---|---|---|---|
| **MASTER ARM** | PSR-31 (1) | ARM / SAVE | connects the stick's combat triggers to the weapon circuits | `DCS-EA p.12` |
| **Release quantity** | PSR-31 (5) | **"ALL"** / **"SINGLE 0.5 ALL"** | ALL = **paired** launch per trigger press; SINGLE = **single** launch per trigger press | `DCS-EA p.12` |
| **PREPARE** | PSR-31 (4) | MAN / **AUTO** | missile launch-preparation mode; manual if the WCS computer cannot resolve target range | `DCS-EA p.12` |
| **SPAN** | PSR-31 (6) | metres + S/MED/L | **target wingspan** fed to the sight for *indirect* range computation, and to the R-27R's programming. S = cruise missiles, MED = MiG-21/F-15/F-16 class, L = Tu-16/F-111/SR-71/B-1 class | `DCS-EA p.12` |
| **WCS MODES** | PSR-31 (7) | TOSS / NVG / RAD / IR / CC / HELM / OPT / BS | the master sighting-mode rotary | `DCS-EA p.12–13` |
| **A/A – A/G** | PU-S31 (2) | AIR / GROUND | deployment mode for **cannon and missiles both** | `DCS-EA p.59` |
| **COOP** | PU-S31 (1) | — | dual function: (a) radar + IR homing in cooperative mode, (b) **retarded vs. non-retarded** bomb release configuration | `DCS-EA p.59` |
| **Jettison explosion** | PU-S31 (3) | ARMED / SAFE | whether jettisoned stores are live | `DCS-EA p.59` |
| **Emergency bomb release** | PU-S31 (4) | guarded button | — | `DCS-EA p.59` |
| **LOCK** | PU-S31 (6) | FOE / FRIEND | whether the radar may lock a contact the IFF reports as friendly | `DCS-EA p.59` |
| **External stores selector** | left console | **left = INNER pair**, **right = OUTER pair** | selects "the exact pair of stores (pylons) for the current attack" | `DCS-EA p.60`; keybind `[RAlt]+[P]` at `DCS-EA p.87` |
| **Emergency missile launch** | left console | button | bypass path | `DCS-EA p.53` |

**Rebuild takeaway (the architectural difference).** The F-16 resolves *which store* through an SMS
inventory + MSL STEP; the MiG-29 resolves it through **two coarse switches**: an
**inner-pair/outer-pair** selector and a **pair/single** quantity switch. There is no notion of a
per-station step, and the natural release granularity is a **symmetric pair**. Mapping onto
`weapons/FBStoresSystem`: the F-16 semantics ("release one store from a named station") are the
*special case* here, reached only with the quantity switch in SINGLE; the default is "release the
selected pair". `FBStoresSystem::Release` therefore needs a pair-release path for this module, or
the module has to emit two `FBStoreRelease` records in the same tick — the latter is preferable
because it keeps the core's one-store-one-unit invariant intact `[SET]`.

---

### 3. Air-to-air missiles

#### 3.1 R-27 family (AA-10 Alamo)

**Family-level facts** — these are the launch *constraints* and matter more for a rebuild than the
headline ranges `DCS-FM p.67–68`:

| Parameter | Value | Source |
|---|---|---|
| Seeker gimbal limit at launch | **50°** (SARH) / **55°** (IR) | `DCS-FM p.67` |
| **Max launch-aircraft g** | **5 g** | `DCS-FM p.67` |
| Max target speed | 3,500 km/h (3,600 km/h quoted for the R version) | `DCS-FM p.67, p.68` |
| Target altitude band | **20 m … 27 km** | `DCS-FM p.68` |
| Max shooter/target altitude difference | **10 km** | `DCS-FM p.68` |
| Max target g | **8** | `DCS-FM p.68` |
| Guidance architecture | INS + radio correction (mid-course) → seeker terminal | `DCS-FM p.67` |
| Body material | titanium alloy airframe, steel motor case | `DCS-FM p.67` |
| Service entry | 1987–1990 | `DCS-FM p.67` |

**Per-variant** (`DCS-FM p.68–69` unless noted). The `DCS-FM` range column and the T3 export-brochure
range column **disagree by roughly a factor of two** and are kept separate on purpose:

| Variant | Seeker | Launch mass | Length | Ø | Wing span | Fin span | Warhead | Range `DCS-FM` | Range T3 (brochure) |
|---|---|---|---|---|---|---|---|---|---|
| **R-27R** ("470R", AA-10A) | SARH | **253 kg** | 4.00 m | 0.23 m | 0.77 m | 0.97 m | **39 kg** expanding-rod | **30–35 km** | 60 km |
| **R-27T** ("470T", AA-10B) | IR | **254 kg** (T3: 245) | 3.70 m | 0.23 m | 0.80 m | — | 39 kg | **30 km**, targets to 24 km alt | 50 km |
| R-27ER ("470ER", AA-10C) | SARH | 350 kg | 4.78 m | 0.26 m | 0.80 m | 0.97 m | 39 kg | **66 km** | 95 km |
| R-27ET ("470ET", AA-10D) | IR | 343 kg | 4.50 m | 0.26 m | 0.80 m | — | 39 kg | **60 km** (requires IR lock before launch) | 90 km |

- **All four share the same 39 kg expanding-rod warhead**, radar-proximity + impact fuzed
  (`DCS-FM p.68`; T3 corroborates the 39 kg rod warhead across the family — high confidence).
- **Aspect dependence, quoted for the ER as the family's worked example** `DCS-FM p.66`:
  max forward-hemisphere range **66 km at 10,000 m**, **28 km at 1,000 m**, and
  **rear-hemisphere 10 km at 1,000 m**. `[ABL]` That is a **2.4× altitude factor** and a further
  **2.8× aspect factor** — i.e. the worst case is ~1/6.6 of the headline number. Any DLZ model that
  is not altitude- and aspect-scheduled will be wrong by most of its range.
- `DCS-FM p.65` states the general rule: *"rear hemisphere, low aspect, launch ranges are usually
  two to three times less than high aspect"*, and *"near ground level … the launch range is more
  than halved"*.
- **`[GAP]` Motor burn times are not given per variant.** `DCS-FM p.64, p.65` only bounds the class:
  solid motor burning **2…15 s**, boosting to **Mach 2–3**, and the missile becomes *"almost
  uncontrollable"* below **800–1,000 km/h**. That last figure is the most rebuild-useful of the
  three — it is a **terminal-controllability floor**, directly usable as the `FBMissileGuidance`
  give-up condition `[SET-from-DOC]`.
- **`[T4], contested`** Seeker designations. The R-27R's SARH head is commonly cited as
  **9B-1101K** (Avtomatika); one T3-ish source instead names **9B-1032 / PRGS-27**. Unresolved —
  do not put either into a header comment as fact.
- **9-12 carriage of the E-variants** is asserted by `DCS-EA p.86` (which lists R-27ER and R-27ET
  among "the MiG-29's missile armament"), while `DCS-FM p.68–69` says of both E-variants *"The Su-27
  and its variants can be equipped with this missile"* — i.e. the two ED manuals contradict each
  other on exactly this point. `[ED-MODEL]` Treat R-27ER/ET on a 9-12 as an ED module decision, not
  as an established 9-12 capability; the historically safe 9-12 medium-range fit is **R-27R / R-27T**.

#### 3.2 The SARH support obligation — the R's defining tactical property

This is the single most important behavioural difference between this airframe's BVR weapon and the
F-16's AIM-120, and FlightBox's `pilot/FBEngagement` state machine is built around the *opposite*
assumption. Stated with its source, then its consequences:

> *"Semi-active missiles home in on the reflected radar energy of a target. For such missiles, it is
> necessary that the supporting aircraft **retain radar lock until the missile hits the target**.
> This can often lead to 'jousting' of SARH armed aircraft."* — `DCS-FM p.65`
>
> *"Active missiles at long ranges have the same features as semi-active systems … Once the missile
> is within **10 to 20 km** of the target, the onboard radar seeker activates and continues the
> intercept **without need of support** from the launch aircraft's radar."* — `DCS-FM p.65`

**Consequences, each labelled by how it follows:**

| # | Consequence | Basis |
|---|---|---|
| 1 | **Support time = whole time of flight**, not "flight time minus pitbull range". | `DCS-FM p.65`, direct |
| 2 | **`[DER]` Worked support time.** Head-on shot at 25 km, own 900 km/h (250 m/s), target 900 km/h; missile average ground speed taken as 700 m/s over the coast phase. Closing rate missile↔target = 700 + 250 = 950 m/s → **t ≈ 26 s of unbroken STT illumination**. For the same 25 km shot the AIM-120 comparison in `doc/modules/f16/weapons.md` ends the shooter's obligation at the 10–20 km activation ring, i.e. after ~5–15 s. | `[DER]` from `DCS-FM p.65`'s 800–1,000 km/h floor and Mach 2–3 boost; **assumptions stated, not measured** |
| 3 | **Cranking is bounded by the antenna, not by tactics.** The N019's azimuth deviation is **67°** (`DCS-FM p.12`) and STT must stay inside it, so the crank angle is a hard geometric ceiling — unlike the F-16 module's `kCrankDeg = 45°` (gimbal 60° minus reserve), the MiG-29's reserve has to be taken off **67°** and the aircraft may not turn cold **at all** while supporting. | `[ABL]` from `DCS-FM p.12` + consequence 1 |
| 4 | **No defensive break while supporting.** `pilot/FBEngagement`'s `Defend` state (turn beam-on, put own radial velocity in the opponent's Doppler notch) is *mutually exclusive* with supporting an R-27R — beaming breaks the illumination the missile lives on. The F-16 can crank-and-defend; this jet cannot. | `[ABL]` |
| 5 | **The lock IS the warning.** The shooter is in continuous STT from before launch to impact, so the target's RWR has the full ~26 s to react. `DCS-FM p.65`'s "jousting" is the emergent behaviour: both sides illuminate and neither may turn away first. | `[ABL]` |
| 6 | **The IR members of the family are the answer to (1)–(5).** R-27T/ET need only pre-launch IR lock (`DCS-FM p.69`), then are fire-and-forget, and `DCS-FM p.68` says explicitly: *"the combined launch of R-27 missiles with different seeker variants increases the resistance to target counter measures."* A mixed R+T load is a doctrine, not a preference. | `DCS-FM p.68–69` |

**Rebuild note.** `pilot/FBEngagement`'s `Support` state currently holds the lock "until the
predicted time of flight expires" and cranks to a hook angle — the mechanism is already right; what
changes for this module is (a) the hold duration equals the *full* predicted TOF for a SARH round,
(b) `Defend` must be inhibited (or must be an explicit *abandon-the-shot* decision with the missile
going stupid), and (c) the crank hook is derived from 67°, not 60°. None of that needs new
architecture — it is three module-level hook values plus one state-transition guard `[SET]`.

#### 3.3 R-73 (AA-11 Archer)

| Parameter | Value | Source |
|---|---|---|
| Launch mass | **105 kg** | `DCS-FM p.72` |
| Length / Ø | **2.90 m** / **0.17 m** | `DCS-FM p.72` |
| Wing span / control-surface span | 0.51 m / 0.38 m | `DCS-FM p.72` |
| Warhead | **7.4 kg** expanding-rod, **blast radius ≈ 3.5 m** | `DCS-FM p.72`, `p.72` (radius) |
| Fuze | active radio proximity | `DCS-FM p.72` |
| Motor | single-mode solid propellant | `DCS-FM p.72` |
| Control | **aerodynamic surfaces + gas-dynamic vanes** (thrust vectoring), ailerons + exhaust vanes driven by a solid gas generator | `DCS-FM p.72` |
| Autopilot inputs | **α and β sensors mounted ahead of the destabilisers** — the "pine cone" nose section | `DCS-FM p.72` |
| Range | **0.3 … 20 km**; **30 km forward hemisphere at high altitude** | `DCS-FM p.72` |
| Max target altitude | 20 km | `DCS-FM p.72` |
| Max target speed | 2,500 km/h | `DCS-FM p.72` |
| **Max target g** | **12** | `DCS-FM p.72` |
| Seeker | **"Mayak" OGS MK-80** (Arsenal, Kiev); acquisition to **60°**, later **gimbal limits raised to 75°**, **max seeker slew rate 60 °/s** | `DCS-FM p.71` |
| Seeker CCM | pulse-time signal modulation + multi-channel digital processing; extended photodetector sensitivity band | `DCS-FM p.71` |
| Aimpoint bias | steers to a point **forward of the target's nozzles** — deliberately into the fuselage/cockpit, not the exhaust | `DCS-FM p.71` |
| Launcher | P-72 / P-72D (APU-73-1 / -1D) | `DCS-FM p.72` |
| Missile airframe g | T4: >60 g manoeuvre capability | T4, cross-check only |

**Helmet-sight coupling (Shchel-3UM)** — `DCS-EA p.92`, and this is the number that matters:

| Parameter | Value |
|---|---|
| HMS cueing envelope | **±60° azimuth, +60° … −14° elevation** |
| Handoff | pressing LOCKON slaves the **missile IR seeker directly from the HMD**, *regardless* of whether radar or KOLS has the target |
| If radar/KOLS locks first | "ТП capture" on the HMD, HUD reverts to the radar/KOLS picture, and on **release** of LOCKON the missile designation comes from RLPK/KOLS instead |
| **R-73 release rule** | release LOCKON, then shoot judging range by eye — *"the target is no longer tracked via the HMD"*. It **may** be fired with LOCKON still held, but then the target must be tracked with the head. |
| **R-60M release rule** | **hold** LOCKON, shoot judging range by eye, keep tracking via HMD |

`[ABL]` The R-73's ability to be *released from the HMD* while the R-60M must be *held on it* is the
practical expression of the two seekers' acquisition angles (75° vs ~20°, §3.4): the Archer can be
handed a target far enough off-boresight that it will find it alone; the Aphid cannot.

`[ABL]` **The 9-12's high-off-boresight capability is bounded by the HMS (±60° az) rather than by
the missile (75° gimbal).** For FlightBox that means the *cueing* limit, not the seeker limit, is
the module hook that decides whether a shot is offered.

#### 3.4 R-60M (AA-8 Aphid)

| Parameter | Value | Source |
|---|---|---|
| Layout | canard ("duck"), destabilisers ahead of the seeker, kinematically paired control surfaces | `DCS-FM p.72` |
| Seeker | **"Komar-M" OGS-75 / TGS-75**, nitrogen-cooled | `DCS-FM p.72`, T3/T4 |
| Off-boresight | **T4, contested**: 17° (vs 12° on the base R-60) *or* ±20° | T4 |
| Warhead | **3.5 kg** (warhead bay lengthened by 42 mm vs. R-60) | `DCS-FM p.72` |
| **Fuze** | optical, operating range **≈ 5 m** | `DCS-FM p.73` |
| Motor | solid, **time-varying thrust with a strong initial impulse**, burn **3–5 s** | `DCS-FM p.73` |
| Launch mass | **≈ 43.5 kg** | T3/T4 (not in `DCS-FM`) |
| Range | ≈ 8 km; all-aspect capability ≈ 2 km | T3/T4 |
| Wings | large-sweep trapezoidal, mounted on the motor section, **rollerons** on the trailing edges | `DCS-FM p.73` |

`[ABL]` A **5 m optical fuze radius with a 3.5 kg warhead** is the smallest lethal volume in this
inventory by a wide margin — in `core/FBDamageModel` terms, the isotropic fragment areal density
`m/(4πr²)` at the fuze radius is what decides whether an R-60M hit degrades or kills, and with
`kCaseFraction` 0.5 that is 1.75 kg over 4π·25 m² ≈ **5.6 g/m²** versus the R-73's 3.7 kg over
4π·12.25 m² ≈ **24 g/m²** `[DER]`. **≈4× the areal density at burst** — the R-60M should routinely
produce degradation where the R-73 produces a kill, and that falls out of the existing damage model
without a single new constant.

#### 3.5 Launch-envelope symbology — what the DLZ actually shows

`DCS-EA p.88–89` (HUD lock-mode elements) + `DCS-FM p.66` (the concept behind them):

| HUD label | Meaning | Basis |
|---|---|---|
| **Dr max1** | maximum launch range against a **non-manoeuvring** target | `[ABL]` from `DCS-EA p.88` label + `DCS-FM p.66` concept |
| **Dr max2** | maximum launch range **accounting for target manoeuvre** — the Russian *Rpi*, western *Rtr* | `[ABL]`, same two sources: *"a different gauge of maximum range — maximum launch range that takes into account target maneuverability (Rpi in western terminology) … much shorter range but ensures a much higher probability of kill"* `DCS-FM p.66` |
| **Dr min** | minimum launch range | `DCS-EA p.88` |
| **"ПР"** | *Пуск Разрешён* — **launch authorised** | `DCS-EA p.89, 91, 92` |
| **"А"** | attack / target captured | `DCS-EA p.88, 93` |
| **"Г"** (Gorka) | commanded climb to reach the target's altitude band | `DCS-EA p.88–89` |
| **"ОТВ"** | *отворот* — **break off**, exit the attack in the direction the aiming ring moves | `DCS-EA p.89` |
| Aiming ring + fixed crosshair | steering: fly to put the ring on the crosshair | `DCS-EA p.89` |
| Target aspect | displayed element | `DCS-EA p.88` |

**Range-scale auto-switching:** 54 → 27 → 13.5 → 5.40 nm as range decreases `DCS-EA p.89`;
gun/close-in scale changes again at ~1 km `DCS-EA p.96`.

**Rebuild mapping.** `Dr max1` / `Dr max2` / `Dr min` map **1:1** onto `FireControl`'s
`Raero` / `Rtr` / `Rmin` (`modules/f16/FBF16FireControl`) — the block already carries exactly these
three, so this jet needs no new FBState field, only its own forward-integration of the R-27's
performance table `[SET]`.

#### 3.6 Radar/IR mode → weapon availability

| WCS mode | Sensor | Detection range | Notes | Source |
|---|---|---|---|---|
| **RAD** (search/TWS/lock) | N019 | 70 km FH / 35 km RH; 10 targets tracked; antenna ±67° az, +60/−38° el | in close combat the antenna moves **in the vertical axis only** | `DCS-FM p.12` |
| RAD **CLOSE CMBT** | N019 vertical bar | **5.4 nm … 1,500 ft** | stable auto-track at equal speeds and in lag → supports manoeuvring combat; lock command must be held ≤ 2 s | `DCS-EA p.89` |
| **IR** (KOLS) | opto-electronic + laser | **13.5 … 5.4 nm**; with thermal countermeasures **5.4 … 1.6 nm** | *"turn on IR mode after takeoff"*; used to approach **without emitting** | `DCS-EA p.91` |
| IR **CC** | KOLS | as RAD CC | attack in rear hemisphere at aspect up to 3/4 | `DCS-EA p.92` |
| **HELM** | HMS | see §3.3 | ±60° az, +60/−14° el | `DCS-EA p.92` |
| **OPT** | manual designation | — | designation handed to radar, KOLS **and** missile seeker from the control button | `DCS-EA p.93` |
| **BS** (boresight) | missile seeker only | — | **degraded mode** for when the WCS fails; fixed crosshair; **no capture indication on the HUD**, only the voice message "Launch permitted" plus an audio tone for R-60M/R-73 | `DCS-EA p.94` |
| **RETICLE** | fixed grid | — | not a combat mode; a calibrated fallback image | `DCS-EA p.94`, `DCS-FM p.61` |

**Radar tracking floor** `DCS-EA p.87` — a genuine Doppler-notch specification, and the one place
this manual gives the number FlightBox's `sensors/FBRadarSystem` already models abstractly:

| Condition | Minimum closure/separation rate for track |
|---|---|
| Range **> 8 nm** | **> 81 kts** |
| Range **< 8 nm** | **> 27 kts** |
| Head-on, range < 5.4 nm, closure < 32.4 kts | *"detection is not guaranteed"* |

`[ABL]` This is the MiG-29's own **`kDopplerNotchMs`** — 81 kts = **41.7 m/s**, 27 kts = **13.9 m/s**.
`FBRadarSystem`'s notch constant for this module is therefore **range-scheduled**, not a single
number, which the current generic system does not express. That is a real (small) extension, and it
is documented rather than invented.

---

### 4. GSh-301 30 mm cannon

Built to be read next to `doc/modules/f16/weapons.md` §4.1 (M61A1) — same rows, so the two guns are
comparable at a glance.

#### 4.1 Documented parameters

| Parameter | GSh-301 (9A-4071K) | Source / tier |
|---|---|---|
| Calibre / cartridge | **30 × 165 mm** | T3 (weaponsystems.net) |
| Type | **single-barrel**, short-recoil / gas operated, water-evaporation cooled | `DCS-FM p.64` ("single-barreled"), T3/T4 for the operating principle |
| Installation | port side of the nose/LERX section, **built into the airframe** ahead of the cockpit | `DCS-FM p.64`, `DCS-EA p.86` |
| **Ammunition capacity** | **150 rounds** | `DCS-FM p.64`, `DCS-EA p.86` |
| **Rate of fire** | **1,500 rd/min** | `DCS-FM p.64`, `DCS-EA p.86`; T3 gives the hardware range **1,500–1,800 rd/min** |
| **Muzzle velocity** | **860 m/s** | T3 (weaponsystems.net); **T4 says 900 m/s** — contested, use 860 and flag |
| Projectile mass | **390 g** | T4 (multiple, consistent) |
| Complete round mass | ≈ 832 g | T4 — **weak**, wanted for ammo-mass modelling |
| Gun mass (no ammo) | 46 kg | T3 |
| Gun length | 1.978 m | T3 |
| **Barrel life** | **≈ 2,000 rounds** | T3 — i.e. ~13 full magazines; a real maintenance property, not a sim parameter |
| Effective range, air | **200 … 800 m** | T3 |
| Effective range, ground | 1,200 … 1,800 m | T3 |
| Dispersion | **`[GAP]`** — no T1–T3 figure found | — |

#### 4.2 The manual's own firing-range numbers — an independent cross-check

`DCS-EA p.97`, gun employment against an air target:

| Statement | Metric |
|---|---|
| *"Aimed shooting is provided at ranges of **4000…660 ft**"* | 1,220 … 200 m |
| *"Effective firing range **2600…660 ft**"* | **790 … 200 m** |
| HUD switches to the gun close-in picture at | **≤ 0.65 nm (1,200 m)** |
| Range scale changes again at | ≈ 1 km |

`[ABL]` The manual's **790–200 m effective band and T3's 800–200 m air-target band agree to within
1 %** — two independent sources, one Russian-literature-derived and one German-manual-derived,
landing on the same envelope. This is the highest-confidence gun number in the file, and it is
exactly the interval `core/FBGunBallistics` must reproduce.

#### 4.3 Ballistic calibration anchor — the A-G correction table

`DCS-EA p.100` gives, for **RETICLE (fixed-grid) mode**, the *angular correction in the aircraft's
plane of symmetry* for each unguided weapon at a stated condition. It is the only quantitative
ballistics table in either manual:

| Weapon | Firing range | Speed | Dive angle | Angular correction |
|---|---|---|---|---|
| S-24 | 0.97 nm (1,800 m) | 432 kts (222 m/s) | 20° | **60** thousandths |
| S-8 | 0.86 nm (1,590 m) | 432 kts | 20° | **43** |
| S-5 | 0.81 nm (1,500 m) | 432 kts | 20° | **54** |
| **Gun** | 0.81 nm (1,500 m) | 432 kts | 20° | **14** |

`[DER]` **Drag-free sanity check on the gun row.** Launch speed = 860 + 222 = 1,082 m/s along the
bore; ignoring drag, `t = 1500/1082 = 1.39 s`, gravity drop `= ½·9.81·1.39² = 9.5 m`, angular
`= 9.5/1500 = 6.3 mrad = 6.0` Russian thousandths (1 тысячная = 1/6000 turn ≈ 1.047 mrad). The table
says **14**. The factor ~2.3 gap is expected and instructive: the real correction also carries
**projectile drag** (which lengthens the time of flight superlinearly at 30 mm/390 g) and the
**boresight/reticle datum offset**. **Use this row as an acceptance target for
`core/FBGunBallistics`, not as an input**: a correct drag model fired from these exact conditions
must land near 14, and if it lands near 6 the drag retardation is missing.

#### 4.4 Derived firing model (for `core/FBGun.h` / `FBGunBallistics`)

All `[DER]` from §4.1, formulas stated:

| Quantity | Formula | GSh-301 | M61A1 (from `doc/modules/f16/weapons.md`) |
|---|---|---|---|
| Rounds per second | rate/60 | **25.0 rd/s** | 100 rd/s |
| Rounds per 0.1 s tick | rate/600 | **2.5** | 10 |
| Continuous fire time | drum / rate | **6.0 s** | 5.1 s |
| Muzzle KE per round | ½·m·v² | ½·0.39·860² = **144.2 kJ** | ½·0.10·1030² ≈ 53 kJ |
| Muzzle power (energy flux) | KE · rate | **3.61 MJ/s** | ≈ 5.3 MJ/s |
| Momentum flux (recoil) | m·v·rate | 0.39·860·25 = **8.39 kN** | 0.10·1030·100 ≈ 10.3 kN |
| Total impulse of a full drum | m·v·N | 0.39·860·150 = **50.3 kN·s** | — |

`[ABL]` **The comparison FlightBox actually needs:** per-round the Soviet gun carries **2.7× the
energy**, but delivers **0.68× the energy flux** and has **6.0 s of trigger time against the F-16's
5.1 s**. Against `core/FBDamageModel`'s kinetic path (which sums areal energy density per zone), the
GSh-301's damage profile is **fewer, heavier impacts concentrated in a smaller footprint** — the
same total energy needs a *tighter* dispersion cone to be delivered, which is precisely why the
documented effective range (800 m) is *shorter* than the F-16's typical gun-envelope numbers despite
the higher muzzle energy. Recoil at 8.4 kN on a 15,300 kg aircraft is **0.056 g of deceleration**
`[DER]` — small, but it is the same order as the F-16's and is a real trim disturbance.

`[SET]` **Dispersion, pending a source.** Bound it from the documented envelope instead of guessing
freely: at the 800 m effective limit a fighter-sized target presents ~10 m of span, so keeping the
bulk of a burst on it requires a full cone ≤ 12.5 mrad, i.e. a **half-angle ≤ 6 mrad**. Declare
`kDispersionHalfMrad = 6.0 [SET]` in the module header **with this derivation written next to it**,
and mark it as the first number to replace when GAF T.O. 1F-MIG29-1 becomes available.

#### 4.5 Gun sight modes

| Method | Condition | Procedure | Source |
|---|---|---|---|
| **"Asynchronous shooting"** | radar or KOLS auto-track available | set SPAN; at ≤ 0.65 nm align the movable cross with the target, then with the ring; fire | `DCS-EA p.95–96` |
| Asynchronous, **invisible target** (cloud/night) | radar track, no visual | rough aim (ring on fixed crosshair) → precise aim (dot into ring) → fire | `DCS-EA p.96–97` |
| **"Gun funnel"** | **no** radar/KOLS lock, visible target | set target base in metres; fly the target into the funnel contour; fire. *"Best accuracy … by reducing the target's angular movement on the HUD"* | `DCS-EA p.97` |
| **RETICLE** | all sighting modes failed | estimate lead from the fixed grid at ~0.22 nm; fire between max and min lead marks | `DCS-EA p.94–95` |
| Fire-quantity switch | — | **ALL = "Automatic"**, **SINGLE 0.5 ALL = "Cutoff"** (burst limiter) | `DCS-EA p.95` |

`[ABL]` The funnel is the *fallback*, the lead-computing ring is the *primary* — the exact inverse
of the F-16's EEGS Level II/Level V relationship in `doc/modules/f16/weapons.md` §2.5, where the funnel is
the no-radar default. Same two mechanisms, opposite defaults.

---

### 5. Air-to-ground stores

#### 5.1 Free-fall bombs

| Store | Mass / class | Documented detail | Release limits | Source |
|---|---|---|---|---|
| FAB-100/250/500 (M-62 family) | 100 / 250 / 500 kg | HE GP, against ground objects, equipment, fortifications, bridges | **release speed 500–1,000 km/h** | `DCS-FM p.75` |
| FAB-500 M-62 detail | 2,470 mm × 400 mm, **filling 201 kg** | low-drag 1962 model for external carriage | — | T3 |
| FAB-250 M-62 detail | filling **100 kg** | — | — | T3 |
| **BetAB-500ShP** | 500 kg class | **parachute-retarded, then rocket-boosted** to pierce concrete; heavier casing, buried detonation | **150–1,000 m altitude, 550–1,100 km/h** | `DCS-FM p.75–76` |
| **RBK-250 AO-1** | 273 kg, 2,120 × 325 mm | **150 fragmentation bomblets**, 150 kg of submunitions; **footprint up to 4,800 m²** | — | `DCS-FM p.76` |
| **RBK-500 AO-2.5RTM** | 504 kg, 2,500 × 450 mm | **108 × AO-2.5RTM** bomblets (2.5 kg, 150 × 90 mm), 270 kg of submunitions | **500–2,300 km/h, 300 m – 10 km** | `DCS-FM p.77` |
| **KMGU-2** | dispenser | **8 × BKF cartridges**; inter-cartridge interval **0.005 / 0.2 / 1.0 / 1.5 s**; typical fills 12 × AO-2.5RT, 12 × PTM-1 mines, or 156 × PFM-1C | **50–150 m, 500–900 km/h** | `DCS-FM p.77` |
| ZB-500 (incendiary tank) | ≈ 500 kg | thickened-fuel ("Ognesmes": toluene/kerosene/polystyrene) + WP igniter, burst-dispersed; tumbles on release | T4: **50–1,500 m, ≤ 1,500 km/h** | T3/T4 — **not named in either DCS manual as a 9-12 store**; carriage is `[GAP]` |

**Cluster dispersal mechanism** `DCS-FM p.76` (relevant if RBK is ever more than a mass on a pylon):
a black-powder charge in the nose, fired by a **time-delay screw fuze that starts spinning after
release**, splits the canister; the footprint is circular or elliptical depending on the fall angle
at the dispersal point, and its size is set by canister speed and altitude at that moment.

`[ABL]` **Bomb ballistics come from the store's own JSBSim model, not from this table.** Per
`CLAUDE.md`'s `modules/stores/` rule, an unguided store is a full unit flying its own pinned model —
the numbers above are the **catalogue** entries (`core/FBStore.h`: kind, key, model name, mass, drag
area, life) and the **release limits** that `FBStoresSystem` must reject outside of. The 500–1,000
km/h envelope for FAB bombs is a *rejectable condition* (`out_of_context`), not a guideline.

#### 5.2 Unguided rockets

| Rocket | Calibre | Documented detail | Source |
|---|---|---|---|
| **S-8** | 80 mm | **20 per station in B-8M1**; 6 stabiliser fins deployed by a piston driven by motor exhaust; motor burn **0.69 s**; **dispersion/CEP = 0.3 % of range**; **max effective launch range 2 km**; S-8TsM is the smoke/marking variant | `DCS-FM p.78–79` |
| S-8KOM (representative round) | 80 mm | 1,570 mm, **11.3 kg**, warhead **3.6 kg**, **610 m/s**, range 1,300–4,000 m | T3 |
| **S-24** | 240 mm | **2.33 m, 235 kg, 123 kg blast-fragmentation warhead**, range ~2–3 km; S-24B uses low-smoke BN-K propellant | T3; named as MiG-29 store at `DCS-EA p.98` |
| **S-5** | 57 mm | named as a MiG-29 store; no per-round data in either manual | `DCS-EA p.98` |
| B-8M1 pod | — | 2,760 mm, Ø 520 mm, **160 kg empty**, 20 rounds | T3 |

**Class-level rocket facts** `DCS-FM p.78`: motor burns **0.7–1.1 s**, accelerating to
**2,100–2,800 km/h**; thereafter pure ballistic flight; calibres 57–370 mm; typical launch
**600–1,000 km/h at 10°–30° dive**; fired in salvos.

`[DER]` The **0.3 % CEP-of-range** figure is directly usable: at the documented 2 km effective range
that is a **6 m CEP**, and it is the only dispersion number in either manual for **any** weapon —
strictly better sourced than the gun's (§4.4 `[SET]`).

#### 5.3 A-G delivery modes

| Mode | Applies to | Mechanism | Source |
|---|---|---|---|
| **OPT, with pre-designation** | rockets, gun, bombs | dive ≤ 40°; hold LOCKON → aiming mark goes to acquisition position; **LRF auto-starts at dive > 10°** inside the range gate (rockets/gun: `Dr max + 1,640 ft`; bombs: ~11,500 ft); align mark, release LOCKON = **preliminary target acquisition**; re-align; pickle | `DCS-EA p.99, p.101` |
| **OPT, without pre-designation** | rockets, gun, bombs | level or ≤ 40° dive; start LRF **3–5 s before overflight** for level bombing; apply wind/target-motion lead manually; hold the trigger — **the aircraft releases** when the sighting angle equals the computed release angle | `DCS-EA p.100, p.101` |
| **TOSS ("KBR")** | bombs | level run-in; LRF 3–5 s before the mark reaches the target; pickle and **hold**; range scale becomes a countdown; audio 1.5–3 s before pull; at zero the "Г" symbol appears and the director ring jumps up; fly the half-loop at **4–5 g through 90–95° pitch**; release happens automatically | `DCS-EA p.103–104` |
| **RETICLE** | bombs, rockets, gun | fixed grid, computed correction from the §4.3 table; if the correction is inside the vertical grid line, align the computed point; if outside, align the outer warning-ring intersection and **count the time manually** | `DCS-EA p.104–105`, `p.94` |
| **BS** | rockets, gun | boresight fallback when OPT fails; uses the §4.3 correction table | `DCS-EA p.100` |

**Timing constraints — a genuinely unusual, rebuild-critical rule** `DCS-EA p.99, p.101`:

> Between releasing **LOCKON** and pressing the release trigger, the elapsed time must be
> **> 1 s and < 10 s**; **highest accuracy in the 1.5–4 s window.**

`[ABL]` This is a *human-in-the-loop* accuracy schedule baked into the fire-control computer: the
designation is a **snapshot** that then ages, and the solution degrades outside the window. It maps
directly onto `core/FBCommandBus`'s latency classes plus a designation-age term — and it means
`FBPilot`'s Attack phase for this module cannot pickle "as soon as the cue appears"; it must pickle
**1.5–4 s after** the designation it is shooting on. That is the opposite of the F-16 Attack phase,
which advances the pickle by the actuation latency (`FBCommandBus::LatencyS`).

**Bomb-release symbology in the countdown state** `DCS-EA p.102–103`: director control ring, time
index to release, time scale, store-presence indicator, zero-roll mark, **current overload (g)
vector** — the pilot flies the g-vector tip into the ring. Audio 1.5–3 s before release. Store
presence symbols extinguish when the store leaves `DCS-EA p.102`.

`[ABL]` **The "fly the g-vector into the commanded-g ring" director is this jet's CCRP.** It is not
a release *cue* the pilot reacts to; it is a **closed-loop steering command** with the release taken
by the system. `modules/f16/FBF16FireControl`'s CCRP countdown is the right block to publish it in,
but the *pilot* behaviour differs: hold trigger, fly a director, do not pickle at a moment.

**KMGU note** `DCS-EA p.102`: the aiming mark shows where the **first block** falls; after unloading
starts the flight parameters must be **held constant** until the presence symbols disappear.

---

### 6. Loadout templates

| Role | Loadout | Basis |
|---|---|---|
| **Air defence (standard)** | 2 × R-27R (inboard) + 4 × R-73 (mid/outboard) + 150 rds | `DCS-FM p.64` verbatim |
| Air defence, CM-resistant | 1 × R-27R + 1 × R-27T + 4 × R-73 | `[ABL]` from `DCS-FM p.68`'s mixed-seeker doctrine |
| Air defence, legacy | 2 × R-27R + 4 × R-60M | T3/T4 (period photos, model-kit pylon sets pair APU-470 with APU-60) |
| CAP with tank | as standard + PTB-1500 centreline — **note: speedbrake inhibited** (`DCS-EA p.57`), gun possibly inhibited on early jets (T4) | `[ABL]` |
| Ground attack | 4 × FAB-250 or 2 × FAB-500 (inboard) + 2 × R-73 (outboard) + 150 rds | `[ABL]` from §2.1 station assignment + the 3,000 kg ceiling |
| Rocket attack | 2 × B-8M1 (40 × S-8) + 2 × R-73 | `[ABL]` |
| Ferry | PTB-1500 only (9-12 cannot take wing tanks, §2.2) | `[ABL]` |

`[DER]` **Standard-loadout mass check.** 2 × 253 + 4 × 105 = **926 kg** of missiles, plus 150 rounds
× 0.832 kg ≈ **125 kg** of ammunition = **1,051 kg**. Empty 10,900 + internal fuel 3,200 + pilot/kit
~100 + 1,051 = **15,251 kg**, against the documented **normal takeoff weight of 15,300 kg**
(`DCS-FM p.14`). **Agreement to 0.3 %** — this closes the loop between the weapons file and the
mass budget in `flight-model-spec.md` §3, and it is the strongest single consistency check either
file contains.

---

---

## State

**Implemented as of MiG-29 stage 2c: the R-27R, the R-73 and the GSh-301.** Each is a catalogue entry
in the FlightBox core (`core/FBStore.h`, `core/FBGun.h`) with its own FlightBox-own JSBSim deck under
`sim/assets/aircraft/`, and the aircraft carries them on `modules/mig29/FBMig29Sms`'s seven declared
stations with `FBMig29Gun` in the port LERX. Behaviour is the generic `weapons/` code throughout; what
this file supplied is the numbers and the two mechanics that are properties of these weapons rather
than of any weapon:

| From this file | Where it landed | Measured |
|---|---|---|
| §3.1 R-27R mass/geometry/warhead/range, §3.2 the SARH support obligation | `core/FBStore.h::kR27r` + `FBSeekerKind::SemiActiveRadar`; the illumination gate in `modules/missile/FBMissileGuidance` | 28.56 s of unbroken illumination for one shot; break it in flight and the round misses by 27.04 m |
| §3.2 consequences 3 and 4 (the antenna-bounded crank, Support/Defend exclusivity) | `FBMig29Pilot::InterceptCrankAtaDeg` = 50° off 65°, and `SupportInhibitsDefend` = true | `mig29-intercept` runs designate → shot → support to impact |
| §3.3 R-73 mass/geometry/warhead/3.5 m blast radius/75° gimbal | `core/FBStore.h::kR73` + `FBMissileIrSeeker` | rear-quarter hit at 0.138 m; decoyed head-on at `tgtIntensity=0.16` |
| §4.1/§4.4 the GSh-301 row and its `[SET]` 6 mrad dispersion bound | `core/FBGun.h::kGsh301` | the documented 200-790 m effective band emerges from the dispersion: a kill at 294 m of round path, avionics-only at 571 m |
| §2.1 the station map, flagged `[GAP]` and therefore a convention | `FBMig29Sms`, seven stations, stated as a convention | — |

**Not implemented, and each named rather than approximated:** the pair-release semantics of §2.4 (the
F-16's one-store-one-command path is used, which is §2.4's SINGLE case), the R-27T/ER/ET and the
R-60M, every air-to-ground store, the release-envelope rejections of §5.1, and the §4.3 correction
table as an acceptance target for the 30 mm drag coefficient (it remains an unspent check).

| Roadmap stage | What it will take from this file |
|---|---|
| **R3** — knowledge base | *running*: this file is the R3 deliverable for the armament |
| **R6** — asymmetric weapons + RCS | the primary consumer: R-27R/T, R-73 and R-60M are the "enemy missile family" R6 names, each to become its own unit with its own module and FDM, exactly as the AIM-120 is today. The DLZ vocabulary of §3.5 maps onto the existing `Raero`/`Rtr`/`Rmin` block |
| **R7** — enemy units at BVR scale | the **R-27R support obligation** (§3.2) is the doctrine-defining property: ~26 s of illumination against the AIM-120's 5–15 s, a crank ceiling bounded by the antenna at 67°, and Support/Defend therefore **mutually exclusive** |
| **R8** — JSBSim model | station geometry and store masses/drag areas (§2) are what a MiG-29 `FBStoresSystem` would mount as point masses and an external force; the GSh-301 row set (§4) is built to line up with the M61A1 table `core/FBGun.h` already carries |

**The scale caveat that governs every row** (from the module file): the MiG-29 is a
**BVR-scale** opponent — what has to be right is what he can reach, how fast he gets there, what he
can see and what he can shoot. A failing knife-fight comparison is not a defect of the model; a wrong
envelope is.

Roadmap chain: [`../flightbox/roadmap.md`](../../roadmap.md) — **R3** (this knowledge base,
running) → **R6** (asymmetric weapons + RCS) → **R7** (enemy units, MiG-29 at BVR scale) → **R8**
(the JSBSim MiG-29 model). Nothing after R3 has begun.

---

## Gaps

**Source gaps** — the file's own itemised list follows, section number unchanged. The
governing caveat is at the top of the file: `DCS-FM` is itself a **secondary distillation of Russian
open literature**, treated as a good T3 and never as T1; the **GAF T.O. 1F-MIG29-1** is the document
that would settle most `[GAP]` rows and was not available to this pass.

**Implementation gaps** — none statable yet: nothing is built (see State).

### 8. Open gaps — not guessed

1. **`[GAP]` Official station numbering and per-station load limits.** Six wing pylons + centreline
   is certain; everything finer is convention.
2. **`[GAP]` R-27 motor burn times, per variant.** Only the class bound (2–15 s) and the
   controllability floor (800–1,000 km/h) are documented. Without burn time the boost/coast split in
   a `modules/missile` rebuild is a `[SET]`.
3. **`[GAP]` R-27 proximity-fuze burst radius.** The R-73 (3.5 m) and R-60M (5 m) are documented;
   the R-27's is not, and it is the one that matters most for BVR damage resolution.
4. **`[GAP]` GSh-301 dispersion.** §4.4 gives a defensible bound, not a source.
5. **`[GAP]` 30 mm exterior ballistics** (Cd vs Mach, retained velocity vs range). §4.3 gives an
   acceptance target that a correct model must hit, which is second-best but usable.
6. **`[GAP]` R-27R seeker designation** — 9B-1101K vs 9B-1032/PRGS-27, unresolved at T4.
7. **`[GAP]` Whether the 9-12 carries ZB-500 / BetAB-500ShP operationally.** `DCS-FM` describes
   BetAB in the weapons chapter but the chapter covers the MiG-29 family generically.
8. **`[GAP]` Complete-round mass for 30 × 165 mm** at better than T4 — needed for the ammunition
   mass/CG term and for the §6 loadout check (which currently leans on a T4 figure).
9. **`[GAP]` The N019's own DLZ computation** — the manuals show the three range labels but not the
   algorithm behind them. FlightBox will forward-integrate a performance table instead, exactly as
   `modules/f16/FBF16FireControl` does, and the difference between that prediction and the measured
   intercept is (per `CLAUDE.md`) a property worth measuring rather than hiding.
10. **The single document that would close 1, 2, 4, 5 and 8: GAF T.O. 1F-MIG29-1.** Not consulted in
    this pass. Everything above is written so that its arrival is an edit, not a rewrite.

---

---

## Knowledge

§7 carries the researched depth *and* the cross-manual conflict registrations
(§7.1) — both values kept, never silently resolved.

### 7. Technical depth — researched, with tiers

#### 7.1 What the two DCS manuals disagree about

| Item | `DCS-FM` | `DCS-EA` | Verdict |
|---|---|---|---|
| Gun designation | "GSh-30-1" `p.64` | "GSh-301 (9A-4071K)" `p.86` | Same weapon; **9A-4071K** is the GRAU index. Use GSh-301. |
| R-27ER/ET on a 9-12 | "the Su-27 and its variants can be equipped" `p.68–69` | listed in the 9-12's own armament `p.86` | `[ED-MODEL]`, see §3.1 |
| Range units | km | nm / kts / ft | `DCS-EA` is a metricated German-manual descendant; conversions in this file are stated in both |
| Radar detection | 70 km FH / 35 km RH `p.12` | not restated | Use `DCS-FM`; T4 gives 60–70 km with a design goal of 100 km never achieved |

#### 7.2 Numbers taken from research, with tier

| Fact | Value | Tier | Note |
|---|---|---|---|
| GSh-301 muzzle velocity | **860 m/s** | T3 | T4 says 900 m/s |
| GSh-301 projectile mass | 390 g | T4 | consistent across sources; **wanted at T2** |
| GSh-301 barrel life | 2,000 rd | T3 | |
| R-60M launch mass | 43.5 kg | T3/T4 | not in either manual |
| R-60M off-boresight | 17° or ±20° | T4 | contested |
| R-27 family brochure ranges | 60/95/50/90 km | T3 (Vympel export data via airforce-technology) | **head-on, high altitude, ideal** — not a DLZ |
| R-27 warhead | 39 kg rod, radar-proximity + impact | T3 | agrees with `DCS-FM` |
| S-8KOM round data | 11.3 kg / 3.6 kg / 610 m/s | T3 | |
| S-24 round data | 235 kg / 123 kg / 2–3 km | T3 | |
| FAB-500 M-62 filler | 201 kg | T3 | |
| Max external load | 3,000 kg | T3 (Jane's-derived) | one T4 says 3,500 kg |

#### 7.3 Rebuild notes — mapping to FlightBox types

| FlightBox element | What this file supplies | What is still missing |
|---|---|---|
| `core/FBStore.h` catalogue | mass, warhead mass, guided/unguided flag, model name per store | **drag area (CdA)** for every store — must come from each store's own JSBSim model, as for Mk-82 |
| `core/FBGun.h` | muzzle velocity 860 m/s, 1,500 rd/min, 150-round drum | dispersion cone (`[SET]` 6 mrad, §4.4) |
| `core/FBGunBallistics` | acceptance target: 14 thousandths at 1,500 m / 432 kts / 20° dive (§4.3) | 30 mm drag coefficient vs Mach |
| `weapons/FBStoresSystem` | pair-release semantics (§2.4); release-envelope rejections (§5.1) | per-station weight limits `[GAP]` |
| `sensors/FBRadarSystem` | range-scheduled Doppler notch: 81 kts > 8 nm, 27 kts < 8 nm (§3.6) | the N019's actual PRF set |
| `pilot/FBEngagement` | SARH support-to-impact rule, 67° crank ceiling, Defend/Support exclusivity (§3.2) | R-27R time-of-flight table |
| `FireControl` block | Dr max1 / Dr max2 / Dr min ↔ Raero / Rtr / Rmin (§3.5) | the R-27's own performance table for forward integration |
| `core/FBDamageModel` | warhead masses 39 / 7.4 / 3.5 kg; R-60M fuze radius 5 m; R-73 blast radius 3.5 m | R-27 fuze burst radius `[GAP]` |
| `FBPilot` Attack phase | the 1.5–4 s post-designation pickle window (§5.3) | — |

---

### Sources

- `doc/DCS MIG-29 Flight Manual EN.pdf` — Eagle Dynamics, *DCS: MiG-29 Fulcrum Flight Manual*, 2018.
  Printed pages 9–14, 63–79, 83–92, 110 (= PDF pages 15–20, 69–85, 89–98, 116).
- `doc/DCS MiG-29A Early Access Manual EN.pdf` — Eagle Dynamics, *DCS: MiG-29 Fulcrum Flight Manual,
  Early Access v.09*, 2025. Pages 11–13, 59–60, 63, 86–105.
- [weaponsystems.net — 30 mm Gryazev-Shipunov GSh-301](https://weaponsystems.net/system/98-30mm+Gryazev-Shipunov+GSh-301) (T3)
- [weaponsystems.net — 80 mm S-8](https://weaponsystems.net/system/183-80mm%20S-8) (T3)
- [Airforce Technology — R-27 (AA-10 Alamo)](https://www.airforce-technology.com/projects/r-27-aa-10-alamo-guided-medium-range-air-missile/) (T3, Vympel export data)
- [GlobalSecurity — AA-11 Archer / R-73](https://www.globalsecurity.org/military/world/russia/aa-11.htm) (T3)
- [SirViper — MiG-29 'Fulcrum-A'](https://sirviper.com/index.php?page=fighters%2Fmig-29%2Fmig-29a) (T3, Jane's-derived)
- [Machtres — MiG-29 and variants](https://www.machtres.com/lang1/mig-29.html) (T3/T4, station and tank carriage)
- [Wikipedia — FAB-500](https://en.wikipedia.org/wiki/FAB-500), [FAB-250](https://en.wikipedia.org/wiki/FAB-250) (T4, filler masses)
- [Wikipedia — S-24 rocket](https://en.wikipedia.org/wiki/S-24_rocket) (T4)
- [weaponsystems.net — ZB-500Sh](https://weaponsystems.net/system/1463-ZB-500Sh) (T3)
- [ODIN/TRADOC — R-60 (AA-8 Aphid)](https://odin.tradoc.army.mil/WEG/Asset/R-60_(AA-8_Aphid)_Russian_Short-Range_Infrared_Homing_Air-to-Air_Missile) (T3)
- Companion file: `doc/modules/mig29/flight-model-spec.md` (airframe, engine, envelope anchors).
- Format template: `doc/modules/f16/weapons.md`.
