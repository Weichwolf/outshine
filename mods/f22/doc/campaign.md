# F-22 Lightning II (1996) — Campaign One: "Gambling on the Mekong"

> **Source document:** research distillation
> `scratchpad/novalogic/f22.md`, §2 (l. 84–98), §3 (l. 100–196), §5 (l. 304–353), §7 (l. 461–499).
> **Independently re-checked in this run** against the manual full text
> (`archive.org/stream/f-22-lightning-2-pc-manual/F-22_Lightning_2_Manual_djvu.txt`) — see
> [`sources.md`](sources.md) for what that check confirmed, corrected and added.
> Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has **no `test/`**: the missions are the test.

Provenance tag on every fact:

| Tag | Means |
|---|---|
| `[ORF]` | measured by the source research from `C01M<nn>.ORF` inside `RESOURCE.RES` — **second-hand measurement**, not re-measured here (the archive was not fetched in this run) |
| `[MAN p.N]` | NovaLogic manual, page N. `p.N*` = page number taken from the table of contents only, **no page footer survived OCR** |
| `[TXT]` | briefing text `C01M<nn>.TXT` inside `RESOURCE.RES`, via the source research |
| `[DERIV]` | computed here, formula given |

## Spec

### 1. Frame

| Item | Value | Provenance |
|---|---|---|
| Game | F-22 Lightning II, NovaLogic, 1996 — first title of the series | `[MAN p.5]` |
| Campaign | **One: "Gambling on the Mekong"** | `[MAN]` verbatim heading "2. Campaign One: Gambling on the Mekong" |
| Missions in this campaign | **8** (`C01M01`–`C01M08`) | `[ORF]` archive index |
| Missions in the shipped game | **31 combat + 5 training**, four campaigns | `[MAN]` verbatim: *"five training and thirty-one combat missions"*; *"four separate combat campaigns (with 31 missions)"* |
| Player unit | **Storm Squadron**, player is *Storm Leader*, one wingman | `[TXT]` |
| Parent formation | **454th Composite Wing**, at FOB **Tyler**, built by the **101st Airborne** | `[TXT]` |
| Other friendly squadrons named | Viper (1.5), Mule (1.6), Vampire (1.8) | `[TXT]` |
| Antagonist | **Chang Tzu Ming**, Golden Triangle warlord, Kuomintang descent, drug-financed | `[MAN]` verbatim |
| Casus belli | UN Security Council vote **15–0, two abstaining**; Thailand invites the force; USA contributes two F-22 squadrons | `[MAN]` verbatim |

The eight mission names are gambling motifs throughout (Snake Eyes, Double Down, Four of a Kind, Aces
Low, Black Mariah). That is the campaign title confirmed from inside the game data, not from the manual.

### 2. The eight missions

| # | File | Name | Time | Task | Success condition | Opposition |
|---|---|---|---|---|---|---|
| 1.1 | `C01M01` | **Snake Eyes** | 0900, day 1 | air superiority over Chang's MiG-27s; flight group *Alpha* 50 NM NNW of FOB Tyler | *Alpha* destroyed **and** all further hostile aircraft destroyed | MiG-27 |
| 1.2 | `C01M02` | **Party Crashing** | 1500, day 1 | bomb the **assembly house at Objective Talbot**, 45 NM NE — Chang is in session with his generals | assembly house destroyed | MiG-27 CAP, AAA, **SA-2**, unspecified count of SAM sites |
| 1.3 | `C01M03` | **Luckiest Man in Laos** | 1800, day 1 | destroy **Objective Madison**, a bridge over the Mekong, 35 NM NE | bridge completely destroyed | MiG-27, MiG-29, AAA |
| 1.4 | `C01M04` | **Silkworm Jungle** | 0800, day 2 | intercept **three An-225** carrying Silkworm missiles, group *Alpha*, 60 NM NNE | all three An-225 destroyed | 3× An-225 + MiG-27 escort (target of opportunity) |
| 1.5 | `C01M05` | **Double Down** | 1000, day 3 | destroy *Flight Group Alpha* so Viper Squadron can bomb the **Nanuchka missile boats**; 40 NM SE | *Alpha* destroyed **and** Viper Squadron's bomb run completed | MiG-29 CAP, Nanuchka boats, SAM |
| 1.6 | `C01M06` | **Four of a Kind** | 1300, day 4 | escort four **C-5 Galaxy** (*Mule Squadron*), 65 NM W | all four Galaxies survive | MiG-27, MiG-29 from the north |
| 1.7 | `C01M07` | **Aces Low** | 0230, day 5 | destroy the **EF2000** of *Flight Group Epsilon* on the ground at **Chiang Rai**, 70 NM NW (their takeoff 0315) | all EF2000 destroyed | EF2000, MiG-27/MiG-29 CAP, **SA-2**, radar |
| 1.8 | `C01M08` | **Black Mariah** | 0300, day 6 | fighter sweep ahead of *Vampire Squadron* (**B-1B**) onto **Objective Talbot** — Chang's command centre in Laos, 85 NM N. **Night** | enemy command centre destroyed **and** Vampire Squadron survives | EF2000, MiG-29, MiG-27, **SA-2**, **SA-9** |

Names, times and short texts `[ORF]`; tasks and stated distances `[TXT]`.

Verbatim short descriptions `[ORF]`:

```
~31.1 Snake Eyes~2 The first mission in Southeast Asia, Storm Squadron is called upon to establish
air superiority over Chang Tzu Mings Mig-27s.
~31.3 Luckiest Man in Laos~2 Objective Madison is a bridge over the Mekong River being used by
Warlord forces to supply units in Laos.
~31.8 Black Mariah~2 Final bombing of Chang Tzu Mings base in Laotian territory. […] This mission
will be flown under the cover of night.
```

**Three of eight missions are night or pre-dawn** (1.7 at 0230, 1.8 at 0300, 1.3 at 1800 dusk). That is
a renderer and sensor requirement, not decoration.

### 3. Reading rule per mission

The `.fbm` header in this tree carries how its exit code is to be read (`CLAUDE.md`). None of these
missions exists yet, so the rule is stated here as **intent** — one sentence, the thing that decides.

| # | Reading rule |
|---|---|
| **1.1** | SUCCESS only when every aircraft of group *Alpha* **and** every other hostile air contact inside the box is destroyed and Storm Leader is recovered; *Alpha* down with one stray MiG alive is a mission loss, not an engine defect. |
| **1.2** | SUCCESS only when the assembly-house object reads destroyed; dying to the SA-2 belt on egress is a mission loss, but an SA-2 that never launches is an engine defect and must be read as one. |
| **1.3** | SUCCESS only when **every span** of the bridge object reads destroyed — a partly dropped bridge is a fail, and a bridge that dies to a single hit is a damage-model defect, not a win. |
| **1.4** | SUCCESS only when all three An-225 are destroyed **before they leave the box**; the MiG-27 escort is a target of opportunity and killing it proves nothing. |
| **1.5** | Two conditions, both required — *Alpha* destroyed **and** Viper Squadron's bomb run completed; the run where *Alpha* dies and Viper dies anyway must be distinguishable in telemetry from the run where *Alpha* survives, because they fail for opposite reasons. |
| **1.6** | SUCCESS only when all four C-5 are alive at their exit waypoint; own kills are irrelevant to the verdict and must not enter it. |
| **1.7** | SUCCESS only when every EF2000 is destroyed **while still on the ground**; a kill after rotation still counts as a kill, so the telemetry must carry each EF2000's takeoff time or the two cases collapse into one number. |
| **1.8** | SUCCESS only when the command centre is destroyed **and** Vampire Squadron survives; the F-22 sweep is required to kill nothing at all — the verdict hangs entirely on someone else's aircraft, which makes this the campaign's only true escort-of-strike test. |

### 4. Unit inventory per mission `[ORF]`

Each `.ORF` names the `.PAK` models it loads. That list is the exact inventory of the mission.

Base set present in **every** mission: `F22` `RUNWAY` `MIG27` `HNG_03` (hangar) `FUELTANK` `F15`
`TWR_02` (tower).

| Mission | Additional to the base set |
|---|---|
| 1.1 | — |
| 1.2 | `ARTEMIS` `VTNM_04` **`SA2`** `RBB_H` |
| 1.3 | `CTR` `CTRMP` `ARTEMIS` `MIG29` `TWR_02N` |
| 1.4 | `AN225` |
| 1.5 | `NANUCHKA` `MIG29` `DOT` |
| 1.6 | `MIG29` `C5` `AWACS` |
| 1.7 | `EF2000` `RDAR_03` **`SA2`** `ARTEMIS` `TWR_02N` |
| 1.8 | `THAP2` `EF2000` `ARTEMIS` **`SA2`** `B1B` `RBA_H` `TWR_02N` `VTNM_04` `MIG29` **`SA9`** |

**SAM types in campaign one are therefore closed: SA-2 in 1.2, 1.7, 1.8; SA-9 additionally in 1.8. No
SA-6 anywhere in this campaign** — the manual documents SA-6 `[MAN p.93–95*]` but the data does not load
it here.

No vehicle columns, no infantry — neither appears in any `.ORF` of this campaign, and the manual's
enemy chapter `[MAN p.75–97*]` has no entry for either.

### 5. Briefing text against measured data

The briefings round distances to 5 NM and give directions only as compass octants. The `.ORF` data is
exact. **Where they disagree the measurement wins** — it is what the game actually places.

| # | Briefed | Measured `[ORF]` | Δ | Briefed octant | Measured bearing | Δ |
|---|---|---|---|---|---|---|
| 1.1 | 50 NM | 54.3 | +4.3 | NNW 337.5° | 345.1° | +7.6° |
| 1.2 | 45 NM | 45.6 | +0.6 | NE 45° | 39.4° | −5.6° |
| 1.3 | 35 NM | 36.1 | +1.1 | NE 45° | 60.3° | +15.3° |
| 1.4 | 60 NM | 56.5 | −3.5 | NNE 22.5° | 43.0° | +20.5° |
| 1.5 | 40 NM | 40.4 ¹ | +0.4 | SE 135° | 156.2° | +21.2° |
| 1.6 | 65 NM | 65.3 | +0.3 | W 270° | 246.2° | −23.8° |
| 1.7 | 70 NM | 71.2 | +1.2 | NW 315° | 335.5° | +20.5° |
| 1.8 | 85 NM | 87.0 | +2.0 | N 0° | 17.6° | +17.6° |

¹ for the Nanuchka boats, the actual target area; the MiG-29 CAP sits at 34.0 NM / 158.5°.

**Distances are good** (worst 4.3 NM, inside the rounding frame). **Bearings are consistently coarse —
up to 23.8° off.** That is the whole return on measuring: the prose cannot be used to place anything.

Re-derived here `[DERIV]` from the per-mission offsets in [`terrain.md`](terrain.md) §3: every
distance and every bearing in this table reproduces to ±0.1 NM / ±0.1°. The table is internally sound.

### 6. Finding: "Objective Talbot" names two different places

| Mission | Object at Talbot | Measured from FOB Tyler `[ORF]` |
|---|---|---|
| 1.2 | assembly house | 45.6 NM, bearing 39.4° |
| 1.8 | command centre | 87.0 NM, bearing 17.6° |

`[DERIV]` separation of the two Talbots = **88.3 km**, computed in the game's own measured offsets
from FOB Tyler ([`terrain.md`](terrain.md) §3) so that no geodetic assumption enters:
`hypot(48.7 − 53.6, 153.5 − 65.3) = hypot(−4.9, 88.2) km`. Cross-check via the reconstructed
lat/lon gives 87.9 km — the 0.4 km spread is the reconstruction's own rounding, not a second result.

That is not measurement error at any plausible scale. **The game issues one codename twice.** Kept as
found; a rebuild must decide which one it means and say so per mission, not silently merge them.

### 7. Player platform (what the campaign assumes you fly)

| Item | Value | Provenance |
|---|---|---|
| Designation | Lockheed-Martin **F-22 Lightning II** ("Raptor" was not assigned until 1997) | `[MAN p.5]` verbatim spec block |
| Wingspan / length / height | 44 ft 6 in · 62 ft 1 in · 16 ft 5 in | `[MAN p.5]` |
| Engines | 2× Pratt & Whitney **F119-PW-100**, 35,000 lb class, thrust vectoring | `[MAN p.5]` |
| Max speed sea level | **800 kt** | `[MAN p.5]` |
| Supercruise | **Mach 1.58** | `[MAN p.5]` |
| Max G | **+9** | `[MAN p.5]` |
| Thrust/weight | 1:1 | `[MAN p.5]` |
| Radar | **AN/APG-77** | `[MAN p.5]` |
| Defensive suite | **AN/ALR-94**, Sanders / General Electric, integrated RWR + ECM | `[MAN p.5]`, `[MAN p.71–74]` |

Stores — **internal only**, no external pylons `[MAN p.84–85]`:

| Weapon | Detail | Provenance |
|---|---|---|
| **AIM-120C AMRAAM** | radar-guided, BVR; **requires own radar on to launch** | `[MAN p.5]`, `[MAN p.32*]` |
| **AIM-9X Sidewinder** | IR, all-aspect; launchable with radar off | `[MAN p.5]`, `[MAN p.32*]` |
| **GBU-30 (V)-1 JDAM 1000** | Mk. 83, nose cap + tail guidance section, **two** on hydraulic racks in the ventral bay; emits nothing, needs no uplink | `[MAN p.84–85]` verbatim |
| **M61A2 20 mm Vulcan** | six barrels, **480-round magazine**, ~6,000 rds/min ≈ 100 rds/s | `[MAN p.85]` verbatim |
| Chaff | **100 bundles** | `[MAN p.71–74]` verbatim |
| Flares | **100** | `[MAN p.71–74]` verbatim — *the source research recorded this as "not quantified"; it is quantified* |

JDAM release envelope, from the training briefing `[ORF]`: released from **10,000 ft at 500 nmph** a
JDAM flies **8 miles** forward and tolerates up to **1 mile** of lateral offset.

Opening the weapons bay **raises the radar signature** and this is visible in real time as the enemy
detection circle widening `[MAN p.37*]`.

### 8. Contradiction in the source, unresolved

| Gun effective range | Where |
|---|---|
| "Effective range with guns is **1.5 nautical miles**" | `[MAN p.30]` — verbatim, page footer "30" survives OCR |
| "effective range of **0.5 km** with a maximum fall-off range benchmarked at 3 kilometres" | `[MAN p.85]` — verbatim, page footer "85" survives OCR |

1.5 NM = 2.78 km ≠ 0.5 km — factor 5.6, inside one manual. Not resolvable without measuring in the
game; no such measurement was made. **Both are recorded; neither is chosen.**

### 9. Enemy inventory

Campaign one, measured `[ORF]` — see §4. Aircraft: MiG-27 Flogger (all 8), MiG-29 Fulcrum (1.3, 1.5,
1.6, 1.8), EF2000 Eurofighter (1.7, 1.8), An-225 Cossack (1.4). Ships: Nanuchka (1.5). Radar:
`RDAR_03` (1.7).

Whole game, from the manual `[MAN p.86–97*]`:

| Class | Entries |
|---|---|
| Aircraft | A-50 Mainstay AWACS · An-225 Cossack · MiG-27 Flogger · MiG-29 Fulcrum (manual highlights its **helmet sight**) · Su-27 Flanker · Tu-160 Blackjack. **EF2000 has no manual entry** but exists as `EF2000.PAK` and in the briefings |
| Air-to-air missiles | **R-27RE / AA-10c "Alamo C"** (SARH) · **R-73A / AA-11 "Archer"** (IR, ~5 miles, small warhead) |
| SAM | **S-75 Divina / SA-2 Guideline** — two-stage, beam rider with command guidance, slant range from 35 km · **SA-6 Gainful** — self-propelled, Mach 2.8, battery = 6 launchers × 3 + "Straight Flush" radar, from 30 km · **SA-9 Gaskin** — TEL with 4 IR rounds, all-aspect, warhead only **2.6 kg** |
| AAA | ZPU-4, S-60, heavier towed guns; ZSU-23/4 as an accompanying system |
| Ships | **Nanuchka-III class, two triple SS-N-9 "Siren" launchers**. The manual's spelling "Nanchuka" is an error, corrected in the patch readme |

Friendly `[MAN p.75–79*]`: E-767 AWACS · F-14B · F-15C · F-16C · C-5A/B Galaxy. Archive adds
`B1B.PAK` (Vampire, 1.8) and `AWACS.PAK` (1.6).

### 10. Wingman command set `[MAN]`

The player commands exactly one wingman, and the command list is short — this is the declaration list a
wingman module would have to carry (`doc/mods.md` §2.1).

| Command | Effect, verbatim intent |
|---|---|
| Cover | attack any enemy aircraft holding a **current radar lock on the player** |
| Engage | engage enemy targets at will |
| Form on wing | take station **off the right wing** and hold until ordered otherwise |
| Attack my target | attack the target the player currently has locked on radar |
| Patrol home base | return to home base and fly CAP over it |

The manual states the bound explicitly: *"You can issue only the most general of orders. Your wingman
has a large degree of freedom with which to carry out your directives."*

### 11. What campaign one demands of the engine

Straight out of §2–§4, in the form `doc/mods.md` §2 asks for — what the title needs that cannot be
declared today:

| Need | Missions | Declarable today? |
|---|---|---|
| Escort verdict on a **third party's** survival | 1.5, 1.6, 1.8 | mission goal format carries own outcome, not "these four foreign units live" |
| Multi-span structure with per-span damage | 1.3 | no |
| Kill-before-takeoff, i.e. a verdict against a **clock the enemy owns** | 1.7 | no |
| Night and dusk | 1.3, 1.7, 1.8 | renderer question, open |
| Large transports as targets (An-225, C-5, B-1B) | 1.4, 1.6, 1.8 | `FBSystemId` is a closed 14-entry aircraft enum |
| Naval target on inland water | 1.5 | no ship model at all |
| Briefing prose per mission | all | `.fbm` carries a reading rule, not a briefing |

## State

**All eight sorties are built and run.** `mod.json` + `src/missions/c01m0{1..8}-*.fbm` +
`src/campaigns/c01-mekong.fbc`; every substitution and its measured cost is in
[`substitutions.md`](substitutions.md). There is still no F-22 flight model and none was invented —
Storm Squadron flies `module f16`, and the whole substitution table is that document.

`fb-gym --mod mods/f22 --mission <name>`, measured, each read by **its own file's** reading rule and
not by the exit code:

| # | File | Exit | Read as | What decided it |
|---|---|---:|---|---|
| 1.1 | `c01m01-snake-eyes` | 3 | **loss** | 1 of 2 Floggers down after ten AIM-120 |
| 1.2 | `c01m02-party-crashing` | 3 | **success** | assembly house destroyed (`aimErrM` 37.2); the SA-2 fired all six and missed |
| 1.3 | `c01m03-luckiest-man-in-laos` | 1 | **loss** | both bombs arrive at 27.6 / 34.8 m; a `target_hard` span needs ~8 m |
| 1.4 | `c01m04-silkworm-jungle` | 0 | **success** | all three transports inside `until 548` |
| 1.5 | `c01m05-double-down` | 1 | **loss** | both boats destroyed, Storm Leader shot by the MiG-29 pair |
| 1.6 | `c01m06-four-of-a-kind` | 3 | **success** | four Galaxies home, four `protect` met, zero kills |
| 1.7 | `c01m07-aces-low` | 1 | **loss** | the SA-2 belt kills the strike 12.1 km short of the ramp |
| 1.8 | `c01m08-black-mariah` | 1 | **not measurable** | the sweep absorbs the SA-2 magazine and its loss ends the run 210 s before the strike would release |

Determinism: telemetry byte-identical over `--threads 1/2/4` for all eight, `events.log` identical
modulo `wallS`/`speedup`/the output path.

**Three of eight fly their own success condition. §3's reading rules are all satisfiable except
1.7's rotation clause and 1.8's escort verdict** — see `## Gaps`.

## Gaps

- **§11's table is wrong on one row and it was wrong when it was written.** "Escort verdict on a third
  party's survival" is declarable: `objective protect unit <callsign>` has existed since round `C12`
  ([`doc/missions/verdict.md`](../../../doc/missions/verdict.md)) and sorties 1.5, 1.6 and 1.8 use it.
  1.6 measures it green — four `protect` met, zero kills. The rest of §11 stands.
- **§11's "kill-before-takeoff" row is half wrong too.** `objective ... until <s>` declares the clock
  (used in 1.4 and 1.7). What is NOT declarable is the enemy owning it: nothing schedules a take-off,
  so 1.7's parked Eurofighters never roll and **§3's requirement that the telemetry separate a kill
  before rotation from one after it cannot be met at all**.
- **Sortie 1.8 cannot measure its own subject, and this is the round's most valuable finding.** The run
  ends at the first decisive failure and at the first physical K.O., so a package mission in which any
  member dies first is truncated before the thing it exists to measure. Measured: the escort absorbing
  an S-75 magazine at t = 123–167 s ends the run at t = 182.7 s with the strike 55 km short. No
  declaration available today avoids it.
- **`set task attack` cannot deliver a guided bomb.** Four measured deliveries in
  [`substitutions.md`](substitutions.md) §6.1: the automatic attack phase releases and turns, the laser
  designation breaks in flight, and the miss grows with the time of flight (12.7 s → 122 m; 25.6 s →
  349 m). A mod has to compute the release instant itself.
- **No air-to-ground delivery in this tree kills a `target_hard` from level flight.** Best of four
  attempts: 23.8 m against a ~8 m radius. Sortie 1.3 is red for that reason and no other.
- **Every combat outcome above is ONE run of ONE geometry.** `doc/doctrine-evolution.md` exists because
  single runs of combat missions flip on a spawn grid; the delivery numbers (nothing shoots before the
  release) are the trustworthy ones and the air-to-air verdicts are not.
- **The `.ORF` measurements are second-hand.** `RESOURCE.RES` was not fetched or parsed in this run.
  Everything tagged `[ORF]` rests on the source research's parse. Its arithmetic reproduces exactly
  (§5, and [`terrain.md`](terrain.md) §2), which tests consistency — **not** that the bytes were read
  correctly. First rebuild step should re-parse the archive.
- **Bytes 12–39 of each 40-byte object record are not decoded** — they carry type, squadron assignment
  and behaviour. Without them a mission can be placed but not populated with the right unit identity.
- **`.REF` files not examined** — 49 of them, each exactly 24,576 bytes. Suspected terrain-tile or
  waypoint data. If they hold the waypoint routes, the missions' navigation legs are recoverable;
  today they are not.
- **Waypoint routes are unknown.** The Navigation Display documents four waypoint shapes and their
  meaning `[MAN p.38*]`, but no actual route for any of the eight missions has been read.
- **The second string in each `.ORF`** ("and Robbery", "kin'", "live") is uninterpreted.
- **Enemy behaviour is undocumented.** No source read here says how the MiG CAP is triggered, when the
  EF2000 in 1.7 actually rolls, or whether Viper/Mule/Vampire fly a scripted path. Every one of those
  is load-bearing for the reading rules in §3.
- **Gun range contradiction unresolved** (§8) — a rebuild must pick one and record the choice as a
  deliberate deviation, not silently.
- **No Let's Play with a documented mission order was found.** Four candidates were checked by the
  source research and none carried chapter marks or a description; the videos were not watched. The
  order 1.1–1.8 rests on the `.ORF` numbering alone, which is strong but is a filename, not a
  demonstrated sequence.
- **Whether the campaign branches** — whether failing 1.3 changes what 1.4 is — is unknown. Nothing
  read says it does; nothing read says it does not.
