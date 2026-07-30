# O4 — GAF MiG-29G against the F-16, from 1991 (the DACT evaluations)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of the Luftwaffe's MiG-29G operation and its dissimilar air combat training against western types, 1991–2003 | §Knowledge 1, cited and tiered |
| **FlightBox sources** | the two flyable modules and everything already measured between them | [`../duels.md`](../duels.md), [`../formation.md`](../formation.md), [`../modules/mig29/module.md`](../modules/mig29/module.md), [`../modules/f16/module.md`](../modules/f16/module.md), [`../missions/combat.md`](../missions/combat.md) |

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

**Temporal honesty: none needed, and uniquely so — this is the one campaign in which BOTH FlightBox
airframes really flew against each other, in the configuration modelled, in the period named.** It is
also the campaign closest to what the tree already does: [`../duels.md`](../duels.md) is, in effect,
its first three missions already flown and measured.

### One provenance note that must not be glossed

The MiG-29 reference base in `doc/modules/mig29/` is distilled from **two DCS manuals** plus tiered
research — *not* from the German air force's own technical order. **GAF T.O. 1F-MIG29-1** is named in
three separate files of that directory as the one acquisition that would raise several [T4] numbers to
[T1] ([`../modules/mig29/defence-rwr-cm.md`](../modules/mig29/defence-rwr-cm.md),
[`../modules/mig29/cockpit-displays.md`](../modules/mig29/cockpit-displays.md),
[`../modules/mig29/engines-fuel.md`](../modules/mig29/engines-fuel.md)) and it was **not available**.
So: the campaign is named after the operation that produced the best western documentation of the
type, and FlightBox does not have that documentation. That is a reason to build the campaign — it is
the campaign that would justify acquiring the document — and a reason to be careful about every claim
it makes.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Origin | MiG-29s inherited from the NVA at German reunification (October 1990) and taken into the Luftwaffe | [T4] |
| Unit and base | **Jagdgeschwader 73 "Steinhoff"**, **Laage** (≈ 53.92 N 12.28 E — *approximate, verify*) | [T4] |
| Modification | made NATO-compatible as the **MiG-29G** | [T4] |
| DACT volume | **≈450 sorties against F-16**, **>400 against F-15**, **≈350 against F/A-18** of various nations | [T4] |
| What impressed western pilots most | **low-speed manoeuvrability combined with the helmet-mounted sight** | [T4] |
| The Fulcrum pilot's own claim | *"Inside ten nautical miles I'm hard to defeat, and with the IRST, helmet sight and 'Archer' … I can't be beaten"* | [T4], a **quotation, not a measurement** |
| The helmet sight's significance | the USAF and Navy had no operational equivalent until **2003** | [T4] |
| Where the Germans conceded | **American pilots had the advantage at night and in adverse weather** | [T4] |
| Disposal | the fleet was sold on to Poland in 2003–2004 | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| **This campaign measures, it does not stage** | it is the only one whose historical anchor is itself a measurement exercise. Its missions are therefore *experiments*, and their results go beside `duels.md`'s table rather than into a narrative |
| The claimed asymmetry is stated as a **hypothesis with a range** | "inside ten nautical miles the Fulcrum is hard to defeat" becomes: *at entry ranges below 10 nm, the MiG-29's outcome share is higher than at entry ranges above it.* That is falsifiable in the gym |
| The conceded asymmetry is stated the same way | "the Americans owned the night and the weather" becomes a `wx` variable and a (currently missing) time-of-day variable, and the missions that need the second one say so |
| **Every mission is run in both seats** | the geometry is flown with each airframe in each role where that is meaningful, as `fb_tournament.py` already does for pairings |
| A loss caused by an **AI defect** is not a result | the duel campaign's own rule, inherited verbatim: if a side loses because its pilot flies its weapon system badly, the defect is fixed and the geometry re-measured ([`../duels.md`](../duels.md)) |
| **Ground targets in every mission** | a DACT range has range targets; more usefully, they force each side to have somewhere to be, which stops the "both jets drift to the merge" degenerate case |
| The verdict is machine-read | `kill unit` + `survive` on both sides, so the expected-loss rule gives a duel one winner and one loser ([`../missions/verdict.md`](../missions/verdict.md)) |

### 3. The ten missions

| # | Mission | Task | Time | Wx | Ours (MiG-29) | Blue (F-16) | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `o4-01-bvr-headon` | BVR, head-on, co-altitude | day | calm | 1 | 1 | 1 `target_soft` behind each side | both `kill unit` + `survive` | The baseline — and it is **already flown**: `duel-headon`, a draw, both Rtrs within half a mile, both rounds arriving outside the lethal radius ([`../duels.md`](../duels.md)) |
| 2 | `o4-02-bvr-offset` | BVR, 50° crossing | day | calm | 1 | 1 | as above | as above | Already flown (`duel-offset`): the F-16 shoots twice and both rounds expire; the MiG finds and never locks, its track dropping three times on a ±6° bar |
| 3 | `o4-03-energy-split` | 6,000 m and 100–150 kt to one side, then the other | day | calm | 1 | 1 | as above | as above | Already flown both ways (`duel-viper-high`, `duel-fulcrum-high`). Energy is worth more to the side whose round is fire-and-forget — measured, and the number is in `duels.md` |
| 4 | `o4-04-entry-10nm` | WVR entry at 10 nm | day | calm | 1 | 1 | 1 `target_soft` | as above | **The claim's outer edge.** At exactly the range the quotation names, does the outcome share start to move? |
| 5 | `o4-05-entry-5nm` | WVR entry at 5 nm | day | calm | 1 | 1 | as above | as above | Inside the claim. The R-73's ±75° seeker gimbal and the ±60° helmet cueing bound against the AIM-9's ±30° — the module hook that decides whether a shot is *offered* |
| 6 | `o4-06-merge` | a genuine merge from 0.8–2 nm | day | calm | 1 | 1 | as above | read from telemetry, not the exit code | Already attempted (`duel-merge`): the full 300 s with no KO on either side, a 24-round GSh-301 burst that missed, and **the honest blocker is that neither ACM box re-acquires after the first pass** — blind 231.6 s of 300.1 for the F-16, 223.7 s for the MiG, 220.1 s simultaneously. The MiG's CFIT that used to end this cell is closed (`pilot.md` §5.10a, the missing rate damper on the `Manual` path) |
| 7 | `o4-07-flares` | the IR shot against a defending target | day | calm | 1, BVP-30-26 armed | 1, ALE-47 armed | as above | read from telemetry | Both dispensers already work through the same deterministic seduction model; the asymmetry is now magazine size (60/60 vs 30/30), not presence. Does that difference show up over a full engagement? |
| 8 | `o4-08-weather` | mission 4 in real weather | day | `wx fixture` | 1 | 1 | as above | as above | **The conceded asymmetry, half of it.** The MiG's IRST is the only sensor in the tree a cloud deck can blind (`irst_masked`, measured in `mig29-irst.fbm`). Does weather move the WVR result? |
| 9 | `o4-09-night` | mission 4 at night | **night** | calm | 1 | 1 | as above | as above | **The conceded asymmetry, the other half — and it cannot be flown** (`C2`, and more deeply `C3`: there is no visual channel for darkness to degrade). The mission is specified so that the hole is named |
| 10 | `o4-10-two-v-two` | 2v2, mixed doctrines, both seats | day | `wx fixture` | 2 (flight, contract sort only) | 2 (flight, cooperative sort) | 2 `target_soft` + 1 `target_hard` | `kill team` + `survive` | The flight-level version of the whole campaign — and already partly flown as `pair-2v2-asym.fbm`: the SARH binding measured at **17.3 s** per shot against the AIM-120's **0.3 s**, a factor of 58, and the MiG has no channel on which "my leader is bound" could travel |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| MiG-29 | flyable module | **yes** | the MiG-29G is the 9-12 with NATO-compatible radios/IFF; the *aerodynamic and weapon* model is unchanged, so the substitution is small and stated |
| F-16C | flyable module | **yes** | |
| `target_soft` / `target_hard` | ground | **yes** | range targets |
| F-15 / F/A-18 | flyable module | **no** (`C7`) | the anchor's other two DACT partners; out of scope for a *this-campaign* build but named because the anchor's sortie counts include them |
| Range instrumentation (ACMI) | infrastructure | **not needed** | FlightBox's telemetry **is** the ACMI pod, and per unit — this is the one campaign where the tree is better instrumented than the original |
| Tanker | air, support | **no** (`C5`) | DACT sorties over the Baltic tanked; irrelevant to the measurement |

### 5. What must be true before mission 1 can fly

**Eight of ten are buildable today**, and three of them (1, 2, 3) plus large parts of 6, 7 and 10 have
**already been flown and published** in [`../duels.md`](../duels.md) and
[`../formation.md`](../formation.md). O4 is therefore not a new campaign so much as the **re-framing
of the existing measurement campaign around its historical anchor**, plus four genuinely new
missions: the two entry-range probes (4, 5), the weather probe (8) and the night probe (9, blocked).

That is worth stating plainly because it changes the build order: **O4 is the cheapest campaign in
the set**, and the one that would produce a result soonest.

**Amended by the build (2026-07-29), and the amendment is the point of a Spec that is written first.**
Both missions this section calls blocked were re-checked against the tree instead of trusted:

| Spec said | Today |
|---|---|
| mission 6 is blocked by close-combat re-acquisition | **runs and DECIDES.** The blocker was overtaken by [`../pilot.md`](../pilot.md) §5.11 (`Phase::Bfm` had no missile employment path at all): the merge is now settled on the FIRST pass, so re-acquisition never becomes the question. `o4-06` exits 0 at t = 6.5 s |
| mission 9 "cannot be flown" (`C2` + `C3`) | **runs, and still cannot ANSWER.** Both gaps are closed as capabilities — `time` is mission data and `FBVisualSystem` exists — so the file is a legal, running mission. But nothing consumes the visual block and nothing in the tree emits light, so darkness reaches no decision. The mission is kept at full size with its own reading rule, and what it publishes is the SIZE of the hole (6 of 184 columns) rather than a tactical claim |

**Ten of ten are buildable today; nine of ten are answerable.**

---

## State

**BUILT AND FLOWN, 2026-07-29 — the first of the ten campaigns to exist as files.** Ten `.fbm` in
`sim/missions/o4-*.fbm` plus `sim/campaigns/o4-gaf-mig29g-dact.fbc`, run as a campaign, replayed
step by step, and measured. Nothing under `sim/assets/` was touched (`git status --porcelain sim/assets`
empty, `verify-models` green) and every pre-existing mission is byte-identical.

### The arena, and why it is where it is

A block of Baltic airspace centred **55.20 N 13.60 E** `[SET]`, ~145 km NNE of Laage, flown with
`--elev const` because it is over water and 0 m **is** sea level there rather than an approximation.
Two moored range hulks stand at its ends in every sortie (rule 3), and each side's briefed run is to the
OTHER side's hulk, which is what stops the degenerate "both jets drift to the merge". The campaign clock
is a June morning inside the anchor's own period; mission 9 declares its own December night.

### The ten sorties, their exit codes and their answers

Campaign exit **3** (the worst step's, and the campaign's verdict is the lines below rather than the
code). Fingerprint of the whole campaign under `--elev const`:
`461e0ff5299d83d03b7fe303842e019f02699f096bccfe45acdbd35a7a203724`. **The FRAME round of 2026-07-29 moved it again** — the spawn state is now the trimmed airframe's own rather than position only, so the first 0.01 s of guidance moved in every mission with an airframe (`sensors.md` §10, item 24). Post-frame-round value, both criteria re-measured and still holding: `7ad40b31d572574cf71f822746f5e614af761316526357e38ab593628e8d4f00`.

| # | Mission | exit | fingerprint | The answer to its one tactical question |
|---|---|---:|---|---|
| 1 | `o4-01-bvr-headon` | **0** | `3fbca77e269de1f6` | The baseline REPRODUCES to within a rounding error — detect 24.0 s / 52.19 nm against 108.2 s / 25.41 nm, shot 9.56 nm at Rtr 9.79 against 10.07 at Rtr 10.26 ([`../duels.md`](../duels.md) row 1: 9.57/9.78 and 10.02/10.25). What does NOT reproduce is the outcome: `duel-headon` draws, this file is an F-16 kill at t = 549.8. **The draw was a magazine.** With four AIM-120 instead of two the F-16 still has rounds when the two jets re-merge at t = 540 and its second AMRAAM arrives **2.23 m** out where the first arrived 6.54 m |
| 2 | `o4-02-bvr-offset` | 3 | `29a03f130d5f2907` | A 50° crossing costs each radar a different KIND of thing, exactly as row 2 says. The AIM-120 arrives **6.57 m** out (the round spends its energy on the turn); the R-27R arrives **11.96 m** against a 13.8 m fuze — inside the ring, outside the lethal radius. Draw |
| 3 | `o4-03-energy-split` | 3 | `951e9f9203258a71` | **Standalone: 6,000 m and 100 kt let the MiG shoot and the F-16 not shoot at all** (fulcrum releases at t = 147.4 and 393.1, R-27R 10.16 m out; viper releases nothing — row 4 reproduced). **In the campaign the same file inverts**, and that is the carry: see below |
| 4 | `o4-04-entry-10nm` | **0 → 1** | `d6f2052cf8f42ffd` → **`c397e62f2d40b1f7`** | **At exactly ten miles the outcome moves, and the mechanism is nameable.** The MiG's shot is accepted at **t = 1.5 s, 17,796 m, cueDeg 60**; the F-16's not until **t = 12.5 s, 11,963 m, cueDeg 30**. 11.0 s and 5.8 km of first shot, and neither number is mission data: the R-73's Raero is 20,000 m against the AIM-9's 12,000, and the Shchel-3UM cues to ±60° where the AIM-9 sees ±30°. The second R-73 kills at **1.65 m**. **MiG-29 wins** | **RE-MEASURED 2026-07-29 (frame round): a TRADE, and the campaign's own claim survives it.** The first-shot advantage is untouched — MiG at t = 1.5 s from 17,796 m, F-16 not until t = 12.4 s from 11,985 m. What moved is that the F-16's AIM-9, fired at **99.9 % of its own 12,000 m Raero**, now arrives (1.61 m, `damage KILL` on the fulcrum at t = 24.9) where it used to miss by 1,720 m; the third R-73 then kills the viper at t = 29.4. **Ten miles no longer belongs to the MiG alone — it is a trade decided by whether a shot at maximum kinematic range reaches, which is not a claim this arena can carry.**
| 5 | `o4-05-entry-5nm` | 1 | `bcd2cbc888e3d25c` | Inside the claim both rounds are in envelope from t = 0 (shots at t = 1.5 and t = 1.9) and the hook that decided sortie 4 decides nothing. **A TRADE**: AIM-9 1.63 m at t = 11.8, R-73 2.00 m at t = 11.9 — 0.1 s apart, both dead. Exit 1 is the verdict layer refusing to invent a winner |
| 6 | `o4-06-merge` | **0 → 1** | `e3147e607fd50377` → **`4f2d93487a3eba2e`** | At 2.00 nm the MiG still shoots first (t = 1.5 against 1.9) and **loses**. R-73 **2.608 m** against its own 2.08 m kill radius → degraded; AIM-9 **1.396 m** against 2.32 m → `damage KILL` at t = 6.5. What decides the knife fight is the warhead, not the cueing envelope and not the gun. **F-16 wins** | **RE-MEASURED 2026-07-29 (frame round): a TRADE, and the WARHEAD claim is WITHDRAWN.** Same two shots at the same instants and ranges (t = 0.8 / 1.0, 3,309 / 3,211 m); 1 m of arrival moved and both rounds are now inside both kill radii — R-73 **1.646 m** against 2.08 (was 2.608, outside), AIM-9 **2.067 m** against 2.32. Both jets die 0.1 s apart. At 380 kt head-on inside 3.5 km this arena decides nothing: which K.O. is declared first is which round was fired first minus its time of flight, and 9.4 kg against 7.4 never enters.
| 7 | `o4-07-flares` | 3 | `207b10cb3d944f99` | **Yes, the magazine shows up, and it is worth 18.9 seconds.** Both jets throw PRGM 3 at the identical rate on the identical schedule; the BVP-30-26 is dry at **t = 23.8 s** (four dispenses accepted, the rest `rejected reason=depleted`), the ALE-47 at **t = 42.7 s**. Both rounds are pickled at t = 32. The AIM-9 finds a MiG with nothing in the air and detonates **0.0035 m** out; the R-73 is seduced repeatedly (`irst FLARE_SEDUCED` ×9), walks onto a cartridge and never arrives |
| 8 | `o4-08-weather` | 1 | `5d5fd1fddef31e44` | **The outcome moves — and the cloud is not what moves it.** See the attribution below. What the deck DOES take away is measured: the MiG's eye goes from **50 contact frames and a RECOGNISED state** to **0 contacts and 7 `vis MASKED` frames at transmittance 0.011** |
| 9 | `o4-09-night` | **0 → 1** | `efbb190c451c9cfa` → **`27a23c6d0fa78e3a`** | **Runnable at last, and still unanswerable — measured as a column count.** Of **184 telemetry columns**, the night run differs from the identical daylight run in **six**, all of them visual: `vis_glare` on both jets, plus `vis_contacts` / `vis_best_mrad` / `vis_best_az` / `vis_best_el` / `vis_best_state` on the MiG (50 rows each). Every flight, sensor, weapon and engagement column is byte-identical, and the two runs kill the same jet at the same tick | **RE-MEASURED 2026-07-29 (frame round): the direction holds, the SIZE does not.** Its daylight control (sortie 4) now ends at t = 29.5 s instead of 76.9, so the eye gets a third of the run: o4-04 still produces one `vis` line and this run still produces none, but the telemetry diff is **one column (`blk_env`) instead of six** — with no daylight contact held long enough, the five `vis_*` columns are identical zeros on both sides. Darkness still decides nothing; how much it reaches is no longer measurable here.
| 10 | `o4-10-two-v-two` | 3 | `7c2f5bb13660c049` | Four arrivals, no kill: two AIM-120 at **5.09 m** and **6.19 m** (both on `fulcrum9` — the cooperative sort did NOT split them), an R-27R at 12.20 m and an R-73 at 2.64 m against the F-16's 2.32 m radius. The channel asymmetry is visible as thrash rather than as advantage: **52 and 19 `flt_switch` on the cooperative pair against 2 and 1 on the contract pair, 74 `SORT_ASSIGN` lines and 0 `COVER_DEFER`** |

### The carry, and where it lands

`carry ground stores`. **Units are deliberately not carried** — a DACT sortie ends with both aircraft
landing at Laage, and JG 73 flew its ~450 sorties against F-16s without losing a jet to one — so O4
shows no attrition arc at all and is not an attrition campaign. `ground` carries and moves nothing (no
mission in O4 has an air-to-ground weapon; the MiG cannot fly `set task attack` at all, `C9`), and the
campaign says so rather than implying an arc it does not have.

**`stores` is the carry that bites, and it bites in sortie 3.** Sorties 1–3 are one pair of airframes
flying a range period without rearming; sortie 3 therefore takes off with what sorties 1 and 2 landed
with — three `set store` lines dropped from the F-16, two from the MiG, every one logged as
`campaign CARRY`:

| Sortie 3 | standalone | inside the campaign |
|---|---|---|
| F-16 rack at spawn | 4 × AIM-120 + 2 × AIM-9 | **1 × AIM-120** + 2 × AIM-9 |
| MiG rack at spawn | 2 × R-27R + 4 × R-73 | **0 × R-27R** + 4 × R-73 |
| who shoots | fulcrum 2 × R-27R + 1 × R-73; **viper never fires** | viper 1 × AIM-120 + 1 × AIM-9; fulcrum one R-73 launched at **16,713 m**, a heat-seeker at nine miles |
| closest arrival | R-27R **10.16 m** on the F-16 | AIM-120 **5.13 m** on the MiG |

The engagement inverts. Both runs draw, and they draw for opposite reasons: standalone the MiG owns the
shot, in the campaign it has nothing to shoot with. **The F-16's BVR advantage in this tree is a
magazine as much as a radar**, and a magazine is precisely what a sequence of sorties can take away.

### The attribution of sortie 8, because a weather result must not be assumed

The fixture changes two things — cloud and wind — and only one of them can be blamed. It was measured
rather than argued: the identical file with `wx wind 247 21.9` (the fixture's own vector at the fight's
own level, so wind WITHOUT cloud) reproduces the weather run's every event to three decimals —
detonations at t = 22.9 / 25.0 / 29.5 with missM **3.033 / 1.615 / 2.026** against the fixture's
**3.007 / 1.608 / 2.027**, same exit 1, same trade. **The outcome flip from sortie 4 is entirely the
wind.** The cloud's contribution is exactly zero because both channels it can touch — the KOLS and the
eye — have no consumer in `pilot/FBPilot`.

The KOLS is worse off than that: on this geometry it never had a contact to lose in EITHER run
(`irst_contacts` = 0 throughout both), because an IRST's reach is an aspect function and head-on is its
worst case, 10 km against 25 km astern. **The quotation's third reason contributes nothing to a head-on
WVR entry even in clear air.**

### Both determinism criteria, measured

Under `--elev const`, which is what the campaign's own header declares and what every comparison must
read out of `campaign-summary.txt` rather than assume:

| # | Criterion | Result |
|---|---|---|
| **1** | 3 repetitions × `--threads 1/2/4` produce one campaign fingerprint | **9 runs, 1 fingerprint** `461e0ff5299d83d03b7fe303842e019f02699f096bccfe45acdbd35a7a203724`; **re-measured after the frame round of 2026-07-29: 9 runs, 1 fingerprint** `7ad40b31d572574cf…`, exit 3 in all nine |
| **2** | every step's per-mission fingerprint equals that mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included; **10/10 again after the frame round of 2026-07-29** |
| **1 — re-run 2026-07-30** | the same criterion under the branch-order change of `b433950` ([`../pilot.md`](../pilot.md) §7.4a) | **9 runs, 1 fingerprint** `9c994069f01595c2291755fc6dc573733ba8c0e0bf3122369cb5aa4a03854f85`, `--elev const`. **The value above is kept with its date; this is the current one.** Step exits `0 3 3 1 1 1 3 1 1 3` — unchanged |
| **2 — re-run 2026-07-30** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 | | `0bdcf37a93d411f7 86a8fd8944c61e33 64b28b2b4321b687 c397e62f2d40b1f7 c071f9b210642eb8 4f2d93487a3eba2e 177f53387eb13368 15eb7f14e31ab95a 27a23c6d0fa78e3a 7cd6ad00af518446` |
| **1 — re-run 2026-07-30 (`E6`)** | the same criterion after the judge-completion fix of [`../doctrine-evolution.md`](../doctrine-evolution.md) X-1 | **9 runs, 1 fingerprint** `9c994069f01595c2291755fc6dc573733ba8c0e0bf3122369cb5aa4a03854f85`, `--elev const`. **The rows above are kept with their dates; this is the current one.** Step exits `0 3 3 1 1 1 3 1 1 3` — unchanged. **Unchanged, byte for byte** — campaign fingerprint and all ten step fingerprints. |
| **2 — re-run 2026-07-30 (`E6`)** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 (`E6`) | | `0bdcf37a93d411f7 86a8fd8944c61e33 64b28b2b4321b687 c397e62f2d40b1f7 c071f9b210642eb8 4f2d93487a3eba2e 177f53387eb13368 15eb7f14e31ab95a 27a23c6d0fa78e3a 7cd6ad00af518446` |

**Criterion 2 failed 9 of 10 on the first attempt, and the failure was a real hole in the campaign
layer rather than in this campaign.** `fb-gym --mission --state` had no way to receive the CAMPAIGN
CLOCK, so every step of a campaign that declares a `time` line replayed under a different sky; only
mission 9, which declares its own clock, matched. `viper-attrition` declares no `time`, which is why the
`C0` round never saw it. Closed the way §5 already closes the ground: the clock is **recorded** by the
runner (`campaign-summary.txt`: `time …`) and **read** by the checker, with `fb-gym --campaign-time ISO`
as the receiving flag — campaign data, never a client clock, so it fills in for a mission without a
`time` and never displaces one that has it. Details in [`../missions/campaign.md`](../missions/campaign.md).

### Conservation

`fb-gym` built from the same tree with the two touched sources reverted, both binaries run over all
**150** `sim/missions/*.fbm`: **515/515 `telemetry*.csv` byte-identical, 150/150 `events.log` identical
modulo `wallS`/`speedup`/the `--out` path, exit codes identical.** The same 150 missions at
`--threads 1`, `2` and `4`: **0 differing files.** `viper-attrition` re-verified: 9 runs one fingerprint
(`dfa2f97d026e0fa2…`), 4/4 standalone replays MATCH.

### What the ten runs say about the pair — against the anchor

The anchor's headline claim is *"inside ten nautical miles I'm hard to defeat, and with the IRST, helmet
sight and 'Archer' … I can't be beaten"* [T4]. Converted into the falsifiable form of §Spec 2 and flown,
the entry-range sweep answers it in a way no interview would:

| entry | outcome | why |
|---|---|---|
| 10.00 nm | **MiG-29 wins** | the R-73's 20 km Raero and ±60° helmet cueing offer a shot 11.0 s before the AIM-9's 12 km Raero and ±30° offer one |
| 5.00 nm | **trade**, 0.1 s apart | both rounds in envelope from t = 0; the hook that decided 10 nm decides nothing |
| 2.00 nm | **F-16 wins** | 9.4 kg of AIM-9M warhead (2.32 m kill radius) against 7.4 kg of R-73 (2.08 m), on arrivals of 1.40 m and 2.61 m |

**The claim is true at its own outer boundary and false at the knife-fight end, and the two halves have
different mechanisms.** What makes the Fulcrum hard to beat at ten miles is the ENVELOPE and the CUEING —
the two of the quotation's three reasons FlightBox models — and what beats it at two miles is a warhead
mass the quotation never mentions. The third reason, the IRST, contributes nothing at any range in this
tree, and sortie 8 measures why twice over (no consumer, and no aspect).

**Where our result contradicts the anchor, plainly.** The Germans conceded the night and the weather.
FlightBox can say nothing at all about the night — six visual columns out of 184, none of them read by
anybody — and what it measures about the weather is the WIND, not the cloud. Any O4 statement about
either half of that concession would be manufactured, and none is made. Against that, the campaign
produces one asymmetry the anchor does not mention at all and which is not about flying: **the F-16
carries a three-sortie BVR magazine and the MiG-29 a one-sortie one**, and over a range period that is
worth more than any doctrine lever measured in [`../duels.md`](../duels.md).

### What the tree already had, and this campaign consumed unchanged

| Already measured | Where | Value |
|---|---|---|
| Eight BVR geometries, F-16 vs MiG-29, with outcomes | [`../duels.md`](../duels.md) | five draws, two decided by doctrine, two decided merges |
| The four structural asymmetries with their numbers | ″ §Knowledge 1 | radar reach 100.1 km vs 50.0 km; search bar ±60°/±10.5° vs ±30°/±6°; AIM-120 activation 0.3 s vs R-27R support to impact; warhead 20.5 kg/10 m vs 39 kg/13.8 m |
| The mixed tournament, 30 runs, both seats | ″ | early launch worth an outcome band to the MiG, essentially nothing to the F-16 |
| The flight level | [`../formation.md`](../formation.md) | SARH binding 17.3 s vs 0.3 s; sort quality cooperative 0.962 vs contract 0.750 |
| Flare seduction, both directions | `mig29-r73.fbm`, `f16-aim9.fbm`, `mig29-defend.fbm` | deterministic, measured on both branches |

---

## Gaps

| ID | What is missing | State here |
|---|---|---|
| ~~`C0`~~ | no campaign layer | **CLOSED before this round** and consumed by it: `sim/campaigns/o4-gaf-mig29g-dact.fbc`, both criteria measured above. This campaign found and closed the layer's clock hole |
| ~~`pilot.md` 2.9~~ (for mission 6) | neither ACM box re-acquires after the first pass | **NO LONGER BLOCKING.** The merge is decided on the FIRST pass (`o4-06` exit 0 at t = 6.5 s), so there is no second pass to acquire for. The gap itself is open and still owns any fight that survives a merge |
| `C2` + `C3` (for mission 9) | **now BUILT, and mission 9 runs — and cannot answer its question** | the hole moved from "cannot be declared" to "is declared and reaches nobody". MEASURED: 6 of 184 columns, 0 visual contacts at night, identical kill at an identical tick. What is missing is a CONSUMER of `FBVisualBlock` in `pilot/FBPilot`, plus an aircraft-lighting model without which the channel is empty after sunset regardless |
| `D3` (`duels.md`) | **the pilot does not use the IRST** | the sharpest gap this campaign hit, and it now has two independent measurements: no consumer (so masking it changes nothing) and no aspect (so on a head-on entry there was nothing to mask — `irst_contacts` = 0 in both sortie-4 and sortie-8) |
| `D4` (`duels.md`) | weapon selection is not a pilot decision | worked around exactly as the spec predicted: every rack in all ten files is declared in FIRING ORDER, and each header says so. It is the single largest piece of authoring artifice in the campaign |
| **new — the cooperative sort thrashes** | on `o4-10`'s geometry the F-16 pair re-assigns **52** and **19** times against the MiG pair's **2** and **1**, and both AMRAAMs still went to the same target | the sort is dynamic and unhysteresised. It costs nothing here because neither round killed, but it is the first measurement in the tree that says the cooperative channel can be NOISIER than the briefed contract |
| **new — `brief_flare_s` is a MiG-only spelling** | `modules/f16/FBF16Module` accepts only `brief_chaff_s`; `modules/mig29/FBMig29Module` accepts both | a symmetric two-branch mission must use the spelling both modules answer to, which reads as chaff while throwing flares (the dispense plays the SELECTED programme). One alias would fix it |
| `C7` | no F-15, no F/A-18 | the anchor's other ~750 sorties are still out of reach, and the substitution is not attempted |
| `C12` | no objective for "won the engagement" | unchanged and unproblematic: eight of the ten missions carry their reading rule in their own header and are read from telemetry |

### The honest headline

**O4 exists, it runs, it replays, and its result is not the one the anchor implies.** The Fulcrum's
famous ten-mile claim survives at ten miles and dies at two, for reasons that are a launch envelope, a
cueing limit and a warhead mass — all three of them catalogue numbers rather than flying. The two things
the campaign genuinely cannot examine are the two the Germans themselves conceded, and FlightBox's
inability is now a MEASUREMENT (six columns, zero consumers) instead of an assertion. The one finding
that belongs to nobody's anchor is the magazine: three sorties of AMRAAM against one of R-27R.

---

## Knowledge

### 1. The anchor with its sources

- **Origin, unit, base, NATO conversion.** [Taktisches Luftwaffengeschwader 73 (Wikipedia)](https://en.wikipedia.org/wiki/Taktisches_Luftwaffengeschwader_73)
  [T4]; [German Luftwaffe and the MiG-29 Fulcrum (MiGFlug)](https://migflug.com/jetflights/german-luftwaffe-mig-29-fulcrum/)
  [T4]; [How the Soviet MiG-29 became a NATO fighter jet (We Are The Mighty)](https://www.wearethemighty.com/articles/how-the-soviet-mig-29-became-a-nato-fighter-jet/)
  [T4].
- **The DACT sortie counts** (≈450 vs F-16, >400 vs F-15, ≈350 vs F/A-18) and the conduct of the
  training from Laage: [German Fulcrums flying for both sides (Key.Aero)](https://www.key.aero/article/german-fulcrums-flying-both-sides-0)
  [T3]; [A rocket in the sky: NATO's first impression of the MiG-29 Fulcrum (Medium)](https://murtiedjokobayu.medium.com/a-rocket-in-the-sky-natos-first-impression-of-the-mig-29-fulcrum-cdb161119999)
  [T4].
- **The helmet sight, the "inside ten miles" quotation, and the 2003 western equivalence date.**
  [Here's why the MiG-29 could defeat the best western fighters in close air combat (The Aviationist)](https://theaviationist.com/2015/04/08/mig-29-in-close-air-combat/)
  [T4]; [F-16 vs MiG-29: when the mighty Viper dogfighted with the Fulcrum for the first time (Aviation Geek Club)](https://theaviationgeekclub.com/f-16-vs-mig-29-when-the-mighty-viper-dogfighted-with-the-fulcrum-for-the-first-time/)
  [T4].
- **The German concession on night and adverse weather.** Same two sources [T4].
- **Energy-manoeuvrability comparison from a test report** exists in the community record —
  [F-16 vs MiG-29 energy manoeuvrability from test report (f-16.net forum)](https://www.f-16.net/forum/viewtopic.php?f=30&t=53852)
  [T4] — **not retrieved on this pass** and flagged in [`PROGRESS.md`](PROGRESS.md). It is the most
  likely public route to a T2/T3 number for mission 4/5's expectations.

### 2. What the anchor's headline claim is and is not

> *"Inside ten nautical miles I'm hard to defeat, and with the IRST, helmet sight and 'Archer' I can't
> be beaten."*

This is a **pilot's statement in an interview** [T4]. It is not a test result, it carries no
conditions (entry geometry, energy state, number of aircraft, whether the F-16 was AIM-9X-equipped —
it was not, in the period) and it is precisely the kind of claim a measurement campaign exists to
bound. The campaign therefore does **not** try to reproduce it. It converts it into the falsifiable
form in §Spec 2 and reports the entry range at which the outcome share actually moves, with the
geometry stated.

The same discipline applies to the concession about night and weather: it is an equally
unconditioned statement, and FlightBox can examine exactly half of it (`wx`) and none of the other
half (`C2`/`C3`).

### 3. Why this campaign is the one that justifies acquiring GAF T.O. 1F-MIG29-1

Three of the numbers that decide missions 4–7 are currently research-tier:

| Number | Current tier | What the T.O. would give |
|---|---|---|
| The helmet sight's actual cueing envelope (FlightBox uses ±60° az as a module hook) | [T4]/hook | a documented limit, and whether it is symmetric |
| The R-73's seeker gimbal (±75° [DOC] against the AIM-9's ±30° [T4]) | mixed | the western-operated aircraft's own figure |
| SPO-15 behaviour details, engine and fuel figures | [T4], flagged in three files of `doc/modules/mig29/` | [T1] |

The campaign named after the western operation of the type is the natural place to record that the
western documentation of the type is the missing source. It is recorded here rather than only in
`doc/modules/mig29/`'s Gaps, because a reader arriving from the campaign side would otherwise never
learn it.
