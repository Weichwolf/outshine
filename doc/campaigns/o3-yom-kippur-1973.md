# O3 — Yom Kippur 1973: ground attack in contested airspace

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of the opening Arab air operations of 6 October 1973 | §Knowledge 1, cited and tiered |
| **FlightBox sources** | what the MiG-29 module can and cannot do on a ground-attack sortie | [`../modules/mig29/weapons.md`](../modules/mig29/weapons.md), [`../modules/mig29/module.md`](../modules/mig29/module.md), [`../missions/weapons.md`](../missions/weapons.md), [`../weapons.md`](../weapons.md) |

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

### Temporal honesty — and the harder problem underneath it

**The MiG-29 was not in 1973.** The type entered service a decade later; the opening strikes were
flown by **MiG-17, Su-7, Su-20, MiG-21 and Hawker Hunter** [T4]. Bekaa's substitution rule applies
again — *the situation is the anchor, not the serial number* — and again it makes the flying side
**stronger** than history.

But this campaign has a second, sharper problem that no other campaign in the set has:

> **The FlightBox MiG-29 cannot fly a ground-attack mission at all.**
>
> `set task attack` is **closed** on the `mig29` module, and for a stated reason rather than a missing
> weapon: the 9-12 carries no guided air-to-ground store, and its unguided delivery is a *director the
> pilot flies* rather than a *release moment he reacts to* — so `FBMig29FireControl` publishes no
> CCIP/CCRP block, and the attack phase has no cue to pickle on
> ([`../modules/mig29/module.md`](../modules/mig29/module.md), [`../modules/mig29/weapons.md`](../modules/mig29/weapons.md) §5.3).

That is gap `C9`, and it is not a detail: **it blocks this entire campaign**, not a mission in it.
The campaign is specified anyway, for two reasons — it is the only campaign that states the
requirement for a *director-based* delivery mode, and it is the eastern half of the ground-attack
question that W2/W3/W4 answer from the western side.

> **SUPERSEDED 2026-07-30, and the two paragraphs above are left standing because they are the record of
> why this file existed for two rounds with no `.fbm`.** `C9` closed on 2026-07-30: the director is built
> (`core/FBDirector.h`, `modules/mig29/FBMig29Director.*`), the ten missions exist, run, replay and are
> measured, and the campaign came out at **10 of 10 runnable and 10 of 10 answerable** — see §State. What
> did NOT change is the substitution: it is still the largest in the ten-campaign set and it still makes
> the attacker **stronger** than history, so a bad O3 result says more than the record did and a good one
> says nothing.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Date / time | **6 October 1973, ≈14:00**, a coordinated Egyptian and Syrian surprise attack | [T4] |
| Egyptian opening strike | **≈220 strike aircraft** — MiG-21, Su-7, MiG-17, Hawker Hunter — plus ≈100 Mi-8 assault helicopters | [T4] |
| Syrian opening strike | **≈100 aircraft**; **Su-7 and MiG-17 fighter-bombers came in very low while MiG-21s provided top cover** | [T3]/[T4] |
| Targets | command posts, observation points, artillery positions, armour, fortifications | [T3] |
| Opposition on the first strike | **none from the Israeli air force**; defensive fire from Hawk batteries and scattered AAA | [T4] |
| Losses on the first strike | **light** | [T4] |
| The SAM umbrella | **>200 SAM batteries** (SA-2, SA-3, SA-6) massed to shield the canal crossing; **>40 SA-6 batteries**, each typically 3–6 launchers, providing mobile low-level cover, with fixed and semi-mobile SA-2/SA-3 above them | [T4] |
| Later Arab types | MiG-23 and Su-20 also present in the campaign | [T4] |
| Anchor region | Suez Canal ≈ 30.4–31.2 N 32.2–32.6 E; Golan ≈ 32.9–33.3 N 35.7–36.0 E (**approximate, verify**) | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| The subject is **delivery under an umbrella**, not air combat | the striker's job is to place ordnance and come home *inside* friendly SAM cover. Air-to-air is what happens when it goes wrong |
| **The umbrella is a constraint, not a shield** | until `C1` exists it protects nothing, so every mission declares its umbrella boundary as a **geographic limit on the `wp` set** and measures how often the striker leaves it |
| The delivery mode must be **the aircraft's own** | not the F-16's. A MiG-29 attack mode built by copying `FBF16FireControl`'s CCIP/CCRP would be a false statement about the aircraft. §Knowledge 3 states what the honest version requires |
| **Ground targets in every mission** — obviously, but also as *defended* objects | this is the one campaign where the ground target is the point and the fighters are the interruption |
| Low level is the profile | "came in very low" is the anchor's own phrase; that meets `C20` head-on |
| The verdict is machine-read | `objective kill unit <target>` + `survive`; a striker that dies after release still killed the target, and the verdict rule already handles that ([`../missions/verdict.md`](../missions/verdict.md)) |

### 3. The ten missions

Ours = MiG-29 (as the archetype fighter-bomber). Blue = Israeli side (F-16 module).

| # | Mission | Task | Time | Wx | Ours | Blue | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `o3-01-unopposed` | single-ship strike, no opposition | day | calm | 1 MiG-29 with unguided stores | — | 1 `target_soft` (artillery position) | `kill unit` | **The blocking mission.** Can this module deliver an unguided store at all? Today: no (`C9`). This mission is the acceptance test for the director-mode work |
| 2 | `o3-02-low` | the same strike at very low level | day | calm | 1 MiG-29 | — | 1 `target_soft` | `kill unit` + `waypoints` | Low-level delivery over real terrain, meeting `C20` (the guidance holds an ASL altitude, not an AGL one) |
| 3 | `o3-03-pair` | two-ship strike, line astern | day | calm | 2 MiG-29 (flight) | — | 2 `target_soft` (a battery position) | both `kill unit` | Does the second aircraft's release inherit the first's error, and can a flight put two aircraft over one target without a timing mechanism (`C15`)? |
| 4 | `o3-04-top-cover` | strikers with a fighter escort above | day | calm | 2 MiG-29 strike + 2 MiG-29 top cover | 2 F-16 CAP | 2 `target_soft` + 1 `target_hard` (command post) | strikers `kill unit` + ≥3 of 4 `survive` | The anchor's own structure: low fighter-bombers, MiG-21 top cover. Does the cover engage early enough to matter, or does it follow the strikers down? |
| 5 | `o3-05-hawk` | strike into a defended position | day | calm | 4 MiG-29 | — | 1 `target_hard` + 2 AAA/SAM sites (**inert, `C1`**) | `kill unit` on the hard target + all `survive` | The historical defence was Hawk batteries and AAA. Unanswerable today — the mission exists to keep the shape on record |
| 6 | `o3-06-inside-the-umbrella` | strike with a hard geographic limit | day | calm | 4 MiG-29, `wp` set confined | 4 F-16 (two flights) intercepting | 3 `target_soft` | `kill unit` + ≥3 `survive` **and no striker leaves the declared box** | The umbrella's real cost: a striker that may not chase, may not extend and may not climb. Measurable today as a **discipline** metric even though the umbrella itself is inert |
| 7 | `o3-07-pursued` | egress with an interceptor behind | day | calm | 2 MiG-29 with stores still aboard | 2 F-16 | 1 `target_hard` | `kill unit` + both `survive` | Does the pilot jettison? There is **no jettison decision** in the tree — stores come off through a release, so "get rid of the drag and fight" is not expressible |
| 8 | `o3-08-armour` | strike a moving column | day | calm | 4 MiG-29 | 2 F-16 | 6 `target_soft` in a column (**static — `C14`**) | ≥4 of 6 | Against small dispersed targets, how many aircraft does one column cost — and how much of the answer is the store rather than the pilot? |
| 9 | `o3-09-two-fronts` | two simultaneous strikes | day | calm | 4 + 4 MiG-29 (two flights, two axes) | 4 F-16 | 4 `target_soft` + 2 `target_hard` | ≥4 of 6 targets + ≥6 of 8 `survive` | With the defender forced to choose an axis, does splitting the attack pay — and does the run stay deterministic at 14 units? |
| 10 | `o3-10-october-six` | the opening strike | day | calm | 8 MiG-29 strike + 4 top cover (three flights) | 4 F-16 scrambled **late** | 6 `target_soft` + 2 `target_hard` | ≥6 of 8 targets killed AND ≥9 of 12 recover | The anchor's own result was "surprise, light losses". **Can FlightBox reproduce a strike that succeeds because the defender was late** — which is a statement about spawn timing, not about flying? |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| MiG-29 | flyable module | **yes, but cannot attack the ground** (`C9`) | archetype substitution for MiG-17/Su-7/Su-20 — and the substitution is *very* generous: those were subsonic or first-generation supersonic attack aircraft |
| MiG-17 / Su-7 / Su-20 / MiG-21 class | flyable module | **no** (`C7`) | the actual force |
| F-16 | flyable module | **yes** | Israeli side stand-in (the real defender flew F-4, Mirage and A-4) |
| Unguided bombs for the MiG (FAB class) | store catalogue | **no** (`C8`) | `core/FBStore.h` has Mk-82 only among unguided stores |
| Rocket pods | store catalogue | **no** (`C8`) | a primary weapon of the type being represented |
| SA-2 / SA-3 / SA-6 (friendly umbrella) | ground, emitting + shooting | **no** (`C1`) | the campaign's defining asset, and it belongs to **our** side here — the first campaign in the set where that is true |
| Hawk battery / AAA (hostile) | ground, shooting | **no** (`C1`) | the historical defence |
| Artillery position, command post, fortification | ground | **yes** (`target_soft`/`target_hard`) | |
| Armour column | ground, **moving** | **static only** (`C14`) | |
| Assault helicopter (Mi-8 class) | flyable module | **no** (`C7`) | ≈100 of them in the anchor |

### 5. What must be true before mission 1 can fly

**Nothing in this campaign is buildable today.** Mission 1 is blocked by `C9` and `C8` at once: the
module publishes no release cue and the catalogue holds no store it would drop. That makes O3 the
**only campaign in the set with zero runnable missions**, and it is listed as such in
[`INDEX.md`](INDEX.md).

---

## State

**BUILT AND FLOWN, 2026-07-30 — the eighth of the ten campaigns to exist as files, and the ONLY ONE
WHOSE SURFACE-TO-AIR UMBRELLA IS FRIENDLY.** Ten `.fbm` in `sim/missions/o3-*.fbm` plus
`sim/campaigns/o3-yom-kippur-1973.fbc`, run as a campaign, replayed step by step, and measured. **No
file under `sim/src/`, `sim/tools/` or `sim/assets/` was touched** (`git status --porcelain` lists
eleven new untracked files and **no modified one**), so the **216** pre-existing `sim/missions/*.fbm`
are byte-identical **by construction rather than by comparison**.

### The spec's own headline is superseded, and by measurement

This file said: **"Nothing in this campaign is buildable today"** and *"that makes O3 the only campaign
in the set with zero runnable missions."* Rule 7 says a blocked mission is re-checked against the
**tree** rather than against a gap's status line. Every blocker was re-checked one by one:

| The spec said | The tree says, 2026-07-30 |
|---|---|
| `C9` — the module cannot fly `set task attack` at all | **CLOSED 2026-07-30**, and closed as a DIRECTOR: `core/FBDirector.h` + `modules/mig29/FBMig29Director.*`. Every O3 striker flies `set attack_mode opt` |
| `C8` — no FAB-class bomb | **BUILT 2026-07-28.** `fab500` is the campaign's only air-to-ground store. **The rocket pod is still absent**, and it was a primary weapon of the types being represented, so no O3 sortie flies a rocket attack |
| `C1` — no SAM, no AAA | **CLOSED 2026-07-28.** `sa2` `sa3` `sa6` `p18` `zu23` all fly here — and `sa2`/`sa3`/`sa6` are the anchor's own designations |
| `C22`/`C23` — no connected defence, no judged belt | **CLOSED.** `net` with `link wire`/`control`/`period`/`hold`/`wcs`, and `zone` + `objective avoid zone`. **The net is OURS here**, which the design allows: a net declares its members by callsign and inherits their teams |
| `C2` — no time of day | **CLOSED.** All ten declare `1973-10-06T12:00:00Z` (= 14:00 local at UTC+2 [DERIVED], the anchor's own launch time; the capstone is 12:30Z). MEASURED sun elevation at spawn **40.55°**, which is what makes the optical AAA sight of sortie 08 work at all |
| `C12` — three more objective kinds | **CLOSED.** `protect`, `no_fire` and `deny release` each decide an O3 sortie |
| `C0` — no campaign layer | **CLOSED.** This campaign has a `.fbc` and a one-step chain |
| `C7` — no period aircraft | **BUILT-and-open**, and O3 flies **no catalogue row at all** — see below. This is the largest substitution in the set |
| `C14` — no moving ground units | **STILL OPEN.** Sortie 08's column is parked and reports an upper bound |
| `C20` — no terrain-following guidance | **STILL OPEN**, and harmless here: under `--elev const` on a 0 m plane ASL and AGL are the same number, which is also why the spec's mission 2 was dropped |
| `C15` — no package coordination | **STILL OPEN**, and sorties 09/10 price it |
| `C4` — no terrain masking | **STILL OPEN.** Every detection range below is an upper bound |
| `C11` — no strafing · no jettison decision | **STILL OPEN.** The spec's mission 7 is dropped for the second one, with its slot named in the `.fbc` header |

**So the spec's count of ZERO runnable missions is TEN**, and `INDEX.md`'s deliberate *"see note"* for
this campaign now reads **10 of 10 runnable, 10 of 10 answerable**.

### The arena, and the one requirement that is not a preference

The anchor's Suez Canal sector, ~30.4–31.2 N 32.2–32.6 E [T4, approximate], flown **west → east**
because that is the direction the Egyptian strike crossed the canal, with the egress back **west through
the own umbrella**. Scale: 1° of latitude = 111 132 m, 1° of longitude at 30.80 N =
111 320·cos(30.80°) = **95 619 m** [DERIVED]. `--elev const`, **0 m datum** (the real Sinai here is
0–60 m), **no terrain masking (`C4`)**.

**`--elev const` is a hard requirement for this campaign and not a convenience.** Every striker spawns
at **300 m ASL**, because a 6 km rangefinder caps this airframe's level-bombing altitude at 2.0–2.2 km
(`missions/mig29-opt-refused.fbm`). Under `--elev swiss` — which is `fb-gym`'s **own default** when the
baked DEM is on disk — an explicit spawn altitude is validated against the resolved ground, and a
mission whose ground comes out above it FAILS at set-up before it flies. Every command in this record
therefore passes `--elev const` explicitly, and every fingerprint below is comparable only against runs
whose `campaign-summary.txt` records `elev const`.

### The substitution, its direction, and why it is stated eleven times

**The MiG-29 was not in 1973.** The opening strikes were flown by MiG-17, Su-7, Su-20, MiG-21 and
Hawker Hunter [T4]. All five are rows in `../modules/air/catalogue.md` and **every one is `ALPHA`** —
`make -C sim test-air` puts 10 of 10 generated decks outside their own bands, and an `ALPHA` row may not
answer a campaign question. **O3 therefore flies no catalogue row at all**, exactly as W3 and W4 chose,
and the substitution is declared with its direction in the `.fbc` header and again in all ten files:

| Real | Flown | Direction |
|---|---|---|
| MiG-17 / Su-7 / Su-20, low fighter-bomber | `mig29` + `fab500` | attacker **materially stronger** |
| MiG-21, top cover | `mig29` + R-27R/R-73 | attacker **materially stronger**, and the anchor's own *layer difference* is ERASED — both layers are the same aircraft |
| F-4 / Mirage / A-4, the Israeli defender | `f16` | Blue **stronger** |
| SA-2 / SA-3 / SA-6 | `sa2` / `sa3` / `sa6` | **none** — same designations. But three positions stand for >200, so every count is per-battery |
| Israeli Hawk batteries | **absent, not substituted** | Blue **weaker**. There is no Hawk row |
| Israeli scattered AAA | `zu23`, sortie 08 only | Blue **weaker** (the anchor's mix had heavier calibres) |
| ~100 Mi-8 | **absent** (`C7`) | — |

**Consequence for every reading, and it is the campaign's most important sentence: an O3 result that
comes out BADLY says something STRONGER than the record did. One that comes out well says nothing at
all about 1973.**

### The number that decides every O3 verdict

Derived once from the tree's own damage model (`core/FBDamageModel.cpp` +
`modules/ground/FBGroundTarget.h`; no new number is introduced):

```
flux(J/m²) = 0.5 · (WarheadKg · 0.5 / (4π r²)) · (1800² + closure²)
```

FAB-500 (201 kg, closure ≈ 214 m/s): a `target_soft` **fails at 68.4 m**, a `target_hard` at
**12.1 m** [DERIVED]. The OPT director delivers at **46–70 m** on a 6 km final. **So a soft position is
killable with 2–14 m of margin and a hardened one is unreachable by a factor of 5.2**, and that single
ratio decides which half of the anchor's own five-item target list this campaign can attack.

### The ten sorties, their fingerprints and their answers

Campaign exit **3**, step exits `0 3 3 0 0 0 1 0 3 1`. Campaign fingerprint under `--elev const`:
`01e4f956ca915c6a984178df782a2e07d52ed0e7ddd6485717edd177c5f9cb13`. Wall clock for the whole campaign:
**31.4 s**.

| # | Mission | ctrl | exit | fingerprint | The answer to its one tactical question |
|---|---|---|---:|---|---|
| 1 | `o3-01-unopposed` | — | 0 | `cc5682956b788fc9` | **The blocking mission delivers, and the margin is 8.5 m.** `LRF_RANGE` 3 624.9 m, consent 3.1 s later, `countdownS` 4.347, `openLoopAlongM` **−70.33 m**; `aimErrM` **66.39 m** (long −45.52, across +48.34). The position dies with the burst 59.89 m from its centre — fluxJm2 3 662.9 against 2 800. **That 8.5 m is this campaign's whole tolerance** and every sortie is read against it |
| 2 | `o3-02-command-post` | 01 | 3 | `91e8fea172d46813` | **[ctrl 01, ONE fact: the aim point's fragility class.] A hardened position is not reachable, and a 66 m miss on it is not even a recorded event.** The delivery is identical to o3-01's to the centimetre; the arriving energy is ~3 376 J/m² against a `Degrade` threshold of 2.5e4, so **there is no `damage DAMAGE` line at all**. Shortfall: 27× in energy, 5.2× in distance. **Two of the anchor's five target kinds are out of reach** |
| 3 | `o3-03-ingress` | self | 3 | `12edfcf6f8e45792` | **This airframe can only bomb what it is already pointed straight at, within 6 km — and the two error terms trade exactly oppositely.** 6.02 / 12.00 / 24.00 km finals give across **+48.3 / +87.2 / +90.9 m** and `openLoopAlongM` **−70.3 / −9.9 / −1.0 m**: the DIRECTOR gets 71× better with leg length and the AIRFRAME 1.9× worse, and the airframe wins because only its axis is compared against a lethal radius. **1 kill of 3.** Attribution A2 supplies the other half: **across = 48.3 m + 33.9 m per degree of dog-leg** [DERIVED, three-point fit], so the largest turn admissible inside a final is **0.59°** |
| 4 | `o3-04-umbrella` | — | 0 | `2beb4142c7a02e79` | **THE CAMPAIGN'S CENTRAL SORTIE. A friendly belt with no IFF spends its entire magazine on its own striker: 7 launches, 7 of 7 at the MiG-29, both magazines empty.** The strike is untouched (identical tick, identical centimetre as o3-01) because the belt's first round leaves 11 s AFTER the bomb. `sa2` fires **nothing** in 600 s — 450 m floor against a 300 m striker, the anchor's own layering falling out of one number. `zone_umbrella_s` **584.7 of 600.0 s** (97.5 %). **Zero arrivals, and the margin is 10–20 m, not kilometres** |
| 5 | `o3-05-umbrella-held` | 04 | 0 | `0397c8e6ce78a6ec` | **[ctrl 04, ONE token: the net's `wcs free` → `wcs hold`.] Weapons hold costs nothing and buys the magazine.** `site LAUNCH` 7 → **0**, magazines 0+0 → 4 V-601 + 3 3M9 intact, **2 × `net WCS … effect="launch inhibited"`** at the tick each battery's reaction time elapsed. `site TRACK`/`RADIATE`/`net CUE` and the whole delivery **unchanged**. It is the RESTRAINED nothing and not the ABSENT one (W5's rule), and the honest half is that o3-04's seven rounds killed nobody either |
| 6 | `o3-06-umbrella-sort` | 04 | 0 | `fae132b551cd43d8` | **[ctrl 04, ONE fact: a hostile F-16 in the same envelope.] The price of a missing IFF interrogator is the WHOLE engagement. All 44 `site` decision lines are IDENTICAL to o3-04's** — adding an enemy changed nothing; the F-16 is never firmed, never engaged, never counted, and A1 shows it would have absorbed all seven rounds. **And the only round that touched the enemy is one fired at its own side**: `y6sa3_v601_4` → `y6blu` at **9.51 m** against a 10 m fuze, ten systems failed, no kill |
| 7 | `o3-07-top-cover` | — | 1 | `3582a2521f2d72f0` | **The top cover does not cover, and the low profile hides nothing.** Blue's FIRST contact (t = 3.9, 21.94 nm, elDeg **−7.44**) is a striker, **5.24 nm nearer than the escort**. The cover's first shot is at t = 69 — 43 s after the bombs are down — and its 8 rounds produce **zero arrivals**; it ends 103 km south with an empty rack. Blue never fires at it once and kills a striker on the egress (4.33 m). **The strike succeeds because it is over 31.3 s before the first weapon of the air battle leaves a rail** |
| 8 | `o3-08-armour` | — | 0 | `ea1d1ac898e38bf6` | **One aircraft per vehicle, and the store is 100 % of that answer.** 4 of 4 released, `aimErrM` 66.31–66.54 m — a **0.23 m spread** over four aircraft. **4 of 6 vehicles die: exactly the four aimed at, not one neighbour** (2 × 68.4 < 150.0 m spacing, so a bomb between two neighbours kills neither). **And the gun died to the miss**: the +48.3 m cross-track bias walked the bomb aimed at v4 to 50.4 m from the ZU-23-2 nobody aimed at. The gun saw with the eye alone, tracked, fired **38 rounds in 29 bursts** and hit nothing |
| 9 | `o3-09-two-fronts` | — | 3 | `d819e5c103397b30` | **CHAIN HEAD. Splitting pays, and it pays because neither side can divide.** 8 of 8 released; 4 of 4 soft aim points dead, **0 of 2 hard** (four bombs, four arrivals, not one `damage` line). The line-astern aircraft does not inherit its leader's error — it has its own, and the only difference is 2 km of final (+48.4 → +66.4 m). The belt fires 7, **all 7 at its own strikers**, and only at the SOUTH axis, because the 2K12's 24 km reach covers the north aim points at 23.3 km and falls **57 m short** of the south ones. The CAP splits correctly and puts **four AIM-120 inside their own 10 m fuze (4.03–5.35 m) with ZERO kills** |
| 10 | `o3-10-october-six` | chain | 1 | `d73b57fd0e8ddbe0` | **CHAIN TERMINUS. Yes — a strike succeeds because the defender was late, and "late" is a CLOSURE fact and not a detection one.** Blue has the low strikers at t = 3.9 s at **45.16 nm** and cannot shoot until t = 148.4, **122.2 s after the last bomb landed**. 8 of 8 released, **6 of 8 aim points destroyed** (the spec's bar, met at the ceiling: the two survivors are the two the store cannot kill), **10 of 12 recovered** (bar ≥ 9). **And the crossing's own 2K12 shot down its own striker**: `yxsa6_3m9_13` → `yxd` at **4.74 m** against an 8 m fuze |

### What a friendly umbrella without an IFF interrogator costs its own attackers — the campaign's own question

No other campaign in the set can ask this, because in the other nine the surface-to-air layer belongs to
the enemy. Two structural facts, both re-checked against the tree in this round:

1. **`modules/ground/FBSiteFireControl` contains no IFF path of any kind.** `FBRadarSystem` reads a
   unit's team in exactly one place — the transponder reply, `FBRadarSystem.cpp:69` — which no site ever
   asks for. **A fire unit's target choice is a function of geometry alone.**
2. **Every SAM row has `Channels = 1`.** A battery is a one-target machine, so putting it on the wrong
   aircraft does not degrade the engagement, it **deletes** it.

Measured across the campaign:

| Quantity | Measured |
|---|---:|
| `site LAUNCH` over the ten sorties | **28** |
| of those, aimed at its OWN aircraft (every `sms LAUNCH_SOLUTION … tgtLat/tgtLon` attributed against both sides' telemetry at that tick) | **28 of 28** |
| aimed at an enemy aircraft, ever | **0** |
| enemy aircraft ever held on a firm track by the umbrella | **0** |
| own aircraft destroyed by the umbrella | **1** (`yxd`, a 3M9 at 4.74 m in step 10) |
| enemy aircraft damaged by the umbrella | **1** (`y6blu`, ten systems failed by a V-601 at 9.51 m **fired at a MiG-29**) |

**The net military effect of ten sorties of friendly SAM cover is minus one MiG-29.** And the two
sentences that have to be read together:

- **Under `wcs free` (o3-04, 06, 09, 10) the cost is the whole magazine and, once, an aircraft.** With
  one of its own aircraft inside the envelope the belt never considers the enemy at all — o3-06's 44
  `site` decision lines are byte-identical to o3-04's, which has no enemy in it.
- **Under `wcs hold` (o3-05) the cost is zero and the magazine stays on the rails.** Which is the only
  countermeasure this format can express for a missing interrogator, and it is a DECISION with a price
  rather than a fix: a belt on hold is a belt that also cannot shoot an intruder.

**Rule 11 applied to this campaign's own result.** Four of the five umbrella sorties look like *"the
friendly belt is harmless"*, and that reading is wrong twice over. First, it is a result under
`wcs free` and o3-05 is the other policy. Second — and this is the part a single sortie hides — **the
belt's rounds are MARGINAL, not broken.** Attribution A3 measured it on both sides of its threshold
(O2's rule 10): a lone S-125 and a lone 2K12 against four identical straight transits at 300 / 1 000 /
3 000 / 5 000 m produce `stores MISS closestM` **9.07 m against an 8 m fuze** (3M9) and **11.39 /
11.43 / 14.50 / 15.21 m against a 10 m fuze** (V-601). **The shortfall is 1.1 to 5.2 metres.** Which is
why the same belt that missed 27 times killed on the 28th, and why the deciding variable in step 10
turned out to be 500 kg of unexpended bomb.

### The run-in geometry: the campaign's own hardest constraint, and it is the airframe

Every O3 striker spawns 6 km from its aim point on a heading that already points at it. That looks like
an author's convenience, so it is priced (W4's rule):

| Lever | Measured | Consequence |
|---|---|---|
| **Final leg length** (o3-03) | across **+48.3 m** at 6.02 km, **+87.2 m** at 12.00 km, **+90.9 m** at 24.00 km | the 68.4 m kill radius is crossed **between 6 and 12 km**. 1 kill of 3 |
| **Dog-leg angle** (attribution A2, same 6.02 km final) | across **+48.3 / +229.7 / −131.0 / +401.1 m** at **0 / +5.3 / −5.3 / +10.5°** | **across = 48.3 m + 33.9 m per degree** [DERIVED]. Maximum admissible turn inside a final: **0.59°**. 1 kill of 4 |
| **Line astern** (o3-09) | +48.4 m at 6.02 km, **+66.4 m** at 8.02 km, on the same track | a trailing aircraft does not inherit its leader's error, it has its own, and 2 km of extra final is 18 m of it |
| **Unexpended store on the mirror station** (o3-10 as campaign step against standalone) | across **+48.3 m** with two FAB-500, **−39.2 m** with one | **an 87.5 m swing, and the sign flips** — larger than the store's own kill radius |

The mechanism is one airframe property with a source: the yaw channel is measured OFF
(`../modules/mig29/module.md` gap 4i), so the aircraft holds a standing cross-track offset. **What that
does to this campaign's anchor is decisive: the opening strike of 6 October was 220 aircraft flying
coordinated approach routes, and in this tree such an approach cannot hit anything.** Every O3 geometry
is the one geometry in which a kill is possible at all, and the campaign says so in three files rather
than implying it in ten.

### The four attribution runs

Not part of the ten; each is a throwaway file under the scratch tree, and each number below is what it
produced.

| # | What it changes | What it measured |
|---|---|---|
| **A1** | `o3-06` with the friendly striker **deleted** | the belt fires the **same seven rounds**, all at the F-16, from t = 61.4 instead of t = 37.5, and **zero arrive**. This is what turns *"7 of 7 at its own side"* into a **price**: the enemy would have absorbed the entire magazine |
| **A2** | four aircraft, one 6.02 km final, **dog-leg angle** the only variable | `across = 48.3 m + 33.9 m/deg` [DERIVED], **0.59°** admissible, 1 kill of 4 |
| **A3** | one S-125 + one 2K12 against four transits at **300 / 1 000 / 3 000 / 5 000 m** | the belt engaged 1 000 m (2K12) and 3 000 m (S-125) and never 300 or 5 000 — `Channels = 1` again. `closestM` **9.07 m against an 8 m fuze** and **11.4–15.2 m against a 10 m fuze**: the ground-launched round misses by **1.1–5.2 m**, which is a marginal terminal-guidance shortfall and not a gross failure |
| **A4** | `o3-10` run **standalone**, i.e. with 16 FAB-500 instead of 8 | fingerprint `0e7a46e6a4277174`, exit 2. **0 of 48 telemetry files byte-identical.** `aimAcrossM` +48.3 → −39.2 m; `aimErrM` mean 60.0 → 55.4 m and **not uniformly** (three worse by 3.7–4.4 m, five better by 9.6–10.1 m); aim points killed **6 of 8 in both**; Red losses 2 in both but **different aircraft and a different cause** — standalone all three 3M9 miss and `yxd` lives. Run length 160.7 → 398.8 s |

**So the carry is the most decisive one of the eight built campaigns, and that was not expected of it.**
W3's carried node left 30 of 58 telemetry files byte-identical and moved no trajectory column; O3's
carry changes the **mass and the lateral balance** of eight aircraft, so the physics diverge at the
first substep. **One unexpended bomb is worth one aircraft and 238 seconds of run — and the aircraft it
is worth was killed by its own side's missile.**

### Both determinism criteria, measured on the first attempt

Under `--elev const`, read out of `campaign-summary.txt` rather than assumed:

| # | Criterion | Result |
|---|---|---|
| **1** | 3 repetitions × `--threads 1/2/4` produce one campaign fingerprint | **9 runs, 1 fingerprint** `01e4f956ca915c6a984178df782a2e07d52ed0e7ddd6485717edd177c5f9cb13`, exit 3 in all nine |
| **2** | every step's per-mission fingerprint equals that mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included, on the first attempt |
| **1 — re-run 2026-07-30** | the same criterion under the branch-order change of `b433950` ([`../pilot.md`](../pilot.md) §7.4a) | **9 runs, 1 fingerprint** `f6e8767579e1982b5453591dd6180be69f2f4a7fa0931cb63cf6f47962495f85`, `--elev const`. **The value above is kept with its date; this is the current one.** Step exits `0 3 3 0 0 0 3 0 3 1`, and the seventh of those is new: **STEP 7's EXIT MOVED, 1 → 3** — see the note under the table |
| **2 — re-run 2026-07-30** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 | | `cc5682956b788fc9 91e8fea172d46813 12edfcf6f8e45792 2beb4142c7a02e79 0397c8e6ce78a6ec fae132b551cd43d8 b8cc757093a225cb ea1d1ac898e38bf6 71f3375870fe5be8 7809098bcc4cde76` |

> **STEP 7's EXIT MOVED, AND IT IS THE ONE SHIFT IN NINE CAMPAIGNS THAT CHANGES A VERDICT.**
> `o3-07-top-cover` was exit **1** and is exit **3**: the run reaches its 600 s timeout with all four
> Egyptians reporting SUCCESS instead of losing `y7sb` and its escort at t = 512.6 s. The cause was
> traced tick by tick when the branch was re-ordered and is NOT the fuel line
> ([`../pilot.md`](../pilot.md) §7.4b): `y7ba` now holds its beam to the end of its 12 s defence hold
> instead of snapping back into the intercept at t = 189.3, which puts it somewhere else at
> t = 220.6, where an R-73 arrives at **2.67 m / 67,285 J/m²** — the first arrival this campaign's top
> cover has ever scored. Its radar degrades, the resumption test is reachable for the first time and
> answers *"no sensor"*, and the remaining CAP jet alone does not get the second pair away in time.
> **The mission's answer changed and its own header no longer describes it.** `pilot.md` booked the
> change here on 2026-07-30 and this is the entry it was booked into; the row's text above is the
> pre-change measurement and is kept.

**And the replay was run after the FIRST mission**, on a throwaway one-step `.fbc`
(`sim/campaigns/o3-step1-check.fbc`, deleted afterwards): `01 … campaign fp=cc5682956b788fc9 standalone
fp=cc5682956b788fc9 MATCH`. **Annotating the ten files with their MEASURED blocks after the runs left
all ten per-mission fingerprints and the campaign fingerprint unchanged** — the check that a comment is
a comment.

### What this campaign found while building, none of it fixed here

Rule 9: *the defect sits in the seam you did not look at.* O3 found three, and none is in the
friendly-umbrella mechanism the campaign was written about.

| # | Finding | The measurement that pinned it |
|---|---|---|
| **1** | **A ground-launched command round misses a low crossing target by 1–5 m, i.e. by just more than its own fuze radius.** This is the third visible layer of O1's ground-launch defect family (a caged MANPADS seeker, a V-750 that cannot fly its own pitch-over, one gain set across three orders of magnitude of missile mass) and it is now a NUMBER on both sides of its threshold rather than a nullity | **A3**: `closestM` 9.07 m against an 8 m fuze (3M9) and 11.39 / 11.43 / 14.50 / 15.21 m against a 10 m fuze (V-601), on four straight non-manoeuvring transits. And the confirmation from the other side: in `o3-10` as a campaign step one 3M9 arrives at **4.74 m and kills**, so the mechanism works and the margin is what fails. Two V-601 in `o3-04` also climb to **15 948 / 15 978 m** against a 300 m target before stalling, which is the pitch-over defect unchanged |
| **2** | **An unexpended store on one inboard pylon with its mirror station empty reverses the airframe's standing cross-track offset**, by more than the store's own lethal radius | **A4** against campaign step 10, one file, one difference: `aimAcrossM` **+48.31…+48.37 m** on eight aircraft with two FAB-500, **−39.11…−39.22 m** on the same eight with one. An 87.5 m swing against a 68.4 m kill radius, and it changed which aircraft the campaign lost |
| **3** | **`core/FBStore.h`'s FAB-500/FAB-250 comment is stale and now says the opposite of the truth**: *"the MiG-29 cannot fly `set task attack` at all (C9 …), so these rows exist and their delivery mode does not."* `C9` closed on 2026-07-30 and thirty FAB-500 were delivered by the OPT director in this campaign alone | a documentation defect, not a behavioural one; named here because `sim/src/` was deliberately not touched in this round |

**A fourth thing is understood mechanics rather than a defect, and it is worth recording because it
looks like one:** an optical-only position (`zu23`) publishes a firm track with `rangeM=1250` and
`closureMs=0` — an eye has no range, so the phantom is placed at the midpoint of the position's own
envelope as a **pointing** aid (`FBSiteFireControl.cpp:443`, documented there). The bearing and
elevation are measured; only the range is invented, and the barrels are aimed off the measured angles.

### Where the built campaign departs from §3's table, and why

The Spec above is **left standing as written** and the departures are listed here, because each was
discovered by building. Three of the spec's ten are dropped and three are new; the slot numbers shift.

| §3 says | Built as | Reason |
|---|---|---|
| mission 1 `unopposed`, "can this module deliver at all" | exactly that, and it delivers | the mission the spec called *"the acceptance test for the director-mode work"* is the acceptance test, and it passed with 8.5 m of margin |
| mission 2 `low`, "the same strike at very low level" (`C20`) | **dropped**; slot → `o3-02-command-post` | under `--elev const` the ground is a 0 m plane, so ASL and AGL are the same number and the question answers itself with no run. And the campaign is *already* entirely low — every striker in all ten files is at 300 m |
| mission 3 `pair`, two-ship on one battery position (`C15`) | **folded** into `o3-09`'s two line-astern pairs and `o3-08`'s four-ship | its own question is answered there: the second aircraft does not inherit the first's error, it has its own, and the difference is the length of its final |
| mission 4 `top-cover` | exactly that, as `o3-07` | unchanged, and the answer is that the cover does not cover |
| mission 5 `hawk`, "strike into a defended position" | **dropped as specified**; the AAA half merged into `o3-08`, the Hawk half declared absent; slot → `o3-05-umbrella-held` | there is no Hawk row and no Western AAA row of any kind, and putting a Soviet `sa3` on the Israeli side would be a substitution in the wrong direction with no way to price it |
| mission 6 `inside-the-umbrella`, the geographic limit as a verdict | **split into three**: `o3-04` / `o3-05` / `o3-06`, with `objective avoid zone ceiling` carrying the discipline half | the umbrella turned out to have two separate questions in it (what it costs, and where its rounds go), and `C23`'s inverted-zone idea landed as a CEILING zone rather than a box: the striker's real geographic limit in this tree is the **2 200 m** its own weapon system imposes, which is a measured number and not a `[SET]` one |
| mission 7 `pursued`, "does the pilot jettison?" | **dropped**; slot → `o3-06-umbrella-sort` | there is no jettison decision, so the sortie would have measured an absence with no lever to pull. And `../modules/mig29/weapons.md` §5.4.5 already records the same conclusion from the aircraft's side |
| mission 8 `armour`, 6 targets in a column | exactly that, plus the column's own gun | the two spec missions merged, and the merge is what let the campaign ask what the forced-low profile costs |
| mission 9 `two-fronts`, 4+4 against 4 | exactly that | unchanged, and it became the chain head |
| mission 10 `october-six`, 8+4 against 4 late | exactly that | unchanged. Its own bar (≥ 6 of 8 targets, ≥ 9 of 12 recovered) is met at **6 of 8 and 10 of 12** |
| **new** | `o3-02-command-post`, `o3-03-ingress`, `o3-05-umbrella-held`, `o3-06-umbrella-sort` | the first two are the campaign's own boundaries (the store's and the airframe's) and the second two are single-lever controlled variants of the spec's mission 6 |

### Conservation, and the gates

`git status --porcelain` lists **eleven new untracked files and no modified one**: ten
`sim/missions/o3-*.fbm` and one `sim/campaigns/o3-yom-kippur-1973.fbc`. Gates:
`make core-lib gym native wasm` warning-free (`gym -> build/fb-gym (GPU-free: 0 Dawn/WebGPU symbols)`,
`WASM -> web/gpu.js + web/fbtileworker.js + web/missions/`); `verify-layers` *"304 files, 841 internal
include(s), 12 layers — no upward include, 3 restricted header(s) respected, 6 registry reader(s) inside
the perception boundary, 1 antenna-cue poster(s), 291 file(s) in their layer's namespace (5 C-island
file(s) exempt)"*; `verify-models` *"4 upstream-backed model path(s) match assets/MODEL-DELTAS.md
(1 declared delta(s), 34 FlightBox-own)"*; seven harnesses rc = 0 (`test-monitor` `test-fdm`
`test-corner` `test-gun` `test-weather` `test-missile` `test-air`).

---

## Gaps

**Re-checked against the tree on 2026-07-30, before the ten files were written (rule 7). Eight of the
thirteen entries below had closed since the spec was written; the five that remain are named with what
each one cost this campaign, measured.**

| ID | What is missing | Cost to this campaign, measured |
|---|---|---|
| ~~`C9`~~ | ~~the MiG-29 module cannot fly `set task attack`~~ — **CLOSED 2026-07-30.** Built as a **director**, not as a release cue: the aircraft picks the release moment, the pilot flies an instruction. The acceptance was the one measurement that could catch a shortcut — same geometry, same `fab500` on both so ballistics are not a variable, F-16 CCRP **34.02 m** against MiG-29 OPT **65.65 m**. Refusal is its own sourced case (`mig29-opt-refused.fbm`) | **nothing any more.** Thirty FAB-500 were delivered by the OPT director in this campaign, at `aimErrM` 46.4–94.0 m |
| ~~`C8`~~ | ~~no FAB-class bomb~~ — the FAB rows landed with the air-to-ground round (`9682448`) and the director flies them; **the rocket pod (`hydra70`/`s8`) is still absent** | **one weapon class.** A rocket pod was a primary weapon of the MiG-17/Su-7 class this campaign substitutes for, and no O3 sortie can fly a rocket attack. It would also have been the only store in this tree small enough to make sortie 08's 150 m column spacing a real trade |
| ~~`C1`~~ | ~~no SAM, no AAA~~ — built (`99897b5`) | **nothing, and it supplied the campaign's subject.** `sa2` `sa3` `sa6` `p18` `zu23` all fly. What it also supplied is a defect: see §State finding 1 |
| ~~`C22`~~ | ~~no connected air defence~~ — **CLOSED.** And the design's promise held: **a net declares its members by callsign and inherits their teams**, so a FRIENDLY net needed no mechanism of its own | **nothing.** Three positions on one `link wire` net stand for the anchor's >200, and every count in §State is per-battery |
| ~~`C23`~~ | ~~no declared, judged belt geometry~~ — **CLOSED**, and the spec's own idea for using it turned out to be the wrong shape | the spec wanted `objective avoid zone` INVERTED to judge *"no striker leaves the declared box"*. **What was built instead is a CEILING zone**, `zone ceiling … 2200 20000` + `objective avoid zone ceiling`, because the striker's real geographic limit in this tree is not a box somebody drew — it is the **2 200 m** its own rangefinder imposes (`mig29-opt-refused.fbm`), a MEASURED number rather than a `[SET]` one. Measured: `zone_ceiling_s` **0.0 s** in every sortie that declares it |
| ~~`C2`~~ | ~~no time of day~~ — **CLOSED** | **nothing, and it decided one sortie.** 12:00Z = the anchor's own 14:00 local, sun elevation **40.55°** — which is what lets sortie 08's optical AAA sight see anything at all |
| ~~`C12`~~ | ~~only four objective kinds~~ — **CLOSED** | **nothing.** `protect` decides sortie 07's verdict, `no_fire` makes sortie 06's F-16 a declared presence, `deny release` is Blue's job in 07/09/10 |
| ~~`C0`~~ | ~~no campaign layer~~ — **CLOSED** | **nothing, and this campaign's carry is the most decisive of the eight built** — one unexpended FAB-500 is worth one aircraft (§State, A4) |
| `C7` | **no period aircraft at all, on either side.** Eighteen rows exist; all ten generated decks are `ALPHA` and an `ALPHA` row may not answer a campaign question | **the largest substitution in the whole set, and it is stated eleven times.** One module plays the low fighter-bomber AND the top cover, so the anchor's own layer difference — a subsonic bomb truck under a lightweight interceptor — is ERASED. Sortie 07 can therefore measure whether a two-layer package works and cannot measure the trade the real one was making. And the direction bites in a measurable place: an F-16's look-down radar found a 300 m striker at **21.94 nm**, which an F-4 or a Mirage of 1973 would not have |
| `C14` | **no moving ground units** | **sortie 08 is an upper bound and says so.** The column is parked for the whole 8.2 s the bomb is in the air. And `../modules/mig29/weapons.md` §5.4.5 adds the worse half: on this aircraft the lead for target motion is the PILOT's arithmetic and FlightBox's pilot does none, so closing `C14` would not by itself let this module hit a moving vehicle |
| `C20` | **no terrain-following guidance** | **nothing here, and that is why a spec mission was dropped.** Under `--elev const` on a 0 m plane ASL and AGL are the same number, so the spec's mission 2 would have been answered by the author's arithmetic. The campaign is *already* entirely low: 300 m in all ten files, forced by a rangefinder |
| `C15` | **no package coordination** | **priced twice.** "Simultaneous" in sorties 09/10 is identical spawn geometry and nothing else — eight releases inside 0.1 s. And "the defender chooses an axis" is not a decision: each CAP pair collapses onto the axis nearer its own spawn line, so a two-axis attack is engaged at half density with nobody deciding |
| `C4` | **no terrain masking** | **every detection range in §State is an upper bound.** The one that matters most: Blue's 45.16 nm contact on a 300 m striker in sortie 10 is over a 0 m plane, and the Sinai is 0–60 m so the DEM would not have changed it — this campaign's arena is one of the few where `C4`'s absence costs almost nothing |
| `C11` | **no strafing** | **nothing measurable.** No O3 sortie resolves a gun attack on the ground. Sortie 08's ZU-23-2 fires the other way and misses (38 rounds, 29 bursts, zero hits) |
| — | **no jettison decision** | **one spec mission, dropped rather than faked.** Stores leave only through a release, so "get rid of the drag and fight" is not expressible. And the campaign then measured what the drag is WORTH by accident: one unexpended FAB-500 swings the cross-track placement by **87.5 m** and flips its sign (§State, A4) |

### The honest headline

**Was:** *"O3 is the campaign that says what the MiG-29 module is missing. Every other eastern campaign
can be flown, partly or fully, with the module as built; this one cannot start. Its value is therefore
not ten mission files but one requirement … Until that exists, the eastern half of FlightBox is an
air-to-air air force."*

**That requirement was built on 2026-07-30 and this campaign flew, and the new headline is a different
sentence: O3 IS THE CAMPAIGN THAT SAYS WHAT A FRIENDLY UMBRELLA COSTS, AND THE ANSWER IS MINUS ONE
AIRCRAFT.** Twenty-eight surface-to-air rounds over ten sorties, **28 of 28 aimed at its own aircraft**,
one MiG-29 destroyed by its own 2K12, one enemy F-16's avionics destroyed by a round fired at a MiG-29,
and **not one round ever aimed at an enemy.** A battery in this tree has no interrogator and one
engagement channel, so a friend inside the envelope does not degrade the engagement — it deletes it.
The only countermeasure the format can express is `wcs hold`, which costs nothing and gives up the
ability to shoot anybody at all.

**And the campaign's second finding is not about the umbrella: it is that this airframe cannot fly the
anchor's own operation.** The opening strike of 6 October was 220 aircraft on coordinated approach
routes; the MiG-29's yaw channel gives it a standing cross-track offset of **+48.3 m + 33.9 m per degree
of dog-leg**, against a FAB-500 kill radius of **68.4 m**. The largest turn admissible inside a final is
**0.59°** and the longest usable final is **under 12 km**. Every geometry in these ten files is the one
geometry in which a kill is possible at all, and that is stated in three of them rather than implied in
ten.

---

## Knowledge

### 1. The anchor with its sources

- **The opening strikes.** [The Arab-Israeli War of 1973: honor, oil, and blood (HistoryNet)](https://historynet.com/the-arab-israeli-war-of-1973-honor-oil-and-blood/)
  [T3] — the 220 Egyptian strike aircraft with the type list (MiG-21, Su-7, MiG-17, Hawker Hunter) and
  ≈100 Mi-8s, launched at 14:00 on 6 October against no air opposition.
  [Air operations during the 1973 Arab-Israeli war (Marine Corps study, GlobalSecurity)](https://www.globalsecurity.org/military/library/report/1985/MML.htm)
  [T1] — the Syrian strike of close to 100 aircraft against command posts, observation points,
  artillery, armour and fortifications, with **Su-7 and MiG-17 fighter-bombers coming in very low
  while MiG-21s provided top cover**. This is the campaign's most important single sentence and it is
  the one with the strongest source in the file.
- **The SAM umbrella.** [Yom Kippur War (Wikipedia)](https://en.wikipedia.org/wiki/Yom_Kippur_War)
  [T4] and [1973 raid on Egyptian missile bases (Wikipedia)](https://en.wikipedia.org/wiki/1973_raid_on_Egyptian_missile_bases)
  [T4] — over 200 batteries of SA-2/SA-3/SA-6 shielding the canal crossing, more than 40 of them SA-6
  with 3–6 launchers each, the SA-6 providing mobile low-level cover under the fixed SA-2/SA-3 layer.
- **Losses and defensive fire.** [Yom Kippur War (Wikipedia)](https://en.wikipedia.org/wiki/Yom_Kippur_War)
  [T4] — Hawk batteries and scattered AAA, Egyptian losses on the opening strike light.
- **Type presence.** [Sukhoi Su-17 (Wikipedia)](https://en.wikipedia.org/wiki/Sukhoi_Su-17) [T4],
  [Yom Kippur War aircraft (Military Factory)](https://www.militaryfactory.com/aircraft/yom-kippur-war-aircraft.php)
  [T4].

### 2. Where the sourcing is thin, and it is stated

| Thing | Status |
|---|---|
| Attack profiles: ingress altitude in metres, run-in speed, delivery mode (level, dive, toss) | **not sourced.** "Very low" is all the strongest source says. Every altitude and speed in a mission of this campaign is therefore `[SET]` and must be labelled |
| Ordnance per aircraft | **not sourced** |
| Precise loss figures for the opening strike, by type | **not sourced**; "light" is the strongest statement found and it is not converted into a number |
| The umbrella's actual coverage geometry | **not sourced**; missions declare a box `[SET]` |

### 3. The requirement this campaign exists to state

What a MiG-29 ground-attack mode must be, if it is built — and what it must **not** be:

| Must be | Must not be |
|---|---|
| Derived from the aircraft's own documented sighting: a **director** the pilot flies onto, computed by the aircraft and displayed as a steering cue, with the release decision belonging to the pilot | a copy of `FBF16FireControl`'s CCIP/CCRP block with different constants. That would be a false statement about the aircraft, and the reason the MiG module refused to publish the block in the first place |
| Measured against the same error budget the F-16 side already has (22.2 m clean, 482 m two seconds late) so the two are comparable | tuned until the numbers look similar |
| Accompanied by **one** unguided store in `core/FBStore.h` with mass, drag and a ballistics table from its own model, exactly as Mk-82 was | a re-labelled Mk-82 |
| Honest about the pilot's role: on a director delivery the *flying* is the accuracy, so the campaign's error budget will be dominated by tracking, not by computation | assumed to behave like a release-cue delivery |

That is a single, bounded work item, and this file is where its requirement lives until it is built.

### 4. Why the campaign is kept despite being unbuildable

Three reasons, all of them structural rather than sentimental:

1. It is the only place in the tree where the **friendly** side owns a SAM umbrella. Every other
   campaign treats surface-to-air as the enemy; `C1`'s design must accommodate both, and this file is
   the requirement that says so.
2. It is the eastern counterpart of W2/W3/W4 — without it, "FlightBox can do ground attack" is a
   claim about one module rather than about the architecture.
3. Its blocking gap (`C9`) is invisible from every other campaign. An air-to-air-only eastern set
   would never have surfaced it.
