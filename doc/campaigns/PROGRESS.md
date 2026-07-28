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

---

## Campaign coverage

| Campaign | File | Anchor researched | Missions | Cast | Gaps | Status |
|---|---|---|---:|---|---|---|
| W1 Red Flag | [w1-red-flag.md](w1-red-flag.md) | yes ([T3]/[T4]; primary fact sheet **403**) | 10 | yes | yes | **complete, one source blocked** |
| W2 Osirak | [w2-osirak.md](w2-osirak.md) | yes ([T4] primary + 3 cross-checks, 1 [DISPUTED] value) | 10 | yes | yes | **complete** |
| W3 Desert Storm | [w3-desert-storm.md](w3-desert-storm.md) | yes ([T4] primary; one [T3] PDF unread) | 10 | yes | yes | **complete, one source unread** |
| W4 Allied Force | [w4-allied-force.md](w4-allied-force.md) | yes ([T1] + [T3] + [T4]) | 10 | yes | yes | **complete** |
| W5 Baltic QRA | [w5-baltic-qra.md](w5-baltic-qra.md) | yes ([T3]/[T4]) | 10 | yes | yes | **complete** |
| O1 Bekaa 1982 | [o1-bekaa-1982.md](o1-bekaa-1982.md) | yes ([T4] primary; disputes carried) | 10 | yes | yes | **complete** |
| O2 PVO intercept | [o2-pvo-intercept.md](o2-pvo-intercept.md) | partial — doctrine [T3]/[T4], hardware [T2] via `doc/modules/mig29/`; **[T1] material unread** | 10 | yes | yes | **complete, thinnest sourcing in the set** |
| O3 Yom Kippur | [o3-yom-kippur-1973.md](o3-yom-kippur-1973.md) | yes ([T1] Marine Corps study + [T4]) | 10 | yes | yes | **complete** |
| O4 GAF DACT | [o4-gaf-mig29g-dact.md](o4-gaf-mig29g-dact.md) | yes ([T3]/[T4]; one forum test-report thread unread) | 10 | yes | yes | **complete** |
| O5 Airfield defence | [o5-airfield-defence.md](o5-airfield-defence.md) | yes ([T4]; totals [DISPUTED] and deliberately omitted) | 10 | yes | yes | **complete** |
| — | [INDEX.md](INDEX.md) | — | — | aggregated | aggregated | **complete**: map, reading rules, cast by frequency, gaps by blocking degree, the identification section, the Bekaa yardstick |

**100 missions specified. 50 runnable against the tree as it stands** (per-campaign breakdown in
[`INDEX.md`](INDEX.md)).

---

## Sources identified but NOT read

Each of these would raise a claim's tier. None was reachable or retrieved on this pass.

| Source | Would fix | Why not read |
|---|---|---|
| [414th CTS "Red Flag" fact sheet (nellis.af.mil)](https://www.nellis.af.mil/About/Fact-Sheets/Display/Article/2605882/414th-combat-training-squadron-red-flag/) | W1's exercise cadence, package sizes, participant counts, the "first ten missions" rationale at [T1] | **HTTP 403** to automated retrieval |
| [Package Q (Air Force Magazine, Jan 2016, PDF)](https://www.airandspaceforces.com/PDF/MagazineArchive/Documents/2016/January%202016/0116packageq.pdf) | W3's package composition, tanker and SEAD detail at [T3] instead of [T4] | not retrieved this pass |
| [CIA reading room 1979-02-16A](https://cia.gov/readingroom/docs/1979-02-16A.pdf) and [1976-10-12a](https://www.cia.gov/readingroom/docs/1976-10-12a.pdf) | **O2's entire doctrine half at [T1]** — cooperation between PVO troops and fighter aviation; control of front aviation's combat actions | not retrieved this pass. **The highest-value unread source in the directory** |
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
- O2's ten-mission structure in full — no PVO exercise syllabus was sourced.
- O3's every altitude and speed ("very low" is all the strongest source says).
- O5's degradation pattern across missions 3, 8 and 10.

---

**Run 3 — 2026-07-28, `C2` built.** The first code round of the foundation. The `time` line parses, the
clock binds all three clients, the flag collision is a boot error, `FBEphemeris` moved from `render/` to
`core/`, and `fb-gym` writes `FBEnvironmentBlock` for the first time. All 84 pre-round missions
byte-identical; reference mission `sim/missions/clock-night-payerne.fbm`, refusal fixtures in
`sim/missions/negative/`. Details in [`../missions/syntax.md`](../missions/syntax.md),
[`../clients/clients.md`](../clients/clients.md) and [`../journal.md`](../journal.md).

## What the next run should do

Nothing in this directory needs re-writing. The next work is **outside** it:

1. ~~**Build the remaining three foundation contracts**, in the order their dependencies allow: `C2`
   → `C12` → `C3` → `C0`.~~ — **all four built 2026-07-28.** `C0` closed last, with
   `sim/campaigns/viper-attrition.fbc` as its first campaign; the ten campaigns of this directory now
   lack their `.fbm` files and nothing else.
2. Build the four one-line experiments named in [`INDEX.md`](INDEX.md) — `o1-01/02`, `w3-07/08`,
   `w4-01/02`, `w5-02/03`. They need nothing that does not exist, and three of them answer questions
   the tree has been circling for rounds.
3. `C1` — step 2 of the owner goal. Its round begins by changing the Spec of
   [`../weapons.md`](../weapons.md), whose Gaps now carries the bounded entry and the five open
   questions.
4. Read the two CIA reading-room documents and re-tier O2's §Knowledge 1.

~~3. Open a gap entry for `C1`…~~ — **done in run 2**, in [`../weapons.md`](../weapons.md).
