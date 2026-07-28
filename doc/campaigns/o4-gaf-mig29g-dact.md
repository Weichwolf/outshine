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
| 6 | `o4-06-merge` | a genuine merge from 0.8–2 nm | day | calm | 1 | 1 | as above | read from telemetry, not the exit code | Already attempted (`duel-merge`): 232.3 s, the MiG's first WVR employment (a 12-round GSh-301 burst that missed), and **the honest blocker is that neither ACM box re-acquires after the first pass** — blind 190.1 s of 232.3 for the F-16, 143.2 s for the MiG, 133.6 s simultaneously |
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

---

## State

Unusually for this directory, **partly built already** — not as a campaign, but as the measurements
it would consume:

| Already measured | Where | Value |
|---|---|---|
| Eight BVR geometries, F-16 vs MiG-29, with outcomes | [`../duels.md`](../duels.md) | five draws, two decided by doctrine, one merge |
| The four structural asymmetries with their numbers | ″ §Knowledge 1 | radar reach 100.1 km vs 50.0 km; search bar ±60°/±10.5° vs ±30°/±6°; AIM-120 activation 0.3 s vs R-27R support to impact; warhead 20.5 kg/10 m vs 39 kg/13.8 m |
| The doctrine matrix over three geometries | ″ §Knowledge 4 | `{base, early} × {base, early}`, with the F-16-high geometry drawing when both sides move their trigger out |
| The mixed tournament, 30 runs, both seats | ″ | early launch worth an outcome band to the MiG (−393.7 → +585.0), essentially nothing to the F-16 (601.8 → 603.3) |
| The flight level | [`../formation.md`](../formation.md) | SARH binding 17.3 s vs 0.3 s; sort quality cooperative 0.962 vs contract 0.750 |
| Flare seduction, both directions | `mig29-r73.fbm`, `f16-aim9.fbm`, `mig29-defend.fbm` | deterministic, measured on both branches |

**Nothing is built as a campaign**: no ordered file set, no entry-range sweep, no weather probe.

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C2` + `C3` | **no time of day and no visual channel** | mission 9. The anchor's own concession — "the Americans owned the night" — is the one claim in this campaign FlightBox cannot examine at all, because darkness degrades a sensor the tree does not have |
| `D3` (`duels.md`) | **the pilot does not use the IRST** | the quotation names the IRST as one of three reasons the Fulcrum is hard to beat inside 10 nm. Two of the three (helmet-cued R-73, gun) are modelled; the third is published to a block nobody reads |
| `pilot.md` 2.9 / 2.8 | **neither ACM box re-acquires after the first merge pass; the lift-vector law has a downward singularity; there is a BFM floor both jets sink through** | mission 6 — the WVR half of the campaign, which is *the* half the anchor is famous for |
| `D4` (`duels.md`) | **weapon selection is not a pilot decision** | missions 4/5/7 need a jet carrying both a radar and an IR round to *choose*; today it fires whatever pylon the SMS stepped to, and the missions must load the racks in firing order |
| `D6` (`duels.md`) | the existing campaign measures BVR only; nothing exercises a decided gun or IR engagement | missions 5, 6, 7 are exactly that gap's closure |
| `C7` | no F-15, no F/A-18 | the anchor's other 750 sorties |
| `C0` | no campaign layer | |
| `C12` | no objective for "won the engagement" — a WVR fight ends in TIMEOUT by construction | missions 5–7 are read from telemetry, as `duels.md` already does |

### The honest headline

**O4 is where FlightBox is already standing.** Eight missions buildable, six of them substantially
measured, and the two remaining questions are the two the tree has been circling for rounds: **the
merge** (blocked by re-acquisition, not by control law any more) and **the night** (blocked by having
no eye at all). If any campaign in this set should be built first, it is this one — and its first
deliverable is not a mission file but the entry-range sweep at 10 and 5 nm, which is two `.fbm` files
against modules that exist.

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
