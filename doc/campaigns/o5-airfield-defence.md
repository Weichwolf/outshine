# O5 — Airfield defence against a strike package

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Yugoslav MiG-29 operations from Batajnica, 24–26 March 1999, with the Iraqi 1991 case as the corroborating parallel | §Knowledge 1, cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare and what the MiG-29 module does | [`../modules/mig29/module.md`](../modules/mig29/module.md), [`../missions/verdict.md`](../missions/verdict.md), [`../missions/sensors.md`](../missions/sensors.md), [`../formation.md`](../formation.md), [`../duels.md`](../duels.md) |

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

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

> **SUPERSEDED 2026-07-29 by the build, and kept because the sentence above is what commissioned `C12`.**
> The vocabulary now expresses the **first and the third**: `deny release` is O5's primary measure and
> `protect` states the field's survival as a verdict. The second — broken timing — is still a telemetry
> read, and O5 read it: the terminal zone's first entry is t = 198.6 s with and without the jammer, to
> the tick. What replaced this as the campaign's central gap is **that the scramble itself cannot be
> declared** — see `## State`, defect 1.

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

> **SUPERSEDED 2026-07-29 by the build.** Ten of ten were built and ten ran; what the ten slots actually
> hold is one fighter baseline + four single-change variants + one control pair + a three-night chain,
> and the two spec missions that did **not** fit are named in the `.fbc` header (mission 7, the runway,
> because `C17` makes it a target nothing can kill whose death changes nothing; mission 6, saturation,
> because the first wreck ends the run and a six-ship raid is measured for exactly as long as a two-ship
> one). And the sentence *"they differ by one `spawn` line"* did not survive contact: **missions 1 and 2
> cannot differ by a spawn line, because the format cannot spawn an aircraft on alert at all.**

---

## State

**BUILT AND FLOWN, 2026-07-29 — the third of the ten campaigns to exist as files.** Ten `.fbm` in
`sim/missions/o5-*.fbm` plus `sim/campaigns/o5-airfield-defence.fbc`, run as a campaign, replayed step
by step, and measured. **No file under `sim/src/` and none under `sim/assets/` was touched**
(`git status --porcelain sim/assets` empty, `verify-models` *"4 upstream-backed model path(s) match …
(1 declared delta(s), 34 FlightBox-own)"*, `verify-layers` unchanged word for word), so the 160
pre-existing missions are byte-identical **by construction rather than by comparison**: the binary is
the one that was already there.

### The arena, and the two disclosures that belong in the first paragraph

Batajnica at **44.93000 N 20.26000 E** `[SET]`, flown under `--elev const` on a **0 m datum** (the real
field is ~80 m) with **no terrain masking** (`C4`), so the Fruska Gora and every mask a Serbian battery
used do not exist. The package ingresses due east along 44.93 N from 19.60000 (52.0 km out) at 5 000 m
and 450 kt; the field's point defence is a P-18 node on the field, an S-125 7.9 km west, a 2K12 5.7 km
west and a ZSU-23-4 0.7 km west. Scale: 1° of longitude = 78 800 m [DERIVED].

**Two layers cannot both be live in one file, and that is this campaign's structural finding rather
than a layout preference.**

1. A FlightBox SAM site has **no IFF interrogator** and an `FBRadarContact` carries no identity, so a
   battery engages the **nearest firm track** whoever it belongs to. An airfield is exactly the geometry
   in which the friendly aircraft are the nearest track — O1 could lay its CAP outside its own umbrella,
   **O5 cannot, because the defenders live under it.** MEASURED, in this campaign's own standalone
   sortie 09: of the belt's first three launches, **all three go east** (`brgDeg` 116.5 / 90.6 / 91.0)
   at the field's own MiG-29s, and only the next four go west (258.6 / 271.5 / 272.4 / 258.8) at the
   package. Sortie 10 standalone: **3 of 6** (81.9 / 90.6 / 91.0).
2. `FBMissionRunner` ends the run at the first flight-monitor K.O., and a mission-killed F-16 falls from
   5 000 m in about **122 s**. An air engagement and a bomb run in one file are therefore one measurement
   plus a truncation. The budget is arithmetic: with a 34 s bomb time of flight, a defender may kill at
   most **88 s** before the release point and still leave the strike observable.

The campaign's answer is the one a real airfield takes and the only one the format can express: the
missiles are **held while the fighters are up** (`set emcon hold <offS> <onS>`) and free when they are
not. Sorties 01–05 measure the fighter layer with the belt briefed quiet; 06–07 measure the missile
layer with nothing friendly airborne; the chain lets the campaign's own attrition decide which layer is
left. **O5 never measures both at once, and no statement below claims it did.**

### The ten sorties, their fingerprints and their answers

Campaign exit **3** (the worst step's; every sortie is a measuring rig whose own header says the verdict
is the telemetry). Campaign fingerprint under `--elev const`:
`f59fc642c86ccecd2691371c3c4c2dd41d6f04c891702316b85d32badb7070f4`. **The FRAME round of 2026-07-29 moved it again** — the spawn state is now the trimmed airframe's own rather than position only, so the first 0.01 s of guidance moved in every mission with an airframe (`sensors.md` §10, item 24). Post-frame-round value, both criteria re-measured and still holding: `6d04abb45294e8e6b5ea28e939081dcda562067f756020a7998b4b37210aafad`.

| # | Mission | ctrl | exit | fingerprint | The answer to its one tactical question |
|---|---|---|---:|---|---|
| 1 | `o5-01-cap` | — | 3 | `35ed72fcab3aa924` | **The fighter layer denies half a two-ship and no more.** Contact t = 21.0 s at 25.58 nm, both R-27R away at t = 66.2 — and **both onto the same striker** (the sort does not split, exactly as in `o1-02` and `o4-10`): arrivals **3.23 m** and **10.50 m** against a 13.8 m fuze, `m1strb` dead at t = 90.2. The survivor is unmolested, releases at t = 160.7 and destroys the S-125 at t = 195.0 with a **45.87 m** Mk-84. `deny release` **unmet** |
| 2 | `o5-02-scramble` | 01 | 3 → **0** | `e3e4b10c79c7cf6f` → **`9c4dff16ea1f33b2`** (2026-07-29, frame round) | **THE SCRAMBLE COSTS THE ENTIRE ENGAGEMENT — and the mechanism is a defect, not a delay.** Zero radar contacts in 700 s over a **726 m** closest approach; zero shots; **both** strikers release and both aim points die. The cause is measured below (§The three defects, #3): the pair climbs at 25.0 m/s at +5.9…+6.1° of pitch and the GCI's scan-elevation entry is a **world-frame** angle posted to a **body-frame** antenna command | **RE-MEASURED 2026-07-29 AND THE ANSWER INVERTS, because the defect this row FOUND is now fixed** (a world-frame scan elevation in a body-frame antenna command — [`../pilot.md`](../pilot.md) §Gaps 2.15). Same file, same three GCI calls, entered elevation **+2.891 → −2.754°**: first contact **t = 48.0 s at 25.70 nm** where there had been none in 700 s, four R-27R away from t = 105.7, **BOTH strikers shot down** (t = 123.2 and t = 132.1), and `kill team friendly` + `deny release team friendly` + `survive` all met on both interceptors. **A SCRAMBLED PAIR DEFENDS THE FIELD; what the row measured before was the frame error, not the scramble.** The pre-fix attribution set (late/low/slow, above) is therefore a measurement of the DEFECT and not of the tactic, and the campaign's cost-of-a-scramble question is OPEN again.
| 3 | `o5-03-no-gci` | 01 | 3 | `fa3de2437bd7ab45` | **The controller is worth six seconds of the wingman's first look and nothing else.** Lead's first contact identical (t = 21.0), wingman's **24.0 → 30.0 s**; both shots at the identical t = 66.2; same kill, same release, same aim point, run length 216.7 → 216.6 s. On a collision course, confident blindness and full control are the same state — **the third campaign to say so, after `o1-03`, and the second geometry in which deleting the brief changes no outcome** |
| 4 | `o5-04-no-radar` | 01 | 3 | `ee634cab56b128ce` | **Zero, and the zero is the point.** No contact, no shot, `irst_contacts` = **0** for the whole run — the KOLS never had anything to offer and nothing would have read it if it had (`duels.md` D3). Both strikers release. A radarless MiG-29 is not a degraded interceptor in this tree, it is **an absent one**, and that prices the Batajnica serviceability claim exactly |
| 5 | `o5-05-escorted` | 01 | 3 | `8854dae7dda355a8` | **The defender goes for the escort, and all four rounds go to the same aeroplane.** First contact on the escort at t = 9.1 (11.9 s before the strikers appear), four R-27R away, `m5esca` dead at t = 75.8 at **3.86 m**; the second escort is untouched and the strikers are never engaged. **`deny release` reads MET and this campaign refuses the reading**: the run ended 35.6 s after the hit with both strikers still 27 km short of their release point. What this sortie measures is the SORT and nothing downstream of it |
| 6 | `o5-06-belt` | — | 1 | `7fc2aa31eca722b7` | **The missile layer alone denies one striker of three and then loses the field.** `net JOIN` ×3 at t = 3.9, 12 `net CUE`, first firm track t = 39.0, **7 `site LAUNCH`** (4 × V-601 at 28.1/27.5/20.5/19.9 km, 3 × 3M9 at 16.2/10.6/11.2 km) — **all seven at the same aircraft**, one engagement channel. `m6stra` killed at t = 139.3 by a **2.31 m** arrival against a 10.0 m fuze (the V-601 salvo: 6.87 / 6.09 / 2.31 / 2.20 m). The two survivors release; `protect unit m6ful` is **violated at t = 226.1** and that timestamp is the verdict. `zone_umbrella_s` = **188.2–188.5 s** per striker: an ingress with no way round |
| 7 | `o5-07-jammed` | 06 | 1 | `fee713cf08e8509a` | **One line takes the whole missile layer away.** `set jam_comm_m 90000` on the lead striker: **0 `net JOIN`, 0 `net CUE`, 0 `site LAUNCH`, 0 detonations** against 3 / 12 / 7 / 5 — the belt never comes up at all (O1's `net-jam-start` case, not its `net-jam-late` one). Ordnance on the field **2 → 3** Mk-84 and the striker the belt killed comes home. **And it buys ordnance, not time:** the terminal zone's first entry is t = **198.6 s in both runs**, to the tick |
| 8 | `o5-08-night-one` | — | 3 | `f8e818f931519561` | **CHAIN HEAD. Nobody dies and the field loses its eyes.** 3 R-27R + 3 R-73 against 2 AIM-120, no aircraft on either side made combat-ineffective, and two Mk-84 destroy **`batnod` (the P-18) and `batsa3` (the S-125)**. Against the anchor this is the wrong answer — the 127th lost two aircraft that night — and it is reported rather than tuned away |
| 9 | `o5-09-night-two` | — | 3 | `4db885e9157b58c3` | **CHAIN. The missiles were released and there were no missiles left to release.** The carry drops `batnod` and `batsa3`, so the surviving 2K12 has no node: `net WCS unit=batsa6 state=hold netState=SILENT correlated=0 effect="launch inhibited" rangeM=6591.55` at t = 33.0 — **it holds a firm track on one of its own fighters at 6.6 km and the missing node is what stops it firing.** 0 launches against 7 standalone. `knight1` is lost to an AIM-120 |
| 10 | `o5-10-batajnica` | — | 3 | `2ed7d6b2a84e3a42` | **CHAIN TERMINUS. Three numbers: 4 of 6 targets surviving, 0 of 4 strikers denied, 4 of 5 defenders recovered.** Four Mk-84 released, `bathgr` and `batful` destroyed, `knight3` lost, 0 `site LAUNCH`. **Standalone the same file is a different night**: the belt fires 6 times, no bomb is released at all and the run ends at t = 116.6 on a dead escort. The difference is the campaign |

### The carry, where it lands, and what it was worth

`carry units ground stores` — **O5 does not narrow it**, for O1's reason and against O4's: attrition is
the subject. Sorties 01–07 are seven pairwise-disjoint casts (`m1*`…`m7*`) that carry nothing in or out —
**including their ground callsigns**, which is the one place O5 had to be stricter than O1: O5's field is
attacked in every sortie, so a shared belt would let sortie 01's damage leak into sortie 02 and destroy
the experiment. The chain is **08 → 09 → 10** over one defending roster (`knight1`…`knight5`) and one
field (`bat*`); the attacker does not carry, because each night's package has its own callsigns.

| Entering | `campaign CARRY` lines |
|---|---|
| step 09 | 6 × `action=stores` (knight1 r27r; knight3 r27r + r73; knight4 r27r + 2 × r73) · 2 × `action=drop` (`batnod`, `batsa3`) |
| step 10 | 8 × `action=stores` · 1 × `action=drop` on an **aircraft** (`knight1`) · 2 × `action=drop` on the field |

Campaign totals: `ATTRITION unitsFriendly=4 unitsHostile=2 groundFriendly=0 groundHostile=15`,
`EXPENDED mk84=17 r27r=12 r73=9 aim120=7`.

**And the honest measurement of what the carry was worth** — the same file run STANDALONE with no
carried state against the same file as a campaign step:

| | 09 standalone | 09 in campaign | 10 standalone | 10 in campaign |
|---|---|---|---|---|
| `site LAUNCH` | **7** | **0** | **6** | **0** |
| rounds fired at the field's OWN fighters | **3** of 7 | — | **3** of 6 | — |
| Mk-84 released | 2 | 0 | **0** | **4** |
| ground positions/installations destroyed | `batsa3`, `batful` | — | — | `bathgr`, `batful` |
| aircraft lost | `n2esca` | `knight1` | `n3escb` | `knight3` |
| run length | 536.2 s | 121.3 s | 116.6 s | 486.1 s |

> **The campaign's headline, and it is the `C22` prediction arriving as a number: one Mk-84 on the P-18
> on night one is worth the field's entire missile layer on nights two and three.** The node dies at
> t ≈ 900 s of sortie 08; the carry drops it; the 2K12 that survives both later nights holds firm tracks
> and fires **nothing**, because `autonomy hold` without a node is a battery with a radar and no orders.
> The attacker's first bomb also bought the defender's fratricide protection — the three rounds that
> would have gone east at its own MiGs are the three rounds it never launched.

### The scramble, attributed rather than argued

Sortie 02 changes three things in one lever (13.89 km of position, −7 000 m of altitude, −100 kt) and
says so in its own header. Four attribution runs, none of them one of the ten, separate them — the same
discipline O4 used on its weather sortie:

| variant | spawn | first MiG contact | R-27R | Mk-84 on the field |
|---|---|---|---:|---:|
| `o5-01` baseline | 20.35000 / 9 000 m / 450 kt | **t = 21.0 s** | 2 | 1 |
| **a** — late only | 20.52640 / 9 000 m / 450 kt | t = 48.0 s | 3 | 1 |
| **b** — low only | 20.35000 / 2 000 m / 450 kt | t = 24.0 s | 4 | **0** |
| **c** — slow only | 20.35000 / 9 000 m / 350 kt | t = 21.0 s | 4 | 1 |
| `o5-02` — all three | 20.52640 / 2 000 m / 350 kt | **never** | **0** | **2** |
| **d** — all three, GCI deleted | ″ | **never** | **0** | 2 |

**No single component blinds the pair; all three together delete the engagement, and deleting the
controller does not restore it.** The effect is superadditive and its mechanism is defect #3 below.

### The three defects this campaign found, none of them fixed here

Rule: *a campaign is the first thing that fires a subsystem in anger.* O1 found one and a stack under it;
O5 found three, all on FlightBox's side of the seam, all reported rather than built around.

| # | Defect | The measurement that pinned it |
|---|---|---|
| **1** | **The alert scramble is not expressible.** `set task <combat>` sets the pilot's phase AT SPAWN and `FBPilot`'s phase machine has **no transition from `Route` into `Intercept`**, so a `spawn ground` unit with a combat task never runs Preflight/Takeoff/Climb at all | with a `runway` line the MiG steers off the strip and FAILs *"touchdown off the assigned runway"* at **t = 11.1 s** (59.3 m of lateral error 357 m down the roll); without one it rolls, lifts and cartwheels — `monitor KO … reason=ATTITUDE_CONTACT` at **t = 35.4 s**. **This is the parameter §Spec 2 calls the campaign's most important single one** |
| **2** | **FIXED 2026-07-29 — and the finding stands as the reason the round happened.** **WAS:** `modules/air/FBAirModule` composed **no fire control**, so `FBState::FireControl` was never written and all three of `FBPilot`'s employment gates (`InterceptCommands`' `inParams`, `BfmMissileShot`, the gun's `GunTolDeg`) stayed shut for every one of the eighteen rows | an `f15c` with 4 × AIM-120 designated at **t = 8.6 s at 18.64 nm**, held `eng_locked = 1` from t = 12.1 to t = 40.1 while closing **18.6 → 8.8 nm**, and never posted a `WeaponRelease`; its telemetry carried **no `fc_*` column at all**. **NOW:** `modules/air/FBAirFireControl` ([`../modules/air/module.md`](../modules/air/module.md) §Spec 12, A13 closed) gives the ten armed rows a launch zone, a target estimate and a gun solution, and the nine `fc_*` columns exist so the same hole cannot hide in an absence again. First catalogue kill measured: `air-eagle-amraam.fbm`, AIM-120 at 25.35 km, **0.905 m miss, `damage KILL`**. `flight-model-recipe.md`'s promotion gate still measures the DECK only, so **`ACCEPTED` still means "accepted as a flight model" and never "accepted as a combatant"** — that half is unchanged and stays stated. **What O5 still cannot have** is a catalogue GUN engagement (A15: a generated deck's close-combat law cannot close and does not recover) and the two pure-cannon rows (A14) |
| **3** | **The MiG-29's GCI scan-elevation entry is a WORLD-frame angle posted to a BODY-frame antenna command.** `FBMig29Pilot` computes `elDeg = atan2(g.AltM − ownAlt, g.RangeM)` and posts it to `RadarSlewEl`; `FBPilot`'s own uncued search law posts the same geometry **minus `st.pitch`** ( *"der eigene Nick macht daraus ein Kommando statt einer Konstante"* ). On a level CAP the two agree inside the command deadband, which is why no committed mission has ever exposed it | sortie 02: the pair climbs at **25.0 m/s** at **+5.86…+6.13°** of pitch; the raid sits at **−3.1…−4.2° in the body frame** at 57 → 25 km, well inside the N019's ±6.0° RAD bar — and the antenna is commanded to **+2.83°** where the pilot's own law would command **−3.03°**, leaving the bar's lower edge at −3.17° and the raid 0.5–1.0° outside it for the whole closure. **Zero contacts in 700 s over a 726 m closest approach** |

### What holds the airfield, and what is free

The campaign's own question, answered in the currency of ordnance that did not arrive. "Bombs on the
field" counts Mk-84 released against the field's installations in each sortie.

| Layer | Setting | Bombs on the field | What it moved |
|---|---|---:|---|
| — | nothing (sortie 02/04's condition) | **2 of 2** | the reference: an undefended field takes everything |
| **fighters, on station** | `o5-01` | **1 of 2** | **half the package.** The most any single layer achieves in this campaign |
| fighters, GCI deleted | `o5-03` | 1 of 2 | **nothing** — 6 s of one wingman's first look |
| fighters, radar off | `o5-04` | 2 of 2 | **the whole layer.** A radarless MiG-29 is an absent one |
| fighters, scrambled late | `o5-02` | 2 of 2 | **the whole layer**, and by defect #3 rather than by geometry |
| fighters vs an escorted package | `o5-05` | not measurable | the sort takes the escort and the strikers are never engaged |
| **missiles, netted and cued** | `o5-06` | **2 of 3** | **one striker of three**, at the cost of 7 rounds and the S-125 |
| missiles, net jammed | `o5-07` | 3 of 3 | **the whole layer**, on one line |
| **the node** | chain 08 → 09/10 | 0 → 4 | **the whole layer, for two nights, from one bomb** |

**What holds the airfield: a fighter pair that is already on station, and nothing else in this tree comes
close.** It denies one striker of two, and it does it with a radar and a magazine — not with a
controller, not with a warning receiver, not with the missiles at its feet.

**What is free, and the list is long and specific:**

- **The controller.** Six seconds of one wingman's first look, no outcome. Three campaigns now agree
  (`o1-02`, `o1-03`, `o5-03`) and the three geometries are head-on, 45° and high-to-low.
- **The night.** Declared in every file, and O4 measured its worth: six visual telemetry columns out of
  184, none read by any pilot. No sortie of O5 is different from its daylight twin in any flight, sensor,
  weapon or engagement column.
- **The inner gun.** `objective no_fire` on the ZSU-23-4 is **met in every sortie that publishes it**
  (01, 05, 10) and it is met for a reason that has nothing to do with discipline: the package runs in at
  5 000 m and the Shilka's ceiling is 1 500 m. The false positive is named, and what the line actually
  measures is **the altitude decision** — which is why a package accepts SAM exposure to stay out of the
  flak.
- **The hardened shelters and the runway.** They stand in every one of the ten files and **nothing in
  the package can kill them**: a Mk-84 CCRP delivery on this arena lands 38–51 m out and a `target_hard`
  needs ~8 m (`attack-hardened.fbm`). Add `C17` — a runway has no state and cannot be closed — and
  "the shelters survived" is never reported here as a defensive success.
- **The belt, the moment its node is gone.** Not free in itself — it denies a striker in `o5-06` — but
  worth exactly zero for two consecutive nights after one bomb, because `autonomy hold` without a node
  is a battery with a radar and no orders.

**And the one thing that is worse than free:** on this geometry a live belt fires its first three rounds
at its own fighters. The only reason O5's chain does not lose aircraft to its own missiles is that the
attacker destroyed the node that would have authorised the launches.

### Both determinism criteria, measured

Under `--elev const`, read out of `campaign-summary.txt` rather than assumed:

| # | Criterion | Result |
|---|---|---|
| **1** | 3 repetitions × `--threads 1/2/4` produce one campaign fingerprint | **9 runs, 1 fingerprint** `f59fc642c86ccecd2691371c3c4c2dd41d6f04c891702316b85d32badb7070f4`, exit 3 in all nine; **re-measured after the frame round of 2026-07-29: 9 runs, 1 fingerprint** `6d04abb45294e8e6b…`, exit 3 in all nine |
| **2** | every step's per-mission fingerprint equals that mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included, on the first attempt; **10/10 again after the frame round of 2026-07-29** |
| **1 — re-run 2026-07-30** | the same criterion under the branch-order change of `b433950` ([`../pilot.md`](../pilot.md) §7.4a) | **9 runs, 1 fingerprint** `9c635c594f7084206ec1193762c32b418f81c7b7691a0a42c97039c6fa0d2e59`, `--elev const`. **The value above is kept with its date; this is the current one.** Step exits `3 0 3 3 3 1 1 3 3 3` — unchanged |
| **2 — re-run 2026-07-30** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 | | `5b076c5851cfa250 9c4dff16ea1f33b2 ec0ad60b2141913f 16db265b11e7f2b0 a91f759c1918789e f1ce0b9332b3a429 d6975683e30ca058 70f9e7eeebae4a51 aae4d0a559b16e67 230df9b36893840d` |

**One process deviation, stated:** `INDEX.md`'s rule 5 says run `fb_campaign_verify.py replay` after the
FIRST mission rather than after all ten. It was run after all ten. It passed 10/10 on the first attempt,
so nothing was lost — but the rule exists to bound the damage when it does not, and this round did not
earn its luck.

### Conservation, and one control that had to be run

**Nothing to compare, and that is the strongest form of it.** `git status --porcelain` lists eleven new
untracked files and **no modified one**: ten `sim/missions/o5-*.fbm` and one `sim/campaigns/*.fbc`. No
`sim/src/` file, no tool and no asset was touched, so the binary that flew O5 is the binary that flew
everything before it and the 160 pre-existing missions are byte-identical by construction. Gates:
`make core-lib gym native wasm` warning-free; `verify-layers` *"297 files, 805 internal include(s), 12
layers — no upward include, 3 restricted header(s) respected, 6 registry reader(s) inside the perception
boundary, 284 file(s) in their layer's namespace (5 C-island file(s) exempt)"*; `verify-models` *"4
upstream-backed model path(s) match assets/MODEL-DELTAS.md (1 declared delta(s), 34 FlightBox-own)"*;
eight harnesses rc = 0 (`test-monitor` `test-fdm` `test-corner` `test-gun` `test-missile` `test-weather`
`test-mig29` `test-air`).

**The control the objective lines needed.** Sorties 06 and 07 declare `no_fire` and `protect` on units
that also fly the experiment, so the question *"did the declaration change what happened?"* is real.
Measured: `o5-06-belt` with the `objective no_fire` line removed produces **20 of 20 `telemetry*.csv`
byte-identical** to the campaign step. An objective is a declaration, not a behaviour — it can change
what a run is CALLED and, when it is violated, when a run STOPS, and nothing else.

### What the tree already had, and this campaign consumed unchanged

| Already built | Where | Used for |
|---|---|---|
| the four `C12` objective kinds + `avoid zone` | [`../missions/verdict.md`](../missions/verdict.md) | `deny release` (01–05, 08–10), `protect` (06, 07), `no_fire` (the field's gun), `avoid zone` (every striker) |
| the connected air defence: `net`, cue, `wcs`, `autonomy`, `net WCS … "launch inhibited"` | [`../air-defence-network.md`](../air-defence-network.md) | the field's point defence in all ten sorties |
| the briefed emission plan `set emcon <mode> <offS> [<onS>]` | ″, round `C26` | the weapons-hold that makes an airfield defence expressible at all |
| `set jam_comm_m` | ″ §6 (`C24`) | sortie 07, one number against sortie 06, at O1's own 90 000 m |
| the ground-launch fix of 2026-07-29 | [`../modules/ground/module.md`](../modules/ground/module.md) §4.1 | **every launch in this campaign.** On the pre-fix binary sortie 06's seven rounds would have hit their own launchers |
| the campaign layer, its overlay and its two fingerprints | [`../missions/campaign.md`](../missions/campaign.md) | the three-night chain and both criteria |
| `mission OBJECTIVE` per declared objective (`E1`) | [`../missions/verdict.md`](../missions/verdict.md) | the whole reading of this campaign |

---

## Gaps

**Re-checked against the tree on 2026-07-29, not trusted** (`INDEX.md`'s inherited rule). Six of the
twelve rows below were closed by other rounds between the spec and the build, and one that was listed as
present turned out not to be.

| ID | What is missing | Blocks here |
|---|---|---|
| ~~`C12`~~ | **CLOSED and flown.** All four kinds plus `avoid zone` are declared in O5's files; `deny release` is the campaign's primary measure | — |
| ~~`C1`~~ / ~~`C22`~~ / ~~`C23`~~ / ~~`C24`~~ | **CLOSED and flown**, and the ground-launch fix of 2026-07-29 made them able to move an outcome. The field's point defence is a real net with a killable node — and sortie 08 kills it | — |
| ~~`C2`~~ / ~~`C0`~~ | **CLOSED and flown.** Three declared nights, one `.fbc`, both determinism criteria | — |
| **NEW — the alert scramble** | `FBPilot` has **no `Route` → `Intercept` transition**, and `set task` applies AT SPAWN, so a ground start plus a combat task destroys the aircraft (§State, defect 1) | mission 1's whole subject. Expressed as a spawn DISPLACEMENT with its arithmetic and four attribution runs |
| ~~**NEW — no catalogue aircraft can shoot**~~ | **CLOSED 2026-07-29.** `modules/air/FBAirFireControl` (§State, defect 2). The F-15C now employs both of its rounds and the AIM-7/AIM-120 pair is the one thing this campaign wanted from that row: with Sparrows the escort is tied to its target for the whole time of flight and cannot answer a launch warning, with AMRAAMs it is free 8.25 s after the shot | **what is still blocked:** a catalogue row cannot fight with its cannon (A15) and the two pure-cannon rows cannot aim one at all (A14), so O5 may score catalogue MISSILE engagements and no catalogue gun engagement |
| **NEW — the GCI elevation frame** | world-frame angle into a body-frame antenna command (§State, defect 3) | any interceptor that is not in level flight when its controller calls — i.e. every scramble |
| `C17` | **a runway cannot be closed, an airfield has no state, there is no divert field** | the spec's mission 7, **not built**: it would be a `target_hard` nothing in the package can kill whose death changes nothing. "Defenders recovered" in sortie 10 means *combat-effective at the end*, never *landed* |
| `C4` | **no terrain masking** | Batajnica sits in the Pannonian plain, so this campaign loses less to `C4` than O1 or W4 — but a Serbian battery's whole survival tactic was terrain, and there is none |
| `C6` | **no live controller** | the brief is written against the CAP's altitude and a scrambling aircraft is by definition not at it. With a live controller defect 3 would still be a defect, but it would be recoverable |
| `D3` (`duels.md`) | **the pilot does not use the IRST** | sortie 04 measured it at exactly zero: `irst_contacts` = 0 for 700 s |
| `C15` | **no package coordination, no scramble timing** | the chain's three nights are three spawn sets, not a tasking order |
| `C3` | **visual acquisition contributes nothing at night** | every merge in this campaign is eyeless, and the campaign is entirely nocturnal |
| **the runner's first-wreck rule** | not a gap ID, but the constraint that shaped every file: `FirstFlightKo` ends the run ~122 s after a kill at 5 000 m | sortie 05 cannot report a denial it earned; the chain's nights are each measured for as long as their first casualty takes to fall |
| the belt's magazine does not carry | `set rounds` is not `set store` and the campaign layer's store carry is keyed on `set store` ([`../missions/campaign.md`](../missions/campaign.md) §4) | a battery that shot itself dry is full again the next night. Inherited from O1, unchanged |

### The honest headline

**The vocabulary was the easy half.** `C12` closed before this campaign was built and all four of its
words are flown here — the defender's success really is expressible now. What replaced it as O5's worst
hole is smaller and harder: **the campaign's declared most-important parameter, the scramble, cannot be
declared at all**, and the substitute for it is blinded by a coordinate-frame error in the one entry
chain the whole eastern half of the tree depends on. O5 was built to measure a posture and it measured
three defects instead — which is what a campaign is for, and the reason the other seven should expect a
stack rather than a finding.

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
