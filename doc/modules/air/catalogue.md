# The catalogue aircraft — eighteen rows

**Subject:** the **real** aircraft behind the scenario's catalogue manifest
(`mods/f16/src/catalogue.fba`). One row per `FBModuleRegistry` key, each
with the five quantities that decide a mission — **acquisition range and time · weapon envelope ·
whether the weapon binds the shooter · warning receiver and countermeasures · the flight-performance
anchor set** — and nothing else.

**Delimitation:** this file is the DATA. The class that consumes it, the layer it lives in, the pilot
tiers and the anti-cheat argument are [`module.md`](module.md); the procedure that turns the
performance anchors into a JSBSim deck is [`flight-model-recipe.md`](flight-model-recipe.md).

**Status: BUILT 2026-07-28, ARMED 2026-07-29, MOVED INTO THE MOD 2026-08-05.** All eighteen rows exist
as `mods/f16/src/catalogue.fba` declarations and `FBModuleRegistry` keys, ten decks and seven rounds are generated, and eight `.fbm` files fly them.
**Ten rows now carry a fire control** and can employ what they declare — see "What each row's fire
control can actually do" below, which also names the four armed rows that still cannot.
**Four deck rows are `ACCEPTED` and may answer a campaign question — `f15c` · `mig21` · `mirf1` ·
`f5e`; six are `ALPHA`, each on one or two NAMED anchors** — see
[`flight-model-recipe.md`](flight-model-recipe.md) `## State` for the residual table and R14–R17 for the
six causes. **Two of this file's own numbers were confirmed by that measurement rather than by a
source:** `PitchStickMax` is no longer a `[SET]` 0.85 on nine rows but `[DERIVED]` per row from the
row's own deck (recipe §6.1), and the published maximum rates of climb are shown to name **no weight**
— inverted for one, three of them (`f15c` 13 141 kg, `su22` 9 982 kg, `mirf1` 6 024 kg) come out below
the row's own empty weight, so A4 is a probe on nine rows and an anchor only on `mig17`. Two numbers in this file MOVED during the build and are marked below: the six guns'
ballistics, declared `[TODO]` here and filled at [T4] because a gun cannot be built without them.

**Schema:** the same as [`../mig29/`](../mig29/INDEX.md) and
[`../ground/catalogue.md`](../ground/catalogue.md) — every number carries a **source** and a
**confidence tier**, values that disagree between sources are carried **both**, and a value nobody
published is `[SET]` with a reason or `[TODO]` and not guessed.

| Tag | Meaning |
|---|---|
| **[T1]** | official / government / service document |
| **[T3]** | established specialist literature |
| **[T4]** | encyclopaedic / community consensus (here: Wikipedia's aircraft specification templates and radar articles) |
| **[DISPUTED]** | sources conflict, or the published derived figures do not close; both carried |
| **[SET]** | a FlightBox setting, with its reason |
| **[DERIVED]** | computed from a named number by a named relation |
| **[TODO]** | not sourced and not set — the field is open |

**The sourcing is thin in one specific way and it must be said first.** The **flight-performance**
half of every row is well sourced: seventeen of eighteen rows publish a complete `Aircraft specs`
set — geometry, masses, thrust, two Vmax anchors, ceiling, climb — at [T4], and five of the ten deck
rows close **both** free consistency probes to under 1.5 % ([`flight-model-recipe.md`](flight-model-recipe.md)
§Knowledge 1). The **sensor and weapon** half is much thinner: **no scan period is published for any
airborne fire-control radar in this catalogue** (only the E-3's rotodome rate), radar detection range is
`[TODO]` on four rows, IRST reach on all but one, warning-receiver type on half, and **no radar figure
in the catalogue states the target RCS it was measured against** except the Su-27's. No [T1] source (a
flight manual, a service intelligence handbook) was read for this round; naming them is in §Sources.

---

## The one-glance table

`Camp.` = campaigns that name the type (measured against `../../campaigns/*.md`). `Motion`: **deck** =
generated JSBSim deck, **mover** = kinematic ([`module.md`](module.md) §Spec 4). `Tier` = the pilot
staffelung of [`module.md`](module.md) §Spec 5.

| Key | Type | Camp. | Motion | Tier | Acquisition: range / frame | Weapon envelope | Binds shooter? | RWR / CM |
|---|---|---:|---|---|---|---|---|---|
| `e3` | E-3 Sentry (AWACS) | 3 | mover | T1 | **400 km** low-flyer / **10.0 s** (6 rpm, the one sourced rate) | none | — | none / none |
| `e2c` | E-2C Hawkeye | 1 | mover | T1 | 640 km / 10.0 s `[SET]` | none | — | **ALR-73 ESM** / none |
| `f15c` | F-15C Eagle | 4 | deck | **T4** | 160 km / 4.0 s `[DERIVED]` | AIM-7 70 km · AIM-9 · AIM-120 | **AIM-7: yes, to impact** | ALR-56 / **ALE-45** |
| `mig21` | MiG-21bis | 3 | deck | T3 | **20 km**, ±30°×±10° / **2.0 s** `[DERIVED]` | K-13 1.0–3.5 km, **rear aspect** · R-60 8 km | no (IR) | Sirena `[TODO]` / none |
| `mig23` | MiG-23ML/MLD | 4 | deck | T3 | 52 km high / **23 km look-down** / 2.0 s | R-23/R-24 35 km · R-60 8 km | **R-23R/R-24R: yes** | Sirena/SPO-15 `[TODO]` / `[TODO]` |
| `mig25` | MiG-25PD | 1 | deck | T3 | 120 km search / **50 km track** / 4.0 s | R-40 50–80 km · R-60 8 km | **R-40R/RD: yes** | **SPO-10M** / BVP-50-60 (some) |
| `mig17` | MiG-17F | 1 | deck | T2 | **none — the eye** | guns only: 2×NR-23 + 1×N-37 | trivially | none / none |
| `su7` | Su-7BKL | 1 | deck | T2 | **none — the eye** | guns only: 2×NR-30 | trivially | none / none |
| `su22` | Su-17M4 / Su-22M4 | 2 | deck | T2 | **none — the eye** (laser ranger only) | 2×NR-30 · K-13/R-60 self-defence | no (IR) | `[TODO]` / `[TODO]` |
| `su27` | Su-27S | **0** | deck | **T4** | **79.5 km** head-on at the tree's reference RCS / 4.0 s | R-27 · R-73 · GSh-30-1 | **R-27R: yes, to impact** | **SPO-15** / `[TODO]` |
| `mirf1` | Mirage F1C | 1 | deck | T3 | 96–100 km / 4.0 s | Super 530F 25 km · Magic 1 10 km · 2×DEFA | **Super 530F: yes** | `[TODO]` / `[TODO]` |
| `f5e` | F-5E Tiger II | **0** | deck | T2 | APQ-159 `[TODO]` — **probably eye-bound** | 2×AIM-9 · 2×M39A2 | no (IR) | `[TODO]` / `[TODO]` |
| `kc135` | KC-135R Stratotanker | 3 | mover | T0 | none | none — **and the boom does not exist** (`C5`) | — | none / none |
| `tu95` | Tu-95MS | 2 | mover | T1 | none | tail GSh-23 — **turret not modelled** | — | `[TODO]` / `[TODO]` |
| `an26` | An-26 | 1 | mover | T0 | none | none | — | none / none |
| `ef111` | EF-111A Raven | 1 | mover | T1 | none | **none at all** — comms jamming only (`C24`) | — | `[TODO]` / `[TODO]` |
| `mi8` | Mi-8MT | 1 | mover | T1 | none | S-5 rockets, PK MGs — not modelled | — | none / none |
| `ah64` | AH-64A/D Apache | 1 | mover | T1 | Longbow `[TODO]` (D only) | M230 30 mm, Hellfire — not modelled | — | `[TODO]` / `[TODO]` |

**The three facts a reader should take from that table:**

1. **Nine of the twelve armed rows carry a weapon that binds the shooter to impact.** Semi-active
   homing is the catalogue's normal case, not its exception — so the mechanic
   [`../../weapons.md`](../../weapons.md) built once for the R-27R (`FBSeekerHandoverS = −1`, measured
   at **28.56 s of unbroken pointing for one shot**) is what almost the whole catalogue runs on, and
   **no new seeker kind is asked for anywhere in this file.**
2. **Four rows have no radar at all** and acquire through the eye — which
   [`../ground/catalogue.md`](../ground/catalogue.md)'s `zu23` row already proved is a *design*, not a
   hole: detection is then `FBVisualSystem`'s measured behaviour (3 784 m beam-on against an F-16,
   2 493 m head-on, **zero at night**), and nothing about it is tuned.
3. **Two rows are in the catalogue without a campaign asking for them** (`su27`, `f5e`) and the table
   says so in its own `Camp.` column.

## What each row's fire control can actually do

Added 2026-07-29 with [`module.md`](module.md) §Spec 12. `FC` = the row composes a
`modules/air/FBAirFireControl` at all, which is `FBAircraftSpec::CanEmploy()` — tier ≥ T2 **and** (a
station **or** a gun) — so every mover is out by construction and a tanker has no computer.
`Zone` = a launch zone can exist for a round on its rail; it needs a **radar lock**, so a row with
`SearchRangeM = 0` can never produce one. `Gun sol.` = the same lock feeds `FBGunSolveLead`.
`Employable today` is the honest column: it is `Zone`/`Gun sol.` **and** a pilot phase that presses the
button, and the two things that block it are the tier gate's plant (recipe step 8) and A14/A15.

| Key | FC | Launch zone | Gun solution | Binds, **as flown** | Employable today |
|---|---|---|---|---|---|
| `f15c` | **yes** | yes (160 km set) | yes | **AIM-7 yes / AIM-120 no** — the round in the air decides, not the airframe | **missiles: measured kill.** Gun: plant written (step 8), A15 |
| `su27` | **yes** | yes (79.5 km set) | yes | R-27R yes / R-73 no | missiles yes; `bfm` refused — deck `ALPHA`, no plant |
| `mig21` | **yes** | yes (20 km set) | yes | K-13/R-60 no (IR) | **missiles + cannon: both measured** (`air-fishbed-guns.fbm`) |
| `mig23` | **yes** | yes (52 km set) | yes | **R-24R yes, to impact** | **measured, both ends** — shot at 20.78 km, and the break in the illumination measured too |
| `mig25` | **yes** | yes (120 km set) | — (no gun) | **R-40R yes, to impact** | missiles yes; deck `ALPHA` |
| `mirf1` | **yes** | yes (96 km set) | yes | **S530F yes** | missiles yes; gun plant written (step 8), A15 |
| `mig17` | **yes** | **never** (no radar) | **never** (A14: no ranging source) | trivially | **NO — A14.** A gun with no range is not a gun |
| `su7` | **yes** | **never** | **never** (A14) | trivially | **NO — A14** |
| `su22` | **yes** | **never** | **never** (A14) | K-13/R-60 no (IR) | **NO — A14** |
| `f5e` | **yes** | **never** | **never** (A14) | AIM-9 no (IR) | **NO — A14**, though its plant is written |
| `e3` `e2c` `kc135` `tu95` `an26` `ef111` `mi8` `ah64` | **no** | — | — | — | **no, and that is the tier** (`blk_firecontrol` = 0 for the whole run) |

**The two facts that column forces into the open.** First, **binding is per-loadout**: the same F-15C is
tied to its target for 39.2 s with a Sparrow and for 8.6 s with an AMRAAM, on the same geometry with four
`store` lines changed — which is the pairing this row was put in the catalogue for. Second, **four of the
ten armed rows cannot use a weapon at all**, and it is one missing mechanism, not four: a gun and an IR
round both need a *range* the eye does not publish (A14).

---

## `e3` — E-3 Sentry, the row that must not become a track source

The AEW row is [`module.md`](module.md) §Spec 7's whole subject: it sees 400 km and it tells somebody.

| Quantity | Value | Source |
|---|---|---|
| Radar | **AN/APY-1 / APY-2**, passive electronically scanned array | [T4] [Wikipedia, Boeing E-3 Sentry](https://en.wikipedia.org/wiki/Boeing_E-3_Sentry) |
| Detection, **low-flying** targets | **more than 400 km** (250 mi), pulse-Doppler | [T4] ibid. |
| Detection, medium/high altitude | ~650 km (400 mi), pulse (beyond-the-horizon) mode | [T4] ibid. |
| **Rotodome rate** | **6 revolutions per minute** | [T4] ibid. — **the only sourced antenna rate in the whole catalogue** |
| Service ceiling | 29 000 ft (8 800 m) **minimum** | [T4] ibid. |
| Max / optimum cruise speed | 461 kt / 360 mph (580 km/h) | [T4] ibid. |
| Endurance | > 8 h unrefuelled (~11 h with CFM56) | [T4] ibid. |
| Masses | empty 185 000 lb · gross 344 000 lb · MTOW 347 000 lb | [T4] ibid. |
| Powerplant | 4 × TF33-PW-100A, 21 500 lbf | [T4] ibid. |
| Geometry | wing area 3 050 ft² · span 145 ft 9 in · length 152 ft 11 in | [T4] ibid. |
| Armament | **none** | [T4] ibid. |
| Role | surveillance, identification, **weapons control**, battle management; Link 16 | [T4] ibid. |

**FlightBox row:** `Motion = mover`, `Tier = T1`. `SearchRangeM = 400 000` [T4] — **the low-flyer
figure**, because the campaigns' targets are fighters at low and medium altitude, and taking the 650 km
beyond-the-horizon figure would claim a mode against the wrong target class. `SearchAzHalfDeg = 180`,
`SearchElHalfDeg` `[TODO]` → the tree's own `[SET]` ±10°. `SearchFrameS = 10.0` **[DERIVED]** from the
sourced 6 rpm — one revolution per 10 s. `EmitterKind = AirborneFireControl` `[SET]`, because the tree
has no *airborne early warning* value and adding one would change what every RWR in the tree reports
about it; the honest consequence is that **a fighter's receiver cannot tell an AWACS from a fighter
radar**, which is stated rather than fixed. `RadarCrossSectionM2` `[TODO]`. `Channels = 0`, no weapon.

**What it does, and the exact boundary of it:** it publishes `FBNetReport` — a point reconstructed from
its own anonymous `FBRadarContact`, carrying its own look age, with **no id field and no team field**.
A subscribing fighter re-centres its own antenna over the command bus and must still find, firm and gate
the target itself. Full argument, and the one interface price (`sensors/FBNetLinkSystem`), in
[`module.md`](module.md) §Spec 7.

**Three limits that are arithmetic and not policy:**

| Limit | Number |
|---|---|
| the cue is stale by construction | node look age ≤ **10.0 s** (its own frame) + one link period; at a 250 m/s target that is **≥ 2.5 km** of error the cued fighter searches out itself [DERIVED] |
| the cue is one antenna pointing | azimuth centre and elevation centre. **Not** the range gate, not the frame time, not a track file entry |
| the node reports at most **8** tracks | `FBRadarSystem`'s fixed track file, first eight in registry order ([`module.md`](module.md) §Spec 7 design A, accepted with the number stated) |

**When killed:** `Radar` failed → block `Invalid` → `Emission()` stops → it reports nothing, by the
coupling that already exists. **This is what makes killing an AWACS worth a sortie**, and it is the
airborne twin of the measurement `net-cue.fbm` / `net-cue-unnetted.fbm` already made on the ground.

---

## `e2c` — E-2C Hawkeye, the same row with a carrier deck under it

| Quantity | Value | Source |
|---|---|---|
| Radar, by group | APS-120/125 (Group 0) · **APS-139** (Group I) · **APS-145** (Group II, Hawkeye 2000) · APY-9 (E-2D) | [T4] [Wikipedia, E-2 Hawkeye](https://en.wikipedia.org/wiki/Northrop_Grumman_E-2_Hawkeye) |
| Detection | Hawkeye 2000: detects **20 000 targets to greater than 640 km** (400 mi) and tracks over **2 000** simultaneously | [T4] ibid. |
| Simultaneous intercepts guided | **40–100** | [T4] ibid. |
| ESM | **AN/ALR-73 Passive Detection System** | [T4] ibid. |
| Datalink | AN/ACQ-5 TADIL-A / Link 11 modem | [T4] ibid. |
| Rotodome rate | **not published** | **[TODO]** |
| Performance (E-2D figures, the article's spec) | max 350 kt · cruise 256 kt · ceiling 34 700 ft · endurance 6 h (8 h land-based) | [T4] ibid. |
| Masses / geometry | empty 40 200 lb · MTOW 57 500 lb · wing area 700 ft² · span 80 ft 7 in · AR 9.15 | [T4] ibid. |
| Powerplant | 2 × T56-A-427 turboprop, 5 100 shp | [T4] ibid. |

**FlightBox row:** `Motion = mover`, `Tier = T1`, everything else as `e3` with `SearchRangeM = 640 000`
[T4] and `SearchFrameS = 10.0` **[SET]** at the E-3's sourced rate, for want of one of its own.

**Why it is a separate row and not a rename of `e3`:** it is the AEW of
[`../../campaigns/o1-bekaa-1982.md`](../../campaigns/o1-bekaa-1982.md), where the anchor states that the
E-2Cs *"guided Israeli fighters to attack from the beam, where Syrian radar warning had no coverage"*
[T4] — i.e. the campaign's decisive use of an AEW is a **geometry instruction**, which is exactly what
this catalogue's cue is and exactly what it is not allowed to become (an assignment). And its ceiling is
**10 600 m against the E-3's 8 800 m**, which moves the radio horizon the link is range-tested against.

**The ALR-73 is NOT modelled as an ESM.** FlightBox has one passive receiver (`FBRwrSystem`) and it
reports a bearing, a power and an estimated kind. Giving the E-2C a *different* passive sensor with a
different output would be a second perception path for one row. It gets `FBRwrSystem` like everybody
else, and the difference from the real box is declared here.

---

## `f15c` — F-15C Eagle, the peer western opponent

Four campaigns: escort in W2/W3, opposition in O1/O5 — *the aircraft that historically shot down every
MiG in O5's two anchors* ([`../../campaigns/INDEX.md`](../../campaigns/INDEX.md)).

| Quantity | Value | Source |
|---|---|---|
| Radar | **AN/APG-63**, X-band pulse-Doppler, look-up and look-down; basic range **100 mi (87 nmi, 160 km)** | [T4] [Wikipedia, F-15 Eagle](https://en.wikipedia.org/wiki/McDonnell_Douglas_F-15_Eagle) · [APG-63 and APG-70](https://en.wikipedia.org/wiki/APG-63_and_APG-70) |
| Radar, ACM | *"for close-in dogfights, the radar automatically acquires enemy aircraft and projects this onto the HUD"* | [T4] APG-63 article |
| Gun | 1 × M61A1, **940 rounds** | [T4] |
| Missiles | 4 × AIM-7 + 4 × AIM-9 · **or** 6 × AIM-120 + 2 × AIM-9 · **or** 8 × AIM-120 | [T4] |
| AIM-7F/M | **70 km**, SARH, 510 lb, 88 lb warhead, Mach 4 | [T4] [AIM-7 Sparrow](https://en.wikipedia.org/wiki/AIM-7_Sparrow) |
| RWR / EW | **AN/ALR-56** RWR · ALQ-135 internal countermeasures · **AN/ALE-45 chaff/flare** · APX-76 IFF interrogator | [T4] |
| **A1** Vmax at altitude | **Mach 2.5** (1 650 mph) | [T4] |
| **A2** Vmax at sea level | **Mach 1.2** (800 kt) | [T4] |
| **A3** Ceiling | 65 000 ft | [T4] |
| **A4** Climb | 67 050 ft/min maximum (with 3 pylons) | [T4] |
| **A5** g limit | **+9** | [T4] |
| **A6** Masses | empty 29 000 lb · gross 44 500 lb · MTOW 68 000 lb · internal fuel 13 455 lb | [T4] |
| **A7** Thrust | 2 × F100-PW-220: **14 590 lbf dry / 23 770 lbf AB** | [T4] |
| **A8** Geometry | wing area 608 ft² · span 42 ft 10 in · length 63 ft 9 in | [T4] |
| Probes | T/W 1.07 · wing loading 73.1 lb/ft² — **both close to 0.2 %** ([`flight-model-recipe.md`](flight-model-recipe.md) §Knowledge 1) | [T4] |

**FlightBox row:** `Motion = deck`, `Tier = **T4**`. `SearchRangeM = 160 000` [T4] — **against the
source's unstated reference target, taken as the tree's own 1.2 m² F-16 reference** `[SET]`, and the
direction of the error is stated: if the published figure is against a bomber, the row is materially too
good. `SearchAzHalfDeg` `[TODO]` → the tree's own ±60°, so `SearchFrameS = 4.0` **[DERIVED]**.

**Stores: no new deck for two of three.** `aim9` and `aim120` are already `core/FBStore.h` rows with
their own flown decks. Only **`aim7`** is new — `FBSeekerKind::SemiActiveRadar`, **built and unchanged**,
sized by the slender-body recipe from the published 510 lb / 8 in / Mach 4. Gun: **`m61a1` unchanged**,
with a 940-round drum instead of the F-16's.

**Binds the shooter:** on the AIM-7, **yes, to impact** — the same `FBSeekerHandoverS = −1` mechanic the
R-27R already measures. On the AIM-120, **no** past activation. **That pairing on one aircraft is what
makes this row interesting**: an F-15C with Sparrows fights the MiG-29's fight (bound, cannot defend
while supporting), and the same aircraft with AMRAAMs fights the F-16's.

**Engine analogy is the best in the catalogue:** the F100-PW-220 is the **same engine one dash number**
from the F100-PW-229 deck the tree already pins in the F-16 model
([`flight-model-recipe.md`](flight-model-recipe.md) §5).

---

## The period Soviet family — one class, five rows

W3, O1, O3 and W2 between them need MiG-21, MiG-23, MiG-25, MiG-17 and Su-7/Su-20. They are five
catalogue rows, they share one class, and their differences are exactly the five decisive quantities.

### `mig21` — MiG-21bis

| Quantity | Value | Source |
|---|---|---|
| Radar | **RP-22 Sapfir-21 "Jay Bird"**; 30 km maximum in search, **detects a fighter-sized target at 20 km co-altitude** | [T4] [flyshark, Sapfir-21/RP-22](http://www.flyshark.ayz.pl/Stacja_angielska/sapfir.htm) · [T4] DCS reference chart |
| Predecessor RP-21, for the ratio | detect **20 km** / lock **10 km** in theory, **13 / 7 km** in practice | [T4] [Wikipedia, RP-21 Sapfir](https://en.wikipedia.org/wiki/RP-21_Sapfir) |
| **Scan field** | **60° horizontal × 20° vertical** — limited by the nose inlet's small radar cone, *"still a significant limiting factor"* even in the latest derivatives | [T4] ibid. — **the most decisive sourced sensor number in the catalogue** |
| Look-down | **none.** "It couldn't intercept targets flying under the MiG, because the radar was unable to filter out ground clutter" | [T4] ibid. |
| Doctrine | *"pilots were tied to a GCI system which, through ground radars and datalinks, provided more extensive and precise information"* | [T4] ibid. |
| Gun | 1 × GSh-23L, **200 rounds** | [T4] [Wikipedia, MiG-21](https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-21) |
| Missiles | K-13 (R-3S/R-13M) · R-55 · **R-60**; 5 hardpoints (4 wing + 1 ventral for fuel) | [T4] ibid. |
| K-13 / R-13M | **0.97–3.5 km**, IR, 90 kg, 7.4 kg warhead, ⌀127 mm, Mach 2.5 | [T4] [K-13](https://en.wikipedia.org/wiki/K-13_(missile)) — **[TODO]**, this range is short against commonly quoted R-13M figures and no second source was read |
| R-60 | **8 km**, IR, 44 kg, 3 kg warhead, ⌀120 mm, Mach 2.47, altitude limit 20 000 m | [T4] [R-60](https://en.wikipedia.org/wiki/R-60_(missile)) |
| **A1** | 2 175 km/h / **M 2.05 at 13 000 m** | [T4] |
| **A2** | 1 300 km/h / **M 1.06 at sea level** | [T4] |
| **A3** | 17 500 m | [T4] |
| **A4** | **17 000 m in 8 min 30 s** — a *time*, so it integrates the whole thrust lapse instead of sampling one point | [T4] |
| **A5** | +8.5 g "in the latest variants" | [T4] ibid., article body |
| **A6** | gross **8 725 kg** (2 × R-3S) · MTOW 8 800 / 9 800 / 10 400 kg by runway · **empty mass not published** | [T4] · **[TODO]** |
| **A7** | R-25-300: **40.18 kN dry / 69.58 kN AB** | [T4] |
| **A8** | wing area **23 m²** · span 7.154 m · length 14.7 m (excl. pitot) · **AR 2.226** [DERIVED] | [T4] |
| Also | take-off run 830 m · landing run with SPS and chute 550 m · landing speed 250 km/h · T/W 0.76 | [T4] |
| RWR | Sirena family | **[TODO]** — the exact set for the *bis* was not sourced this pass |
| Countermeasures | **[TODO]** — the type as built carries none; pod-mounted ASO-2 exists on some | |

**FlightBox row:** `Motion = deck`, `Tier = T3`, and it is
[`flight-model-recipe.md`](flight-model-recipe.md) §5's **turbojet calibration row**.
`SearchRangeM = 20 000` [T4]. `TrackRangeM = 10 000` **[DERIVED]** — the RP-21's sourced detect/lock
ratio of 0.5 applied to the RP-22's sourced 20 km detection, for want of a published lock range.
`SearchAzHalfDeg = 30`, `SearchElHalfDeg = 10` **[DERIVED]** from the sourced 60° × 20° field.
`SearchFrameS = **2.0**` **[DERIVED]** by the tree's own volume/frame relation
([`module.md`](module.md) §Knowledge 2). `LookDown = false` [T4].

**[DISPUTED]:** the published T/W of 0.76 inverts to **9 333 kg**, between the published gross (8 725)
and MTOW (9 800) — a third, unnamed loading. Both carried; the deck takes the primaries.

**Why this row is not a weak MiG-29.** Its ±30° × ±10° radar field is **a quarter of the sky** an F-16's
CRM covers; it cannot look down at all; and its longest-ranged weapon is an 8 km infrared round. It is
therefore an aircraft that **can only be brought to a fight by somebody else** — which is what the
source says about its doctrine and what `set brief_gci` already expresses.

### `mig23` — MiG-23ML / MLD

The best-documented radar in the catalogue and the least internally consistent airframe.

| Radar variant | Search, fighter, high alt. | Search, look-down | Track, fighter | Source |
|---|---|---|---|---|
| Sapfir-23L (1971) | pulse only, **no look-down**; can guide only onto targets above **1 000 m** | — | — | [T4] [Wikipedia, RP-23 Sapfir](https://en.wikipedia.org/wiki/RP-23_Sapfir) |
| Sapfir-23D-III (MiG-23M, 1975) | **45 km** (55 km vs bomber) | **10–20 km** tail-chase; **targets slower than 60 km/h are not detected at all** | — | [T4] ibid. |
| Sapfir-23ML (MiG-23ML) | **65 km** | 25 km | — | [T4] ibid. |
| **Sapfir-23MLA-II / N008 (MiG-23MLD)** | **52 km** (75 km vs bomber) | 23 km; **14 km** for a fighter head-on | **39 km** (52 km vs bomber); look-down 15 km tail-chase | [T4] ibid. |
| **Sapfir-23MLAE / N003E (export MLD, Syria)** | — | **no tail-chase capability at all**, relying on the IRST instead | — | [T4] ibid. |
| N003E scan field | **±30° azimuth, ±6° elevation** | | | [T4] ibid. |
| N008 physical | 360 kg, 1 kW average / 60 kW peak | | | [T4] ibid. |

| Quantity | Value | Source |
|---|---|---|
| Gun | 1 × GSh-23L, 200 rounds | [T4] [Wikipedia, MiG-23](https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23) |
| Missiles | **2 × R-23 or R-24** · 8 × R-60 or 4 × R-73 · 2 × R-13M · 4 × R-3S | [T4] ibid. |
| R-23/R-24 | **35 km**, 222 kg, **25 kg** expanding-rod warhead, ⌀223 mm, Mach 3; **SARH** (R-23R/R-24R) **or IR** (R-23T/R-24T); *"comparable to the AIM-7 Sparrow"* | [T4] [R-23](https://en.wikipedia.org/wiki/R-23_(missile)) |
| **A1** | 2 500 km/h / **M 2.35 at altitude** | [T4] |
| **A2** | 1 400 km/h / **M 1.14 at sea level** | [T4] |
| **A3** | 18 500 m | [T4] |
| **A4** | **230 m/s at sea level** | [T4] |
| **A5** | +8.5 | [T4] |
| **A6** | gross 14 840 kg · MTOW 17 800 kg · internal fuel 4 260 L · **empty mass not published** | [T4] · **[TODO]** |
| **A7** | R-35-300: **83.6 kN / 127.49 kN AB** | [T4] |
| **A8** | wing area **37.35 m² spread / 34.16 m² swept** · span 13.965 / 7.779 m · length 16.7 m | [T4] |
| Also | take-off 450 m · landing 690 m · T/W 0.91 · wing loading 370 kg/m² | [T4] |
| RWR / CM | **[TODO]** both | |

**FlightBox row:** `Motion = deck` on the **spread** planform `[SET]` — the row is used as an
interceptor in W3/O1 and the spread wing is the acquisition and landing configuration; the bias is
declared (understates supersonic Vmax, overstates the swept-wing turn). `Tier = T3`.
The **N003E export set** is taken `[SET]`, because O1's anchor is the Syrian force and the source names
that set for exactly those aircraft: `SearchRangeM = 52 000`, `TrackRangeM = 39 000`,
`LookDownRangeM = 14 000` (fighter head-on) [T4], `SearchAzHalfDeg = 30`, `SearchElHalfDeg = 6` [T4] →
`SearchFrameS = **2.0**` [DERIVED]. The full-capability MLA-II figures stay in the table above and a
mission may declare them.

**[DISPUTED], twice, in opposite directions:** the published T/W inverts to **14 281 kg**, the published
wing loading to **13 820 kg**, and the published gross is **14 840 kg** — three masses, none of them
named. Together with the missing empty mass this is **the least internally consistent row in the
catalogue** ([`flight-model-recipe.md`](flight-model-recipe.md) §Knowledge 1).

**Why the low-altitude number is the whole row.** *"Targets slower than 60 km/h are not detected"* and
*"only above 1 000 m"* on the D-III, against **14 km head-on look-down** on the MLD: the difference
between those two sets is the difference between a strike package that can go under the radar and one
that cannot, and both numbers are sourced.

### `mig25` — MiG-25PD, the interceptor with reach and no turn

| Quantity | Value | Source |
|---|---|---|
| Radar (P) | **Smerch-A2**, vacuum-tube; scanning **40 / 80 / 120 km**; **tracking 50–70 km** against fighter-sized, up to 105 km against bomber-sized at high altitude; operational from **500 m** | [T4] [Wikipedia, MiG-25](https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-25) |
| Radar (PD) | **RP-25M Saphir-25**, pulse-Doppler, semiconductor, derived from the MiG-23ML's RP-23ML; **110–120 km** detection | [T4] ibid. |
| **IRST** | **TP-26Sh** (PD): **25 km** lock-on at low altitude against afterburning targets, **50 km+** at high altitude; can slave IR missiles for a sneak attack | [T4] ibid. |
| RWR | **SPO-10M Sirena-3** (SPO-15L Beryoza on the PDSG) | [T4] ibid. |
| Countermeasures | **2 × BVP-50-60** with KDS-155 cassettes, **30 rounds each** (PPR-50 chaff / PPI-50 flares) — **PDSG/PDSL only** | [T4] ibid. |
| Datalink / GCI | **Lazur** (BAN-75 on the PD); **Vozdukh-1 GCI**; SRZO-2M IFF | [T4] ibid. |
| Missiles | 4 × R-40R/RD **or** 2 × R-40T/TD (inboard pylons only); 4 × R-60/R-60M on the PD | [T4] ibid. |
| R-40 | **50–80 km**, 475 kg, **38–100 kg** blast-fragmentation, ⌀310 mm, Mach 2.2–4.5, radar **and active laser** fuzes, **15 g launch overload**; SARH (RD) or IR (TD) | [T4] [R-40](https://en.wikipedia.org/wiki/R-40_(missile)) |
| **A1** | 3 000 km/h / **Mach 3.1** at high altitude | [T4] |
| **A2** | 1 300 km/h at sea level | [T4] |
| **A3** | **20 700 m with four missiles**, 24 000 m with two | [T4] |
| **A4** | 208 m/s; **20 000 m in 8 min 54 s** | [T4] |
| **A5** | **+4.5 g safety limit** (to avoid aileron reversal — the wingtips flexed 70 cm and it flat-spun), ~11 g structural | [T4] |
| **A6** | empty **20 000 kg** · gross **36 720 kg** | [T4] |
| **A7** | 2 × R-15B-300: **73.5 kN / 100.1 kN AB** | [T4] |
| **A8** | wing area **61.4 m²** · span 14.01 m · length 23.82 m | [T4] |
| Probes | T/W 0.55 → **0.556** ✔ · wing loading 598 → **598.0** ✔ — **both exact** | [T4] |

**FlightBox row:** `Motion = deck`, `Tier = T3`. `SearchRangeM = 120 000`, `TrackRangeM = **50 000**`
[T4] — the **lower** bound of the sourced 50–70 km, stated. `SearchAzHalfDeg` `[TODO]` → ±60,
`SearchFrameS = 4.0` [DERIVED]. IRST per the sourced 25/50 km pair into the existing `FBIrstSystem`
aspect law. `Countermeasures` present **only** if the mission declares the PDSG/PDSL variant; the
default row has **none**, which is what the base PD had.

**The `+4.5 g` is the row.** Every other fighter in this catalogue is a +7 to +9 g airframe. A
catalogue cell with a limiter at 4.5 g **cannot turn with anything**, and that is a sourced property of
the real aeroplane rather than a modelling weakness — the exact case
[`module.md`](module.md) §Spec 11's attribution test exists to distinguish. It reaches Mach 3.1, it sees
120 km, it shoots an 80 km missile, and it loses every turning fight it enters.

**[TODO], named loudly:** this row is [`flight-model-recipe.md`](flight-model-recipe.md) R2 — the R-15
is outside the turbojet reference's calibration range by a full Mach number at exactly its most
important anchor. **It will be the recipe's worst row and that is predicted, not discovered.**

### `mig17` — MiG-17F, guns and eyes

| Quantity | Value | Source |
|---|---|---|
| Radar | **none.** The F is a day fighter; the PF carries Izumrud and is a different row nobody asked for | [T4] [Wikipedia, MiG-17](https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-17) |
| Guns | 2 × **NR-23** (80 rounds each) + 1 × **N-37** (40 rounds) | [T4] ibid. |
| Stores | 2 pylons, 500 kg; UB-16-57 rocket pods, 2 × 250 kg bombs; some versions 3 × NR-23 and 2 × K-5 | [T4] ibid. |
| **A1** | 1 145 km/h / **M 0.93 at 3 000 m with reheat** | [T4] |
| **A2** | 1 100 km/h / **M 0.89 at sea level** | [T4] |
| **A3** | 16 600 m | [T4] |
| **A4** | 65 m/s | [T4] |
| **A5** | **+8** | [T4] |
| **A6** | empty **3 919 kg** · gross 5 340 kg · MTOW 6 069 kg | [T4] |
| **A7** | VK-1F: **26.5 kN / 33.8 kN AB** | [T4] |
| **A8** | wing area **22.6 m²** · span 9.628 m · length 11.264 m | [T4] |
| RWR / CM | **none / none** | [T4], by absence |

**BUILT NOTE:** the N-37 is a `core/FBGun.h` row and is NOT what this aeroplane flies — `modules/air`
composes ONE gun slot, so the row flies its NR-23 pair and the heavy cannon is stated rather than
silently dropped. A second barrel group per airframe is a `weapons/` change, not a catalogue one.

**FlightBox row:** `Motion = deck`, `Tier = **T2**`. `SearchRangeM = 0` — acquisition is `FBAirEye`, the
`FBVisualSystem` verbatim, and therefore **3 784 m beam-on, 2 493 m head-on, zero at night**, measured
and not set. Two new `core/FBGun.h` rows (`nr23`, `n37`) with the sourced round counts; muzzle velocity
and rate of fire **[TODO]**.

**[DISPUTED]:** wing loading 268.5 kg/m² closes at **MTOW** (6 069/22.6 = 268.5 exactly) while T/W 0.63
closes at gross. The two published derived figures are quoted at two different weights; both carried.

**This is the catalogue's cheapest deck and its most honest row.** It has no sensor to model, no
missile envelope, no receiver, no dispenser. Everything about it that decides is in its eight
performance anchors and its two guns — which is exactly what "grob where it does not decide" means.

### `su7` — Su-7BKL, the strike aircraft of O3

| Quantity | Value | Source |
|---|---|---|
| Radar | **none** air-to-air (SRD-5M ranging set only) | **[TODO]** — the ranging set was not sourced this pass |
| Guns | 2 × **NR-30** (70 rounds each) | [T4] [Wikipedia, Su-7](https://en.wikipedia.org/wiki/Sukhoi_Su-7) |
| Stores | 4 wing + 2 fuselage, **2 000 kg**; FAB-250, FAB-500, UB-16-57; 2 stations reserved for tanks | [T4] ibid. |
| **A1** | 2 150 km/h / **M 1.74 at high altitude** | [T4] |
| **A2** | 1 150 km/h / **M 0.94 at sea level** | [T4] |
| **A3** | 17 600 m | [T4] |
| **A4** | 160 m/s | [T4] |
| **A5** | **[TODO]** | |
| **A6** | empty **8 940 kg** · gross 13 570 kg · MTOW 15 210 kg · fuel 3 220 kg | [T4] |
| **A7** | AL-7F-1: **66.6 kN / 94.1 kN AB** | [T4] |
| **A8** | wing area **34 m²** · span 9.31 m · length 16.8 m | [T4] |

**FlightBox row:** `Motion = deck`, `Tier = T2`. `SearchRangeM = 0`, the eye. `fab250`/`fab500` are
**already `core/FBStore.h` rows with flown decks** (`C8`, built) — so this row's whole air-to-ground
loadout costs nothing. New gun row `nr30`.

**[DISPUTED]:** wing loading 434.8 kg/m² inverts to **14 783 kg**, between gross and MTOW.

**A7's g limit being `[TODO]` is a hard consequence, not a footnote:** with no published g limit and no
FLCS in the deck, the row's α limiter is `[SET]` from its own measured `CLmax`
([`flight-model-recipe.md`](flight-model-recipe.md) §6) and the setting is logged. Until it is measured
this row is `ALPHA`.

### `su22` — Su-17M4 / Su-22M4 (and the Su-20 of O1/O3)

**The Su-20 is the export Su-17, and the Su-22 the export Su-17M** — so one row covers the type O1 and
O3 both name, and the spec is the Su-17M4's.

| Quantity | Value | Source |
|---|---|---|
| Radar | none air-to-air; **Klen-PS laser ranger / marked-target seeker** on the M4 | **[TODO]** |
| Guns | 2 × NR-30, 80 rounds each; UPK-23 / SPPU-22 gun pods | [T4] [Wikipedia, Su-17](https://en.wikipedia.org/wiki/Sukhoi_Su-17) |
| AAM | K-13 · R-60 · R-73 (self-defence only) | [T4] ibid. |
| ASM / ARM | Kh-23, Kh-25ML, Kh-29L/T/D; **Kh-58, Kh-27PS, Kh-28** anti-radiation | [T4] ibid. |
| Stores | 10 hardpoints, **4 000 kg** | [T4] ibid. |
| **A1** | 1 860 km/h at altitude | [T4] |
| **A2** | 1 400 km/h / **M 1.13 at sea level** | [T4] |
| **A3** | 14 200 m | [T4] |
| **A4** | 230 m/s | [T4] |
| **A5** | **+7** | [T4] |
| **A6** | empty **12 160 kg** · gross 16 400 kg · MTOW 19 430 kg · fuel 3 770 kg | [T4] |
| **A7** | AL-21F-3: **76.4 kN / 109.8 kN AB** | [T4] |
| **A8** | wing area **38.5 m² spread / 34.5 m² swept** · span 13.68 / 10.02 m · length 19.02 m | [T4] |

**FlightBox row:** `Motion = deck` on the **spread** planform `[SET]` (a strike aircraft in O3 flies its
attack run configured for load, not for dash); `Tier = T2`. `SearchRangeM = 0`.

**[DISPUTED]:** wing loading 443 kg/m² inverts to **17 056 kg on the spread wing** — between gross and
MTOW — **or** to 15 284 kg on the swept wing. The swing-wing ambiguity of
[`flight-model-recipe.md`](flight-model-recipe.md) R8 shows up here in arithmetic before it shows up in
a deck.

**The anti-radiation stores are real and one of them is already built:** `agm88` exists as a
`FBSeekerKind` whose seeker **is** the RWR ([`../../air-to-ground.md`](../../air-to-ground.md), built).
A Kh-28-class row is that mechanic with different numbers, and it is **the only place in this catalogue
where an eastern row could do SEAD** — named here, not built.

---

## The remaining fighters

### `su27` — Su-27S, the peer eastern opponent

**No campaign file names this type.** It is in the catalogue because the round asked for it, and that
is stated rather than dressed up: it is capability without a question.

| Quantity | Value | Source |
|---|---|---|
| Radar | **N001 Mech** (NATO *Slot Back II*), 1.075 m twist-Cassegrain, 3 cm pulse-Doppler, medium and high PRF | [T4] [Wikipedia, Mech radar](https://en.wikipedia.org/wiki/Mech_radar) |
| Search, head-on | **80–100 km against a 3 m² target**; 140 km against a large bomber | [T4] ibid. — **the only radar figure in the catalogue that states its reference RCS** |
| Track | **65 km** against 3 m² | [T4] ibid. |
| Search, **pursuit** | **40 km** against 3 m² | [T4] ibid. |
| Azimuth limits | **±60°** | [T4] ibid. |
| IRST / EO | OEPS-27 / **OLS-27** with laser rangefinder; Shchel-3UM helmet-mounted sight | [T4] [Wikipedia, Su-27](https://en.wikipedia.org/wiki/Sukhoi_Su-27) |
| RWR | **SPO-15** — the same set the MiG-29 module already models in full | [T4] ibid. |
| Gun / missiles | GSh-30-1, **150 rounds**; 6 × R-27R/ER/T/ET/P/EP; 6 × R-73E; 10 pylons, 4 430 kg | [T4] ibid. |
| **A1** | 2 500 km/h / **M 2.35 at altitude** | [T4] |
| **A2** | 1 400 km/h / **M 1.13 at sea level** | [T4] |
| **A3** | **18 500 m** clean, 17 750 m with load | [T4] |
| **A4** | 300 m/s | [T4] |
| **A5** | **+9** | [T4] |
| **A6** | empty 16 380 kg · gross **23 430 kg** · MTOW 33 000 kg · internal fuel 9 400 kg | [T4] |
| **A7** | 2 × AL-31F: **75.22 kN / 122.6 kN AB** | [T4] |
| **A8** | wing area **62 m²** · span 14.7 m · length 21.9 m | [T4] |
| Probes | T/W 1.07 → **1.067** ✔ · wing loading 377.9 → **377.9** ✔ — **the cleanest row in the catalogue** | [T4] |

**FlightBox row:** `Motion = deck`, `Tier = **T4**`. **RCS conversion, because this row makes it
possible:** the source states its ranges against **3 m²**, and the tree's radar reach scales as `σ^¼`
against the F-16's 1.2 m² reference. So

```
R(1.2 m²) = R(3 m²) · (1.2/3)^¼ = R(3 m²) · 0.795          [DERIVED]
search head-on   100 km → 79.5 km
track            65 km  → 51.7 km
search pursuit   40 km  → 31.8 km
```

**This is the only row where that conversion can be done rather than assumed**, and it is a warning
about the others: every unconverted range in this catalogue is quoted against an unstated target and is
therefore uncertain by whatever `σ^¼` factor the source's reference implies.

**Everything else on this row already exists:** `r27r`, `r73` and `gsh301` are built
`core/FBStore.h` / `core/FBGun.h` rows with flown decks and measured behaviour; the SPO-15 is
`FBMig29Rwr`; the IRST is `FBIrstSystem` with the KOLS's own aspect law re-parameterised. **The Su-27 row
is a MiG-29's avionics on a bigger airframe with a longer-reaching radar**, and the catalogue says so
instead of pretending it is a new system.

### `mirf1` — Mirage F1C

| Quantity | Value | Source |
|---|---|---|
| Radar | **Thomson-CSF Cyrano IV**, monopulse, angular accuracy **2°**; MTI and >40 dB clutter rejection giving **look-down/shoot-down** against low-flying targets | [T3] [aviationsmilitaires, Cyrano IV](https://aviationsmilitaires.net/v3/kb/radar/show/217/thomson-csf-cyrano-iv) · [flyajetfighter](https://www.flyajetfighter.com/the-mirage-f1s-cyrano-iv-radar-the-cornerstone-of-all-weather-interception/) |
| Air-to-air range | **100 km**; a second source gives **96 km** | [T3]/[T4] **[DISPUTED]**, both carried |
| Guns | 2 × **DEFA 553** 30 mm, **150 rounds per gun** | [T4] [Wikipedia, Mirage F1](https://en.wikipedia.org/wiki/Dassault_Mirage_F1) |
| Missiles | 2 × **Super 530F** or R.530 underwing; 2 × **R550 Magic** or AIM-9 on wingtips | [T4] ibid. |
| Super 530F | **25 km**, **SARH**, 245 kg, **30 kg** HE-frag, ⌀263 mm, Mach 4.5 | [T4] [Super 530](https://en.wikipedia.org/wiki/Super_530) |
| R550 Magic 1 | **10 km**, IR, 89 kg, **12.7 kg** pre-fragmented, ⌀157 mm, Mach 3 | [T4] [R550 Magic](https://en.wikipedia.org/wiki/R550_Magic) |
| **A1** | 2 338 km/h / **Mach 2.2 at 11 000 m** | [T4] |
| **A2** | **[TODO]** — no sea-level figure published | |
| **A3** | 20 000 m | [T4] |
| **A4** | 243 m/s | [T4] |
| **A5** | **[TODO]** | |
| **A6** | empty 7 400 kg · clean take-off **10 900 kg** · MTOW 16 200 kg | [T4] |
| **A7** | Atar 9K-50: **49.03 kN / 70.6 kN AB** | [T4] |
| **A8** | wing area **25 m²** · span 8.4 m · length 15.3 m | [T4] |
| Also | CAP endurance **2 h 15 min** with 2 × Super 530 and a centreline tank; T/W 0.66 → **0.660** ✔ | [T4] |
| RWR / CM | **[TODO]** both | |

**FlightBox row:** `Motion = deck`, `Tier = T3`. `SearchRangeM = 96 000` **[SET within the disputed
band]** — the lower of the two, stated. `SearchAzHalfDeg` `[TODO]` → ±60, `SearchFrameS = 4.0`.
`LookDown = true` [T3] — **the only pre-1980 row in the catalogue with a sourced look-down capability**,
which is exactly why W2's Iraqi defence is a different problem from O3's Egyptian one. New store row
`s530f` (SARH, binds to impact) and `magic1` (IR, free at launch), both from the built slender-body
recipe; new gun row `defa553`.

**A2 missing is a real cost**: [`flight-model-recipe.md`](flight-model-recipe.md) §4.2 inverts `CD0` at
**two** supersonic points, and this row supplies one. It stays `ALPHA` on that account until a sea-level
figure is sourced.

### `f5e` — F-5E Tiger II, the recipe's validation row

**No campaign file names this type either** — W1's aggressors are, by its own anchor, F-16s emulating a
Fulcrum. It is in the catalogue for the round's sake **and** because it carries a number nothing else
does.

| Quantity | Value | Source |
|---|---|---|
| Radar | AN/APQ-153 (early) / **AN/APQ-159** (later F-5E) / APG-69 (US Navy aggressor) | [T4] [Wikipedia, Northrop F-5](https://en.wikipedia.org/wiki/Northrop_F-5) |
| Radar range | **[TODO]** — not published in the read sources | |
| Guns | 2 × **M39A2** 20 mm, **280 rounds per gun** | [T4] ibid. |
| Missiles | 2 × AIM-9 on wingtips (the initial F-5E loadout); 7 hardpoints, 7 000 lb | [T4] ibid. |
| **A1** | **Mach 1.63 at 36 000 ft**; max cruise M 0.98, economical cruise M 0.8 | [T4] |
| **A2** | **[TODO]**; `Vne` 710 kt IAS | [T4] |
| **A3** | 51 800 ft (41 000 ft one engine out) | [T4] |
| **A4** | 34 500 ft/min | [T4] |
| **A5** | **[TODO]** | |
| **A6** | empty 9 583 lb · gross **15 745 lb** clean · MTOW 24 675 lb · internal fuel 677 US gal | [T4] |
| **A7** | 2 × J85-GE-21: **3 500 lbf / 5 000 lbf AB** | [T4] |
| **A8** | wing area **186 ft²** · span 26 ft 8 in (27 ft 11.9 in with tip missiles) · length 48 ft 2.25 in · **AR 3.86** | [T4] |
| **Zero-lift drag coefficient** | **C_D0 = 0.0200** | [T4] — **the only published drag coefficient in the catalogue** |
| **Lift-to-drag ratio** | **10.0** | [T4] — with the line above, the pair that derives `e` |
| Also | stall 124 kt (50 % fuel, flaps and gear down) · take-off run 610 m · landing 1 128 m without chute, 762 m with · T/W 0.40 → **0.405** ✔ · wing loading 133 → **132.7** ✔ | [T4] |

**FlightBox row:** `Motion = deck`, `Tier = **T2**`. `SearchRangeM` `[TODO]` — **and the honest
consequence is that until it is sourced this row acquires through `FBAirEye` like `mig17`**, exactly as
[`../ground/catalogue.md`](../ground/catalogue.md)'s `zu23` acquires through the eye rather than through
an invented radar range. The row therefore *works* without the number, and the number's absence changes
what the row is rather than breaking it. New gun row `m39a2`; `aim9` is an existing store.

**This row is [`flight-model-recipe.md`](flight-model-recipe.md)'s validation cell.** Its published
`CD0` and `(L/D)max` derive the whole catalogue's `e = 0.66` [DERIVED], and its A1 anchor is the one
place the recipe's `CD0` inversion can be checked against a published `CD0` (§Knowledge 2 there: the
inversion gives 0.0351 supersonic against 0.0200 subsonic, a ×1.75 transonic rise). **Both probes close
to 0.3 %.** It is the best-posed row in the catalogue and the campaigns do not need it — which is the
argument for building it first anyway.

---

## The mover rows

Eight rows with no flight model, by the two-part test of [`module.md`](module.md) §Spec 4: their
manoeuvre does not decide, and their drag polar is not published. What decides about each of them is its
**speed, its altitude, its presented size and whether it radiates.**

### `kc135` — KC-135R Stratotanker (and O1's Boeing 707 ECM aircraft)

| Quantity | Value | Source |
|---|---|---|
| Crew | 3 (pilot, co-pilot, **boom operator**) | [T4] [Wikipedia, KC-135](https://en.wikipedia.org/wiki/Boeing_KC-135_Stratotanker) |
| Max / cruise speed | **504 kt / Mach 0.9**; cruise 530 mph at 30 000 ft | [T4] ibid. |
| Ceiling / climb | 50 000 ft / 4 900 ft/min | [T4] ibid. |
| Masses | operating empty 124 000 lb · gross 297 000 lb · MTOW 322 500 lb · **fuel 200 000 lb** | [T4] ibid. |
| Powerplant | 4 × F108-CF-100, 96.2 kN | [T4] ibid. |
| Geometry | wing area **2 433 ft²** · span 130 ft 10 in · length 136 ft 3 in | [T4] ibid. |
| Offload | **150 000 lb of transferable fuel at a 1 500 mi radius** | [T4] ibid. |

**FlightBox row:** `Motion = mover`, `Tier = **T0**`, no radar, no receiver, no weapon.

**The boom does not exist (`C5`), and naming the row does not move it one metre.** The tanker is a
`protect` asset that cannot give fuel, so W2's defining constraint — 1 600 km on internal fuel and tanks
that ran dry — stays unexpressible. That is the same sentence
[`../../campaigns/w2-osirak.md`](../../campaigns/w2-osirak.md) already carries, repeated where the row
is.

**And it is O1's jammer for free.** The Boeing 707 ECM aircraft that
[`../../campaigns/o1-bekaa-1982.md`](../../campaigns/o1-bekaa-1982.md) calls *"the decisive mechanism,
and the one that is furthest out of reach"* is the same airframe family, and in the tree as it stands
both reduce to *a large subsonic jet on a track with a published `jam_comm_m` ring and no weapon*. So
O1's 707 is `module kc135` + `set jam_comm_m <m>` (`C24`, built) and costs **zero new rows**. What it
does not buy is the radar-jamming half (`C13`) — the part the anchor actually turns on.

### `tu95` — Tu-95MS, the intercept subject with a tail gun

| Quantity | Value | Source |
|---|---|---|
| Crew | 6–7, **including a tail gunner** | [T4] [Wikipedia, Tu-95](https://en.wikipedia.org/wiki/Tupolev_Tu-95) |
| Max / cruise speed | **925 km/h** / 710 km/h | [T4] ibid. |
| Ceiling / climb | 13 716 m / 10 m/s | [T4] ibid. |
| Range | 15 000 km | [T4] ibid. |
| Masses | empty 90 000 kg · gross 171 000 kg · MTOW 188 000 kg | [T4] ibid. |
| Powerplant | 4 × NK-12 turboprop, 15 000 PS, contra-rotating propellers | [T4] ibid. |
| Geometry | wing area **310 m²** · span **50.1 m** · length 46.2 m | [T4] ibid. |
| Armament | **2 × 23 mm GSh-23 in a tail turret**; up to 15 000 kg of Kh-20/Kh-22/Kh-55 class missiles | [T4] ibid. |

**FlightBox row:** `Motion = mover`, `Tier = T1` with the drag reflex. **The tail turret is not
modelled** ([`module.md`](module.md) §Gaps A6): it is `FBGunSystem` with a rearward mount az/el and one
hook, and it is not in this round because no campaign's verdict hangs on it — but it is the second thing
to add, because a stern conversion against a bomber that shoots back is a different problem from one
against a bomber that does not.

**Its 50.1 m span is the row's decisive number.** W5's and O2's task is *identification*, and the eye
resolves on presented extent: a Tu-95 is recognised at roughly **seven times** the range of a MiG-21
(span 7.154 m) by the ratio of their extents alone [DERIVED from `FBVisualSystem`'s linear resolution
law]. Nothing about that was set, and it is why the intercept-subject rows are worth having even though
they do nothing.

**The Il-20 ELINT aircraft** that W5 and O2 also name maps onto this class of row and **not** onto a new
mechanism: an ELINT aircraft *receives*, it does not radiate, and FlightBox has no ESM emission to model.
In the tree it is a transport on a track — the `an26` row with different numbers.

### `an26` — An-26, the identification subject

| Quantity | Value | Source |
|---|---|---|
| Crew | 5 | [T4] [Wikipedia, An-26](https://en.wikipedia.org/wiki/Antonov_An-26) |
| Cruise speed | **440 km/h** | [T4] ibid. |
| Ceiling / climb | 7 500 m / 8 m/s | [T4] ibid. |
| Range | 2 500 km (1 100 km at max payload) | [T4] ibid. |
| Masses | empty 15 020 kg · MTOW 24 000 kg | [T4] ibid. |
| Powerplant | 2 × AI-24VT turboprop, 2 103 kW | [T4] ibid. |
| Geometry | wing area 74.98 m² · **span 29.3 m** · length 23.8 m | [T4] ibid. |
| Armament | none | [T4] |

**FlightBox row:** `Motion = mover`, `Tier = **T0**`. **Its decisive quantities are its span (29.3 m,
for the eye) and its transponder state (`set iff_xpdr on|off`, an existing key)** — and that is the
whole of W5's and O2's task. It is the cheapest row in the catalogue and it unblocks the two campaigns
whose success condition contains no weapon.

### `ef111` — EF-111A Raven

| Quantity | Value | Source |
|---|---|---|
| Armament | **none at all** | [T4] [Wikipedia, EF-111A Raven](https://en.wikipedia.org/wiki/General_Dynamics%E2%80%93Grumman_EF-111A_Raven) |
| Max speed | **Mach 2.2** (2 350 km/h) above 30 000 ft | [T4] ibid. |
| Ceiling / climb | 45 000 ft / 11 000 ft/min | [T4] ibid. |
| Masses | empty 55 275 lb · gross 70 000 lb · MTOW 89 000 lb | [T4] ibid. |
| Powerplant | 2 × TF30-P-9, 19 600 lbf | [T4] ibid. |
| Geometry | span **63.0 ft spread / 32.0 ft swept** | [T4] ibid. |
| T/W | 0.598 | [T4] ibid. |

**FlightBox row:** `Motion = mover`, `Tier = T1`, no weapon, `set jam_comm_m` for the comms half.

**Why it is a row of its own and not `kc135` with a different name:** its **flight profile** is different
in kind — supersonic, swing-wing, escorting a strike at strike speed rather than orbiting at 300 kt — and
speed and altitude are precisely the quantities a mover row carries. W3 names it six times.

**Its real job is absent.** The ALQ-99 jammed *radars*; `C13`'s radar half is wholly open, so this row
denies a **link** and nothing else. It is one third of the historical aircraft and the row says so.

### `mi8` / `ah64` — the helicopters

| Quantity | `mi8` (Mi-8MT) | `ah64` (AH-64A/D) | Source |
|---|---|---|---|
| Max / cruise speed | 250 / 240 km/h | 158 / 143 kt (`Vne` 197 kt) | [T4] [Mi-8](https://en.wikipedia.org/wiki/Mil_Mi-8) · [AH-64](https://en.wikipedia.org/wiki/Boeing_AH-64_Apache) |
| Ceiling | 5 000 m | 20 000 ft | [T4] |
| Range | 495 km (960 km ferry) | 257 nmi (with the Longbow mast) | [T4] |
| Masses | empty 7 100 · gross 11 100 · MTOW 13 000 kg | empty 11 387 · gross 17 650 · MTOW 23 000 lb | [T4] |
| Powerplant | 2 × TV3-117MT, 1 454 kW | 2 × T700-GE-701, 1 690 shp (1 890 from 1990) | [T4] |
| Armament | 4 000 kg on six hardpoints: S-5 rockets, bombs, 9M17 ATGM, 1–2 side PK MGs | M230 30 mm with 1 200 rounds; Hydra 70; Hellfire; AIM-92 Stinger on the D's wingtips | [T4] |
| Radar | none | **AN/APG-78 Longbow** on the D/E only; range **[TODO]** | [T4] |

**FlightBox rows:** `Motion = mover`, `Tier = T1`, **no weapon modelled** on either. A rotorcraft is
outside the recipe's fixed-wing set entirely, so a deck was never a candidate.

**The one thing they do that decides, and it is free:** they are **low and slow**, so they sit inside the
Doppler notch of every radar in the tree (`sensors.md` §4.7 — a target below `DopplerNotchMs` is not
detected by a set that declares one). A 240 km/h helicopter is **66.7 m/s**, and any aspect that is not
nearly head-on puts its radial rate below every notch value the tree carries. **A helicopter is
therefore a visual and infrared target and not a radar one, without a line being written for it.**

---

## What the catalogue asks the rest of the tree for

**No new architecture, no new seeker kind, no new health id, no new emitter kind.** The list is
finite and it is here so nobody has to reconstruct it from eighteen rows.

| New | Count | Rows | Note |
|---|---:|---|---|
| `core/FBStore.h` rows | **7** | `k13` `r60` `r24r`/`r24t` `r40r`/`r40t` `aim7` `s530f` `magic1` | all three seeker kinds already exist. Four are SARH (`FBSeekerKind::SemiActiveRadar`, built), three IR (`Infrared`, built) |
| FlightBox-own missile decks | **7** | as above | from the **built** slender-body recipe (`weapons.md` §10.2), sized per row from published diameter, mass and terminal Mach — the identical procedure that produced the six ground rounds |
| `core/FBGun.h` rows | **6** | `gsh23l` `nr23` `n37` `nr30` `defa553` `m39a2` | round counts are sourced for all six; **muzzle velocity, rate of fire and round mass are `[TODO]` for all six** and are the catalogue's largest single gap |
| Reused **unchanged** | — | `aim9` `aim120` `fab250` `fab500` `m61a1` `gsh301` `r27r` `r73` | eight existing rows serve the F-15C, the Su-27 and the two strike rows outright |
| New `FBSeekerKind` | **0** | — | |
| New `FBEmitterKind` | **0** | — | and the cost is stated: an AWACS radiates as `AirborneFireControl`, so a receiver cannot tell it from a fighter |
| New `FBSystemId` | **0** | — | `Engine2` already exists for the six twin-engine rows |
| New author-facing `set` keys | **2** | `orbit`, `drag_threat_s` | [`module.md`](module.md) §Spec 9 |
| New sensor slot | **1** | `sensors/FBNetLinkSystem : FBDatalinkSystem` | [`../../air-defence-network.md`](../../air-defence-network.md) §2 design B, made due. **Not** a seventh registry reader |

---

## What the flight-model round measured back into this file

Three of this file's numbers are now constrained by something other than their source, and the
constraint is written here because a later reader will otherwise re-derive it:

| Number | What the measurement says |
|---|---|
| **the published maximum rate of climb**, nine rows | it names no weight, and at the GROSS weight every other anchor in this file is quoted at it is unreachable on nine of ten rows — **impossible at ANY loading** on `f15c`, `su22` and `mirf1`, whose inverted weights fall below their own empty weights. `mig17` alone reaches its 65 m/s at gross. `[DISPUTED]` in the strict sense: the figure and the mass table cannot both be right |
| **`mig23`'s and `su22`'s spread planform** | the choice costs **+25.7 %** and **+28.1 %** on the service ceiling, measured. It was declared in advance as a bias (recipe R8) and this is its number |
| **`mig17`'s 8.0 g** | reachable by the airframe and NOT by the limiter holding it: the proportional limiter's droop is 1.2° of α on the row with the largest derived pitch authority, worth −11.2 % of g. The row, not the source, is what stays `ALPHA` |

---

## Disputes left standing

| # | Subject | The two values | FlightBox |
|---|---|---|---|
| D1 | `mig21` mass | published T/W 0.76 inverts to **9 333 kg**; published gross **8 725 kg**; MTOW 8 800 / 9 800 / 10 400 by runway surface | takes the primaries; the probe miss is recorded |
| D2 | `mig23` mass | T/W inverts to **14 281 kg**, wing loading to **13 820 kg**, published gross **14 840 kg** — three masses, none named, and no published empty mass at all | takes the primaries; **the least consistent row in the catalogue** |
| D3 | `mig17` mass | wing loading closes at **MTOW**, T/W at **gross** | both carried; the two published derived figures are simply quoted at different weights |
| D4 | `su7` mass | wing loading inverts to **14 783 kg**, between gross 13 570 and MTOW 15 210 | takes the primaries |
| D5 | `su22` mass **or** planform | wing loading inverts to **17 056 kg** spread or **15 284 kg** swept | takes gross on the spread wing; the ambiguity is R8's, seen in arithmetic |
| D6 | `mirf1` radar range | **100 km** [T3] vs **96 km** [T4] | takes 96 km, the lower, stated |
| D7 | `mig23` radar set | five documented variants spanning 45–65 km search and 10–25 km look-down | takes the **N003E export set** (52 / 39 / 14 km), because O1's anchor is the Syrian force and the source names that set for those aircraft |
| D8 | `mig25` track range | **50–70 km** against fighter-sized | takes 50 km, the lower bound, stated |
| D9 | `k13` engagement range | the read source gives **0.97–3.5 km**, which is short against commonly quoted R-13M figures | carried as read, marked **[TODO]** for a second source |
| D10 | `e3` detection | **400 km** low-flyer (pulse-Doppler) vs **650 km** at medium/high (beyond-the-horizon pulse) | takes 400 km — **different modes against different target classes**, not a disagreement |

## What is not sourced at all

| Field | Rows | Status |
|---|---|---|
| Radar scan period | **every row except `e3`** | `[DERIVED]` by the tree's own volume/frame relation ([`module.md`](module.md) §Knowledge 2) |
| Radar azimuth/elevation field | all but `mig21`, `mig23`, `su27`, `e3` | `[TODO]` → the tree's own ±60° fighter default, and the row says so |
| Radar detection range | `f5e`, `su7`, `su22`, `ah64` | `[TODO]` — and for `f5e`/`su7`/`su22` the **eye binds instead**, which is a design and not a hole |
| **Target RCS the range was measured against** | **every row except `su27`** | the catalogue's largest quiet uncertainty; §`su27` shows the conversion that the others cannot get |
| Radar cross-section of the row itself | **all eighteen** | `[TODO]`, with the reason [`../ground/cast.md`](../ground/cast.md) already gave: two measured cross-sections exist in the tree and inventing sixteen more would not be cheap to spot |
| Gun muzzle velocity, rate of fire, round mass | all six new gun rows | **FILLED AT [T4] BY THE BUILD ROUND (2026-07-28)**, because a gun cannot be built without them and six rows that compile and do nothing are worse than six rows at the tier the rest of this file's weapon half sits at. `gsh23l` 715 m/s / 3 400 rpm / 0.175 kg · `nr23` 690 / 850 / 0.200 · `n37` 690 / 400 / 0.735 · `nr30` 780 / 900 / 0.410 · `defa553` 815 / 1 300 / 0.275 · `m39a2` 1 030 / 1 500 / 0.100. Dispersion is [SET] at the M61A1's measured sigma for all six, as the two ground guns already take it. A [T1] source would move these, and each multiplies its row's kinetic damage linearly |
| Warning-receiver type | `mig21` `mig23` `mirf1` `f5e` `su22` `tu95` `ef111` `ah64` | `[TODO]` |
| Countermeasure fit | all but `f15c` and `mig25` | `[TODO]` |
| IRST reach | all but `mig25` | `[TODO]` |
| Empty mass | `mig21`, `mig23` | `[TODO]` — and it costs both rows the mass-closure probe |
| g limit | `su7`, `mirf1`, `f5e` | `[TODO]` — and it costs the α limiter its derivation |
| Sea-level Vmax | `mirf1`, `f5e` | `[TODO]` — and it costs the polar its second inverted point |
| Rotodome rate | `e2c` | `[TODO]`, `[SET]` at the E-3's sourced 6 rpm |

## Sources

| Tier | Document |
|---|---|
| [T3] | [aviationsmilitaires — Thomson-CSF Cyrano IV](https://aviationsmilitaires.net/v3/kb/radar/show/217/thomson-csf-cyrano-iv) · [flyajetfighter — The Mirage F1's Cyrano IV radar](https://www.flyajetfighter.com/the-mirage-f1s-cyrano-iv-radar-the-cornerstone-of-all-weather-interception/) |
| [T4] | Wikipedia aircraft specification templates: [E-3 Sentry](https://en.wikipedia.org/wiki/Boeing_E-3_Sentry) · [E-2 Hawkeye](https://en.wikipedia.org/wiki/Northrop_Grumman_E-2_Hawkeye) · [F-15 Eagle](https://en.wikipedia.org/wiki/McDonnell_Douglas_F-15_Eagle) · [MiG-21](https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-21) · [MiG-23](https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-23) · [MiG-25](https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-25) · [MiG-17](https://en.wikipedia.org/wiki/Mikoyan-Gurevich_MiG-17) · [Su-7](https://en.wikipedia.org/wiki/Sukhoi_Su-7) · [Su-17](https://en.wikipedia.org/wiki/Sukhoi_Su-17) · [Su-27](https://en.wikipedia.org/wiki/Sukhoi_Su-27) · [Mirage F1](https://en.wikipedia.org/wiki/Dassault_Mirage_F1) · [Northrop F-5](https://en.wikipedia.org/wiki/Northrop_F-5) · [KC-135](https://en.wikipedia.org/wiki/Boeing_KC-135_Stratotanker) · [Tu-95](https://en.wikipedia.org/wiki/Tupolev_Tu-95) · [An-26](https://en.wikipedia.org/wiki/Antonov_An-26) · [EF-111A Raven](https://en.wikipedia.org/wiki/General_Dynamics%E2%80%93Grumman_EF-111A_Raven) · [Mi-8](https://en.wikipedia.org/wiki/Mil_Mi-8) · [AH-64 Apache](https://en.wikipedia.org/wiki/Boeing_AH-64_Apache) |
| [T4] | Radars: [RP-21 Sapfir](https://en.wikipedia.org/wiki/RP-21_Sapfir) · [RP-23 Sapfir](https://en.wikipedia.org/wiki/RP-23_Sapfir) · [Mech radar (N001)](https://en.wikipedia.org/wiki/Mech_radar) · [APG-63 and APG-70](https://en.wikipedia.org/wiki/APG-63_and_APG-70) · [flyshark, Sapfir-21 / RP-22](http://www.flyshark.ayz.pl/Stacja_angielska/sapfir.htm) |
| [T4] | Weapons: [K-13](https://en.wikipedia.org/wiki/K-13_(missile)) · [R-60](https://en.wikipedia.org/wiki/R-60_(missile)) · [R-23/R-24](https://en.wikipedia.org/wiki/R-23_(missile)) · [R-40](https://en.wikipedia.org/wiki/R-40_(missile)) · [AIM-7 Sparrow](https://en.wikipedia.org/wiki/AIM-7_Sparrow) · [AIM-9 Sidewinder](https://en.wikipedia.org/wiki/AIM-9_Sidewinder) · [Super 530](https://en.wikipedia.org/wiki/Super_530) · [R550 Magic](https://en.wikipedia.org/wiki/R550_Magic) |

**Identified and NOT read** — the [T1] material that would move the load-bearing numbers: USAF and
NATO flight manuals for the western rows (the F-15C's `T.O. 1F-15C-1` and the `SAC` performance
supplement the article already cites once for its climb figure); the German `T.O. 1F-MIG29-1`, whose
existence [`../mig29/flight-model-spec.md`](../mig29/flight-model-spec.md) §1.1 already names as *"the
highest-value single acquisition"* and which covers the eastern airframe family's format; the US Army
TRADOC *Worldwide Equipment Guide* for the Soviet radar and missile figures; and the CIA reading room's
Soviet air-force assessments. **Naming them is the honest form of the gap**, and the direction is
predictable: a [T1] source would most likely move the **radar ranges** (which are quoted here against
unstated target sizes) and the **missile envelopes**, not the flight-performance anchors, which are the
best-sourced half of this file.
