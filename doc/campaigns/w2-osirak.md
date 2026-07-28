# W2 — Osirak 1981 (Operation Opera)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Operation Opera, 7 June 1981 | §Knowledge 1, every fact cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare and what the F-16 module can do | [`../missions/weapons.md`](../missions/weapons.md), [`../missions/syntax.md`](../missions/syntax.md), [`../missions/verdict.md`](../missions/verdict.md), [`../modules/f16/module.md`](../modules/f16/module.md), [`../modules/f16/weapons.md`](../modules/f16/weapons.md), [`../pilot.md`](../pilot.md), [`../fdm.md`](../fdm.md) |

Confidence legend and gap IDs `C0…C21`: [`INDEX.md`](INDEX.md).

**Temporal honesty: none needed — this is the F-16's own raid.** Eight F-16A **Netz** flew it, and it
is the type's first combat sortie of consequence [T4]. What does NOT match is the *model*: FlightBox
flies the pinned vanilla JSBSim F-16 (a Block-50-class jet with an F100-PW-229, see
[`../modules/f16/flight-model.md`](../modules/f16/flight-model.md)), not an F-16A of 1981. The
campaign therefore reproduces the **profile and its constraints**, not the airframe's own performance
of that day; every fuel number below is a FlightBox number to be measured, never a claim about the
1981 jet.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Date / launch time | **7 June 1981, 15:55 local (12:55 GMT)** | [T4] |
| Departure | **Etzion AB**, Sinai (≈ 29.94 N 34.87 E — *approximate, verify*) | [T4] |
| Target | Osirak / "Tammuz 1" reactor, Al-Tuwaitha (≈ 33.20 N 44.52 E — *approximate, verify*) | [T4] |
| Strike package | **8 × F-16A** | [T4] |
| Escort | **6 × F-15A**; two in close escort, the rest dispersed as a diversion | [T4] |
| Weapons | **2 × Mk-84 (2,000 lb) per F-16, delay-action fuzing** → 16 released, "at least eight" struck the dome | [T4] |
| Distance | **over 1,600 km** for the round trip (≈ 1,090 km straight-line each way) | [T4] |
| Airspace crossed | Jordanian and Saudi Arabian | [T4] |
| Ingress altitude | **30 m** [T4, Wikipedia] / **240 m** [T4, Defence Aviation] — **[DISPUTED]**, both stated, neither preferred | [T4]/[DISPUTED] |
| Fuel | external tanks **exhausted en route** and jettisoned over the Saudi desert; the package flew near the limit of its range | [T4] |
| Pop-up | climb to **2,100 m at 20 km from the target** | [T4] |
| Attack | **35° dive**, **1,100 km/h (≈ 594 kt)**, release at **1,100 m**, pairs at **5-second spacing** | [T4] |
| Time over target | **under 2 minutes** | [T4] |
| Defences met | Iraqi AAA, evaded; a radar blind spot on the Saudi border was exploited; **no SAM engagement documented**; air-defence crews were reported absent at a meal break | [T4] |
| Losses | **none** — all 14 aircraft returned | [T4] |
| Casualties on the ground | 10 Iraqi soldiers, 1 French engineer | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| The subject is **reach**, not combat | eight of the ten missions have no air opposition at all. If a mission can be won by shooting somebody, it belongs in another campaign |
| **Fuel is the antagonist** | every mission reports remaining `fuelLbs` at the last waypoint. A mission that lands with reserve to spare has not tested the thing this raid was about |
| The delivery profile is a **measured deviation**, not a claim | FlightBox's attack phase is a LEVEL laydown by construction ([`../missions/weapons.md`](../missions/weapons.md), gap `C10`). The campaign measures the level-delivery error against the anchor's dive profile and states the difference; it does not pretend to fly a 35° dive |
| **Ground targets in every mission** | including the navigation and fuel rides — a leg with nothing to hit at its end measures nothing about a strike |
| The low-level leg is flown against **real terrain** | `--elev tiles` or a baked DEM; a flat-earth low-level leg is not a low-level leg |
| Every mission has ONE tactical question | stated in its row |
| The verdict is machine-read | `objective kill unit <target>` + `objective waypoints` + `objective survive` per [`../missions/verdict.md`](../missions/verdict.md) |

### 3. The ten missions

| # | Mission | Task | Time | Wx | Blue | Red | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `w2-01-dome` | single-ship attack, no threat, short leg | day | calm | 1 F-16, 2 × Mk-82 | — | 1 `target_hard` (the dome) + 2 `target_soft` (support buildings) | `kill unit dome` | What does a **level** release actually achieve against a hardened point target, and how far is that from the anchor's dive delivery? (`attack-hardened.fbm` already says a good Mk-82 release does nothing to `target_hard` — this mission asks what it takes) |
| 2 | `w2-02-lowlevel` | low-level navigation leg over real terrain | day | calm | 1 F-16 | — | 1 `target_soft` at the leg's end | `waypoints` + `kill unit` | Can the guidance hold a **30 m / 240 m** ingress over varying terrain at all? (`C20`: `Direct` holds an ASL altitude, so the answer is expected to be "no" and the mission exists to quantify how badly) |
| 3 | `w2-03-radius` | the range ride: out and back on internal fuel | day | calm | 1 F-16, clean | — | 1 `target_soft` at the turn point | `waypoints` + return + `kill unit` | **How far can this jet actually go and come home?** The number that decides whether the historical profile is reachable at all without tanks (`C5`) |
| 4 | `w2-04-loaded-radius` | the same ride carrying two Mk-82 | day | calm | 1 F-16, 2 × Mk-82 | — | 1 `target_hard` | as above | What do the stores cost in radius? (the carriage penalty is already measured for four Mk-82: +2,000 lb, Mach 1.416 → 1.364 — this converts it into kilometres) |
| 5 | `w2-05-pair-pop` | two-ship pop attack, 5 s spacing | day | calm | 2 F-16 (flight) | — | 1 `target_hard` + 3 `target_soft` | both `kill unit` + `survive` | Can two aircraft be sequenced onto one aim point **5 seconds apart** without a time-on-target mechanism? (`C15`) |
| 6 | `w2-06-escort` | escort pair against a single scrambled interceptor | day | calm | 2 F-16 strike + 2 F-16 escort | 1 MiG-29 (scrambled) | 1 `target_hard` | strike `kill unit` + escort `kill unit red1` | Does the escort break away far enough to matter and get back before the strikers are exposed? |
| 7 | `w2-07-blindspot` | ingress through an early-warning gap | day | calm | 4 F-16 | 2 MiG-29 on CAP with a briefed vector | 1 EW radar (`target_soft`) + 1 `target_hard` | `kill unit` on both + ≥3 `survive` | Without terrain masking (`C4`), is "a radar blind spot" expressible at all — or does the campaign have to declare the CAP's `brief_gci` wrong instead of the radar blind? |
| 8 | `w2-08-flak` | attack under anti-aircraft fire | day | calm | 4 F-16 | — | 1 `target_hard` + 4 AAA sites (**`C1`: inert today**) | `kill unit dome` + all `survive` | What does the delivery altitude cost when the low delivery is the dangerous one? **Unanswerable today** — the mission is specified so that the gap is visible rather than invisible |
| 9 | `w2-09-bingo` | egress at minimum fuel with one damaged jet | day | calm | 4 F-16, one with `set fuel_pct` low | 1 MiG-29 pursuing | 1 `target_hard` (already hit) | ≥3 `survive` + `waypoints` home | Does the pilot's BINGO logic (`brief_bingo_lbs`) actually change a decision, or is it a warning nobody acts on? |
| 10 | `w2-10-opera` | the full raid | **late afternoon** | calm | 8 F-16 (two flights) + 2 F-16 as the escort stand-in for the F-15s | 2 MiG-29 scrambled late | 1 `target_hard` (dome) + 4 `target_soft` (site) | ≥7 of 8 strikers `survive` AND `kill unit dome` | With eight aircraft, one aim point, a 1,000 km leg and no tanker, **does the package get home?** The single number the campaign exists to produce: bombs on target × aircraft recovered |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| F-16C | flyable module | **yes** (`f16`) | stands in for the F-16A Netz; the deviation is declared above |
| F-15A escort | flyable module | **no** (`C7`) | substituted by a second F-16 flight in `w2-06`/`w2-10`, and that substitution is stated in the mission header |
| MiG-29 | flyable module | **yes** (`mig29`) | stands in for the Iraqi interceptor force of 1981 (MiG-21/MiG-23/Mirage F1) — **an archetype substitution**, and a generous one: the Fulcrum is a much better interceptor than anything Iraq had in 1981 |
| `target_hard` | ground | **yes** | the reactor containment dome |
| `target_soft` | ground | **yes** | site buildings, EW radar vans |
| AAA site | ground, shooting | **no** (`C1`) | the only defence the raid actually met |
| Early-warning radar as an EMITTER | ground, emitting | **no** (`C1`) | the blind-spot mission has nothing to be blind |
| External fuel tanks | store catalogue entry | **no** (`C5`) | **the campaign's defining constraint is exactly the thing FlightBox cannot express** |
| Mk-84 (2,000 lb) | store catalogue entry | **no** (`C8`) | only Mk-82 exists; `w2-01` therefore asks a different question than the raid did |

### 5. What must be true before mission 1 can fly

`w2-01`, `w2-02`, `w2-03`, `w2-04` are buildable **today**, and are the ones worth building first —
they produce the two numbers everything else depends on (combat radius, low-level tracking error).
`w2-05` onwards needs at least one gap closed.

---

## State

**Nothing built.** No `.fbm` exists for any of these missions.

Reused without change when they are built: the attack phase and its CCIP/CCRP cue
([`../missions/weapons.md`](../missions/weapons.md)), the measured air-to-ground error budget (22.2 m
on `attack-ccrp.fbm`, 482 m with a 2 s late release, no effect at all against `target_hard`), the
fuel channel (`fuelLbs` from `FGPropulsion`, [`../fdm.md`](../fdm.md)), `set fuel_pct`, and the
elevation providers (`--elev tiles|swiss|const`).

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C5` | **no aerial refuelling and no external fuel tank in the store catalogue** | **the campaign's subject.** The historical profile is defined by tanks that ran dry early; FlightBox can only fly it on internal fuel and report the shortfall |
| `C1` | **no active surface-to-air threat** | `w2-08` entirely; the "why fly low" of `w2-02`/`w2-07` |
| `C20` | **no terrain-following guidance** — `Direct` holds an ASL altitude, so a 30 m AGL ingress over varying terrain is not flyable | `w2-02`, and the ingress half of every other mission |
| `C10` | **no dive or pop-up delivery** — the attack phase is a level laydown by design and for a stated reason | the anchor's 35°/2,100 m/1,100 m profile cannot be reproduced |
| `C8` | **Mk-84 is not in the store catalogue** (only Mk-82/AIM-120/AIM-9/R-73/R-27R) | `w2-01` asks about a 500 lb bomb against a hardened dome, which the tree already knows does nothing |
| `C7` | **no F-15 module** | the escort is an F-16 stand-in |
| `C0` | **no campaign layer** | the eight-ship raid cannot inherit the four-ship's losses |
| `C2` | **no time of day** | a late-afternoon launch into dusk is the raid's own tactical choice and cannot be declared |
| `C15` | **no time-on-target / no 5 s stream sequencing** | `w2-05` and `w2-10` |
| `C4` | **no terrain masking** | `w2-07`'s radar blind spot |
| `C17` | **one runway per mission, no divert field** | a raid at the limit of range has no alternate to declare |

### The honest headline

**W2 is the campaign FlightBox is furthest from being able to fly**, and not because of the combat —
because of the tanks. Everything else in the list is a degradation; `C5` is a hole where the subject
should be. That is worth stating plainly rather than burying: this campaign's first deliverable is
not a mission file, it is a drop-tank entry in `core/FBStore.h` and a boom.

---

## Knowledge

### 1. The anchor with its sources

Primary retrieval on this pass: [Operation Opera (Wikipedia)](https://en.wikipedia.org/wiki/Operation_Opera)
[T4] — supplied the date and local launch time, the 8 F-16A / 6 F-15A composition, the 2 × Mk-84
delay-fuzed load, the >1,600 km round trip, the 30 m ingress figure, the tank exhaustion and
jettison, the pop-up to 2,100 m at 20 km, the 35° dive at 1,100 km/h releasing at 1,100 m in
5-second pairs, the <2 min over target, the AAA-only defence, the radar blind area, the casualty
figures and the "at least eight of sixteen bombs struck the dome" result.

Cross-checks and the disputed value:

- [Operation Opera — the raid on the Iraqi nuclear reactor (Defence Aviation)](https://www.defenceaviation.com/operation-opera-raid-iraqi-nuclear-reactor/)
  [T4] gives the ingress as **240 m** and states the route as Eilat/Aqaba then south of Jordan along
  the Saudi border, with the external tanks exhausted ≈ 1,000 km into the flight. The **30 m vs 240 m
  ingress altitude is left as [DISPUTED]** — a factor of eight, and it is the single number that
  decides whether `w2-02` is a terrain-following problem or an ordinary low-level leg.
- [Operation Opera (Ofra) — f-16.net](https://www.f-16.net/varia_article12.html) [T4] and
  [Operation Opera: Eight F-16s Erased Iraq's Osirak Reactor (MiGFlug)](https://migflug.com/jetflights/operation-opera-osirak-reactor-raid-1981/)
  [T4] — the pop-up "from ground level to just under 10,000 ft" at ≈ 600 kt, roughly 10 miles out,
  which is a *third* rendering of the pop-up geometry and is noted here rather than averaged in.
- [38 years later, pilots recall how Iran inadvertently enabled the Osiraq raid (Times of Israel)](https://www.timesofisrael.com/38-years-later-pilots-recall-how-iran-inadvertently-enabled-osiraq-reactor-raid/)
  [T3] — pilot recollections; used only for the qualitative fuel picture ("near the limit of range").

### 2. The three numbers a mission builder must derive, not copy

| Number | Why it cannot be copied | How it is obtained |
|---|---|---|
| The leg length that makes `w2-03` a real range test | the F-16A of 1981 and the pinned Block-50-class model have different fuel fractions and a different engine | **measure it**: fly increasing legs until the jet cannot return, and declare the campaign's leg at 90 % of that [DERIVED, once measured] |
| The release altitude for `w2-01` | the anchor's 1,100 m belongs to a 35° dive that FlightBox cannot fly | take the level-laydown altitude the existing `attack-ccrp.fbm` rig already uses and state the difference [SET] |
| The delivery error to expect | `attack-ccrp.fbm` measured 22.2 m on a clean level release; wind adds 12.8 m at 25 kt | already measured — [`../missions/weapons.md`](../missions/weapons.md), [`../missions/weather.md`](../missions/weather.md) |

### 3. Why the escort substitution is declared and not quietly made

Swapping the F-15A escort for a second F-16 flight changes the campaign's air-to-air character
(different radar reach, different missile, different energy). The alternative — writing "F-15" in a
mission file and flying an F-16 — would put a false label in the artefact that every later reader
inherits. The rule for this whole directory: **a substitution is written in the mission header, and
the header comment is binding** ([`../missions/INDEX.md`](../missions/INDEX.md), rule 5).
