# W4 — Allied Force 1999 (mountains, cloud, an air defence that will not die)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

**Status: BUILT AND FLOWN 2026-07-29** — `mods/f16/src/missions/w4-*.fbm` + `mods/f16/src/campaigns/w4-allied-force.fbc`,
ten of ten runnable against a spec that called four runnable, both determinism criteria on the first
attempt, the replay run after the first mission. §State carries the numbers; the Spec below is left
standing as written and its departures are listed in §State.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Operation Allied Force, 24 March – 10 June 1999 | §Knowledge 1, every fact cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare and what the modules can do | [`../missions/weather.md`](../missions/weather.md), [`../missions/weapons.md`](../missions/weapons.md), [`../missions/sensors.md`](../missions/sensors.md), [`../world/weather.md`](../world/weather.md), [`../world/terrain.md`](../world/terrain.md), [`../render/clouds.md`](../render/clouds.md), [`../modules/f16/module.md`](../modules/f16/module.md) |

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

**Temporal honesty: none needed.** The F-16C flew this campaign in both roles that matter here — the
Block 40 strike jet and the **Block 50 F-16CJ** SEAD jet with the AGM-88 [T4]. It is the only
campaign in the western half whose *weather* is as well documented as its order of battle, which is
why it is the campaign that consumes FlightBox's weather hook.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Dates | **24 March – 10 June 1999**, planned for 72 hours, ran **78 days** | [T1]/[T4] |
| SEAD force | **50 × F-16CJ Block 50** carrying **AGM-88 HARM** against emitting Yugoslav radars | [T4] |
| Altitude floor | **15,000 ft or higher**, forced by heavy MANPADS and AAA | [T3] |
| Weather | "the stormy spring weather just kicked our butts for the first 45 days" — Lt Gen Michael C. Short, CFACC; many crews returned with their bombs | [T3] |
| Yugoslav SAM inventory | **SA-6 (2K12 Kub)**, **SA-2 (S-75)**, **SA-3 (S-125)**, plus MANPADS and AAA; largely Cold-War vintage, limited mobility and ECM | [T4] |
| Yugoslav SAM tactics | **dispersed, radars mostly not emitting**; concealment under weather and mountainous terrain; **extensive use of decoys** | [T3] |
| NATO losses (aircraft) | **1 × F-117** (27 March), **1 × F-16C Block 40** (2 May) | [T4] |
| Yugoslav MiG-29s | the 127th Fighter Aviation Squadron "Knights" at **Batajnica**; five scrambled the night of 24/25 March, several shot down by F-15Cs on 24/25 and 26 March; the surviving aircraft had **outdated avionics, degraded radar performance and a limited missile stock** | [T4] |
| Anchor region | Aviano AB ≈ 46.03 N 12.60 E; Batajnica ≈ 44.93 N 20.26 E; Kosovo engagement zone ≈ 42.2–43.2 N 20.2–21.8 E (**approximate, verify against DEM**) | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| The subject is **finding things**, not killing them | an air defence that does not emit and a target under cloud are the same problem twice. Every mission reports what was *detected*, not only what was hit |
| **The weather is a participant** | every mission carries a `wx` line, and at least four use `wx fixture` with a real GFS blob ([`../missions/weather.md`](../missions/weather.md)). A campaign flown in `wx calm` is not this campaign |
| The altitude floor is mission data | the 15,000 ft floor is declared in every striker's `wp` altitudes and is the thing the missions push against |
| Terrain is cover, not scenery | flown with `--elev tiles` over the real Balkans; the moment `C4` closes, these missions are its acceptance test |
| **Ground targets in every mission** | the campaign is a ground campaign; the fighters are an interruption |
| The verdict is machine-read | `kill unit`/`kill team` on the ground set, `survive` per striker |

### 3. The ten missions

| # | Mission | Task | Time | Wx | Blue | Red | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `w4-01-floor` | strike from above the floor | day | calm | 1 F-16, 2 × Mk-82 | — | 1 `target_soft` | `kill unit` | What does a release from ≥15,000 ft do to the delivery error the tree measured at low level (22.2 m on `attack-ccrp.fbm`)? The number that decides whether the whole campaign is flyable with unguided stores (`C8`) |
| 2 | `w4-02-overcast` | the same strike under a real cloud deck | day | `wx fixture` | 1 F-16 | — | 1 `target_soft` | `kill unit` | Does the deck change anything measurable? **Expected answer today: no** (`C16` — cloud affects only the IRST). The mission exists so that the answer is on record before the cloud rebuild lands |
| 3 | `w4-03-valley` | strike in a mountain valley | day | `wx wind` | 2 F-16 (flight) | — | 1 `target_hard` in terrain | both `kill unit` | Can the path law hold an attack heading down a valley, and how much of the miss is the wind (measured: +12.8 m at 25 kt) versus the terrain-relative geometry? |
| 4 | `w4-04-emitter-hunt` | hunt a radar that emits and stops | day | calm | 2 F-16 | 1 emitting ground radar (**`C1`: does not exist**) | 1 `target_soft` (the radar) | `kill unit radar` | **The campaign's central question.** An emitter that goes quiet is a memory problem, not a sensor problem — exactly the shape of `FBBfmTrack::Datum`. Unbuildable today; specified so the shape is on record |
| 5 | `w4-05-decoy` | the same hunt with decoys present | day | calm | 2 F-16 | 1 real + 3 decoy emitters (**`C1`**) | 4 `target_soft`, one of them the real one | `kill unit` on the real one, penalty for the others | Can a pilot that only has bearings distinguish a decoy from a battery? A test of the RWR's own honesty: **it measures received power, never range** ([`../sensors.md`](../sensors.md)) |
| 6 | `w4-06-cap-intercept` | intercept a scrambling defender | day | `wx fixture` | 2 F-16 (flight) | 2 MiG-29 from Batajnica, degraded (`set n019_mode off` on one) | 1 `target_hard` (the airfield) | `kill team hostile` + both `survive` | The 1999 Fulcrum flew with broken systems. What does an opponent with **no radar at all** still do to a flight — and does the F-16 pilot notice that it never gets illuminated? |
| 7 | `w4-07-package` | strike package with a suppression element | day | `wx fixture` | 4 F-16 (2 strike, 2 "SEAD") | 2 MiG-29 | 2 SAM sites (**`C1`**) + 1 `target_hard` (bridge) | strike `kill unit bridge` + all `survive` | Is a suppression element that cannot suppress worth the airframes? The mission is the honest form of "we have no HARM" |
| 8 | `w4-08-weather-abort` | the mission that must be brought home | day | `wx fixture` (heavy) | 2 F-16 with stores | — | 1 `target_soft` under the worst cell | either `kill unit` **or** `waypoints` home with stores | **The 45-day finding as a decision:** can the pilot decide not to release? Today it cannot — the attack phase pickles on the cue and nothing tells it the target is obscured (`C16`) |
| 9 | `w4-09-kez` | armour in the engagement zone | day | `wx wind` | 4 F-16 (two flights) | 2 MiG-29 | 6 `target_soft` in a dispersed column (**static — `C14`**) | ≥4 of 6 killed + all `survive` | Against dispersed, small, moving targets from 15,000 ft, how many passes does a four-ship need — and how much of the answer is the store rather than the pilot? |
| 10 | `w4-10-allied-force` | the full night package | **night** | `wx fixture` | 8 F-16 (four flights: 4 strike, 2 escort, 2 SEAD) | 4 MiG-29 + SAM/AAA (**`C1`**) | 1 `target_hard` (bridge) + 4 `target_soft` (IADS nodes) + 2 decoys | ≥3 of 4 IADS nodes + the bridge, ≥7 of 8 recover | With every one of the campaign's four antagonists present at once — cloud, mountains, an emitter that hides, and a fighter that only sometimes works — **does the package still find its targets?** |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| F-16C strike | flyable module | **yes** | Block 40 stand-in |
| F-16CJ SEAD | flyable module + HARM | **module yes, weapon no** (`C8`) | the campaign's defining aircraft cannot carry its defining weapon |
| MiG-29 | flyable module | **yes** | the 127th's aircraft, with the documented degradation expressible today as `set n019_mode off` / `set n019_emission off` |
| SA-6 (2K12) battery | ground, emitting + shooting + **mobile** | **no** (`C1`, `C14`) | the war's most effective Yugoslav system |
| SA-2 / SA-3 site | ground, emitting + shooting | **no** (`C1`) | one of them killed the F-117 |
| MANPADS / AAA | ground, shooting, short range | **no** (`C1`) | **the reason the 15,000 ft floor exists** — without it the floor is an arbitrary altitude |
| Radar decoy | ground, emitting, not lethal | **no** (`C1`) | the anchor names decoys as a decisive Yugoslav measure |
| Bridge | ground, hard | **yes** (`target_hard`) | |
| Dispersed armour | ground, moving | **static only** (`C14`) | |
| E-3 / EA-6B / tanker | air, support | **no** (`C5`, `C6`, `C13`) | |

### 5. What must be true before mission 1 can fly

`w4-01`, `w4-02`, `w4-03`, `w4-06` are buildable **today**. `w4-01` and `w4-02` should be built
first and as a **pair**: they differ by one `wx` line, and together they put a number on how much of
"weather" FlightBox currently simulates for a strike (the honest expected answer: the wind, and
nothing else).

---

## State

**BUILT AND FLOWN, 2026-07-29 — the seventh of the ten campaigns to exist as files, and the one whose
subject is FINDING rather than killing.** Ten `.fbm` in `mods/f16/src/missions/w4-*.fbm` plus
`mods/f16/src/campaigns/w4-allied-force.fbc`, run as a campaign, replayed step by step, and measured. **No file
under `sim/src/`, `sim/tools/` or `mods/f16/src/` was touched** (`git status --porcelain` lists eleven
new untracked files and **no modified one**), so the **205** pre-existing `mods/f16/src/missions/*.fbm` are
byte-identical **by construction rather than by comparison**.

### The spec's own headline is superseded, and by measurement

This file said: *"Allied Force is the campaign whose subject FlightBox is closest to being able to
model and furthest from having modelled"*, and it called missions 4, 5, 7 and 10 blocked outright.
Rule 7 says a blocked mission is re-checked against the **tree**:

| The spec said | The tree says, 2026-07-29 |
|---|---|
| `C1` — no active surface-to-air threat, no emitter, no decoy | **CLOSED.** `p18` `sa3` `sa6` `sa7` `zu23` `zsu23` all fly here. `set emcon hold` is a passive receiver, i.e. exactly "a radar that emits and stops" — and the DECOY needed no new row at all: a `p18` has `Channels 0` / `RoundsDefault 0`, so it is already "emitting, not lethal" |
| `C8` — no HARM | **BUILT.** `agm88` is the campaign's deciding weapon, and its measured behaviour is what sorties 04, 06, 08 and 10 turn on |
| `C22`/`C23` — no net, no judged belt | **CLOSED.** `net` with `link wire` and per-member `autonomy`, and `zone`+`objective avoid zone`, which is what finally gives the 15,000 ft floor a number instead of a name |
| `C2` — no time of day | **CLOSED.** All ten declare their own clock; sortie 10 is the night package |
| `C0` — no campaign layer | **CLOSED.** This campaign has a `.fbc` and a one-step chain |
| `C16` — cloud affects only the IRST | **STILL OPEN**, and the re-check sharpened it: cloud also reaches `sensors/FBVisualSystem`. Sorties 02 and 07 measure both halves |
| `C4` — no terrain masking | **STILL OPEN, and it is this campaign's central shortfall.** Declared in every header |
| `C14` — no moving ground units | **STILL OPEN.** Sortie 09 says so and reports an upper bound |

**So the spec's count of four runnable missions is ten.**

### The arena, and the two disclosures that belong in the first paragraph

The anchor's Kosovo engagement zone, ~42.2–43.2 N 20.2–21.8 E [T4, approximate], ingress west→east
along 42.55–42.86 N. Scale: 1° of latitude = 111 320 m, 1° of longitude at 42.70 N = **81 833 m**
[DERIVED]. `--elev const`, **0 m datum**.

1. **There is no terrain masking (`C4`), and W4 is its acceptance test.** The anchor's own phrase for
   the Yugoslav method is *"concealment under weather and mountainous terrain"*. Every detection range
   below is therefore an **upper bound**, and sortie 10 states plainly that it stages **three** of its
   own four antagonists.
2. **The ground is a plane where the real Kosovo is 546–1 193 m**, MEASURED off `fb-tiles` `/elev`
   (42.70/21.10 = 546.34 m, 42.60/20.90 = 586.42 m, 42.70/20.70 = 588.60 m, 43.00/21.00 = 1 192.75 m).
   The campaign is not flown on `--elev tiles` because the tile server serves the Balkan DEM lazily and
   answers `no dem` on a cold cache — a nine-run determinism sweep over it would be measuring a cache.
   **What tiles would add was measured instead** (attribution A6, sortie 01 twice under `--elev tiles`,
   ground 547.88 m at the aim point): `predErrM` **58.08 → 46.50 m**, `aimErrM` 41.09 → 46.06 m, target
   still destroyed, **9 of 9 telemetry files byte-identical between the two tiles runs**. Real terrain
   moves the BALLISTIC geometry (the release is 548 m lower above the ground) and creates **zero
   masks**, because masking is a computation that does not exist. The thing this campaign is missing is
   not in the DEM.

### The ten sorties, their fingerprints and their answers

Campaign exit **3**, step exits `0 3 3 3 3 3 3 3 3 3`. Campaign fingerprint under `--elev const`:
`6185addc27ec3ef896cd1aed4750d7a6bdf8555f9a3a1e2c6b12971533b8d80a`. Wall clock for the whole campaign:
**38.3 s**.

| # | Mission | ctrl | exit | fingerprint | The answer to its one tactical question |
|---|---|---|---:|---|---|
| 1 | `w4-01-floor` | — | 0 | `bc669334ab3c0186` | **The floor costs 2.38 m of prediction error per 1 000 m and buys the entire short-range layer.** Delivery `predErrM` 58.08 / `aimErrM` **41.09 m**, target destroyed inside a ~45 m fragment radius. `zone_flak_s` **0.0 s** against `zone_sambelt_s` **285.2 s**; the Strela-2M and the ZU-23-2 produce **0 TRACK, 0 LAUNCH** — and the gun's own eye DOES see the jet (`site CUE … signal=0.773`), so the wall is the ENVELOPE and not a blind sensor. The 2K12 fires 3 of 3 and none arrives, for the reason below |
| 2 | `w4-02-overcast` | 01 | 3 | `89f1997ab141e9de` | **[ctrl 01, ONE line: `wx`.] The deck is scenery; the wind is the weather.** `aimErrM` 41.09 → **257.30 m** and the target lives. Ladder (this file at `wx wind 272 <kt>`): 36.80 / 102.13 / 153.62 / 192.81 / 246.38 / 282.63 m of along-track error at 0/10/20/30/40/46 kt = **5.014 m per knot** [DERIVED]. The fixture's 46 kt behaves like a constant 40.8 kt, so the 26.2 m between them is **5.2 kt of wind PROFILE and not cloud**. Cloud's contribution here is structurally zero: **0 `vis` lines**, one aircraft, and a fire control that takes no weather by design |
| 3 | `w4-03-below-floor` | 01 | 3 | `858393e587dae283` | **[ctrl 01, ONE fact: 4 572 → 1 200 m.] Under the floor the bomb is no better and the aircraft is inside everything.** `aimErrM` **46.99 m**, i.e. 5.9 m WORSE. `zone_flak_s` **347.3 s** (objective UNMET) against `zone_sambelt_s` 0.0. The MANPADS launches **twice**, the gun empties **40 of 40**, and the same 2K12 that missed by kilometres at the floor now passes at **9.22 m and 13.57 m against an 8 m fuze**. Every SAM row in the catalogue reaches to 100–450 m, so **going low buys nothing in this tree** — what it bought in 1999 was terrain, and terrain is `C4` |
| 4 | `w4-04-emitter-hunt` | — | 3 | `77782e76709d55f7` | **THE CAMPAIGN'S CENTRAL RESULT. Yes — but only from below NATO's own floor.** Both AGM-88 kill the node (3.82 m, 1.96 m at t = 66.5/67.5) from 3 000 m. **The ladder that forced that altitude** (attribution A2, one `p18`, one round, 20.0 km, launch altitude the only variable): 1 252 m short at 1 500 m · **4.47 m** at 3 000 · 0.018 at 3 500 · 0.009 at 4 000 · 0.049 at 4 100 · **0.104 at 4 150** · **74.8 m at 4 200** · 1 219 m at 4 400 · **2 484 m at 4 572**. The last FRESH look of every failing shot is at **15.00°** measured at the position — the P-18's own `SearchElCenterDeg 5 + SearchElHalfDeg 10` [T4]. **The 15 000 ft floor sits 372–422 m above the ceiling of its own SEAD weapon** |
| 5 | `w4-05-emcon` | 04 | 3 | `26815af6f6698f06` | **[ctrl 04, ONE line: the node's `set emcon free → hold`.] Total emission discipline is worth the positions and nothing else.** `site RADIATE` 3 → **0**, `site TRACK` 2 → **0**, `site LAUNCH` 4 → **0**, `net CUE` 4 → **0**, AGM-88 released 2 → **0**, positions lost 1 → **0** — and **2 of 2 strikers reach release in both files, at the identical ticks and the identical `aimErrM`**. `net JOIN` stays 2: a buried cable connects whether anybody transmits or not |
| 6 | `w4-06-decoy` | 04 | 3 | `77232651160faceb` | **[ctrl 04, ONE fact: three more emitters.] The decoy works completely, and it is the same catalogue row as the thing it protects.** Both rounds latch and kill `k6dka` (0.019 m, 4.75 m); the node, the 2K12 and the S-125 all live. Second-order: with the node alive the net keeps cueing — `net CUE` **26 against 4** — and the 2K12, which fires nothing in 04 because the node dies inside its 26 s reaction, fires **all three 3M9**. Belt expenditure 4 → **7 launches**. `SUPPRESSION_LOST` at t = 120.1 |
| 7 | `w4-07-cap-intercept` | — | 3 | `2757c699583ca1de` | **A fighter with no radar does nothing, and the flight never learns it is there.** Each Viper's receiver carries **one** fire-control symbol for a PAIR of MiG-29s; the blind jet never produces one, so the F-16 cannot even count the opposition. `k7mig1` fires one R-27R and dies to an AIM-120 at **2.08 m**; `k7mig2` produces 0 radar contacts, 0 IRST contacts, 0 launches and survives. **And this is the one place in the campaign where cloud bites**: `vis MASKED … transmittance=8.00571e-13` between a MiG at 5 150 m and an F-16 at 8 000 m through the fixture's 72.6 % mid deck |
| 8 | `w4-08-package` | — | 3 | `b74545eca5c8144f` | **CHAIN HEAD. The suppression element is worth its airframes, and its price is that it cannot obey the floor.** Both AGM-88 into the node at **2.5 cm and 1.6 cm** from 20 km, `SUPPRESSED emittingS=63.7`. The S-125 gets one salvo away west at the Weasels and then stops; the 2K12 fires **nothing** — still inside its 26 s reaction when the cue dies. **2 of 2 strikers reach release and both bombs miss** (244.68 m, 188.75 m under the fixture's 46 kt); `k8stra` is killed 14 s AFTER its own release by an R-73 at 1.30 m, while two R-27R that arrived first passed at 11.24 and 12.25 m inside a 13.8 m fuze and did not kill |
| 9 | `w4-09-kez` | — | 3 | `ae101b182ff6aa83` | **Zero of six in wind, three of six in calm, and four of six is the most the store allows.** 4 of 4 released, `aimErrM` 69.03 / 68.40 / 71.89 / 78.76 m against a ~45 m radius. Attribution A3 (`wx calm`, one line): 57.98 / 40.67 / 50.37 / 43.28 m and **3 of 6 destroyed**. The ZSU-23-4 in the middle of the column produces 0 TRACK, 0 LAUNCH, 0 BURST against a 4 572 m ingress |
| 10 | `w4-10-allied-force` | chain | 3 | `50de24225ef47473` | **CHAIN TERMINUS. The package finds every target, releases on all four, destroys none, and comes home complete.** 4 of 4 strikers release, **8 of 8 recover** (the spec's ≥7 of 8, met); `aimErrM` 275.08 / 264.40 / 270.19 / **181.57 m** and 0 of 5 aim points die (the spec's ≥3 of 4 nodes + the bridge, not met). Both AGM-88 kill a **decoy**. `site LAUNCH` 5, zero arrivals, zero aircraft lost on either side |

### What a defender who will not radiate gains, and what it gives up

The campaign's own question, measured on ONE geometry with ONE lever pulled three ways
(04 → 05 → 06), plus the escalation the package files add:

| | `04` node radiates | `05` nothing radiates | `06` node + 3 decoys |
|---|---|---|---|
| `site RADIATE` / `TRACK` / `LAUNCH` | 3 / 2 / 4 | **0 / 0 / 0** | 4 / 2 / **7** |
| `net CUE` (`net JOIN`) | 4 (2) | **0** (2) | **26** (2) |
| AGM-88 released / what they hit | 2 / **the node** | **0** / — | 2 / **a decoy** |
| positions lost | 1 (the node) | **0** | 1 (a decoy) |
| `objective suppress … emitting 120` | **MET**, `emittingS=63.7`… `66.5` | **MET**, `emittingS=0` | **LOST** at t = 120.1 |
| strikers reaching release | **2 of 2** | **2 of 2** | **2 of 2** |
| the strike's `aimErrM` | 48.07 / 54.15 m | **identical** | **identical** |

**What it gains: its positions, and nothing else.** **What it gives up: the whole engagement** — six
rounds, two firm tracks and every cue message. And the third column is the interesting one: **a decoy
buys the same survival WITHOUT giving anything up.** Three tin sheds on the same catalogue row absorb
the entire suppression element, and because the node lives the net goes on cueing and the 2K12 gets
its full magazine into the air — 7 launches against 4, and 26 cue messages against 4.

So the honest ordering of the three doctrines on this geometry is:

1. **Decoys** — keep the positions AND the engagement, cost: two `p18` shells.
2. **Total silence** — keep the positions, lose the engagement.
3. **Radiate** — keep the engagement, lose the node in 66.5 s.

**And none of the three changes the strike.** In all three files the same two strikers reach the same
two aim points at the same ticks with the same delivery error. That is W3's *"emission discipline is
worth the position and nothing else"* arriving at DOCTRINE scale, one level out and on a whole net:
what the defender protects by hiding is **itself**, and the thing it was defending is unaffected either
way.

### Does the radar decoy work in today's tree? Yes, and it costs nothing

`doc/modules/ground/cast.md` costs the decoy at *"the `p18` row with `rounds 0` and a small
`SearchRangeM`"*. Re-checked against the tree, and both halves need correcting:

| The entry said | Measured |
|---|---|
| `rounds 0` | **not needed.** A `p18` has `Channels 0` and `RoundsDefault 0`; it is already a position that radiates and cannot shoot |
| a small range gate | **not buildable, and not needed either.** There is no `set` key for a position's search range, and inventing one would be a catalogue change. What makes the decoy work is that it is IDENTICAL to the node — the RWR's power law is `1 − (r/2R)²`, so at equal gates the loudest admissible symbol is simply the NEAREST one |
| — | **the seeker's sort is binary and that is the whole discriminator.** `arm_class fire_control` ignores the decoys AND the real node alike (a `p18` and a SAM's search set are both `SurfaceEarlyWarning`; a fire-control beam exists only while a position tracks). `any`/`early_warning` see the whole belt. So today the attacker chooses between *hunt early-warning sets and be decoyed* and *hunt fire-control sets and hear nothing until a battery is already shooting* |
| — | **the floor sets the belt's depth.** Nothing inside `alt/tan(15°)` is audible at all — 11.2 km from 3 000 m, **17.1 km from the 15 000 ft floor**. A first cut of `w4-06` put the belt at 13.5 km with the shooters at the floor and the three decoys **changed not one byte of the run** |

The one thing `air-defence-network.md` names as the honest discriminator — *"a decoy is never on the
net, so it never comes up on a cue"* — is real in the file (the three decoys are on no `net`) and
**unreadable by any sensor in the tree**, because a cue is a message between ground units and no
airborne receiver sees it.

### What of the bad weather is measured, and what is scenery

| Channel | Measured | Verdict |
|---|---|---|
| **Wind on the delivery** | **5.014 m of along-track miss per knot** at 4 572 m [DERIVED from the 6-point ladder in sortie 02]; 20 kt of crosswind turns 3 of 6 kills into 0 of 6 (sortie 09 against A3); the fixture's own 46 kt costs 216 m and every `wx fixture` strike in this campaign misses | **MEASURED, and it is the campaign's biggest single effect.** The fire control gets no wind by design (`doc/missions/weather.md`) |
| **Wind PROFILE against a single vector** | 26.2 m of the difference between the fixture and `wx wind 272 46` = 5.2 kt of effective mean wind | **MEASURED** |
| **Cloud on the eye** | one line of sight closed to `transmittance=8.00571e-13` through a 72.6 % mid deck, sortie 07 | **MEASURED, once, and only because that file puts one aircraft above the deck and one below** |
| **Cloud on the IRST's own masking counter** | `irst_masked` = **0** in every W4 file | measured **zero** |
| **Cloud on radar, on weapon delivery, on a visual ground pickup, on a decision to bring the bombs home** | nothing | **SCENERY (`C16`).** Sortie 02's 0 `vis` lines are the proof that the deck has no consumer in a strike file at all |

**The campaign's own honest sentence: W4 measured a WIND campaign, not a weather campaign.** The
anchor's *"the stormy spring weather just kicked our butts for the first 45 days"* arrives here as a
**miss** and never as an abort, because `FBPilot` has no branch that declines to release — which is
also why the spec's mission 8 was not built (see below).

### Both determinism criteria, measured on the first attempt

Under `--elev const`, read out of `campaign-summary.txt` rather than assumed:

| # | Criterion | Result |
|---|---|---|
| **1** | 3 repetitions × `--threads 1/2/4` produce one campaign fingerprint | **9 runs, 1 fingerprint** `6185addc27ec3ef896cd1aed4750d7a6bdf8555f9a3a1e2c6b12971533b8d80a`, exit 3 in all nine |
| **2** | every step's per-mission fingerprint equals that mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included, on the first attempt |
| **1 — re-run 2026-07-30** | the same criterion under the branch-order change of `b433950` ([`../pilot.md`](../pilot.md) §7.4a) | **9 runs, 1 fingerprint** `f9fc71a35e93315c927d2ac19f32957b4e065bd6f1a5e0779613af0d07a31830`, `--elev const`. **The value above is kept with its date; this is the current one.** Step exits `0 3 3 3 3 3 3 3 3 2`, and the last of those is new: **STEP 10's EXIT MOVED, 3 → 2** — see the note under the table |
| **2 — re-run 2026-07-30** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 | | `bc669334ab3c0186 89f1997ab141e9de 858393e587dae283 77782e76709d55f7 26815af6f6698f06 77232651160faceb b56890970077654f b74545eca5c8144f ae101b182ff6aa83 258058e7820d9c74` |
| **1 — re-run 2026-07-30 (`E6`)** | the same criterion after the judge-completion fix of [`../doctrine-evolution.md`](../doctrine-evolution.md) X-1 | **9 runs, 1 fingerprint** `a403d3b73db4850fd2034cf8715a0026e87975bf788708a6574702d147328050`, `--elev const`. **The rows above are kept with their dates; this is the current one.** Step exits `0 3 3 3 3 3 3 3 3 2` — unchanged. The fingerprint MOVED and **exactly one step fingerprint with it** — step 10 (`w4-10-allied-force`), the cell X-1 was found on: `kamig4` still departs at t = 695.3 and the run still ends there with `LOC`, but the ten remaining judges now publish their vectors, so the eight Blue F-16s read `V = 18, M = 8` where they read `V = 16, M = 0`. |
| **2 — re-run 2026-07-30 (`E6`)** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 (`E6`) | | `bc669334ab3c0186 89f1997ab141e9de 858393e587dae283 77782e76709d55f7 26815af6f6698f06 77232651160faceb b56890970077654f b74545eca5c8144f ae101b182ff6aa83 c30a793c3dfc2f09` |

> **STEP 10's EXIT MOVED, 3 -> 2.** `w4-10-allied-force` no longer times out with everybody alive:
> `kamig4` departs (`monitor KO … stall/mush`) at t = **695.3 s**, **4.7 s** before the 700 s timeout
> would have ended the run. Same displaced trajectory, landing on the known MiG-29 fragility
> ([`../pilot.md`](../pilot.md) §Gaps 2.9); everything the capstone measures — four strikers, two
> Weasels, four ground objects — is unchanged up to that tick. **It is a worse ending on the same
> substance**, booked in `pilot.md` §7.4b on 2026-07-30 and recorded here.

**And the replay was run after the FIRST mission**, on a throwaway one-step `.fbc`
(`mods/f16/src/campaigns/w4-step1-check.fbc`, deleted afterwards): `01 … campaign fp=bc669334ab3c0186 standalone
fp=bc669334ab3c0186 MATCH`. **Annotating the ten files with their MEASURED blocks after the runs left
all ten per-mission fingerprints and the campaign fingerprint unchanged** — the check that a comment is
a comment.

### The carry: one callsign, and its value is 25 % of the cue traffic

`carry units ground stores`, not narrowed. The chain is **08 → 10** and carries exactly one callsign,
`kosnod`, the forward early-warning node. Sorties 01–07 and 09 are pairwise disjoint from both ends and
from each other in every unit they can lose, aircraft *and* ground.

`campaign CARRY unit=kosnod action=drop reason="destroyed in an earlier mission"` — one line, and this
is what it is worth, measured by running sortie 10 **twice**, as campaign step 10 and standalone
(attribution A5):

| quantity | in campaign (node dead) | standalone (node alive) |
|---|---:|---:|
| `net CUE` | **60** | **80** |
| `net JOIN` | 3 | 4 |
| `site RADIATE` / `TRACK` / `LAUNCH` | 3 / 3 / 5 | **identical** |
| `stores DELIVERY` and all four `aimErrM` | 4 / 275.08, 264.40, 270.19, 181.57 | **identical** |
| aircraft lost / positions lost | 0 / 1 decoy | **identical** |
| telemetry | **21 of 41 files byte-identical**; the other 20 differ in **2 to 6 of 184–202 columns**, and the union of every differing column in the whole run is SEVEN: `rwr_threats` `rwr_mode` `rwr_brg` `rwr_el` `rwr_leth` `rwr_new` `dl_near`. **No trajectory column moves** | |

Campaign totals: `ATTRITION` 1 friendly aircraft, 1 hostile aircraft, 9 hostile ground objects;
`EXPENDED mk82=17 aim120=9 agm88=8 r27r=4 r73=3 mk84=2`.

**So killing a forward early-warning node against a two-node net is worth 25 % of the cue traffic and
nothing else** — W3 measured 36 % on its own topology, and W4 reproduces the shape on a second one.
Both are the qualification of O5's *"one Mk-84 on the P-18 costs the missile layer two nights"*: **O5's
field had one node.** The lever's value is a property of the net's topology, three campaigns running.

### What this campaign found while building, none of it fixed here

Rule 9: *the defect sits in the seam you did not look at.* W4 found three, and none of them is in the
emission-discipline machinery the campaign was written about.

| # | Finding | The measurement that pinned it |
|---|---|---|
| **1** | **A semi-active battery that starts a RAILS reload orphans every round it has in the air.** At the third launch a 2K12 goes `site STATE … from=ENGAGE to=RELOAD why="rails empty"` (`RailCount` 3, `ReloadS` 600); Reload radiates beam 0 only, so the illuminator stops. The rails are a LAUNCHER property and the illuminator is a different vehicle in the real 2K12, so this is FlightBox's artefact and not the system's | `w4-01`: all three 3M9 log `missile ILLUMINATION_LOST` 0.2 s after the third launch, with round 2 **1 776.6 m from the aircraft after 27.1 s of flight** — about 9.6 s from arrival at its 185 m/s closure. **Attributed with a control run** (A4): the identical file with `set rounds 4`, so the MAGAZINE is not empty, loses the illumination at the **identical tick** with identical `tofS`/`rangeM` on all three rounds. A 2K12's effective magazine against one target is therefore **2, not 3** |
| **2** | **`objective suppress … emitting <s>` cannot tell "we held it down" from "it was never up".** The judge accumulates emitting seconds and compares them with an allowance, and `0 ≤ 120` is met | `w4-05`: `mission SUPPRESSED … emittingS=0 allowanceS=120` **MET** on a position that never radiated, in a file with 0 `site RADIATE` lines. This is W5's rule 15 — *name WHICH nothing it is* — arriving in the objective vocabulary one layer down, and every W4 header tells the reader to count `site RADIATE` beside the objective line |
| **3** | **An anti-radiation round's "first admissible symbol" latch has no memory of what it was launched at, so a dispersed belt hands it from emitter to emitter.** Combined with the P-18's +15° elevation limit, each symbol drops out of the receiver's table as the round closes and the round takes the next one | an early cut of `w4-04` with the Weasels at the floor: `missile SEEKER_ACTIVE` fires **six times in 11 s** across four symbols on one round, the last at **36.7° off the nose**, and both rounds come down 2 840 m and 3 347 m from anything. The mechanism is contract behaviour on both sides (`air-to-ground.md` §2.2's latch rule and `catalogue.md`'s coverage figure) and the OUTCOME is a defeat mechanism nobody wrote |

### Where the built campaign departs from §3's table, and why

The Spec above is **left standing as written** and the departures are listed here, because each was
discovered by building:

| §3 says | Built as | Reason |
|---|---|---|
| mission 3 is a **strike in a mountain valley**, asking how much of the miss is wind and how much terrain-relative geometry | **`w4-03-below-floor`**, the other side of the altitude trade | `C4` plus `--elev const`: there is no valley, and a straight line called a valley would be a mood. The wind half is measured in full by sortie 02 and its 6-point ladder |
| mission 8 is **the weather abort** — can the pilot decide not to release? | **not built**; its slot went to `w4-06-decoy` | the answer is known without a run and the campaign says so: `FBPilot`'s attack phase pickles on its cue and there is no branch that declines. That is the same shape W3 measured for the BINGO warning (`FBPilot::CanPressOn` unreachable), and the anchor names decoys as decisive while it names no abort rate at all — W4 §Knowledge 2 records that the sortie-abort percentage was **never established** |
| missions 4/5/7/10 are **blocked on `C1`** | all four flown | rule 7 |
| mission 1 carries **2 × Mk-82** against a soft point target | exactly that, plus the short-range layer that gives the floor its cause | `C1` |
| mission 10's Blue is **8 F-16 in four flights** and Red **4 MiG-29 + SAM/AAA + 2 decoys** | exactly that | unchanged |

### Conservation, and the gates

`git status --porcelain` lists **eleven new untracked files and no modified one**: ten
`mods/f16/src/missions/w4-*.fbm` and one `mods/f16/src/campaigns/*.fbc`. Gates: `make core-lib gym native wasm`
warning-free; `verify-layers` *"301 files, 828 internal include(s), 12 layers — no upward include, 3
restricted header(s) respected, 6 registry reader(s) inside the perception boundary, 1 antenna-cue
poster(s), 288 file(s) in their layer's namespace (5 C-island file(s) exempt)"*; `verify-models` *"4
upstream-backed model path(s) match assets/MODEL-DELTAS.md (1 declared delta(s), 34 FlightBox-own)"*;
nine harnesses rc = 0.


## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C1` | **no active surface-to-air threat, no emitter, no decoy** | missions 4, 5, 7, 10 entirely; the 15,000 ft floor loses its cause in all ten |
| `C8` | **no HARM** (and no LGB, no Mk-84, no cluster) | the F-16CJ cannot be an F-16CJ |
| `C16` | **cloud affects only the IRST** — not radar, not weapon delivery, not a visual pickup; the cloud rebuild is unbuilt ([`../render/clouds.md`](../render/clouds.md)) | mission 2 and mission 8 measure an absence |
| `C4` | **no terrain masking** — the hook (`const FBWorld*`) reaches the sensor slots, the computation does not exist | "shielded by mountainous terrain" is the anchor's own phrase for the thing that is missing |
| `C14` | **no moving ground units** | the KEZ armour hunt is a hunt for parked vehicles |
| `C2` | **no time of day** | mission 10 is a night package |
| `C5` | **no tanker** | Aviano-to-Kosovo is a tanked profile |
| `C7` | **no other modules** | F-117, EA-6B, E-3, KC-135 absent |
| `C15` | **no package coordination** | four flights on one target set with no timing |
| `C0` | **no campaign layer** | 78 days of attrition against a defence that hides is *the* campaign-layer subject |
| `C13` | **no jamming** | |
| `C22` | **no connected air defence** ([`../air-defence-network.md`](../air-defence-network.md)) | mission 4's emitter that "emits and stops" is a **posture**, and a posture needs somebody to wake it: a net whose members stay dark until a node cues them is exactly the anchor's "dispersed, radars mostly not emitting". Mission 5's decoys gain their one honest discriminator from it — a decoy is never on the net, so it never comes up on a cue |
| `C23` | **no declared, judged belt geometry** | **the 15,000 ft floor is this campaign's whole subject and today it is an arbitrary altitude.** With `zone` lines the floor becomes a measured trade: seconds inside the AAA band against seconds inside the SA-6 band, per striker, per route |

### The honest headline, and what the build did to it — 2026-07-29

**Two of the three sentences below were overtaken and the third was not.** `C1` closed, so the ground
threat exists and the floor has its cause; the cloud still reaches only the eye, and W4 measured
exactly one line of sight closed by it (`transmittance = 8.0e-13`, sortie 07) against zero effect on
every bomb it dropped; and **terrain that blocks a radar still does not exist, which is why this
campaign staged three of its own four antagonists and said so in the capstone's header.** The campaign
also measured what putting the REAL Kosovo terrain under it would change (`--elev tiles`, ground
547.88 m): the delivery number moves by 11.6 m of prediction error and **zero masks appear**.

The original headline, left standing:

Allied Force is the campaign whose **subject FlightBox is closest to being able to model and furthest
from having modelled**. The weather hook is real, measured and already changes outcomes; the terrain
is real; the elevation provider is real. What is missing is the other half of every one of those
sentences: cloud that a sensor or a weapon notices, terrain that blocks a radar, and a ground threat
that gives the altitude floor a reason to exist. Two of those three (`C4`, `C16`) are already named
as next steps elsewhere in the tree — [`../roadmap.md`](../roadmap.md) R5 and the "terrain masking
next" line in R6.

---

## Knowledge

### 1. The anchor with its sources

- **Dates, duration, scale.** [Kosovo Air Campaign, March–June 1999 (NATO topic page)](https://www.nato.int/cps/en/natohq/topics_49602.htm)
  [T1]; [1999 — Operation Allied Force (Air Force Historical Support Division)](https://www.afhistory.af.mil/FAQs/Fact-Sheets/Article/458957/1999-operation-allied-force/)
  [T1]; [Kosovo — Operation Allied Force (Naval History and Heritage Command)](https://www.history.navy.mil/browse-by-topic/wars-conflicts-and-operations/bosnia-kosovo/allied-force.html)
  [T1]. The "planned 72 hours, ran 78 days" framing is carried by the Osprey campaign study
  [Operation Allied Force 1999 (Osprey, Laslie)](https://www.ospreypublishing.com/us/operation-allied-force-1999-9781472860323/)
  [T3].
- **The 15,000 ft floor, the weather quote, the SEAD force, the Serb concealment and decoy tactics.**
  [Operation Allied Force — how airpower won the war for Kosovo (Air & Space Forces Magazine)](https://www.airandspaceforces.com/article/operation-allied-force-how-airpower-won-the-war-for-kosovo/)
  [T3] and [Operation Allied Force: Lessons for the Future (RAND research brief)](https://www.rand.org/pubs/research_briefs/RB75.html)
  [T3]. The "50 F-16CJ Block 50 with AGM-88" figure and the SAM inventory are from
  [NATO bombing of Yugoslavia (Wikipedia)](https://en.wikipedia.org/wiki/NATO_bombing_of_Yugoslavia)
  [T4] and [The Serbian Air Force against the 1999 NATO Allied Force operation (Defence reDefined)](https://defenceredefined.com.cy/the-obscure-files-of-an-anniversary-or-david-vs-goliath-the-serbian-air-force-against-the-1999-nato-allied-force-operation/)
  [T4].
- **The F-16 loss of 2 May.** [Holloman commander recalls being shot down in Serbia (f-16.net)](https://www.f-16.net/f-16-news-article2167.html)
  [T4].
- **The MiG-29 side.** See [`o5-airfield-defence.md`](o5-airfield-defence.md) §Knowledge 1, which owns
  the Batajnica material in full; it is summarised here rather than duplicated.

### 2. Where the sources are thin, and it is stated

| Thing | Status |
|---|---|
| The percentage of sorties cancelled or aborted for weather | **not established.** The Short quotation is qualitative ("the first 45 days"); no percentage was found and none is invented. Mission 8's difficulty is therefore `[SET]` |
| SA-6 "shoot and scoot" cycle times, emitter discipline intervals | **not sourced.** The campaign describes the behaviour ("emit, engage, move, stay quiet") without numbers, and mission 4/5 must declare its own `[SET]` timings once `C1` exists |
| Which specific bridges / IADS nodes were struck when | **not sourced on this pass**; the ground sets above are archetypes, not a target list |

### 3. Why missions 1 and 2 are the pair to build first

They are one `wx` line apart, they need nothing that does not exist, and they produce a number the
whole cloud rebuild will be measured against: **the delivery error from above the floor, with and
without a real cloud deck between the aircraft and the target.** Today the two runs should differ
only by the wind that the fixture happens to carry — and if they differ by more, that is a finding
about the weather provider, not about the weather.
