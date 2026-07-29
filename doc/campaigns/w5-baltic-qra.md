# W5 — Baltic Air Policing / QRA (identification as the task)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet. It is also **half of the
identification argument**; the other half is [`o2-pvo-intercept.md`](o2-pvo-intercept.md), and the
argument itself is stated once in [`INDEX.md`](INDEX.md) §"The identification task".

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of NATO Baltic Air Policing and QRA practice | §Knowledge 1, every fact cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare, and where the perception boundary runs | [`../sensors.md`](../sensors.md), [`../missions/sensors.md`](../missions/sensors.md), [`../missions/verdict.md`](../missions/verdict.md), [`../vision.md`](../vision.md) ("Anti-cheat is a game decision"), [`../modules/f16/datalink-iff.md`](../modules/f16/datalink-iff.md) |

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

**Temporal honesty: none needed.** F-16s of several NATO air forces have flown Baltic Air Policing
rotations, and the mission is current [T4]. What *is* unusual is that this campaign's success
condition contains **no weapon**.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Mission start | **30 March 2004**, when the Baltic states joined NATO | [T4] |
| Bases | **Šiauliai (Zokniai) AB, Lithuania** (≈ 55.89 N 23.39 E); **Ämari AB, Estonia** from May 2014 (≈ 59.26 N 24.21 E) — *approximate, verify* | [T4] |
| Rotation | three months initially, later **four months** | [T4] |
| Detachment | typically **four fighter aircraft**, 50–100 support personnel | [T4] |
| Framework | part of the **NATO Integrated Air Defence System**, 24/7 surveillance; described as **purely defensive** | [T4] |
| Scramble trigger | an aircraft that **files no flight plan**, **squawks no transponder code**, or **does not talk to air traffic control** | [T3]/[T4] |
| Scramble classes | **Alpha scramble** (a live alert) versus **Tango scramble** (training) | [T4] |
| Reaction | typically **under 15 minutes** | [T4] |
| The task itself | **put eyes on it**: obtain a **visual identification**, shadow the contact along the airspace boundary, escort it clear, peel off. "Policing in the literal sense — presence, identification, escort — not combat" | [T3]/[T4] |
| Typical subjects | Russian military traffic over the Baltic, frequently without transponder or filed plan; transports, ELINT/recon (Il-20 class), bombers with fighter escort | [T4] |

### 2. The campaign contract — and why it is the strictest one in the set

| Contract | Acceptance / measurement anchor |
|---|---|
| **The task is identification, not engagement** | in eight of the ten missions a weapon release is a **failure**, not a result. The pilot's job ends at a stated range abeam a contact |
| **The intruder's team must not leak into the intercepting pilot's behaviour** | *the campaign's core acceptance test:* run each mission twice, identical but for the intruder's `team` line (`neutral` vs `hostile`), and require the interceptor's own telemetry to be **byte-identical up to the first tick at which a SENSOR discriminates**. A pilot that behaves differently before that tick has read the registry, and that is a defect of the same class the tree already guards against structurally ([`../sensors.md`](../sensors.md)) |
| Identity comes only from the two channels that exist | IFF Mode 4 (**two-valued**: friendly / no reply — there is no "hostile" value, [`../sensors.md`](../sensors.md)) and the cooperative datalink (own faction only). **A no-reply contact is not a hostile contact**, and every mission's victory condition must respect that |
| Shadowing is a geometry, not a kill | measured from telemetry: time spent inside a declared range/aspect box abeam the contact, without a weapon event |
| **Ground targets in every mission** | a QRA jet launches over a country: coastal radar sites, ports, a shipping lane. They are never targets here — they are what the intercept is *protecting*, and the campaign needs them declared so that "protect" can eventually be a verdict (`C12`) |
| The verdict is machine-read — and today it cannot be | the objective vocabulary has no *identify*, no *escort*, no *do-not-fire*. Every mission below states the verdict it needs and the workaround it must use until `C12` closes |

### 3. The ten missions

| # | Mission | Task | Time | Wx | Blue | Red / subject | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `w5-01-cooperative` | intercept a contact that answers IFF | day | calm | 2 F-16 (flight), QRA scramble | 1 F-16, `team friendly`, `set iff_xpdr on` | 1 `target_soft` (coastal radar site) | close to the abeam box, **no weapon event** | Does the pilot stop when the contact answers? The trivial case, and the baseline every later run is compared against |
| 2 | `w5-02-silent` | intercept a contact that does not answer | day | calm | 2 F-16 | 1 MiG-29, `team neutral`, `set iff_xpdr off` | as above | reach the abeam box, hold it, **no weapon event** | **A no-reply is not a hostile.** Does the pilot understand that, or does it treat silence as permission? |
| 3 | `w5-03-team-swap` | mission 2 with the intruder's `team` changed to `hostile` and nothing else | day | calm | 2 F-16 | 1 MiG-29, `team hostile`, `set iff_xpdr off` | as above | **the interceptor's telemetry must match mission 2** up to the first sensor discriminator | **The anti-cheat test itself.** One word in a file the pilot may not read. Any divergence is a finding |
| 4 | `w5-04-escorted` | a subject with a fighter escort | day | calm | 2 F-16 | 1 "transport" (F-16 stand-in, `team neutral`) + 2 MiG-29 escort, `team neutral` | 1 `target_soft` + 1 `target_hard` (port) | identify all three, **no weapon event** | With three contacts and two interceptors, does the flight sort onto the *subject* or onto the *escort*? The sort has no notion of "the one that matters" |
| 5 | `w5-05-shadow` | a long shadow along a boundary | day | `wx wind` | 2 F-16 | 1 MiG-29, `team neutral`, on a 200 km leg | 2 `target_soft` along the coast | hold the abeam box for a declared duration | Can the station-keeping law hold a position on a **manoeuvring, non-cooperating** aircraft, when everything it was built for is a datalink report from a friend? ([`../formation.md`](../formation.md)) |
| 6 | `w5-06-two-subjects` | two simultaneous scrambles | day | calm | 2 F-16 (one flight) | 2 MiG-29 on divergent tracks, `team neutral` | 2 `target_soft` | both identified, **no weapon event** | Does the flight split deliberately? There is no lead tasking (`C15`/`F6`) — the split can only emerge from the cost function |
| 7 | `w5-07-locked-on` | the subject locks the interceptor | day | calm | 2 F-16 | 1 MiG-29, `team neutral`, `set n019_emission illum` | as above | identify + **still no weapon event** | A contact that paints you is not thereby hostile. Does the pilot's RWR-driven defend rule ([`../pilot.md`](../pilot.md) §7.2 — a *seeker* on one's own aircraft is never negotiable) fire on a mere lock? It should not — and this mission is where that is checked |
| 8 | `w5-08-weather-id` | the same intercept in cloud | day | `wx fixture` | 2 F-16 | 1 MiG-29, `team neutral` | 2 `target_soft` | identify | **The mission that cannot be flown at all** (`C3`): the real task is a *visual* identification, and FlightBox has no eye. What is left is a radar contact and an IFF silence — which is exactly the information the real QRA pilot launches to get *past* |
| 9 | `w5-09-night-id` | night intercept of a non-squawking contact | **night** | calm | 2 F-16 | 1 MiG-29, `team neutral`, no transponder | 1 `target_hard` (port) + 1 `target_soft` | identify | Night visual identification is the hardest form of the real task and the one FlightBox is furthest from (`C2` + `C3`). Specified so the pair of gaps is visible together |
| 10 | `w5-10-alpha` | the full QRA: unknown intent, escort present, weapons hold | **night** | `wx fixture` | 4 F-16 (two flights, scrambled at different times) | 1 subject + 2 escorts, `team` **declared by the mission author and not visible to any pilot** | 3 `target_soft` + 1 `target_hard` | all three identified; **a weapon event is a FAIL unless the subject shoots first** | Everything at once: two flights, three contacts, no weapons release, a defensive trigger that must be real but must not be trigger-happy — and the whole run repeated across both `team` declarations with the identical-behaviour requirement of mission 3 |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| F-16C | flyable module | **yes** | the QRA pair |
| MiG-29 | flyable module | **yes** | stands in for every non-cooperating subject; a poor stand-in for a transport or an ELINT aircraft, and the mission headers must say so |
| Transport / ELINT (Il-20, An-26 class) | flyable module | **no** (`C7`) | the actual subject of most real intercepts — slow, large, unarmed, and therefore a *completely different* intercept geometry from a fighter |
| Bomber (Tu-22M, Tu-95 class) | flyable module | **no** (`C7`) | the escorted-subject case |
| Ground surveillance radar (coastal) | ground | **`target_soft` today, non-emitting** (`C1`) | what the intercept protects |
| Port / infrastructure | ground | **yes** (`target_hard`) | |
| Ship | surface unit | **no** (`C14`) | the Baltic case is full of them |
| Control and reporting centre (the thing that scrambles you) | infrastructure | **no** (`C6`) | the scramble itself is outside the mission today: `.fbm` spawns an aircraft, it does not scramble one |

### 5. What must be true before mission 1 can fly

`w5-01`, `w5-02`, `w5-03`, `w5-07` are buildable **today** with one caveat that must be written into
each header: the *verdict* has to be read out of telemetry (no weapon event, geometry held) rather
than declared, because `C12` has no objective for it. **Mission 3 is the one to build first** — it
needs nothing new, it is a two-run experiment, and it is the sharpest anti-cheat test in the whole
campaign set.

---

## State

**BUILT AND FLOWN, 2026-07-29 — the fifth of the ten campaigns to exist as files, and the first in
which the F-16 flies.** Ten `.fbm` in `sim/missions/w5-*.fbm` plus `sim/campaigns/w5-baltic-qra.fbc`,
run as a campaign, replayed step by step, and measured. **No file under `sim/src/`, `sim/tools/` or
`sim/assets/` was touched** (`git status --porcelain` lists eleven new untracked files and **no
modified one**), so the **183** pre-existing `sim/missions/*.fbm` are byte-identical **by construction
rather than by comparison**.

### The spec's own headline is superseded, and by measurement

This file said: *"W5 is the campaign whose subject FlightBox cannot simulate … It cannot do the
identification (`C3`, `C12`)."* Both gaps closed on 2026-07-28, and rule 7 says a blocked mission is
re-checked against the **tree** rather than against a gap's status line. The re-check found **three**
capabilities, not two:

| The spec said | The tree says, measured on this campaign |
|---|---|
| `C3` — no eye | `sensors/FBVisualSystem` is the sixth registry reader. It **identifies an An-26 at 1 086 m and a Tu-95 at 2 049 m**, and recognises a MiG-29 at 825 m |
| `C12` — no `identify`, no `do-not-fire` | `objective identify unit … range … hold …` and `objective no_fire` are the victory condition of all ten sorties. **Nine of ten return a real verdict**, not a measuring rig's TIMEOUT — the first campaign in the set of which that is true |
| `C7` — no transport, ELINT or bomber module | `an26` and `tu95` are catalogue **movers**, and a mover has **no generated deck**, so the `ALPHA` verdict that keeps ten catalogue rows out of a campaign does not reach them. What W5 needs of a subject — a straight line at a published speed, a transponder state, a span — is sourced [T4] for both |
| `C1` — coastal radars do not emit, *"what the QRA protects is scenery"* | the `p18` row radiates, is heard on the intruder's receiver from t = 0.1 as `kind=surface-search`, builds its own tracks and can be killed |

What is **not** superseded: `C2`'s night is a night in which **nothing emits light**, so every night
intercept here is eyeless and both night sorties say so in their headers and measure the zero.

### Where the built campaign departs from §3's table, and why

The Spec above is **left standing as written** and the departures are listed here rather than quietly
edited into it, because all four were discovered by building and each is a measurement:

| §3 says | Built as | Reason |
|---|---|---|
| the subject of 1–3, 5, 6, 8, 9 is an **F-16 or a MiG-29** stand-in | an **`an26`** | the row exists and it is the *real* subject of a Baltic intercept. It changes the answer: a fighter subject would have made every visual identification in the campaign impossible (`w5-07` measures exactly that — a MiG-29 is recognised at 825 m and never identified) |
| mission 4's subject is a *"transport (F-16 stand-in)"* | a **`tu95`** | same reason, and it is the anchor's second archetype (the escorted bomber). Its 50.1 m span is what puts a visual identification at 2 049 m |
| mission 5 is a **200 km leg** | a **106 km** leg with a 40 km run-down in 900 s | 200 km at the An-26's published 237 kt is 1 640 s of subject track and the pass would have been over in the first third of it. The measured quantity — the drift rate of a routed shadow — is unaffected |
| mission 6's two subjects are on **divergent tracks** | two **parallel** tracks 17.8 km apart | heading divergence would put trigonometry into every coordinate without changing the question, which is about the sort. Declared in the file's header |

One thing §3 asked for and got **less** of: it wanted the coastal radar as a `target_soft`. It is a
**`p18` that radiates**, which is more, and it is the reason the `C1` row above could be struck out.

### The arena

Šiauliai (Zokniai) AB, Lithuania ≈ 55.89 N 23.39 E [T4]; the intercept itself over the Baltic west of
the Lithuanian coast on an east-bound track toward the boundary, `[SET]`. Ground: a **radiating P-18**
at 55.80 N 21.05 E, the port of Klaipėda (55.70 N 21.13 E [T4], `target_hard`) and Palanga (55.92 N
21.07 E [T4], `target_soft`); **nothing on the ground is a target**. `--elev const`, 0 m datum, no
terrain masking (`C4`). Scale: 1° of longitude = 62 411 m [DERIVED]. **No ships (`C14`)** — the one
cast item a Baltic campaign asks for and cannot have.

The clock is a **January** rotation, 10:00 Z for the seven day sorties and 20:00 Z for the two night
ones plus the capstone. Not June, because at 55.9 N the midsummer solar-midnight elevation is
`lat + dec − 90` = **−10.7°** [DERIVED], barely under the −9° nautical-twilight floor the daylight term
uses: a June "night" would be a marginal reading of a threshold rather than a night.

### The ten sorties, their fingerprints and their answers

Campaign exit **3** (the worst step's). Campaign fingerprint under `--elev const`:
`49d3320f5e9761db2f1df85a12d9008e0d8559395c141c31e2e06903b9fe0200`. **The FRAME round of 2026-07-29 moved it again** — the spawn state is now the trimmed airframe's own rather than position only, so the first 0.01 s of guidance moved in every mission with an airframe (`sensors.md` §10, item 24). Post-frame-round value, both criteria re-measured and still holding: `786b979426614e5ea6ffb4fed65c0b189f464f02b62c361fc4ad67b0b03db367`.

| # | Mission | ctrl | exit | fingerprint | The answer to its one tactical question |
|---|---|---|---:|---|---|
| 1 | `w5-01-cooperative` | — | 0 | `08185df54db8ad9e` | **The channel discriminates, and it moves five columns and not one metre.** `IFF_REPLY … reply=friendly` at t = 1.9 / 4.781 nm, `RADAR_LOCK … iff=friendly` against 02's `iff=unknown`, and **no `flight SORT_ASSIGN` at all** — a contact that answers is never assigned. Six differing log lines of 71; **5 of 184 telemetry columns** (`fcr_iff`, `flt_src`, `flt_assign`, `flt_switch`, `flt_dup`), zero metres of flight path. **This is not O2's result**: on the MiG the same experiment moved *zero* columns. Rule 11 exactly — what changed the mechanism's value is not the aircraft, it is the presence of a wingman |
| 2 | `w5-02-silent` | 01 | 0 | `8b7b8b390c15b297` | **Silence stays silence all the way down — and the transport is named by eye at 1 086 m.** `IFF_REPLY reply=none` t = 1.9 at 8 850 m; `SORT_ASSIGN` on the same tick is the *only* consequence silence has anywhere in 184 columns. The eye: `CONTACT` t = 17.0 / **7 067 m**, `RECOGNISED` t = 80.0 / **1 879 m**, `IDENTIFIED` t = 90.0 / **1 086 m**, `type=an26`. Verdict t = 108.6, closest approach **435.7 m**, cost **108.5 s and 57.9 lb** |
| 3 | `w5-03-team-swap` | 02 | 0 | `9cd50596a87eddad` | **Byte-identical for the whole run, in every channel.** 6 of 6 `telemetry*.csv` identical to 02's; `events.log` differs in **1 line of 75** — `UNIT_RESULT … team=`, written by the runner. Four perception channels live for 300 s and none moved. The **strong** form: there is no discriminator to be identical *up to* |
| 4 | `w5-04-escorted` | — | 0 | `050476a20acd4b31` | **The flight sorts onto the subject, and the split lasts exactly one second.** t = 1.9 lead → track 1 (`free=1`), wingman → track 2 — a real split; t = 2.9 the wingman gives it up for track 1 (`dup=1`). The cost function is time-to-arrive, so both converge on the *nearest* contact and the duplicate is flagged, not resolved. **And the span ratio decides the sensor half**: the Tu-95 is `CONTACT` on the FIRST LOOK at 8 869 m and `IDENTIFIED` at **2 049 m**; the two MiG-29s flying formation on it are contacts at ~3 km and are **never recognised** at 1 734 m and 1 600 m |
| 5 | `w5-05-shadow` | — | 0 | `6d1cca8cbf43a7e0` | **There is no station keeping here, and the calm control run is what proves it.** With wind: box entered t = 395.7, cumulative dwell **236.6 s**, CPA 494.3 m, then it opens again at a steady **13.0 m/s** — which *is* the tailwind component (14.6 m/s [DERIVED]), because `FBAirMover` has no wind and the F-16 does. **A2 (`wx calm`, one line): `identify` UNMET — and not because the hold is worse but because it is PERFECT**, freezing the gap in a 136 m band for 380 s at the 4.5 km the author's arithmetic left. Open loop holds a wrong range beautifully and flies through the right one |
| 6 | `w5-06-two-subjects` | — | 0 | `c96e8ba100b8b6d8` | **The flight cannot split, because neither aircraft can see that there is anything to split.** Two aeroplanes 17.8 km apart, one on each radar, and **both** members log `track=1 free=0 dup=1`. `FBFlightPicture::Assign` matches every member against the contact list of the aircraft doing the computing; the picture is shared in POSITIONS, not in CONTACTS, and on a policing sortie nobody engages, so no target point is ever published |
| 7 | `w5-07-locked-on` | — | 0 | `682daa03665cb8d1` | **Nothing was fired, and the reason is two reasons.** `eng_state` is `idle` for all 3 001 rows: a pilot on `route` never enters the engagement machine, so the defend rule is not restrained — it is not executing (`C19` with a column attached). **And the geometry protects the interceptor for free**: the subject radiates from t = 0 and its first fire-control warning reaches the lead at t = **201.3**, 30.1 s *after* the identification latched and 22.4 s after a 491 m closest approach. A stern conversion is flown into the one place a forward-looking radar does not point |
| 8 | `w5-08-weather-id` | 02 | 0 | `fa61e54780e0a927` | **The cloud closes the path completely, the wind moves the pass, and the two are separated by a control run.** `vis MASKED … transmittance=2.9e-25`; **the lead never acquires the aeroplane it identifies** — its only visual contacts in 300 s are of its own wingman. 02 / **A3 (wind only)** / 08: CPA 435.7 / 1 300.6 / 1 242.5 m and `vis IDENTIFIED` at t = 90.0 / **never** / **never**. So the WIND costs the identification *level* and the CLOUD costs the *channel* |
| 9 | `w5-09-night-id` | 02 | 0 | `5faf1329dd5ab95d` | **Nine visual lines become zero, six telemetry columns move, and the verdict is identical to the tick.** `mission IDENTIFIED … rangeM=704.969 … heldS=30.1` at t = 108.6 in both. The campaign's own advertised price — *"a pilot that flies the box with its eyes shut still scores"* — paid in public and **exactly**, nothing worse |
| 10 | `w5-10-alpha` | — | 3 | `0fca0dd4f36031f3` | **The weapons hold held on all four jets; the identification did not, and what broke it was navigation.** Three `IDENTIFIED` (bomber t = 116.4, one escort t = 145.8, bomber again by the second flight t = 443.9). The northern escort was inside the 2 000 m box for a cumulative **6.3 s** of a required 20, CPA 1 838 m: it *entered* and was not *held*. `no_fire` **met on all four**, with eight rounds on the rails, master arm armed, and two N019s in `illum` for 700 s. `vis` lines: **0** |

### What an identification costs — the campaign's own question, answered

Four currencies, measured on `w5-05` (the only sortie that starts far enough out to price a run-down)
with `w5-02`'s short pass beside it:

| Currency | `w5-02` (9.1 km astern) | `w5-05` (40 km astern, 900 s) |
|---|---|---|
| **Time** to `vis IDENTIFIED` | 90.0 s | **412.9 s** |
| **Time** to the geometric verdict | 108.6 s | 515.6 s |
| **Fuel** to the visual identification | 45.8 lb | **243.5 lb** (10 363.1 → 10 119.6) |
| **Fuel** to the verdict / for the whole station | 57.9 lb / 197.6 lb | 320.0 lb / **652.9 lb** |
| **Proximity** (closest approach) | 435.7 m | 494.3 m |
| **Risk** | none — the subject cannot look back | none |

**And the transit is not in that bill.** There is no scramble (`C6`): 40 km is the last leg of a
run-down, not a sortie. The real 15-minute reaction is unmeasured and unmeasurable here.

**Risk has to be read off `w5-07` and `w5-10`, and it is smaller than the mission profile suggests.**
The one subject in the campaign that can look back is heard by the interceptor **201.3 s** into the
run — after the pass — because the pass is flown from astern. Against two escorts radiating for 700 s
(`w5-10`) the four-ship's `no_fire` is still met and the aggregate `campaign EXPENDED` line is
**absent**, i.e. zero rounds over ten sorties.

### The identification counter-check, with its three runs planned from the start

The requirement (`INDEX.md`): build the mission twice, identical but for the subject's `team`, and
require byte-identity up to the first sensor discriminator. Rule 10 adds the third file.

| Comparison | Telemetry | `events.log` (normalised for callsign prefix and mission name) |
|---|---|---|
| **02 vs 03** — `team neutral` → `team hostile`, one token | **6 of 6 byte-identical** | **1 differing line of 75**: `mission UNIT_RESULT … team=`, written by the RUNNER |
| **01 vs 02** — the control: the subject ANSWERS | interceptors differ in **5 of 184 columns**, all identity/sort; **zero** trajectory columns. The subject's file differs in **1 column** (`iff_xpdr`); the P-18's in **0 of 193** | **6 differing lines of 71**: two `IFF_REPLY`, two `RADAR_LOCK … iff=`, and two `SORT_ASSIGN` lines that exist only in 02 |

**The third file is the one that makes the pair mean anything, and this campaign is the first to
budget it before writing the first file rather than after the fact.** It also produced the round's
most interesting disagreement with O2 — see below.

### Rule 11 applied: the same experiment, the opposite answer, and why both are right

O2 measured the identity channel and reported *"the answer changed and no metre of flight path did,
which is the honest statement of what this pilot does with an identity today: **nothing**"* — zero
telemetry columns moved. W5 runs the identical experiment and gets **five columns**.

The difference is not the airframe and not the theatre. It is that **O2's interceptor pair had no
cooperative sort in the loop and W5's does**: `pilot/FBFlightPicture` reads the radar block's per-track
IFF field, and a track that answers `friendly` is excluded from the assignment. So the comparable
quantity across the two campaigns is not *"what an identity is worth"* but *"what an identity is worth
**to a flight**"*, and a campaign that had not looked would have published a contradiction. That is
rule 11 arriving on its author's own next round.

**What still holds from O2:** the identity moves **no trajectory**. Five columns of assignment
bookkeeping is not a decision; nothing steers, nothing arms, nothing turns.

### Both determinism criteria, measured

Under `--elev const`, read out of `campaign-summary.txt` rather than assumed:

| # | Criterion | Result |
|---|---|---|
| **1** | 3 repetitions × `--threads 1/2/4` produce one campaign fingerprint | **9 runs, 1 fingerprint** `49d3320f5e9761db2f1df85a12d9008e0d8559395c141c31e2e06903b9fe0200`, exit 3 in all nine; **re-measured after the frame round of 2026-07-29: 9 runs, 1 fingerprint** `786b979426614e5ea…`, exit 3 in all nine |
| **2** | every step's per-mission fingerprint equals that mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included, on the first attempt; **10/10 again after the frame round of 2026-07-29** |

**And the replay was run after the FIRST mission this time.** `sim/campaigns/w5-step1-check.fbc`, a
throwaway one-step campaign containing only `w5-01`, was run and replayed **before** the ten were
finalised: `01 … campaign fp=08185df54db8ad9e standalone fp=08185df54db8ad9e MATCH`. The file was
deleted afterwards; it is not part of the campaign. Two previous rounds confessed running the replay
only at the end, and the rule exists to bound the damage when it diverges.

**Annotating the ten files with their MEASURED blocks after the runs left all ten per-mission
fingerprints and the campaign fingerprint unchanged** — the check that a comment is a comment.

### What this campaign found while building, none of it fixed here

Rule: *the defect sits in the seam you did not look at.* W5 found four; only the second is in the
subsystem the campaign was written about.

| # | Finding | The measurement that pinned it |
|---|---|---|
| **1** | ~~**On the SPAWN tick every RWR reports the emitter's TRUE bearing instead of the relative one.**~~ **CLOSED 2026-07-29 by the frame round.** `FBRwrSystem` transforms the line of sight with `st.roll/pitch/yaw`, and on t = 0.0 that attitude has not been published yet, so the rotation is the identity. The same tick also mis-aims every EMITTER's beam, which is why some pairs hear each other on tick 0 and cannot on tick 1. **PRE-EXISTING and visible in the committed `pair-2v2-f16.fbm`** | isolated on a three-unit fixture, receiver on heading 270 with the emitter due south: `brgDeg=180` at t = 0.0 against `brgDeg=-95.5` at t = 1.1 on the identical geometry — **an error of 275.5°**, held for the receiver's 2.0 s hold time and then dropped. Same class as O5's world/body GCI frame error. **THE CAUSE WAS ONE LINE IN THE BOOT, not in the receiver:** `missions/FBMissionBoot.h` published a spawn state carrying POSITION ONLY, so tick 0's attitude was the identity for every unit and every body transform in the tree — receivers AND emitters. It now carries the airframe's own trimmed state (`FBFdm::Sample`, the same read `Step()` ends with). Re-measured on the committed `pair-2v2-f16.fbm`: `brgDeg = −180.0` with signal 0.9998 at t = 0.0 → **`brgDeg = −0.0`**, i.e. the wingman where it actually is, and the phantom is gone rather than merely re-aimed. Full inventory of the 41 angle handovers and the four defects: [`../sensors.md`](../sensors.md) §10 |
| **2** | **`FBPilot` has no behaviour for this campaign's task, and its own machine says so.** `set task intercept` is a shooting task: `FBPilot.cpp:1040` treats *inside `InterceptAbortRangeNm` = 5.0 nm and never shot* as an **abort** — which is the definition of an identification pass | attribution run **A1** (`w5-02` + one `set task intercept` line): `eng_state` = `abort` at t = 5.1 s at 4.52 nm, the interceptor turns away, `identify` UNMET in 300 s. Every pass in all ten files is therefore a declared ROUTE |
| **3** | **A flight cannot sort two targets that only one member each can see.** `FBFlightPicture::Assign` matches all members against the *computing* aircraft's own contact list; the shared half of the picture is a mate's POSITION and, when engaging, one target point — never a contact list | `w5-06`: two subjects 17.8 km apart, both members log `track=1 free=0 dup=1`. Consistent with `formation.md` §5.2 rather than contrary to it, and it means the "does the flight split" question cannot be asked of the tree as it stands |
| **4** | **The guidance does not crab, so a long leg bows downwind — and the bow is bigger than the identification box.** Pure pursuit onto the waypoint, with the crosstrack collapsing only at the waypoint itself | `w5-10`'s second flight on one 176 km leg: **3.4 km** off track, closest approach to the subject **4 038 m**, `identify` unmet. The same aeroplane on 34 km vectors identifies at **677.7 m**. And the bow is *speed-dependent* — a 383 kt escort bows further than a 500 kt interceptor on the same leg, which is what leaves `w5-10`'s northern escort 1 838 m out with 6.3 s of dwell |

**A fifth thing is a modelling asymmetry rather than a defect, and it decides `w5-05`:**
`modules/air/FBAirMover` has no wind at all — a mover flies a ground track at a ground speed while
every JSBSim aircraft flies through an airmass. In wind, a declared formation therefore comes apart,
and a briefed co-speed leg is co-speed in the wrong frame.

### The carry, where it lands, and what it was worth

`carry units ground stores` — **not narrowed, and it lands nowhere.** There is **no chain**: all ten
casts are pairwise disjoint in aircraft *and* ground. Rule 2 says the carry belongs where the campaign
has a question about it, and this one has none — nothing shoots, so nothing is lost, touched or
expended.

It is nevertheless declared at full width, because at campaign scope that *is* the measurement:
`campaign ATTRITION unitsFriendly=0 unitsHostile=0 groundFriendly=0 groundHostile=0` and **no
`campaign EXPENDED` line at all** are the aggregate `no_fire` proof over ten sorties, computed by the
runner rather than by the reader. **The price, stated: this campaign shows no attrition arc and
cannot.**

### Conservation, and the gates

`git status --porcelain` lists **eleven new untracked files and no modified one**: ten
`sim/missions/w5-*.fbm` and one `sim/campaigns/*.fbc`. Gates: `make core-lib gym native wasm`
warning-free; `verify-layers` *"300 files, 826 internal include(s), 12 layers — no upward include, 3
restricted header(s) respected, 6 registry reader(s) inside the perception boundary, 287 file(s) in
their layer's namespace (5 C-island file(s) exempt)"*; `verify-models` *"4 upstream-backed model
path(s) match assets/MODEL-DELTAS.md (1 declared delta(s), 34 FlightBox-own)"*; eight harnesses rc = 0.

---

## Gaps

**Re-checked against the tree on 2026-07-29, when the campaign was built.** Four of the ten entries
below were superseded and are struck through with the measurement that superseded them; the rest stand,
and two are now sharper than the spec could state them.

| ID | What is missing | Blocks here |
|---|---|---|
| ~~`C3`~~ | **CLOSED 2026-07-28 and MEASURED here.** The eye identifies an An-26 at **1 086 m**, a Tu-95 at **2 049 m**, and recognises a MiG-29 at **825 m** | what remains is real and is the campaign's own reading: **the eye contributes nothing at night** (0 `vis` lines in both night sorties) because nothing in the tree emits light, and nothing at all through a 100 % mid deck (transmittance 2.9e-25) |
| ~~`C12`~~ | **CLOSED 2026-07-28 and flown here.** `identify` + `no_fire` are the victory condition of all ten sorties and nine of ten return a real verdict | the *sensor* half is still deliberately outside the verdict, and `w5-09` measures what that costs: an eyeless run scores byte-identically to a seeing one |
| `C19` | **no rules-of-engagement state** — nothing expresses "weapons hold", "weapons tight", "cleared to engage" | **sharper than the spec knew.** `w5-07` puts two armed F-16s with the master arm on under a fighter radar and measures `eng_state` = `idle` for all 3 001 rows: a pilot on `route` never enters the engagement machine, so the hold is not *enforced*, it is *absent*. The alternative (`set task intercept`) does not become trigger-happy — it **aborts the task** (finding 2) |
| ~~`C7`~~ | **the capability is here for THIS campaign.** `an26` and `tu95` are MOVER rows and a mover has no generated deck, so the `ALPHA` verdict does not reach them; span, cruise speed and transponder state are all sourced [T4] | what a mover still cannot do: **feel wind** (`FBAirMover` has none), which is what makes `w5-05`'s shadow undoable and `w5-10`'s formation come apart |
| ~~`C2`~~ | **CLOSED 2026-07-28.** Both night sorties declare a `time` and the sun really is down | — |
| `C6` | **no scramble** — a `.fbm` spawns aircraft, it does not alert them | the 15-minute reaction *is* the mission in reality, and it is **not in the cost table**: `w5-05` prices a 40 km run-down and not a sortie. `w5-10` expresses a readiness stagger as 24 km of distance and says so |
| ~~`C1`~~ | **CLOSED 2026-07-28 and re-checked here.** The `p18` radiates, is heard from t = 0.1 as `kind=surface-search`, tracks and can be killed | what the QRA protects is no longer scenery. What is still missing is a reason to attack it (`C22` is closed; no W5 mission uses it) |
| `C14` | **no ships** | the Baltic case is full of them, and this campaign's ground half is three fixed objects on a coast |
| `C18` | **no radio** | a real intercept includes attempted radio contact on guard, and **its failure is part of the identification** — the anchor's own scramble trigger is "does not talk to air traffic control", and that criterion cannot be expressed at all |
| ~~`C0`~~ | **CLOSED 2026-07-28.** The campaign runs, replays and fingerprints | and it carries **nothing**, by construction — see §State's carry paragraph |
| `C15` | **no lead tasking** (added by this round) | `w5-04` and `w5-06` both turn on it. A flight of two has exactly one free variable, the sort, and the sort ranks by time-to-arrive — so it converges on the nearest contact rather than on the one that matters, and it cannot split onto two subjects at all (findings 3) |

### The honest headline — rewritten after building it

The spec's headline said *"W5 is the campaign whose subject FlightBox cannot simulate and whose
discipline it can test perfectly."* **Half of that was already false when it was written and the other
half is now the interesting half.**

FlightBox *can* do the identification, and the number that makes it possible was never about the eye:
it is the **span of the subject**. A transport is named at 1 086 m and a bomber at 2 049 m on the same
pass where a fighter is not recognised at 1 600 m — one resolution law and two published dimensions.
The campaign that looked blocked was blocked on a **cast row**, not on a sensor.

What it still cannot do is the *pilot's* half. There is no scramble, no radio call, no
rules-of-engagement state, no lead tasking — and, measured here, **no pilot behaviour for the task at
all**: the one task in the ten-campaign set whose success condition contains no weapon is the exact
geometry `FBPilot` classifies as an abort. Every pass in this campaign was flown by a route a human
wrote. That is the honest statement of where W5 stands, and it is a *pilot* gap wearing a sensor gap's
clothes.

And the discipline half held perfectly, which was the prediction: **zero rounds expended across ten
sorties**, an anti-cheat pair that is byte-identical in 6 of 6 telemetry files, and a control run that
proves the channel could have spoken.

---

## Knowledge

### 1. The anchor with its sources

- **Mission, dates, bases, rotation and detachment size.**
  [Baltic Air Policing (Wikipedia)](https://en.wikipedia.org/wiki/Baltic_Air_Policing) [T4] — 30 March
  2004 start, Šiauliai/Zokniai as the original base, Ämari from May 2014, three- then four-month
  rotations, "usual deployments consist of four fighter aircraft with between 50 and 100 support
  personnel", the NATO Integrated Air Defence System framing and the "purely defensive" description.
  Institutional framing: [Baltic Air Policing & Air Shielding (milavreachout.org)](https://milavreachout.org/nato-enhanced-air-policing/)
  [T3].
- **The scramble trigger and the task.** [Baltic Air Policing & Air Shielding](https://milavreachout.org/nato-enhanced-air-policing/)
  [T3] — an aircraft that "files no flight plan, squawks no transponder code, or refuses to talk to
  air traffic control" launches the on-call jets on an **Alpha Scramble** "to put eyes on it"; the
  fighters "identify the contact visually, shadow it through allied airspace boundaries, and peel off
  once it is clear. It is policing in the literal sense — presence, identification, escort — not
  combat." Alpha vs Tango scramble and the sub-15-minute reaction:
  [Quick Reaction Alert (Wikipedia)](https://en.wikipedia.org/wiki/Quick_reaction_alert) [T4],
  [Scramble! (Smithsonian Air & Space)](https://www.smithsonianmag.com/air-space-magazine/scramble-180971215/)
  [T3].
- **What gets intercepted.** [Romania's F-16s score their first Baltic intercept — an Il-20M (MiGFlug)](https://migflug.com/jetflights/romania-f16-first-baltic-intercept-il-20m/)
  [T4]; [Swedish fighters intercept a Tu-22M escorted by Su-35S near NATO airspace (Army Recognition)](https://www.armyrecognition.com/news/aerospace-news/2026/swedish-fighter-jets-intercept-russian-tu-22m-bomber-escorted-by-su-35s-fighters-near-nato-airspace)
  [T4] — the two archetypes this campaign uses: the lone ELINT aircraft and the escorted bomber.

### 2. Where the sources are thin, and it is stated

| Thing | Status |
|---|---|
| Published rules of engagement, minimum separation, the abeam geometry a NATO interceptor actually holds | **not sourced and not guessable.** The "abeam box" in §3 is `[SET]` and must be declared as a FlightBox convention in the first mission header |
| Intercept statistics (how many per rotation, how many were non-cooperative) | **not established.** The Wikipedia article's own incident list is anecdotal |
| Whether QRA aircraft are armed, and with what | **not stated in the sources retrieved.** The campaign therefore arms them (a real QRA jet plainly is) and makes *not firing* the discipline being tested |

### 3. Why "identity" is the hardest thing to fake in this engine — and the easiest to test

The tree's identity picture is deliberately impoverished, and every restriction works in this
campaign's favour:

| Restriction | Where it is enforced | What it means here |
|---|---|---|
| A radar contact carries range, bearing, azimuth, elevation, closure and a radar-local track number — **no unit id, no callsign, no team** | `core/FBRadarContact` | the interceptor genuinely does not know who that is |
| IFF Mode 4 is **two-valued** — friendly, or no reply. There is no "hostile" | `core/FBIffReply` | silence is ambiguity, and the campaign's whole subject is what a pilot does with ambiguity |
| The unit registry reaches **six files** in `systems/`+`modules/`, all of them sensors | grep-checked, [`../sensors.md`](../sensors.md) §1.2 | the pilot has no path to the truth even if it wanted one |
| The cooperative datalink is faction-filtered | `sensors/FBDatalinkSystem` | a friendly picture cannot identify a non-friendly |

So the **test** writes itself: change the intruder's `team` and require the interceptor's own
telemetry not to move. If the boundary holds, the two runs are byte-identical until a sensor
discriminates; if it does not, the diff names the tick and the column where the leak is. That is a
stronger statement than any amount of prose about anti-cheat, and it is why this campaign exists in
a set otherwise made of shooting wars.
