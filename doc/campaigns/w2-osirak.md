# W2 — Osirak 1981 (Operation Opera)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Operation Opera, 7 June 1981 | §Knowledge 1, every fact cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare and what the F-16 module can do | [`../missions/weapons.md`](../missions/weapons.md), [`../missions/syntax.md`](../missions/syntax.md), [`../missions/verdict.md`](../missions/verdict.md), [`../modules/f16/module.md`](../modules/f16/module.md), [`../modules/f16/weapons.md`](../modules/f16/weapons.md), [`../pilot.md`](../pilot.md), [`../fdm.md`](../fdm.md) |

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

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

**BUILT AND FLOWN 2026-07-30 — the TENTH AND LAST of the ten campaigns to exist as files.** Ten
`mods/f16/src/missions/w2-*.fbm` plus `mods/f16/src/campaigns/w2-osirak.fbc`. No file under `sim/src/`, `sim/tools/` or
`mods/f16/src/` was touched — `git status --porcelain` lists eleven new untracked files and no modified
one, so the 241 pre-existing missions are byte-identical by construction.

### Why it was unblocked, and by how much

The spec called four of ten runnable and its own honest headline read: *"W2 is the campaign FlightBox
is furthest from being able to fly … this campaign's first deliverable is not a mission file, it is a
drop-tank entry in `core/FBStore.h` and a boom."* Rule 7 — a blocked mission is re-checked against the
TREE, not against a gap's status line — paid for the ninth time:

| Blocker in the spec | Status on the day it was flown |
|---|---|
| `C5` **the tank** | **HALF CLOSED 2026-07-30.** `tank370` is a catalogue row with `FuelLbs > 0` that owns one of the airframe's own JSBSim tanks while it hangs there; `set fuel_int_pct` makes a CLEAN jet declarable for the first time. **The boom is not built and is not approximated** — see §Gaps |
| `C8` Mk-84 | **BUILT 2026-07-28.** The campaign flies the anchor's own weapon, so `w2-01` asks a sharper question than the spec could |
| `C1` no surface-to-air threat | **CLOSED 2026-07-28.** `w2-08`, which the spec called *"unanswerable today"*, is answered — see below, and the answer is a number |
| `C22` the net | **CLOSED 2026-07-28.** `w2-07` is built on it, and on the receiver it does **not** have |
| `C2` no time of day | **CLOSED.** All ten declare `1981-06-07T12:55:00Z`, the anchor's own 15:55 local launch |
| the pilot's BINGO branch | **REACHABLE since 2026-07-30** ([`../pilot.md`](../pilot.md) §7.4a). W3 had measured it dead at *seven of 184 columns and zero metres*. `w2-09` is the campaign that asks it now that it can be reached |
| `C20`, `C10`, `C15`, `C4`, `C6`(air), `C7`, `C21`, `C17` | **still open**, and each one is declared in the file where it bites rather than in this report |

### The arena, and the anchor's own arithmetic against itself

Etzion AB **29.94000 34.87000** to the Osirak reactor at Al-Tuwaitha **33.20000 44.52000** [T4, both
approximate in the source and carried as given]. Bearing **068.37°**, great circle **982.9 km** each
way, **1,965.9 km** out and back [DERIVED, 1° lat = 111,320 m, 1° lon at 31.570 N = 94,845 m]. **The
anchor's own two distance figures do not agree with each other or with its own coordinates** — "over
1,600 km round trip" and "≈1,090 km straight-line each way" — and all three are carried, none averaged.

`--elev const`, 0 m plane, passed explicitly as a precondition (O3's warning). **The real ground under
this route was MEASURED, not assumed** (`fb-tiles` `/elev`, warmed cell by cell):

| point | ground | point | ground |
|---|---:|---|---:|
| Etzion 29.94/34.87 | **523.59 m** | 31.50/39.50 | 865.62 m |
| 30.20/35.50 | **1,599.22 m** | 32.00/41.00 | 622.15 m |
| 30.50/36.50 | 896.99 m | 32.50/42.50 | 301.25 m |
| 31.00/38.00 | 578.06 m | 33.00/44.00 | 42.83 m |
| 32.20668/41.54287 (the ingress start) | **487.48 m** | the dome 33.20/44.52 | 46.70 m |

### The ten sorties, their fingerprints and their answers

Campaign exit **3**, step exits `3 0 0 2 0 1 0 3 0 1`, whole campaign **76.0 s** of wall clock.
Campaign fingerprint under `--elev const`:
`bdf58c2e58c05d7dc23c15afa6f477dcbdbc58aef45f3a1f036ecc66ecb93d76`.

| # | Mission | ctrl | exit | fingerprint | The answer to its one tactical question |
|---|---|---|---:|---|---|
| 1 | `w2-01-dome` | — | 3 | `d169732fe9bb2409` | **A level Mk-84 lands 36.38 m from the dome and the dome does not notice.** `aimErrM` 36.377, `aimLongM` 36.340, `aimAcrossM` −1.640, `predErrM` 52.575 — **99.9 % of the error is ALONG track**. Derived first, from two committed constants: the Mk-84's fragment law is `2.81e7/r²` and `target_hard` fails at 9.0e4 J/m², so **the dome dies iff a bomb lands inside 17.7 m** (degrade 33.5 m). 36.38 m is outside both. The 80 m support building DESTROYED and the 160 m one INTACT brackets the derived 100.2 m soft radius from both sides. **AND ONE BOMB OF TWO LEAVES**: the attack phase pickles once per pass and there is no re-attack ([`../pilot.md`](../pilot.md) §2.11), so the anchor's *two* Mk-84 per aircraft is not expressible — 16 releases become 8 |
| 2 | `w2-02-ingress` | — | **0** | `673753b8b0cf3f3e` | **The guidance holds 240 m to 0.85 m over 300 km — over a PLANE, and that is the whole finding.** `altM` min 239.70 / max 240.55 / mean 240.002 over 14,500 rows of the ingress leg. Then the same file under `--elev tiles` over the real Jordan and Iraq: **`mission RESULT result=FAIL reason="spawn altitude is below ground" altM=240 groundM=487.48`** — it does not reach tick one. `C20` is not a tracking-error gap here, it is a **247.48 m** gap at the ingress start and up to **1,569 m** further west |
| 3 | `w2-03-radius` | — | **0** | `a97fba1ebbc02613` | **The clean jet gets home from 671.7 km with 516.4 lb left.** Both waypoints reached; outbound leg 3,261.1 s, home at 6,576.9 s, 1,343.4 km flown. Flown to exhaustion instead of stopped, the same jet crashes at t = 7,368.5 s **152.3 km past its own airfield** — the whole margin, and it is 7.4 % of the fuel it launched with |
| 4 | `w2-04-loaded` | 03 | **2** | `42c542e4ec72c306` | **THE WAR LOAD DOES NOT GET HOME, AND THAT IS THE CAMPAIGN'S CENTRAL NEGATIVE RESULT.** One lever — two `set store … mk84` lines — and the identical leg ends in fuel exhaustion at t = 5,751.6 s, **179.0 km short of Etzion**, having reached the turn point at the same 3,261.2 s. Two 2,039 lb stores are worth **330.3 km of clean range, 22.1 %** |
| 5 | `w2-05-tanks` | 04 | **0** | `a92dfa00db6f4837` | **Two tanks turn 179.0 km short into 2,373.4 lb in hand.** One lever — two `set store … tank370` lines on the plumbed stations 4 and 6 — and the same jet flying the same leg lands with **2,373.4 lb** where its control had **0.0**. 11,523.8 lb aboard at spawn against 6,971.8 |
| 6 | `w2-06-escort` | — | 1 | `2ea54fe711951a11` | **The escort breaks away and does not come back, and the strike is already done when it dies.** Separation striker↔escort **9.99 km at spawn → 18.40 km at the release → 74.06 km at the end**. Both Mk-84 away (36.38 m and 45.32 m), the 80 m building destroyed by 2 hits, the dome intact. The escort lead is killed by an R-27R at t = 296.2 s, **131 s after the last bomb left**, and `FirstFlightKo` ends the run there — which is O1's rule working as designed rather than biting |
| 7 | `w2-07-blindspot` | — | **0** | `7ec2fe1cf5759c1f` | **A radar blind spot is not expressible, and not for the reason the spec expected.** The node holds the raid from **294.6 km** and sends **12 `net CUE`** to the one receiver it has — an anti-aircraft gun 200 km behind the target that can never engage what it is being told about. The two MiG-29 on CAP, fully briefed, log `fcr_contacts` **max 0.0 for 700 s**. The gap between the node's detection and the fighters' is not a masking gap: **`C6`'s airborne half means there is no channel between them to be blind in.** Both strikers release; the hardened shelter dies at **11.21 m** |
| 8 | `w2-08-flak` | 01 | 3 | `6cbec31db6bf0bf5` | **The anchor's only defence fires 39 of 40 rounds and is worth SEVEN OF 184 COLUMNS AND ZERO METRES.** One lever — four guns on the site, the striker 1,260 m inside all four ceilings. `w2flka` (ZSU-23-4) goes `SEARCH → TRACK → ENGAGE` and empties its magazine in 7 bursts. Against its gun-free control the striker's telemetry differs in **7 of 184 columns**, every one of them RWR bookkeeping (`rwr_threats/mode/brg/el/leth/new/act`), and `aimErrM` is **36.3772 in both, to four decimals**. There is no pilot reaction to a ground threat (`C1`'s own G11), so the layer that decided the anchor's altitude decides nothing here |
| 9 | `w2-09-bingo` | itself | **0** | `c8c33b7a793ac8a1` | **THE BINGO LINE IS ALIVE AND IT COSTS THE JET ITS WAY HOME.** `intercept BINGO_ABORT … t=4.1 fuelLbs=2773.81 bingoLbs=4000 from=closing haveTgt=1` — **from `closing`, with a target**, which is the strongest form of the branch W3 measured as dead code. The file's own control is the jet 1.5 km abeam with the one line deleted: it stays in the engagement, fires an AIM-120 at t = 62.4 s and ends **on its way home**; the aborting jet turns cold and ends **199.2 km from its own home waypoint, 87.1 km BEYOND the target it was leaving**. Neither dies. `Abort` is terminal and is NOT a return to the landing waypoint ([`../pilot.md`](../pilot.md) §Gaps), so the spec's own victory condition for this mission — *"waypoints home"* — is not achievable and the file says so instead of scoring it |
| 10 | `w2-10-opera` | — | 1 | `660805cbd5aa3b1b` | **THE PACKAGE GETS ITS BOMBS ON AND ITS AIRCRAFT BACK, AND IT PAYS THE ESCORT.** Eight strikers, eight releases, **five of eight inside the 17.7 m fail radius** (6.42 / 7.02 / 6.78 / 7.34 / 6.36 m; the other three 26.32 / 26.23 / 26.67 m) — **`w2dom0` DESTROYED by 8 hits**, `w2sit1` with it, the other three site buildings intact. The four pairs release at t = **290.8 / 295.8 / 300.8 / 305.9 s**: the authored 1,029 m spacing comes out at **5.00 s**, to the tick, four times. Time over target **15.1 s** against the anchor's "under 2 minutes". All eight strikers alive with 4,675–4,677 lb each; **the escort is lost** |

### The range, measured five ways — the number this campaign exists to produce

Attribution run **A1**, outside the ten: one straight leg at **240 m / 400 kt / `fuel_int_pct 100`**,
flown to fuel exhaustion, one configuration per row. The 8,000 m rows are the committed ones from
[`../modules/stores.md`](../modules/stores.md) and are here only as the altitude control.

| configuration | fuel at t=0 | flameout | lb/km | **radius** (half, zero reserve) | vs the anchor's 982.9 km each way |
|---|---:|---:|---:|---:|---|
| clean, 8,000 m | 6,971.9 | 2,627.4 km | 2.6535 | 1,313.7 km | +330.8 km |
| 2 × tank370, 8,000 m | 11,523.9 | 3,862.9 km | 2.9832 | 1,931.5 km | +948.6 km |
| **clean, 240 m** | 6,971.8 | **1,492.6 km** | 4.6708 | **746.3 km** | **−236.6 km (−24.1 %)** |
| **2 × Mk-84, 240 m** | 6,971.8 | **1,162.3 km** | 5.9984 | **581.2 km** | **−401.7 km (−40.9 %)** |
| **2 × tank370, 240 m** | 11,523.8 | **2,173.4 km** | 5.3022 | **1,086.7 km** | **+103.8 km** |
| **2 × tank370, dropped when dry, 240 m** | 11,523.8 | **2,327.9 km** | 4.9503 | **1,164.0 km** | **+181.1 km** |
| **the war load: 2 × Mk-84 + 2 × tank370, 240 m** | 11,523.8 | **1,748.8 km** | 6.5896 | **874.4 km** | **−108.5 km (−11.0 %)** |

Four readings, and every one of them is a subtraction rather than an opinion:

1. **The anchor's own ingress altitude costs 43.2 % of the range** (1,492.6 against 2,627.4 km clean).
   That is the largest single lever in the whole campaign, larger than the tanks and larger than the
   bombs.
2. **The tanks are worth +45.6 % clean and +50.5 % under the war load**, on +65.3 % more fuel — the
   difference is what the extra 480 lb of dry mass and 0.95 ft² of drag area take back.
3. **Dropping them when they are dry is worth another 154.5 km, 7.1 %** — the anchor's own reason for
   dropping them, as a number. And the externals run dry at **675.8 km** under the war load
   [MEASURED, the tick at which total fuel reaches the model's own 6,972.0 lb internal capacity],
   against the anchor's *"about 1,000 km into the flight"* [T4]: the same order, from an entirely
   independent direction.
4. **The raid is not flyable in this tree, and the shortfall is 108.5 km.** With the anchor's own load
   and the anchor's own tanks, at the anchor's own altitude, the radius is 874.4 km against 982.9 km
   needed. And that number is an **upper bound in four separate ways**: zero reserve, zero combat
   allowance, an air start with no taxi/take-off/climb burn, and a straight line where the raid flew a
   dog-leg down the Gulf of Aqaba. Against the anchor's *lower* self-reported figure — 1,600 km round
   trip, i.e. 800 km each way — the war load clears it by 74.4 km and the clean jet is still 53.7 km
   short.

### The level-delivery error, and what it is a function of

Attribution run **A2**: `w2-01`'s geometry at **300/350/400/450 kt × 240/600/1,100 m**, twelve points,
one Mk-84 each. `predErrM` (the fire control's own prediction error) is **monotone in both variables**;
`aimErrM` (what was delivered) is not.

| release | 240 m | 600 m | 1,100 m |
|---|---|---|---|
| 300 kt | 37.07 / **23.16** | 39.59 / 30.28 | 42.06 / 33.50 |
| 350 kt | 42.08 / 35.21 | 44.57 / 38.01 | 46.89 / 35.22 |
| 400 kt | 47.27 / 37.23 | 49.75 / 34.01 | 52.01 / 43.76 |
| 450 kt | 52.57 / 36.38 | 55.11 / 48.74 | 56.97 / **50.83** |

*(`predErrM` / `aimErrM`, metres.)*

- **`predErrM` is ground speed × a constant 0.228–0.241 s** [DERIVED across the four 240 m rows:
  37.07/154, 42.08/180, 47.27/205.8, 52.57/231]. It grows **+5.2 m per 50 kt** and **+2.5 m per 430 m**
  of release altitude. It is a latency, not an aiming error.
- **`aimErrM` is a lottery inside a band, and the band's lower tail reaches the dome.** Over all
  seventeen Mk-84 released in this campaign it runs **6.36 m to 50.83 m**, is 96–99 % along track, and
  **five of the capstone's eight fall inside 17.7 m**. So the honest statement is not *"a level laydown
  cannot kill a hardened dome"* — `w2-07` killed one at 11.21 m and `w2-10` killed one with eight
  bombs — it is: **the release lands somewhere in a ~44 m band whose position is set by where the
  0.1 s decision tick falls, and the dome's lethal radius is inside the lower quarter of it.** The
  anchor's answer to the same problem was a 35° dive, which is exactly what `C10` removes.

### The disputed ingress altitude, both halves flown or refused with a number

The spec carries **30 m** [T4] and **240 m** [T4] and prefers neither, because *"it is the single number
that decides whether `w2-02` is a terrain-following problem or an ordinary low-level leg"*. Both were
put to the tree rather than averaged:

| | 240 m (flown, `w2-02`) | 30 m (attribution **A3**) | real ground (attribution **A4**) |
|---|---|---|---|
| flyable over the `--elev const` plane | yes | **yes** — `altM` min 29.02 over the leg | — |
| altitude hold | 0.85 m over 300 km | 1.0 m over 300 km | — |
| `aimErrM` | 48.11 m | **44.39 m** | — |
| `aimAcrossM` | −28.35 m | −27.77 m | — |
| **the bomb's arming margin** | 5.11 s of a 2.0 s arming time | **0.486 s** | — |
| flyable over the REAL ground | — | — | **NO. `mission RESULT result=FAIL reason="spawn altitude is below ground" altM=240 groundM=487.48`** |

**The answer is that neither value is flyable over the ground the raid actually crossed, and the
campaign's arena is the reason 240 m works at all.** Over the plane the 30 m variant is not a
terrain-following problem — it is a **fuzing** problem: at 30 m the Mk-84's 2.0 s arming time leaves
**0.486 s**, 24 % of the delay, and 3.0 m/s of sink would consume it. That is a real answer to the
dispute and it is one the spec could not have guessed.

### The carry — and the campaign whose subject it cannot carry

`carry units ground stores`. The chain is **06 → 10** on the escort pair, and what it claims is stated
in the `.fbc` header rather than dressed up: the raid flew ONE sortie, so unlike W1's kill removal this
is **the campaign's own experiment** (*"is an escort that has already fought still an escort?"*), not
the anchor's procedure.

| Measured | Value |
|---|---|
| What the layer did | **one `campaign CARRY` line**: `unit=w2esc1 action=drop reason="destroyed in an earlier mission"` |
| What it is worth (attribution **A5**, `w2-10` run standalone) | standalone the escort element ends **1 of 2 alive** — `w2esc1` killed at t = 515.1 s, `w2esc2` alive. In campaign it ends **0 of 1**: `w2esc1` never takes off and `w2esc2` is killed at t = 517.6 s. **Removing the aircraft that would have died kills the one that would have lived** |
| How deep it reaches | **8 of 29** common telemetry files byte-identical, run 515.1 → 517.7 s |
| Which 8 | **exactly the eight bombs.** Every aircraft file moved; not one bomb file did |
| What it does NOT move | the same eight releases at the same ticks, the same five inside 17.7 m, the same dome destroyed, the same three buildings standing. **The carry moves the air half and not one metre of the strike** — W1's finding, on a different continent |

**And the finding that is this campaign's alone: the carry cannot carry W2's subject.**
[`../missions/campaign.md`](../missions/campaign.md) refuses **fuel** as a carried fact with a stated
and correct reason (*"carrying fuel across a landing models the GROUND time, which does not exist"*).
The consequence stands anyway: the one campaign in the ten whose antagonist is fuel is the one whose
campaign layer is structurally blind to it, and every W2 fuel number is therefore a **within-sortie**
number.

### Determinism — both criteria, on the first attempt

| # | Criterion | Measured |
|---|---|---|
| **1** | one campaign fingerprint over 3 reps × `--threads 1/2/4` | **9 runs, 1 fingerprint** `bdf58c2e58c05d7dc23c15afa6f477dcbdbc58aef45f3a1f036ecc66ecb93d76`, `--elev const`, `time 1981-06-07T12:55:00Z` |
| **2** | each step's per-mission fingerprint equals the same mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included |
| Conservation | annotating all ten files with their MEASURED blocks afterwards left all ten per-mission fingerprints and the campaign fingerprint unchanged |
| Per-step fingerprints | `d169732fe9bb2409 673753b8b0cf3f3e a97fba1ebbc02613 42c542e4ec72c306 a92dfa00db6f4837 2ea54fe711951a11 7ec2fe1cf5759c1f 6cbec31db6bf0bf5 c8c33b7a793ac8a1 660805cbd5aa3b1b` |
| **1 — re-run 2026-07-30 (`E6`)** | the same criterion after the judge-completion fix of [`../doctrine-evolution.md`](../doctrine-evolution.md) X-1 | **9 runs, 1 fingerprint** `d951102ca9e3f311da1b258bd7dd40e03ba08e8b43598ef0a0ce47a46dd53494`, `--elev const`. **The row above is kept with its date; this is the current one.** Step exits `3 0 0 2 0 1 0 3 0 1` — unchanged. The fingerprint MOVED and **exactly three step fingerprints with it** — steps 4, 6 and 10 (`w2-04-loaded`, `w2-06-escort`, `w2-10-opera`), each a run that ended before its judges were finished; the other seven are byte-identical |
| **2 — re-run 2026-07-30 (`E6`)** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 (`E6`) | `d169732fe9bb2409 673753b8b0cf3f3e a97fba1ebbc02613 2551cb3b497c3a3a a92dfa00db6f4837 03883124b5cd9af9 7ec2fe1cf5759c1f 6cbec31db6bf0bf5 c8c33b7a793ac8a1 51862d9e17e18857` |

### The saturation gate is not run on this campaign, and the reason is the campaign's own contract

W1 measured it on 2026-07-30 and the result reaches here: **an air-to-air result above two aircraft a
side is a fixed point in this tree.** W2's contract says *"eight of the ten missions have no air
opposition at all; if a mission can be won by shooting somebody, it belongs in another campaign"* —
and eight of the ten indeed have none. The two that do are laid out at **2 v 1** (`w2-06`) and
**2 v 1** (`w2-09`, whose second F-16 is its own control), which is the largest size W1 found
informative; the capstone is 10 v 2 and **its air half is declared outcome-blind in its own header,
before the run**, not afterwards. Pointing the nine-lever gate at a campaign with eight combat-free
geometries would have measured the geometry's emptiness, not the doctrine's.

### The four defects this campaign found

Reported, not fixed, and not tuned around.

| # | Defect | The measurement that pinned it |
|---|---|---|
| **1** | **A jettison on a bomb-carrying jet drops the BOMB.** `FBStoresSystem::Release` releases in STATION ORDER, so the anchor's own selective jettison — let the dry tanks go, keep the Mk-84 — is not expressible. `stores.md` §Gaps names selective jettison as a simplification; this is the file where the simplification changes an outcome | attribution **A6**: the war load told to let its tanks go at t = 3,295 s logs `sms RELEASE station=3 store=mk84` at **t = 3,295.6** and `sms TANK_JETTISON station=4` only at **t = 3,300.6**. `w2-10` therefore declares the post-jettison STATE instead of flying the transition, and says so |
| **2** | **A gun's acquisition set takes a falling bomb for an aircraft.** This is [`../air-to-ground.md`](../air-to-ground.md)'s known "a bunker and a falling bomb both radiate a fighter radar" reproduced on the GROUND side, where the consequence is a fire unit spending its reaction time on ordnance | `w2-08`: `site TRACK unit=w2flkc brgDeg=200.202 rangeM=1250 closureMs=0 altM=111.256` — 111 m and zero closure is a Mk-84 in the last seconds of its fall, not a 450 kt striker at 240 m. Same line in `w2-10` on `w2flk9` |
| **3** | **An early-warning node cues a fire unit it can see is out of reach.** `FBSiteFireControl` has no range test against the receiver's own envelope on the cue path | `w2-07`: 12 `net CUE` to a ZSU-23-4 whose own effective range is 2.5 km, carrying `rngM=294623 … 204912`. The gun does exactly what it is told and points at something 200 km away |
| **4** | **The attack phase's one release per pass is a campaign-scale number, not a detail.** `AtkReleased_` latches and there is no re-attack ([`../pilot.md`](../pilot.md) §2.11) | the anchor's 16 released bombs become **8** with the same 8 aircraft and the same 16 stores; the eight unexpended Mk-84 are then carried forward by `carry stores` and are worth 2,039 lb of mass each on station 7 — O3's own finding waiting to happen |

### What stays unmeasurable, named rather than caveated

| Absent | What it costs this campaign |
|---|---|
| **aerial refuelling** (`C5`, the other half) | **the campaign cannot say what a tanker would have been worth.** Every number above is about an aircraft that launched full and never took another pound. The anchor's "tanks exhausted en route" is reproduced by CARRYING tanks that run dry (measured at 675.8 km), never by filling anything in flight — and the 108.5 km shortfall is exactly the size of hole a boom would have filled |
| **terrain-following guidance** (`C20`) | the 30 m variant of the campaign's own [DISPUTED] anchor value is not flyable over the ground it was flown over, and the campaign's whole arena is a 0 m plane under a route whose real ground reaches 1,599.22 m |
| **the dive** (`C10`) | the anchor's 35° / 2,100 m pop-up / 1,100 m release is the answer to the same 17.7 m problem this campaign measures a 44 m band against, and it cannot be flown |
| **a divert field** (`C17`) | `w2-04` runs out of fuel 179.0 km from Etzion with nowhere else declared, which is the correct answer to the mission's question and a poorer one than the real map would give |
| **initial damage** (`C21`) | `w2-09`'s "already hit" dome stands intact |

---

## Gaps

**Re-checked against the tree on the day the campaign flew, and the status column is the tree's rather
than the spec's.** Six of the eleven had closed since the spec was written.

| ID | What is missing | Status 2026-07-30 | What it did to this campaign |
|---|---|---|---|
| `C5` | aerial refuelling **and** an external fuel tank | **HALF CLOSED** — `tank370` exists and feeds the engine's own books; **the boom does not and is not approximated** | the tank IS the campaign: `w2-05` is a new sortie built for it and the tanks are worth **+50.5 %** of range on the war load. What the missing half costs is a named absence and not a caveat: **no W2 number says what a tanker would have been worth**, and the raid's own shortfall here is **108.5 km**, which is exactly the size of hole a boom fills |
| ~~`C1`~~ | no active surface-to-air threat | **CLOSED 2026-07-28** | `w2-08` ran, which the spec called unanswerable — and the answer is that four guns firing 39 rounds are worth **7 of 184 telemetry columns and zero metres**, because there is no pilot reaction to a ground threat (`C1`'s own G11) |
| `C20` | **no terrain-following guidance** | **OPEN** | the sharpest measured consequence in the campaign: under `--elev tiles` `w2-02` **fails at boot**, `altM=240 groundM=487.48`, and the real ground under the route reaches **1,599.22 m**. The 30 m half of the [DISPUTED] anchor value is not flyable over the ground the raid crossed, and neither is the 240 m half |
| `C10` | **no dive or pop-up delivery** | **OPEN** | the anchor's 35° / 2,100 m / 1,100 m profile is the answer to the same 17.7 m problem this campaign measures a **6.36–50.83 m** band against. It also deleted the spec's own mission 5 as a separate file |
| ~~`C8`~~ | Mk-84 not in the catalogue | **BUILT 2026-07-28** | the campaign flies the anchor's own weapon; `w2-01` asks the sharper question the spec could not |
| `C7` | **no F-15 module** | **OPEN** (`f15c` exists as a flight model and cannot fire — MEASURED by O5) | the escort of `w2-06`/`w2-10` is an F-16 stand-in, and only two of the anchor's six: the other four were a dispersed diversion with no geometry this tree could give them |
| ~~`C0`~~ | no campaign layer | **CLOSED 2026-07-28** | `mods/f16/src/campaigns/w2-osirak.fbc` exists — **and it cannot carry fuel**, by a stated and correct decision, which makes the one campaign whose antagonist is fuel the one whose layer is blind to it |
| ~~`C2`~~ | no time of day | **CLOSED 2026-07-28** | all ten declare `1981-06-07T12:55:00Z`, the anchor's own 15:55 local launch. It decided nothing measurable here: nothing in W2 depends on the sun |
| `C15` | no time-on-target / no 5 s stream sequencing | **OPEN** | the capstone's four pairs are spaced **1,029 m along track = 5.00 s at 400 kt**, arithmetic done by the author in a comment, and the releases come out 5.0 s apart four times. That is what `C15` means: it works and nothing in the simulator maintains it |
| `C4` | **no terrain masking** | **OPEN** | `w2-07`'s blind spot has nothing to be blind behind — and it turned out not to be the binding constraint: `C6`'s airborne half is |
| `C6` (air) | **a node cannot cue a fighter** | **OPEN** | the actual reason a radar blind spot is not expressible. The node holds the raid at **294.6 km**; the two briefed MiG-29 log **0 contacts in 700 s**; there is no channel between them |
| `C17` | one runway, no divert field | **OPEN** | `w2-04` runs out of fuel **179.0 km** from Etzion with nowhere else declared |
| `C21` | no declarable initial damage | **OPEN** | `w2-09`'s "already hit" dome stands intact |

### The honest headline, re-written after the campaign flew

The spec's headline was *"W2 is the campaign FlightBox is furthest from being able to fly, and not
because of the combat — because of the tanks … this campaign's first deliverable is not a mission file,
it is a drop-tank entry in `core/FBStore.h` and a boom."* **Half of that deliverable landed and the
campaign flew.** What replaces the headline is a number rather than a lament:

> **With the anchor's own load, the anchor's own tanks and the anchor's own ingress altitude, this
> jet's combat radius is 874.4 km against the 982.9 km the raid needed — 11.0 % short, with zero
> reserve, zero combat allowance, an air start that burns nothing on the ground, and a straight line
> where the raid flew a dog-leg.** The raid is not flyable here, the shortfall is measured, and it is
> exactly the size of the gap the unbuilt half of `C5` would close.

And the second headline, which the spec could not have anticipated: **the largest single lever in this
campaign is not the tanks, it is the altitude.** Flying the anchor's ingress at 240 m instead of
8,000 m costs **43.2 %** of the range — more than the tanks give back (+45.6 % clean) and more than the
bombs take away (−22.1 %). The raid's own defining tactical choice is the most expensive thing in it.

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
