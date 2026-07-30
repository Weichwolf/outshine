# Campaigns — coverage ledger

What was researched, what was written, and what was identified but **not read**. Updated every run.

**Run 1 — 2026-07-28.** Directory created; all ten campaign specs plus [`INDEX.md`](INDEX.md)
written. Nothing under `sim/` touched. One line added to `../INDEX.md`.

**Run 2 — 2026-07-28, the foundation round (step 1 of the owner goal).** Spec only, no code, nothing
under `sim/` touched. Four contracts written into the files that own them, and the fifth gap given a
home:

| Gap | Home file | What landed |
|---|---|---|
| `C2` | [`../missions/syntax.md`](../missions/syntax.md) + [`../clients/clients.md`](../clients/clients.md) | the `time` line: Zulu-only ISO-8601, `HaveTime=false` as the default, per-client precedence with the flag collision as a boot error, the consumer list, the `FBEphemeris` layer move, and what it explicitly does not do |
| `C3` | [`../sensors.md`](../sensors.md) §9 + [`../missions/sensors.md`](../missions/sensors.md) | `FBVisualSystem` as the sixth registry reader with its five-currency price; angular-size/aspect/contrast model with public sources; sun, glare (Stiles–Holladay) and a cloud **transmittance march**; Johnson-N50 recognition; the eight things not modelled; the `set` keys and columns |
| `C12` | [`../missions/verdict.md`](../missions/verdict.md) + [`../missions/syntax.md`](../missions/syntax.md) | `identify` / `protect` / `no_fire` / `deny release`, their roster cost, the rejected `identify` design, four candidates refused with reasons, and the five-part conservation argument |
| `C0` | [`../missions/campaign.md`](../missions/campaign.md) **(new file)** | `.fbc`, the three carried facts and the test that rejects the rest, the overlay rule, the campaign fingerprint and the standalone-replay criterion |
| `C1` | [`../weapons.md`](../weapons.md) Gaps | **gap entry only, with an explicit boundary** — five open questions named and deliberately left to step 2 |

Also updated: the gap table in [`INDEX.md`](INDEX.md) now links every one of the five to its home, and a
"foundation round" subsection records the four decisions. One line added to `../INDEX.md`,
`../missions/INDEX.md` and the loader skill.

**Run 3 — 2026-07-29, O4 BUILT (step 4 of the owner goal).** The first campaign to exist as files:
ten `sim/missions/o4-*.fbm` + `sim/campaigns/o4-gaf-mig29g-dact.fbc`, run, replayed and measured
([`o4-gaf-mig29g-dact.md`](o4-gaf-mig29g-dact.md) §State). Two `sim/src` files and one tool changed —
the campaign layer could not hand a campaign's own `time` to a standalone step replay, so criterion 2
was unsatisfiable for any clocked campaign and O4 was the first to declare one. No source was researched
on this run; the anchor is Run 1's and unchanged, and the forum test-report thread below is still unread.

| Measured | Value |
|---|---|
| Campaign exit / step exits | 3 / `0 3 3 0 1 0 3 1 0 3` |
| Determinism criterion 1 | 9 runs (3 × `--threads 1/2/4`), **1** campaign fingerprint `461e0ff5299d83d03b…`, `--elev const` |
| Determinism criterion 2 | **10/10** steps replay standalone bit for bit (9/10 DIVERGED before the clock fix) |
| Conservation | 515/515 `telemetry*.csv` and 150/150 `events.log` byte-identical against a reverted binary; 0 diffs over `--threads 1/2/4` |
| The campaign's own answer | the ten-mile claim: **MiG wins at 10 nm, trade at 5, F-16 at 2** |

---

## Campaign coverage

| Campaign | File | Anchor researched | Missions | Cast | Gaps | Status |
|---|---|---|---:|---|---|---|
| W1 Red Flag | [w1-red-flag.md](w1-red-flag.md) | yes ([T3]/[T4]; primary fact sheet **403**) | 10 | yes | yes | **complete, one source blocked** |
| W2 Osirak | [w2-osirak.md](w2-osirak.md) | yes ([T4] primary + 3 cross-checks, 1 [DISPUTED] value) | 10 | yes | yes | **complete** |
| W3 Desert Storm | [w3-desert-storm.md](w3-desert-storm.md) | yes ([T4] primary; one [T3] PDF unread) | 10 | yes | yes | **BUILT AND FLOWN** — ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, the replay run after the FIRST mission, four findings. One [T3] source still unread |
| W4 Allied Force | [w4-allied-force.md](w4-allied-force.md) | yes ([T1] + [T3] + [T4]) | 10 | yes | yes | **BUILT AND FLOWN** — ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, the replay run after the FIRST mission, three findings |
| W5 Baltic QRA | [w5-baltic-qra.md](w5-baltic-qra.md) | yes ([T3]/[T4]) | 10 | yes | yes | **BUILT AND FLOWN** — ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, the replay run after the FIRST mission, four findings |
| O1 Bekaa 1982 | [o1-bekaa-1982.md](o1-bekaa-1982.md) | yes ([T4] primary; disputes carried) | 10 | yes | yes | **BUILT AND FLOWN** — ten `.fbm` + one `.fbc`, both determinism criteria measured on the first attempt |
| O2 PVO intercept | [o2-pvo-intercept.md](o2-pvo-intercept.md) | **yes — doctrine [T1] since run 7** (both CIA reading-room documents read), hardware [T2] via `doc/modules/mig29/` | 10 | yes | yes | **BUILT AND FLOWN** — ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, three findings |
| O3 Yom Kippur | [o3-yom-kippur-1973.md](o3-yom-kippur-1973.md) | yes ([T1] Marine Corps study + [T4]) | 10 | yes | yes | **BUILT AND FLOWN 2026-07-30** — ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, the replay run after the FIRST mission, three findings. **The only campaign that went from ZERO runnable to ten**; attack profiles in metres and ordnance per aircraft are still unsourced and still `[SET]` |
| O4 GAF DACT | [o4-gaf-mig29g-dact.md](o4-gaf-mig29g-dact.md) | yes ([T3]/[T4]; one forum test-report thread unread) | 10 | yes | yes | **BUILT AND FLOWN** — ten `.fbm` + one `.fbc`, both determinism criteria measured |
| O5 Airfield defence | [o5-airfield-defence.md](o5-airfield-defence.md) | yes ([T4]; totals [DISPUTED] and deliberately omitted) | 10 | yes | yes | **BUILT AND FLOWN** — ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, three defects found |
| — | [INDEX.md](INDEX.md) | — | — | aggregated | aggregated | **complete**: map, reading rules, cast by frequency, gaps by blocking degree, the identification section, the Bekaa yardstick |

**100 missions specified. 80 BUILT (O4, O1, O5, O2, W5, W3, W4, O3). 50 were counted runnable when the
specs were written; all eight built campaigns came out at 10 of 10 when re-checked against the tree** —
including O3, whose spec counted **zero** (per-campaign breakdown in [`INDEX.md`](INDEX.md)).
**Only W1 and W2 remain spec-only.**

**Run 4 — 2026-07-29, O1 BUILT.** The second campaign to exist as files: ten `sim/missions/o1-*.fbm` +
`sim/campaigns/o1-bekaa-1982.fbc` ([`o1-bekaa-1982.md`](o1-bekaa-1982.md) §State). **No `sim/src/` file,
no tool and no asset was touched** — the whole campaign is mission text, which is what its own spec
predicted ("their subject is doctrine and doctrine is mission text"). No new source was researched; the
anchor is Run 1's and unchanged.

| Measured | Value |
|---|---|
| Campaign exit / step exits | 3 / `3 3 3 3 3 3 3 3 3 3` — ten measuring rigs, and each header says the verdict is its telemetry |
| Determinism criterion 1 | 9 runs (3 × `--threads 1/2/4`), **1** campaign fingerprint `81b549fd04c4591987b9…`, `--elev const` |
| Determinism criterion 2 | **10/10** steps replay standalone bit for bit, on the first attempt |
| Conservation | nothing to compare: `git status --porcelain` lists 11 untracked files and 0 modified |
| The campaign's own answer | the baseline reproduces the rout (2 Red mission-killed, 0 Blue); **one lever inverts it** (`pilot_shot_rtr 1.4` → 0 Red, 2 Blue, on 3.5 s of tempo); the GCI is worth the entire engagement on a 45° entry and **nothing** head-on; the RWR, the belt, the net, the jamming and the carry move mechanisms and **no outcome** |
| Found while building | the 2K12's 3M9 and the S-125's V-601 reach the ground **at their own launcher** 0.8–1.6 s after release, and the 3M9 kills its own battery — **pre-existing, visible in the committed `net-cue.fbm`**, and the reason four closed gaps cannot decide anything |

**Run 5 — 2026-07-29, the ground-launch fix documented (a `sim/src/` round, not a campaign round).**
Run 4's largest finding is closed: three defects, all on FlightBox's side of the seam, **no deck
touched**. Seven files under `sim/src/`; the build account lives in
[`../modules/ground/module.md`](../modules/ground/module.md) §4.1 and
[`../weapons.md`](../weapons.md) §1 / §10.2.

| Measured | Value |
|---|---|
| Conservation | **10 of 160** missions changed, **150 byte-identical**. All ten have a ground launch: `net-belt-high`, `net-cue`, `net-jam-wire`, `o1-08-belt-netted`, `o1-10-mole-cricket`, `sam-beam-notch`, `sam-emcon-hold`, `sam-manpads-day`, `sam-sa2-command`, `sam-sa6-engage`. Exit codes moved twice: `o1-08` **3 → 2**, `sam-sa2-command` **0 → 1** |
| Determinism | `--threads 1/2/4` on four moving missions: one hash each. **Both campaigns still pass both criteria** — O1: 9 runs one fingerprint, 10/10 steps replayed; O4: 10/10 |
| Gates | `verify-layers` 297 files / 6 registry readers, `verify-models` 1 declared delta / 34 FlightBox-own, warning-free build |
| What it does to O1 | `o1-08` unjammed: **8** launches, **7** detonations, `bolt1` shot down, **all five ground positions intact**. `o1-09` jammed: **0** launches, **0** detonations, both attackers meet their objectives, **two positions destroyed**. The pre-fix pair's *"ground damage identical to the metre and the tick"* is **superseded** — [`o1-bekaa-1982.md`](o1-bekaa-1982.md) §"The ground half, re-measured" |
| Left open | the post-fix O1 campaign fingerprint and the two moved per-mission fingerprints are **not written down** (TODO in that file); the carry-versus-standalone delta of step 10 is not re-measured |
| Newly visible | three defects the ground contact was hiding, none of them the fixed one: a caged MANPADS seeker (`module.md` B4), a V-750 that cannot fly its own pitch-over (B5), one missile gain set shared across three orders of magnitude of mass (B6) |

---

## Sources identified but NOT read

Each of these would raise a claim's tier. None was reachable or retrieved on this pass.

| Source | Would fix | Why not read |
|---|---|---|
| [414th CTS "Red Flag" fact sheet (nellis.af.mil)](https://www.nellis.af.mil/About/Fact-Sheets/Display/Article/2605882/414th-combat-training-squadron-red-flag/) | W1's exercise cadence, package sizes, participant counts, the "first ten missions" rationale at [T1] | **HTTP 403** to automated retrieval |
| [Package Q (Air Force Magazine, Jan 2016, PDF)](https://www.airandspaceforces.com/PDF/MagazineArchive/Documents/2016/January%202016/0116packageq.pdf) | W3's package composition, tanker and SEAD detail at [T3] instead of [T4] | not retrieved this pass |
| ~~[CIA reading room 1979-02-16A](https://cia.gov/readingroom/docs/1979-02-16A.pdf) and [1976-10-12a](https://www.cia.gov/readingroom/docs/1976-10-12a.pdf)~~ | ~~O2's entire doctrine half at [T1]~~ | **READ 2026-07-29 (run 7).** The `cia.gov/readingroom/docs/…` path is Akamai-blocked to automated retrieval (302 → *Access Denied*); the **Wayback Machine's captures of the identical URLs are not**, and both came back complete (11 and 13 pages). Content, citations and what each contributes: [o2-pvo-intercept.md](o2-pvo-intercept.md) §1 and §Knowledge 1. **The blocked path was not the only path, and that is the transferable part** |
| [Ground controlled interception & IADS (archive.org PDF)](https://archive.org/download/history-of-the-electro-optical-guided-missiles/IADS,%20GCI.pdf) | O2's GCI mechanism | not retrieved this pass |
| [F-16 vs MiG-29 energy manoeuvrability from a test report (f-16.net forum)](https://www.f-16.net/forum/viewtopic.php?f=30&t=53852) | O4's missions 4/5 expectations at [T3] | not retrieved this pass |
| [Official review of Serb MiG-29 kills, 26 March 1999 (Google Groups)](https://groups.google.com/g/rec.aviation.military/c/JpXMVIrCenY) | O5's loss/claim record | not retrieved this pass |
| **GAF T.O. 1F-MIG29-1** | three [T4] number sets in `doc/modules/mig29/` (SPO-15 behaviour, cockpit geometry, engine/fuel), and O4's helmet-sight and seeker envelopes | **not available** — already recorded as an acquisition note in `doc/modules/mig29/` (three files) and now also in [o4-gaf-mig29g-dact.md](o4-gaf-mig29g-dact.md) §Knowledge 3 |

## Values deliberately left contested

| Where | Values | Treatment |
|---|---|---|
| W2 — Osirak ingress altitude | **30 m** [T4] vs **240 m** [T4] | both carried, neither preferred. A factor of eight, and it decides whether `w2-02` is a terrain-following problem |
| W2 — pop-up geometry | 2,100 m at 20 km [T4] vs "ground level to just under 10,000 ft at ~10 miles" [T4] | both recorded; not averaged |
| O1 — Israeli losses at Bekaa | none in air-to-air, 2 F-15 damaged [T4] vs the Soviet claim of **67** [T4] | both stated, the second with its own source's judgement ("widely dismissed"). Not discounted, not deleted |
| O1 — Syrian losses | **82–86** | range kept; no single figure used anywhere |
| O1 — SAM batteries destroyed on the first strike | 29 of 30 (+6 later) [T4]; a 17–19 figure appears elsewhere in the literature | the second range was **not confirmed** and is therefore not carried as a number |
| O5 — Yugoslav MiG-29 losses and claims | contested across the literature | **no total is stated in the file at all**, and the campaign uses none |

## Values that are `[SET]`, not sourced

Named here so that nobody later mistakes one for a fact:

- All package sizes in the ten mission tables — chosen as a ladder that isolates one variable per
  step, not reproduced from any air tasking order.
- W5's "abeam box" geometry (the range/aspect an interceptor must reach and hold).
- W4's mission-8 weather severity, and mission 4/5's emitter timings.
- O2's ten-mission structure in full — no PVO exercise syllabus was sourced, and the [T1] round did not
  change that: Mitronin names DRUZHBA-76 and says what was learnt at it, never how it was run or scored.
- O3's every altitude and speed ("very low" is all the strongest source says).
- O5's degradation pattern across missions 3, 8 and 10.

---

**Run 3 — 2026-07-28, `C2` built.** The first code round of the foundation. The `time` line parses, the
clock binds all three clients, the flag collision is a boot error, `FBEphemeris` moved from `render/` to
`core/`, and `fb-gym` writes `FBEnvironmentBlock` for the first time. All 84 pre-round missions
byte-identical; reference mission `sim/missions/clock-night-payerne.fbm`, refusal fixtures in
`sim/missions/negative/`. Details in [`../missions/syntax.md`](../missions/syntax.md),
[`../clients/clients.md`](../clients/clients.md) and [`../journal.md`](../journal.md).

**Run 4 — 2026-07-28, the connected air defence (spec only).** Nothing under `sim/` touched, and
nothing under `doc/modules/ground/` either. New file
[`../air-defence-network.md`](../air-defence-network.md), derived from the **five campaigns whose
subject is the net and not the position** (O1, W3, W4, O5, O3) rather than from a textbook: each of its
five capabilities names the mission question that is unanswerable without it.

| Booked | What it is |
|---|---|
| `C22` | the net itself — cue, sector, weapons control, the node that can be killed or jammed |
| `C23` | the belt as declared geometry (`zone`) and as a verdict (`objective avoid zone`, per-unit dwell) |
| `C24` | communications jamming — the bounded subset of `C13`, which **splits**: the radar half stays wholly open |

Also updated: the gap table and the cast table in [`INDEX.md`](INDEX.md); the Gaps tables of the five
source campaigns; `C0…C21` → `C0…C24` in all ten campaign headers; two lines in `../INDEX.md` and two
routing rows in the loader skill.

**Sources: none new.** No document about a real air-defence network was read on this pass. The doctrine
vocabulary is common terminology and the timings are `[SET]`; the file says so in its own §Knowledge 5
and names the same unread [T1] material `../modules/ground/catalogue.md` does, plus the two CIA
reading-room documents already listed below.

## What the next run should do

Nothing in this directory needs re-writing. The next work is **outside** it:

1. ~~**Build the remaining three foundation contracts**, in the order their dependencies allow: `C2`
   → `C12` → `C3` → `C0`.~~ — **all four built 2026-07-28.** `C0` closed last, with
   `sim/campaigns/viper-attrition.fbc` as its first campaign; the ten campaigns of this directory now
   lack their `.fbm` files and nothing else.
2. Build the four one-line experiments named in [`INDEX.md`](INDEX.md) — `o1-01/02`, `w3-07/08`,
   `w4-01/02`, `w5-02/03`. They need nothing that does not exist, and three of them answer questions
   the tree has been circling for rounds.
3. ~~`C1` — step 2 of the owner goal.~~ **Specified 2026-07-28** in
   [`../modules/ground/`](../modules/ground/INDEX.md); building it comes before anything in `C22`,
   because the net needs at least two positions and one early-warning set before its first acceptance
   run means anything.
4. `C22`/`C23`/`C24` — build order inside the net file: the **link** first (it is three setters and one
   test on a class that exists), then the **cue**, then `zone`/`avoid zone`, then weapons control and
   autonomy, and the **jammer last** because it is worthless until there is a link to deny.
5. ~~Read the two CIA reading-room documents and re-tier O2's §Knowledge 1~~ — **done 2026-07-29 in
   run 7.** They are still the highest-value source for `C22`'s doctrine half and that file has not
   consumed them yet: Mitronin's identification argument is the [T1] anchor for *why a FlightBox
   battery without an interrogator is faithful rather than simplified*, and O5's measured
   fratricide is its illustration.

~~3. Open a gap entry for `C1`…~~ — **done in run 2**, in [`../weapons.md`](../weapons.md).

**Run 6 — 2026-07-29, O5 BUILT.** The third campaign to exist as files: ten `sim/missions/o5-*.fbm` +
`sim/campaigns/o5-airfield-defence.fbc` ([`o5-airfield-defence.md`](o5-airfield-defence.md) §State).
**No `sim/src/` file, no tool and no asset was touched.** No new source was researched; the anchor is
Run 1's and unchanged, and the claim-review thread flagged there is still unread. It is the first
campaign to fly the `C12` vocabulary in anger and the first whose subject is the ground defence the
Run-5 fix made able to move an outcome.

| Measured | Value |
|---|---|
| Campaign exit / step exits | 3 / `3 3 3 3 3 1 1 3 3 3` |
| Determinism criterion 1 | 9 runs (3 × `--threads 1/2/4`), **1** campaign fingerprint `f59fc642c86ccecd2691…`, `--elev const` |
| Determinism criterion 2 | **10/10** steps replay standalone bit for bit, on the first attempt |
| Conservation | nothing to compare: `git status --porcelain` lists 11 new untracked files and 0 modified; 160 pre-existing missions byte-identical by construction |
| The campaign's own answer | **a fighter pair already on station denies half a two-ship and nothing else in this tree comes close.** The controller is worth 6 s of one wingman's first look; the night, the inner gun, the hardened shelters and the runway are worth nothing measurable. **The belt denies one striker of three — and one Mk-84 on its P-18 node on night one costs it every launch on nights two and three** (7 → 0 and 6 → 0, measured standalone against in-campaign) |
| Found while building | **three defects, all on FlightBox's side of the seam, none fixed here.** (1) The alert scramble is not expressible: `FBPilot` has no `Route` → `Intercept` transition and `set task` applies at spawn, so a ground start plus a combat task FAILs at t = 11.1 s or cartwheels at t = 35.4 s. (2) `modules/air/FBAirModule` composes **no fire control**, so no catalogue row can employ any weapon — an `f15c` holds a firm lock for 28 s from 18.6 to 8.8 nm and never presses. (3) `FBMig29Pilot` posts the GCI's **world-frame** scan elevation into a **body-frame** antenna command; on a climbing interceptor that is worth the whole ±6° RAD bar — 0 contacts in 700 s over a 726 m closest approach |
| Also measured | the field's own belt fires its **first three rounds at its own fighters** (`brgDeg` 116.5 / 90.6 / 91.0) — a FlightBox battery has no IFF interrogator, and an airfield is the one geometry where that cannot be laid around |

---

**Run 7 — 2026-07-29, O2 BUILT, and the first run since run 1 to read a new source.** The fourth
campaign to exist as files: ten `sim/missions/o2-*.fbm` + `sim/campaigns/o2-pvo-intercept.fbc`
([`o2-pvo-intercept.md`](o2-pvo-intercept.md) §State). **No `sim/src/` file, no tool and no asset was
touched.** The two CIA reading-room documents this ledger has carried as *"the highest-value unread
source in the directory"* since run 1 were retrieved and read, and O2's doctrine half moved from
[T3]/[T4] to [T1].

| Measured | Value |
|---|---|
| Campaign exit / step exits | 3 / `3 0 3 3 3 0 0 0 3 3` |
| Determinism criterion 1 | 9 runs (3 × `--threads 1/2/4`), **1** campaign fingerprint `93b5869298b6b8a5924…`, `--elev const` |
| Determinism criterion 2 | **10/10** steps replay standalone bit for bit, on the first attempt |
| Conservation | nothing to compare: `git status --porcelain` lists 11 new untracked files and 0 modified; 173 pre-existing missions byte-identical by construction |
| The campaign's own answer | **the loop is 11.0 s and it splits 8.0 + 3.0** — and **the controller is worth the ENTIRE intercept on an aircraft that starts silent**, which is why three earlier campaigns measured him at nothing: they all flew with `n019_emission illum` at spawn and were measuring the AIMING of a radar, not its existence. Wrong azimuth third → 0 contacts in 400 s; controller deleted → 0 emissions and 0 contacts; late commit → 45.3 s of the target's silence bought for 77 % of the detection range |
| The identification counter-check | **`o2-06` vs `o2-08`, one token apart: 5 of 5 `telemetry*.csv` byte-identical, 1 differing `events.log` line of 53** (the runner's own `team=` field). Four perception channels live for 300 s and none moved. `o2-07` is the control run that proves the channel could have discriminated — and needing it is the new rule |
| Sources | **two, both [T1], both previously listed here as unread**: Mitronin (Warsaw Pact journal 12/1976, CIA translation 16 Feb 1979) and Pstygo/Ganichev/Reshetnikov (*Military Thought* 5(66)/1962, CIA translation 12 Oct 1976). Four of their facts changed a mission or a header; the sharpest is that the anchor's **ground** picture took **10–15 minutes** (DRUZHBA-76) against FlightBox's zero, which is `C6` with a number attached |
| Found while building | **three findings, none fixed here.** (1) A wrong brief has **no deliberate recovery** — what looks like one is `FBPilot`'s 2.0° elevation dead band drifting, and it saved one aircraft of two, 28 s late. (2) `D3` priced as a byte diff: powering the KOLS across five sorties changed **4 of 184 telemetry columns and nothing else**, while that sensor held a 90 s contact nobody read. (3) An R-27R **inside its own fuze is not a kill** — 4.85 m and 4.75 m in the FORWARD zone left the target combat-effective, 2.48 m elsewhere killed, so an outcome axis ordered by kills is reading warhead geometry |
| Also measured | the GCI world/body elevation defect on the **other** side of its threshold: the error is exactly `st.pitch`, the climb pitch band is **5.36…5.89°**, the RAD bar is ±6.0°, so O2's margin is **0.11–0.64°** and the bite condition is `|pitch| + |target body-frame offset| > 6.0°` |


---

**Run 8 — 2026-07-29, W5 BUILT: the first campaign in which the F-16 flies, and the only one whose
success condition contains no weapon.** The fifth campaign to exist as files: ten `sim/missions/w5-*.fbm`
plus `sim/campaigns/w5-baltic-qra.fbc` ([`w5-baltic-qra.md`](w5-baltic-qra.md) §State). **No `sim/src/`
file, no tool and no asset was touched.** No new source was researched; the anchor is Run 1's and
unchanged. It is the first campaign whose sorties mostly return a REAL verdict rather than a measuring
rig's TIMEOUT, because `identify` + `no_fire` (round `C12`) were built for exactly this task.

| Measured | Value |
|---|---|
| Campaign exit / step exits | 3 / `0 0 0 0 0 0 0 0 0 3` — nine passes and one capstone that identified 2 of 3 contacts |
| Determinism criterion 1 | 9 runs (3 × `--threads 1/2/4`), **1** campaign fingerprint `49d3320f5e9761db2f1df85a12d9008e0d8559395c141c31e2e06903b9fe0200`, `--elev const` |
| Determinism criterion 2 | **10/10** steps replay standalone bit for bit, on the first attempt — **and the replay was run after the FIRST mission**, on a throwaway one-step `.fbc`, which two previous rounds confessed skipping |
| Conservation | nothing to compare: `git status --porcelain` lists 11 new untracked files and 0 modified; 183 pre-existing missions byte-identical by construction. Annotating the ten files with their MEASURED blocks left all ten per-mission fingerprints and the campaign fingerprint unchanged |
| The campaign's own answer | **the task was blocked on a CAST ROW, not on a sensor.** The eye names an An-26 at **1 086 m** and a Tu-95 at **2 049 m** where a MiG-29 is not recognised at 1 600 — one resolution law and two published spans. What an identification costs (40 km run-down, 900 s): **412.9 s and 243.5 lb of fuel** to the visual identification, **494.3 m** of separation, and **zero risk**, because a stern conversion is flown into the one place a forward-looking radar does not point (the subject's beam first reaches the interceptor 30 s AFTER the pass) |
| The three-run counter-check | **`w5-02` vs `w5-03`, one token apart: 6 of 6 `telemetry*.csv` byte-identical, 1 differing `events.log` line of 75.** The control run `w5-01` (the subject ANSWERS) moves **5 of 184 telemetry columns and zero metres** — and that is a DISAGREEMENT with O2, where the same experiment moved zero columns. Rule 11 resolves it: the F-16 flies in a flight and `FBFlightPicture` sorts on the tracks' IFF field, so the quantity is *"what an identity is worth to a FLIGHT"* |
| Found while building | **four findings, none fixed here.** (1) On the SPAWN tick every RWR reports the emitter's **TRUE** bearing instead of the relative one, because the attitude the body transform reads is not published yet — isolated at **180° against −95.5° on the identical geometry, an error of 275.5°**, held 2.0 s; pre-existing and visible in the committed `pair-2v2-f16.fbm`. (2) `FBPilot` has no behaviour for this task: `FBPilot.cpp:1040` calls *inside 5.0 nm and never shot* an ABORT, so `set task intercept` turns the interceptor away at t = 5.1 s and every W5 pass is an authored route. (3) A flight cannot sort two targets that only one member each can see — `FBFlightPicture::Assign` matches all members against the computing aircraft's own contact list, so both jets report `dup=1` on two aeroplanes 17.8 km apart. (4) The guidance does not crab: one 176 km leg bows **3.4 km** downwind and misses a 2 km box by 4 038 m, where the same aeroplane on 34 km vectors identifies at 677.7 m |
| Also measured | the accepted price of judging the GEOMETRY instead of the sensor, paid in public: the night sortie has **0** `vis` lines against 9, differs in **6 of 184** telemetry columns, and produces `mission IDENTIFIED` at the **identical tick, range and dwell**. And a fifth thing that is an asymmetry rather than a defect: `modules/air/FBAirMover` has no wind at all, so in wind a declared formation comes apart and a briefed co-speed leg is co-speed in the wrong frame |


---

**Run 9 — 2026-07-29, W3 BUILT: the first campaign whose opponent is a system, and the first built
entirely out of capabilities that did not exist when its own spec was written.** The sixth campaign to
exist as files: ten `sim/missions/w3-*.fbm` plus `sim/campaigns/w3-desert-storm.fbc`
([`w3-desert-storm.md`](w3-desert-storm.md) §State). **No `sim/src/` file, no tool and no asset was
touched** — `git status --porcelain` lists eleven new untracked files and no modified one, so the 195
pre-existing missions are byte-identical by construction. No new source was researched; the anchor is
Run 1's and unchanged, and the Air Force Magazine PDF listed below is still unread.

| Measured | Value |
|---|---|
| Campaign exit / step exits | 3 / `0 3 3 3 3 3 3 3 3 3`; whole campaign 53.2 s of wall clock |
| Determinism criterion 1 | 9 runs (3 × `--threads 1/2/4`), **1** campaign fingerprint `3490c4fab3f25f533ead565e393cc23d234067e827e5ea7ba733408988f1fa1a`, `--elev const` |
| Determinism criterion 2 | **10/10** steps replay standalone bit for bit, on the first attempt — **and the replay was run after the FIRST mission**, on a throwaway one-step `.fbc` that was deleted afterwards |
| Conservation | nothing to compare: 11 new untracked files, 0 modified. Annotating the ten files with their MEASURED blocks afterwards left all ten per-mission fingerprints and the campaign fingerprint unchanged |
| The spec's own headline, inverted | it said *"of the three things that went wrong on Package Q, FlightBox can measure ZERO today"*. Re-checked blocker by blocker against the tree (`C1` `C8` `C22` `C23` `C24` `C26` `C27` `C2` `C0` all closed since): **10 of 10 missions ran**, and of the three failure modes FlightBox can stage **exactly one** — the campaign names which and why |
| The campaign's own answer | **a suppression element is worth one striker's release and the target, and it is worth that even when its missile falls 10 km short** (four runs on one geometry, the fourth being the attribution run with no SEAD at all); **emission discipline is worth the position and nothing else** — dark at 57.4 % of the round's flight, the AGM-88 lands 214 m short, the crew returns and empties its magazine at the departing Weasels; **the same bomb on the same early-warning radar is worth the whole strike against a one-node net and 36 % of the cue traffic against a two-node one**; and the capstone runs 24 aircraft + 8 ground + 34 stores in 11.7 s with 8 of 8 strikers releasing and 15 of 16 recovering |
| Rule 11, settled on both sides | `w3-07`/`w3-08` under the declared policy `n019_emission off`: **50 Red contacts and 5 launch solutions against 0 and 0**. Attribution run A3, the same file with `illum`: **50 contacts, 7 solutions, run ends at the identical t = 272.8 s** as the briefed control. The five-theatre disagreement is closed by measurement rather than by inheritance |
| Found while building | **four, none fixed here.** (1) **`FBPilot::CanPressOn` is unreachable** — the only line that reads the BINGO warning sits behind `EngState_ == Defend && elapsed >= DefendHoldS`, a branch the general `else if (EngState_ != Abort)` preempts on the first tick after `defendDue` clears. Measured: the bit set for 5 200 of 5 200 rows and a byte-identical `eng_state` column against a run without the brief. (2) **A proximity fuze has no team test** — at 24 aircraft `qamia1`'s R-27R detonated 11.74 m from a MiG-29 of the other Red flight and killed it; 1 of the capstone's 3 losses is friendly fire. (3) **`C15` priced**: three of a four-ship's AGM-88 went into one battery, two of them after it was destroyed. (4) **A battery has no threat priority**: measured four times — all four V-601 west at departing Weasels, all six V-750 south at the SEAD flight while eight strikers ran in, and deleting one striker that never dropped anything changed 10 launches into 8 and its wingman from *survived* into *shot down* |
| Also measured | **a striker is stopped by system damage more often than by destruction.** `w3-02`'s second wave takes four V-601 inside 8.4 m, survives all four, and arrives over its target 56 s later with eleven systems failed including `stores`. Read as a loss table: 0–0. Read as a package result: total failure |

---

**Run 10 — 2026-07-29, W4 BUILT: the campaign whose subject is FINDING, and the first in which two
anchor facts turned out to be mutually exclusive.** The seventh campaign to exist as files: ten
`sim/missions/w4-*.fbm` plus `sim/campaigns/w4-allied-force.fbc`
([`w4-allied-force.md`](w4-allied-force.md) §State). **No `sim/src/` file, no tool and no asset was
touched** — `git status --porcelain` lists eleven new untracked files and no modified one, so the 205
pre-existing missions are byte-identical by construction. No new source was researched; the anchor is
Run 1's and unchanged.

| Measured | Value |
|---|---|
| Campaign exit / step exits | 3 / `0 3 3 3 3 3 3 3 3 3`; whole campaign 38.3 s of wall clock |
| Determinism criterion 1 | 9 runs (3 × `--threads 1/2/4`), **1** campaign fingerprint `6185addc27ec3ef896cd1aed4750d7a6bdf8555f9a3a1e2c6b12971533b8d80a`, `--elev const` |
| Determinism criterion 2 | **10/10** steps replay standalone bit for bit, on the first attempt — **and the replay was run after the FIRST mission**, on a throwaway one-step `.fbc` that was deleted afterwards |
| Conservation | nothing to compare: 11 new untracked files, 0 modified. Annotating the ten files with their MEASURED blocks afterwards left all ten per-mission fingerprints and the campaign fingerprint unchanged |
| The spec's own count, inverted | it called **four** of ten runnable and missions 4/5/7/10 blocked outright on `C1`. Re-checked blocker by blocker: `C1` `C8` `C22` `C23` `C2` `C0` all closed since, so **10 of 10 ran**. Two SPEC missions were dropped rather than blocked — the mountain valley (`C4` + `--elev const`: there is no valley) and the weather abort (`FBPilot` has no branch that declines to release, the same shape W3 measured for BINGO) — both with their reasons in the `.fbc` header |
| The campaign's own answer | **NATO's 15 000 ft floor sits above the ceiling of its own SEAD weapon.** A nine-point ladder (one `p18`, one AGM-88, 20.0 km, launch altitude the only variable): kills at 3 000–4 150 m (0.009–4.47 m), **74.8 m at 4 200 m**, **2 484 m at 4 572 m** — and the last FRESH look of every failing shot is at exactly **15.00°**, the P-18's published `SearchElCenterDeg 5 + SearchElHalfDeg 10`. 15 000 ft = 4 572 m, so every W4 Weasel flies at 3 000 m and says so in its header |
| Emission discipline, at DOCTRINE scale | one geometry, one lever, three ways. Node radiating: dead in 66.5 s. Everything on `emcon hold`: `site RADIATE`/`TRACK`/`LAUNCH`/`net CUE` **3/2/4/4 → 0/0/0/0**, positions lost 1 → 0. Node + three `p18` decoys: both AGM-88 die on a decoy (0.019 m, 4.75 m), the node lives, `net CUE` **26 against 4** and the belt fires **7 launches against 4**. **And none of the three moves the strike** — same two strikers, same ticks, same `aimErrM`. So the defender who will not radiate keeps its positions and gives up the whole engagement, and the decoy buys the same survival while keeping the fight |
| The radar decoy, against the tree | **it works and it costs nothing.** `cast.md` costed it at *"the `p18` row with `rounds 0` and a small range gate"*; a `p18` already has `Channels 0`/`RoundsDefault 0`, a search-range key does not exist, and neither is needed — what makes it work is that it is the SAME row as the node (`1 − (r/2R)²`, so the loudest is the nearest). Two limits are measured: the `arm_class` sort is binary (`fire_control` ignores decoys AND the node alike), and nothing inside `alt/tan(15°)` is audible, i.e. **17.1 km from the floor** — a first cut with the belt at 13.5 km changed not one byte |
| The weather, measured and priced | **the wind is the campaign; the deck is scenery.** Six-point ladder on one file: **5.014 m of along-track bomb miss per knot** at 4 572 m; the fixture's own 46 kt costs 216 m and every `wx fixture` strike in the campaign misses; 20 kt of crosswind turns **3 of 6** kills into **0 of 6**. The cloud is measured exactly ONCE, on the eye: `vis MASKED … transmittance=8.00571e-13` through a 72.6 % mid deck. `irst_masked` = 0 in all ten files, and a strike file logs 0 `vis` lines at all |
| The carry | one callsign, `kosnod`, 08 → 10. Worth **20 of 80 `net CUE` messages (−25 %)** and nothing else: identical launches, identical deliveries, identical losses, **21 of 41 telemetry files byte-identical** and the other 20 differing in 2–6 of 184–202 columns, all seven of them RWR or datalink bookkeeping. **No trajectory column moves.** W3 measured 36 % on its topology; O5 measured "two nights" on a ONE-node net |
| Found while building | **three, none fixed here.** (1) **A semi-active battery that starts a RAILS reload orphans every round in the air** — a 2K12's third launch puts it in `RELOAD`, the illuminator stops, and all three 3M9 log `ILLUMINATION_LOST` 0.2 s later with round 2 **1 776.6 m out after 27.1 s of flight**; attributed with a `set rounds 4` control run that loses it at the identical tick, so it is the RAILS and not the magazine, and the effective magazine against one target is 2. (2) **`objective suppress … emitting <s>` reads MET with `emittingS=0`** — it cannot tell "we held it down" from "it was never up", which is W5's rule 15 in the objective vocabulary. (3) **An anti-radiation round's "first admissible symbol" latch has no memory of what it was launched at**: against a dispersed belt it re-latched **six times in 11 s** across four symbols, the last 36.7° off the nose, and came down 2.8–3.3 km from anything |
| Also measured | `--elev tiles` over the real Kosovo (ground **547.88 m** against a 0 m plane) moves `predErrM` **58.08 → 46.50 m** and creates **zero masks**, twice, byte-identical — the missing half of `C4` is a computation and not data. And the tile server answers **`no dem` on a cold cache**, which is why the campaign is not fingerprinted over it |


---

**Run 11 — 2026-07-30, O3 BUILT: the campaign that had ZERO runnable missions until the day it flew, and
the only one of the ten whose missile umbrella is FRIENDLY.** The eighth campaign to exist as files: ten
`sim/missions/o3-*.fbm` plus `sim/campaigns/o3-yom-kippur-1973.fbc`
([`o3-yom-kippur-1973.md`](o3-yom-kippur-1973.md) §State). **No `sim/src/` file, no tool and no asset was
touched** — `git status --porcelain` lists eleven new untracked files and no modified one, so the 216
pre-existing missions are byte-identical by construction. No new source was researched; the anchor is
Run 1's and unchanged, and the two [T3]/[T4] gaps it names (attack profiles in metres, ordnance per
aircraft) are still unsourced and still labelled `[SET]` wherever this round needed a number.

| Measured | Value |
|---|---|
| Campaign exit / step exits | 3 / `0 3 3 0 0 0 1 0 3 1`; whole campaign **31.4 s** of wall clock |
| Determinism criterion 1 | 9 runs (3 × `--threads 1/2/4`), **1** campaign fingerprint `01e4f956ca915c6a984178df782a2e07d52ed0e7ddd6485717edd177c5f9cb13`, `--elev const` |
| Determinism criterion 2 | **10/10** steps replay standalone bit for bit, on the first attempt — **and the replay was run after the FIRST mission**, on a throwaway one-step `.fbc` that was deleted afterwards (`01 … fp=cc5682956b788fc9 MATCH`) |
| Conservation | nothing to compare: 11 new untracked files, 0 modified. Annotating the ten files with their MEASURED blocks afterwards left all ten per-mission fingerprints and the campaign fingerprint unchanged |
| The spec's own count, inverted | it said **"nothing in this campaign is buildable today"** and named O3 as the only campaign of ten with **zero** runnable missions. `C9` closed on 2026-07-30; re-checked blocker by blocker (`C9` `C8` `C1` `C22` `C23` `C2` `C12` `C0` all closed since), **10 of 10 ran and 10 of 10 answered**. Three spec missions were dropped rather than blocked — `low` (`--elev const` makes ASL = AGL and the whole campaign is already at 300 m), `hawk` (no Hawk row and no Western AAA row of any kind) and `pursued` (no jettison decision) — one was folded into two others, and four are new; all named in the `.fbc` header |
| The campaign's own answer | **a friendly umbrella that cannot interrogate is a NEGATIVE shield, and the number is minus one aircraft.** Over ten sorties: **28 `site LAUNCH`, 28 of 28 aimed at its own aircraft** (attributed by comparing every `sms LAUNCH_SOLUTION … tgtLat/tgtLon` against both sides' telemetry at that tick — 22–95 m from the intended MiG-29, 1.1–10.8 km from the enemy), **0 ever aimed at an enemy**, **0 enemy aircraft ever held on a firm track**, **1 own MiG-29 destroyed** (a 3M9 at 4.74 m against an 8 m fuze) and **1 enemy F-16 with ten systems failed by a V-601 fired at a MiG-29** at 9.51 m. The mechanism is two re-checkable facts: `FBSiteFireControl` contains no IFF path at all, and every SAM row has `Channels = 1`, so a friend inside the envelope does not degrade the engagement — **it deletes it** (o3-06's 44 `site` decision lines are byte-identical to o3-04's, which has no enemy in it) |
| Rule 11, both policies flown | `wcs free` (o3-04/06/09/10) against `wcs hold` (o3-05), one token apart: `site LAUNCH` **7 → 0**, magazines 0+0 → 4 V-601 + 3 3M9 intact, **2 × `net WCS … effect="launch inhibited"`** logged where it bites, and `site TRACK`/`RADIATE`/`net CUE` and the whole delivery **unchanged**. Weapons hold costs nothing and gives up the ability to shoot anybody at all. And the second half of rule 11: four of five umbrella sorties look like *"the belt is harmless"* and it is harmless only by **1.1–5.2 m** — see the finding below |
| The store boundary, measured | a FAB-500 fails a `target_soft` at **68.4 m** and a `target_hard` at **12.1 m** [DERIVED from `core/FBDamageModel.cpp`], and the OPT director delivers at **46–70 m**. So two of the anchor's five target kinds (command posts, fortifications) are unreachable by a factor of **5.2**, and **a 66 m miss on a hardened structure is not even a recorded event** — the arriving 3 376 J/m² is below the `Degrade` threshold, so no `damage` line is written at all. Sortie 09 then spent four aircraft on two of them on purpose, and got four arrivals and nothing else |
| The airframe boundary, measured | **this airframe cannot fly the anchor's own operation.** Cross-track error against a 68.4 m kill radius: **+48.3 m** on a 6.02 km straight final, **+87.2 m** at 12 km, **+90.9 m** at 24 km, **+66.4 m** at 8 km line astern — and **48.3 m + 33.9 m per degree of dog-leg** [DERIVED, three-point attribution ladder], so the largest admissible turn inside a final is **0.59°**. The opening strike of 6 October was 220 aircraft on coordinated approach routes; in this tree such an approach hits nothing. Meanwhile the DIRECTOR's own error goes the other way: `openLoopAlongM` **−70.3 → −9.9 → −1.0 m** over the same ladder |
| The forced-low profile, priced in both directions | 300 m is not a choice: a 6 km rangefinder caps the level-bombing altitude at **2.0–2.2 km** (`mig29-opt-refused.fbm`). It puts the striker under the only two layers of its own umbrella that can reach it (`sa3` 100 m and `sa6` 50 m floors; the `sa2`'s 450 m floor makes it fire **nothing** in 600 s), inside a 23 mm gun's band (which saw with the eye alone in 40.6° of sun, tracked, fired **38 rounds in 29 bursts** and hit nothing) — and it hides the striker from nobody: an F-16's look-down radar took the low pair as its **first** contact, at 21.94 nm and elDeg **−7.44**, **5.24 nm nearer than the escort** |
| The carry, and it is the most decisive of the eight | 8 × `campaign CARRY … action=stores`, one unexpended FAB-500 per aircraft into a 25-unit capstone. **0 of 48 telemetry files byte-identical** (W3's carried node left 30 of 58), because 499 kg on ONE inboard pylon changes mass AND lateral balance: `aimAcrossM` **+48.3 → −39.2 m**, an **87.5 m swing that flips sign** and is larger than the store's own kill radius. Same 6 of 8 aim points killed in both, same 2 Red losses in both — **different aircraft and a different cause**: standalone the 2K12's three 3M9 all miss and `yxd` lives. **One unexpended bomb is worth one aircraft and 238 s of run** |
| Found while building | **three, none fixed here.** (1) **A ground-launched command round misses a low non-manoeuvring crosser by just more than its own fuze radius** — attribution ladder at 300/1 000/3 000/5 000 m: `closestM` **9.07 m against an 8 m fuze** (3M9) and **11.39 / 11.43 / 14.50 / 15.21 m against a 10 m fuze** (V-601), i.e. a **1.1–5.2 m** shortfall, and the confirmation from the other side is the one arrival that DID kill at 4.74 m. Two V-601 also climb to **15 948 / 15 978 m** against a 300 m target before stalling, which is O1's pitch-over defect unchanged. This is the third visible layer of that family and it is now a number on both sides of its threshold. (2) **An unexpended store on one inboard pylon with its mirror empty REVERSES the airframe's standing cross-track offset**, by 87.5 m against a 68.4 m lethal radius — measured on eight aircraft in one file pair. (3) **`core/FBStore.h`'s FAB-500/FAB-250 comment is stale and now states the opposite of the truth** (*"the MiG-29 cannot fly `set task attack` at all (C9)… so these rows exist and their delivery mode does not"*), thirty deliveries later |
| Also measured | the defender's **lateness is a CLOSURE fact and not a detection one**: in the capstone Blue has the 300 m strikers at t = 3.9 s at **45.16 nm** and cannot release until t = 148.4, **122.2 s after the last bomb landed**. And an optical-only position publishes a firm track with `rangeM=1250` / `closureMs=0` — the envelope midpoint as a **pointing** aid, because an eye has no range (`FBSiteFireControl.cpp:443`, documented there); the bearing and elevation are measured and the barrels are aimed off them, so this is understood mechanics and not a defect |
