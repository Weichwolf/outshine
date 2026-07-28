# O5 — Airfield defence against a strike package

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Yugoslav MiG-29 operations from Batajnica, 24–26 March 1999, with the Iraqi 1991 case as the corroborating parallel | §Knowledge 1, cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare and what the MiG-29 module does | [`../modules/mig29/module.md`](../modules/mig29/module.md), [`../missions/verdict.md`](../missions/verdict.md), [`../missions/sensors.md`](../missions/sensors.md), [`../formation.md`](../formation.md), [`../duels.md`](../duels.md) |

Confidence legend and gap IDs `C0…C21`: [`INDEX.md`](INDEX.md).

**Temporal honesty: none needed for the type.** The MiG-29 flew this exact mission, in this exact
posture, twice in the modern record — Batajnica in 1999 and Iraq in 1991 — and lost both times. What
*is* declared is the substitution on the **other** side: the anchor's attackers were F-15Cs, F-117s
and cruise missiles, and FlightBox has only the F-16.

### The point of the campaign, stated before the missions

**This campaign's success condition is not victory.** A scrambled defender against a package it
cannot beat has three achievable outcomes, in descending order of value:

1. the package's ordnance does not reach the target (jettison, abort, or a striker turned away),
2. the package's timing is broken (the target is hit late, or by fewer aircraft),
3. the defender survives to fly again.

None of the three is "shoot down the escort", and FlightBox's objective vocabulary can express
**only the third** today (`C12`). That is the campaign's central gap and it is stated up front rather
than discovered in mission 7.

---

## Spec

### 1. The anchor, in two tables

**Batajnica, 24–26 March 1999**

| Fact | Value | Tier |
|---|---|---|
| The unit | **127th Fighter Aviation Squadron "Knights"**, the Yugoslav air force's only MiG-29 unit, at **Batajnica** (≈ 44.93 N 20.26 E — *approximate, verify*) | [T4] |
| Night of 24/25 March | **five MiG-29s scrambled** against the opening NATO attacks | [T4] |
| The pair from Batajnica | Maj. Nebojša Nikolić and Maj. Ljubiša Kulačin, engaged by USAF Capt. Mike Shower; **Nikolić shot down**; Kulačin evaded several missiles "while fighting to bring his malfunctioning systems back to working order" | [T4] |
| 26 March | two more MiG-29s (Capt. Radosavljević, Maj. Perić) engaged NATO aircraft and were **shot down by F-15Cs** | [T4] |
| Aircraft condition | those that remained serviceable had **outdated avionics, degraded radar performance and a limited stock of R-73/R-27 missiles** | [T4] |

**The corroborating parallel — Iraq, January 1991**

| Fact | Value | Tier |
|---|---|---|
| Force | ≈**40 MiG-29s** | [T4] |
| Result | **six shot down by F-15Cs** in the opening days; **none scored an air-to-air kill**; the fleet did not survive the war with one | [T4] |
| The explanation given | poor training, **no AEW support**, and the aircraft flown "essentially alone, without the tactical architecture the design assumed" — Soviet doctrine called for tight ground control | [T4] |

The two cases agree on the mechanism: **an aircraft designed to be flown to the merge by somebody
else, flown without that somebody, against a package that had one.**

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| **The measure is what the package did not achieve** | every mission reports strikers that released, targets destroyed, and time-on-target slip. A mission that only reports kills has measured the wrong side of the fight |
| The defender is **degraded by declaration**, not by accident | the anchor's aircraft flew with broken systems. Expressible today: `set n019_mode off`, `set n019_emission off`, `set rwr off`, a reduced missile load, a low `set fuel_pct`. Each mission names which |
| **The scramble is a delay** | the defender spawns later than the attacker and starts on the ground where the format allows it. That delay is the campaign's most important single parameter |
| The GCI is present, then absent | the campaign's spine, and the same experiment as `o1-02` and `w3-08`. All three must agree |
| **Ground targets in every mission** — and here they are *ours* | the airfield being defended: hardened shelters, fuel, the runway itself. That is the object of the whole exercise |
| The verdict is machine-read where it can be | defender `survive`; the attacker's `kill unit` on our ground targets **failing** is the defender's success, read from the attacker's verdict (`C12`) |

### 3. The ten missions

Ours = MiG-29 (defender). Blue = the attacking package (F-16).

| # | Mission | Task | Time | Wx | Ours | Blue | Ground targets (ours) | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `o5-01-alert` | scramble and intercept a single striker | **night** | calm | 1 MiG-29, ground start, spawns 60 s late | 1 F-16 with stores | 1 `target_hard` (shelter) | Blue's `kill unit` **fails** | The whole campaign in miniature. **How much warning does this aircraft need to reach a striker before its release point?** A pure geometry question, and the campaign's calibration |
| 2 | `o5-02-airborne` | the same intercept, already airborne | **night** | calm | 1 MiG-29, air start on CAP | 1 F-16 | as above | as above | The control for mission 1. The difference between the two runs **is** the value of being on CAP versus on alert |
| 3 | `o5-03-no-radar` | mission 2 with the radar dead | **night** | calm | 1 MiG-29, `set n019_mode off` | 1 F-16 | as above | as above | The Batajnica condition, isolated: what is a MiG-29 with no radar? It still has the IRST, the RWR and a gun — **and today the pilot uses none of the first** (`D3`) |
| 4 | `o5-04-no-gci` | mission 2 with the GCI brief deleted | **night** | calm | 1 MiG-29, **no `brief_gci`** | 1 F-16 | as above | as above | The Iraqi condition, isolated. **Must agree with `o1-02` and `w3-08`** — three campaigns, one experiment, three theatres |
| 5 | `o5-05-escorted` | striker with an escort | **night** | calm | 2 MiG-29 (flight) | 2 F-16 strike + 2 F-16 escort | 1 `target_hard` + 2 `target_soft` | ≥1 of Blue's targets survives | Does the defender go for the escort or through it? There is **no target-priority declaration** — the sort optimises range and geometry, not value (`C12`, `formation.md` F6) |
| 6 | `o5-06-saturation` | more strikers than interceptors | **night** | calm | 2 MiG-29 | 6 F-16 (two flights) | 1 `target_hard` + 3 `target_soft` | ≥2 of 4 targets survive | With three attackers per defender, does engaging *anybody* still reduce the ordnance on target — or does a defender that cannot win simply die without effect? |
| 7 | `o5-07-runway` | the runway itself is the target | **night** | calm | 2 MiG-29 already airborne | 4 F-16 | 1 `target_hard` (runway) + 1 `target_soft` (fuel) | our aircraft **recover** | **The mission the format cannot express** (`C17`): a cratered runway does not close, a damaged airfield has no state, and there is no divert field to declare. Specified so the requirement is visible |
| 8 | `o5-08-degraded-pair` | two aircraft, both broken differently | **night** | `wx fixture` | 1 MiG-29 `set n019_mode off`, 1 `set rwr off`, low fuel on both | 4 F-16 | 1 `target_hard` + 2 `target_soft` | ≥1 defender recovers AND ≥1 target survives | The anchor's real condition: nothing works properly and the tasking is unchanged. Which of the two broken jets contributes more, and is the answer stable across geometries? |
| 9 | `o5-09-second-wave` | a second package while the first is still egressing | **night** | `wx fixture` | 4 MiG-29 (two flights, staggered) | 4 + 4 F-16 | 2 `target_hard` + 3 `target_soft` | ≥2 targets survive + ≥2 defenders recover | Committing in two waves against two waves. The piecemeal question from `o1-08`, from the defender's side and under a clock |
| 10 | `o5-10-batajnica` | the full night | **night** | `wx fixture` | 5 MiG-29 (matching the anchor's five), mixed degradation, staggered scrambles | 12 F-16 (three flights: 8 strike, 4 escort) | 2 `target_hard` (shelters) + 1 `target_hard` (runway) + 4 `target_soft` | reported as: targets surviving, strikers turned away, defenders recovered — **three numbers, no single verdict** | The anchor's own night. **Can a force that historically achieved nothing measurable achieve anything measurable here** — and if it can, is that a finding about doctrine or a sign that Blue is flown badly? |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| MiG-29 | flyable module | **yes** | the defender; the degradations the anchor describes are expressible as `set` keys today, which is unusual and valuable |
| F-16 | flyable module | **yes** | stands in for the F-15C escorts and the strike aircraft alike — a **declared** substitution that changes the air-to-air character substantially (the F-15C's radar and AIM-7/AIM-120 mix is not the F-16's) |
| F-15C | flyable module | **no** (`C7`) | the aircraft that actually shot down every MiG in both anchors |
| F-117 / cruise missile | air, low-observable / one-way | **no** (`C7`, roadmap R7) | half of what actually attacked Batajnica |
| Hardened aircraft shelter | ground | **yes** (`target_hard`) | |
| Fuel installation, hangars, radar | ground | **yes** (`target_soft`) | |
| **Runway as a damageable, closable object** | ground, stateful | **no** (`C17`) | a runway can be declared as `target_hard` and destroyed — and then nothing changes, because no unit needs it |
| Airfield air defence (SAM, AAA) | ground, emitting + shooting | **no** (`C1`) | the point defence that made both anchors' airfields dangerous |
| GCI / sector control | infrastructure | **static text only** (`C6`) | |
| Divert field | second runway | **no** (`C17`) | one runway per mission |

### 5. What must be true before mission 1 can fly

`o5-01`, `o5-02`, `o5-03`, `o5-04`, `o5-05`, `o5-06`, `o5-08` are buildable **today** — seven of ten,
because the campaign's subject is a *posture*, and posture is spawn data plus `set` keys. Missions 1
and 2 are the pair to build first: they differ by one `spawn` line and they produce the campaign's
calibration number.

---

## State

**Nothing built.**

What exists and carries it: ground start with the runway geometry and the full takeoff phase machine;
per-unit spawn with independent timing expressible as an offset position on the route; all four
degradation switches (`n019_mode`, `n019_emission`, `rwr`, `fuel_pct`); the GCI brief chain; the
declaration-based expected-loss rule that lets a defender's death be somebody's declared objective
without ending the run; and the flight sort.

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C12` | **no `protect` / `deny` objective** | the campaign's success condition. Today "the shelter survived" can only be read as the attacker's failed `kill`, which works but is backwards and unreadable at a glance |
| `C17` | **a runway cannot be closed, an airfield has no state, there is no divert field** | mission 7 entirely; the reason an airfield is attacked in the first place |
| `C1` | **no airfield air defence** | both anchors' bases were defended; here they are not |
| `C7` | **no F-15C, no F-117, no cruise missile** | the attacker is an F-16 stand-in for three different threats |
| `C6` | **no live controller** | the scramble order itself, and the vector that would follow it |
| `C2` | **no time of day** | **every mission in this campaign is a night mission** — Batajnica's five scrambled in the dark |
| `C0` | **no campaign layer** | an air force being destroyed over three nights is precisely a campaign-layer subject: losses must carry |
| `D3` (`duels.md`) | **the pilot does not use the IRST** | mission 3 — a radarless MiG-29 should still be a threat, and today it is not |
| `C15` | **no scramble timing mechanism** | staggering is done by giving units different spawn positions, which is a workaround with a different geometry |
| `C3` | **no visual acquisition** | a night defensive intercept ends at visual range in reality |

### The honest headline

**O5 is the campaign where FlightBox's verdict vocabulary breaks down most visibly.** Seven of its ten
missions will run today, and none of them can *say* what it measured: the defender's whole purpose is
to make something not happen, and `objective` has no word for that. `C12` is therefore this
campaign's first deliverable, and it is a small one — a `deny`/`protect` objective checked against the
same roster the `kill` objectives already use.

---

## Knowledge

### 1. The anchor with its sources

- **Batajnica and the 127th.** [NATO bombing of Yugoslavia (Wikipedia)](https://en.wikipedia.org/wiki/NATO_bombing_of_Yugoslavia)
  [T4] — Batajnica as the home of the Yugoslav air force's only MiG-29 unit, the 127th Fighter
  Aviation Squadron "Knights"; the five MiG-29s scrambled on the night of 24/25 March; the Batajnica
  pair of Nikolić and Kulačin, Nikolić shot down by USAF Capt. Mike Shower, Kulačin evading several
  missiles while fighting his own malfunctioning systems; the two further losses on 26 March
  (Radosavljević, Perić) to F-15Cs.
  Corroborating and adding the serviceability picture (outdated avionics, degraded radar, limited
  missile stock): [The Serbian Air Force against the 1999 NATO "Allied Force" operation (Defence
  reDefined)](https://defenceredefined.com.cy/the-obscure-files-of-an-anniversary-or-david-vs-goliath-the-serbian-air-force-against-the-1999-nato-allied-force-operation/)
  [T4]. A contemporary claim review exists in the community record —
  [Official review of Serb MiG-29 kills on 26 March 1999 (Google Groups archive)](https://groups.google.com/g/rec.aviation.military/c/JpXMVIrCenY)
  [T4] — **not retrieved on this pass**, flagged in [`PROGRESS.md`](PROGRESS.md).
- **The Iraqi parallel.** [MiG-29 Fulcrum: specs, history, combat record (e3aviationassociation)](https://e3aviationassociation.com/aviation-articles/mig-29-fulcrum-specs-history-combat-record/)
  [T4] — ≈40 aircraft, six lost to F-15Cs in the opening days, none scoring an air-to-air kill, and
  the explanation quoted in §1: Soviet doctrine assumed tight ground control and coordinated
  operations, but the aircraft were flown alone, "without the tactical architecture the design
  assumed".

### 2. Where the sourcing is contested or thin, and it is stated

| Thing | Status |
|---|---|
| Yugoslav MiG-29 losses, total and by cause | **[DISPUTED] across the literature** — the sources retrieved name individual engagements rather than a total, and Serbian and NATO accounts differ on several of them. **No total is stated in this file**, and the campaign uses none |
| Yugoslav claims against NATO aircraft | **[DISPUTED]** and not carried here at all |
| Which specific systems were unserviceable on which airframe on which night | **not sourced.** The degradation pattern in missions 3, 8 and 10 is `[SET]` — a plausible spread built from the general statement, and labelled as such |
| The Batajnica ground defences | **not sourced** |

### 3. Why the three GCI experiments must be run together

`o1-02` (Bekaa, no controller), `w3-08` (Desert Storm, no controller) and `o5-04` (airfield defence,
no controller) are **the same one-line experiment in three different geometries**. The anchor for two
of them explicitly names ground control as the missing ingredient. If the three runs disagree — if
deleting `brief_gci` costs the MiG a great deal in one theatre and nothing in another — then the
answer is a property of the *geometry*, not of the doctrine, and that is itself the finding.

FlightBox's own mechanism makes the experiment unusually clean: without the brief the N019 never
receives its scan elevation or its ZONE third, so the aircraft is not "less informed", it is
**pointing its radar at nothing in particular**. The measured quantity is therefore not a modifier but
a detection time, and it is comparable across all three campaigns without any normalisation.

### 4. What "achieving something" would have to mean, measurably

Today the campaign can compute three numbers from existing telemetry and events:

| Number | Where it comes from | What it means |
|---|---|---|
| **Targets surviving** | the attacker's `objective kill unit` verdicts per ground unit | the primary measure — did the ordnance arrive |
| **Strikers that never released** | `sms RELEASE` absent for a unit that reached its target area | a striker turned away is a defensive success even if it lives |
| **Defenders recovered** | `UNIT_RESULT` per defender | the third-order measure, and the only one `objective survive` can express |

The middle one is the interesting one and it needs no new engine feature — only a mission design in
which the striker's release is *interruptible*, and a reading rule in the mission header that says so.
That is the cheapest useful thing this campaign can produce before `C12` closes.
