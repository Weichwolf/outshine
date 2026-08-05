# Substitutions — what campaign one flies instead of what it names

> The F-22 does not exist in this tree and neither do most of its opponents. **No flight model was
> invented** (`CLAUDE.md` Prinzip 1). Every role in [`campaign.md`](campaign.md) §2/§4/§9 is mapped
> onto the nearest EXISTING module, and every mapping is recorded here with the direction of its
> error. Form per [`doc/mods.md`](../../../doc/mods.md) §3. A mod has no `test/`: the missions are the
> test.
>
> Sources for the module rows: `sim/src/core/FBAircraft.h` (`kAircraftCatalogue`, 20 rows),
> `sim/src/core/FBSite.h` (`kSiteCatalogue`, 9 rows), `sim/src/modules/ground/FBGroundTarget.h`
> (2 rows), `sim/src/core/FBStore.h`. Provenance tags as in `campaign.md`.

## Spec

### 1. The rule

**One role, one substitute, one direction of error, stated.** Where two candidates existed the
refused one is named with its reason — a substitution whose alternative is invisible is not a
decision, it is a preference.

**The decisive property wins.** Guidance beats warhead weight; span and speed beat engine count;
IR-versus-radar beats wheeled-versus-shouldered. What decides an engagement in this tree is what the
row is chosen on.

### 2. Aircraft

| Role in campaign one | Substitute | Why this row | Direction of the error |
|---|---|---|---|
| **F-22 Lightning II** (player, Storm Squadron) | `f16` | the only fully composed player module with an FLCS, a HUD, an air-to-ground phase and AIM-120/AIM-9 | **against the original in every axis**: no supercruise (M 1.58 `[MAN p.5]`), no thrust vectoring, no reduced signature — the F-16 is the LOUDER aircraft on every radar in the tree, so every detection range and every SAM engagement against Storm Squadron is an UPPER bound. +9 g is free (both limit at +9). |
| **MiG-27 Flogger-D** (all 8 sorties) | `mig23` (MiG-23MLD) | the MiG-27 **is** the ground-attack Flogger — same swing-wing airframe, same family | **large, against the player.** The real MiG-27 had NO air-intercept radar (a laser rangefinder); the MLD row sees 52 km, tracks 39 km, looks down 14 km head-on and shoots two R-24R. A poor Blue result says less than it looks like it says. |
| **MiG-29 Fulcrum** (1.3, 1.5, 1.6, 1.8) | `mig29` | the tree's own module | **none.** Exact. |
| **EF2000 Eurofighter**, airborne (1.7, 1.8) | `f15c` (F-15C) | the catalogue's only non-Soviet **T4 Peer** row: twin-engine, 8 stations, a gun, and a MEASURED roll plant, so it can fight rather than merely fly | no supercruise, no canard-delta, no helmet sight; a 160 km APG-63 against a Captor whose reach no source read gives. Peer-class western fighter for peer-class western fighter. |
| **EF2000 parked on the ramp** (1.7) | `target_soft` | see §4 and §6.2 | it cannot take off, so the sortie cannot fail the way the original could |
| **An-225 Cossack** (1.4) | `kc135` (KC-135R) | the catalogue's only large four-engine jet. Span 39.88 m against 88.4 m and against `an26`'s 29.3 m; cruise 237 m/s against ~222 m/s and 122 m/s | **span is 45 % of the original**, so visual acquisition happens LATER than it would against the real aircraft. It carries and drops nothing; the Silkworms exist only in the briefing. |
| **C-5 Galaxy** (1.6) | `kc135` | same row, same reason | span 59 % of the original; cruise 237 against ~245 m/s — the speed is right |
| **B-1B Lancer** (Vampire Squadron, 1.8) | `f16` + `mk84` | **the campaign's largest substitution.** `tu95` is the only heavy bomber and it is a MOVER: `Stations` 0, no fire control, nothing it can drop — and `set task attack` exists ONLY on the `f16` and `mig29` modules, so no catalogue row has an air-to-ground phase at all | enormous, both ways: two Mk-84 instead of 84 Mk-82; a 9 g airframe that defends itself instead of a bomber that cannot; no B-1B signature, altitude profile or speed |
| **AWACS** (`AWACS.PAK`, 1.6) | `e3` (E-3 Sentry) | same airframe family as the manual's E-767, same role; holds an orbit, publishes an anonymous `FBNetReport` | not wired into a `net` block: nothing read says the game's AWACS cued anybody, and wiring one would be inventing doctrine |

### 3. Ground positions

| Role | Substitute | Why | Direction of the error |
|---|---|---|---|
| **SA-2 Guideline / S-75** (1.2, 1.7, 1.8) | `sa2` | the catalogue row, with its own envelope, magazine and command guidance | **none.** Exact. |
| **SA-9 Gaskin** (1.8 only) | `sa18` (9K38 Igla) | the SA-9 is a vehicle with four **infrared, all-aspect** rounds, 2.6 kg warhead `[MAN p.86–97*]`. The catalogue has no vehicle-mounted IR SAM. `sa18` matches the **guidance principle** — IR, all-aspect (`OpticalAzHalfDeg 180`), flare-defeatable, 5.2 km, 3 500 m ceiling | it does not move, it carries two rounds not four, and `FBSite.h` states outright that the Igla's documented flare resistance is not modelled. **`sa8` was refused**: a vehicle, but RADAR-command-guided, and guidance is what decides an engagement. |
| **AAA** at 3 000 m (1.2) | `zsu23` | radar-directed, the class the `.ORF`'s `ARTEMIS`/`VTNM_04` implies | **it never fires**: its ceiling is 1 500 m and the run-in is at 3 000 m. Declared anyway — a gun that cannot reach is a measurement, an absent gun is a gap. |
| **AAA** at 900 m (1.3) | `zu23` | optically laid, so its detection is `sensors/FBVisualSystem`'s own measured behaviour rather than a number in a table. At 1800 local it CAN see | **the campaign's only working gun engagement** (measured: 8 bursts, 100→14 rounds). It would see nothing at all in the two night sorties, which is why they carry no gun. |
| **`RDAR_03`** (1.7) | `p18` | an early-warning antenna with no weapon: it finds and it radiates, which is all the model name claims | none worth naming |

### 4. Structures — and the rule that assigned the fragility class

FlightBox has exactly **two** ground-target classes: `target_soft` (a Mk-84 near miss finishes it out
to tens of metres) and `target_hard` (~8 m to be finished, ~15 m to be hurt,
`modules/ground/FBGroundTarget.h`). **No source read gives hardening for any object in this
campaign.** The assignment is therefore made from the `.PAK` model names the `.ORF` loads:

| Object | Model `[ORF]` | Class | The argument |
|---|---|---|---|
| assembly house, Talbot (1.2) | `RBB_H` | `target_soft` | a house |
| command centre, Talbot (1.8) | `RBA_H` | `target_soft` | **same suffix as 1.2's house**, so the same object class, so the same fragility class. From the data, not from convenience |
| bridge span, Madison (1.3) | `CTR` / `CTRMP` | `target_hard` | a different model family from both `*_H` rows, and the one thing in this campaign that is structure rather than building |
| Nanuchka missile boat (1.5) | `NANUCHKA` | `target_soft` | a thin-hulled corvette; the hard class describes a bunker |
| parked EF2000 (1.7) | `EF2000` | `target_soft` | an unarmoured airframe on a ramp |

### 5. Weapons

| Role | Substitute | Why | Direction of the error |
|---|---|---|---|
| **AIM-120C AMRAAM** | `aim120` | the tree's own row | none |
| **AIM-9X Sidewinder** | `aim9` | the tree's own row | the AIM-9X is a later, off-boresight round; `aim9` is not |
| **M61A2, 480 rounds** `[MAN p.85]` | the F-16's M61A1 with `set gun_rounds 480` | the manual's own magazine number is declarable on an airframe that holds 510 | the barrel is the older variant |
| **GBU-30 (V)-1 JDAM 1000** | `mk84` | see §6.1 — **this is the campaign's most consequential substitution and it is measured** | guidance absent (30 m instead of ~10 m), warhead 2 000 lb instead of 1 000 lb, autonomy right |
| **100 chaff + 100 flares** `[MAN p.71–74]` | 60 + 60 | the F-16's ALE-47 holds **120 combined** (`FBF16Cmds::kMaxCombined`); 100+100 is REFUSED at spawn with "chaff + flare exceeds 120 combined" | **no sortie in this campaign can measure the original's countermeasure depth** |

## State

All eight `.fbm` files in `../src/missions/` are built and run. Substitutions §2–§5 are the whole set;
each `.fbm` repeats only the ones it uses, in its own header.

### 6. What the substitutions cost, measured

#### 6.1 The JDAM, four ways

Measured on sortie 1.3's geometry against two `target_hard` spans (`kill` radius ~8 m):

| Store | Task | Run-in | Result |
|---|---|---|---|
| `mk84` | `attack` / ccrp | 900 m, 450 kt | `aimErrM` **27.55** and **34.77** — both spans INTACT |
| `gbu12` | `attack` / ccrp | 900 m, 450 kt | `missile ILLUMINATION_LOST` at `tofS` 12.71; **122.43** and **140.50** |
| `gbu12` | `attack` / ccrp | 4 000 m, 450 kt | ILLUMINATION_LOST at `tofS` 25.61; **349.01** and **343.80** |
| `gbu12` | `route` + `brief_release_s 267` | 4 000 m, 250 kt | lase HELD; **23.83** and **25.28** — spans STILL INTACT |

Two results, and the second is the one worth keeping:

1. **`mk84` is the substitution**, because it is the best of the four numbers and because its named
   error (guidance) is the honest one — the JDAM's own singled-out property `[MAN p.84–85]` is that it
   "emits nothing, needs no uplink", which the semi-active `gbu12` contradicts outright.
2. **`set task attack` cannot deliver a laser-guided weapon.** The automatic attack phase releases and
   turns away, the designation breaks in flight, and the miss grows with the time of flight. The only
   LGB delivery this tree demonstrates is a hand-computed briefed release on `set task route`
   (`mods/f16/src/missions/lgb-designate.fbm`). **A mod cannot declare "attack this target with a
   guided bomb."**

#### 6.2 Roles with no substitute at all

| Role | Sortie | What is missing |
|---|---|---|
| **a warship** | 1.5 | no ship module of any kind: no hull, no naval movement, no SS-N-9, no anti-ship weapon. The "Nanuchka" is a stationary soft object |
| **an aircraft that takes off on a schedule** | 1.7 | nothing declares "this unit starts rolling at t = X". The parked Eurofighters are objects; campaign.md's demand that the telemetry separate a kill before rotation from one after it **cannot be met** |
| **a multi-span structure** | 1.3 | a bridge is N independent objects or it is one. Dropping one span does not weaken its neighbour |
| **a bomber that bombs** | 1.8 | `tu95` has no stations and no air-to-ground phase; no catalogue row does |
| **a briefing** | all | `.fbm` carries a reading rule, not prose. The eight briefings survive only as header comments |

## Gaps

- **`f15c`'s fire control was not re-measured here.** `mods/f16/src/missions/w2-06-escort.fbm` records
  that O5 measured it to "compose no fire control at all"; `FBAircraftSpec::CanEmploy` says it should
  (T4 with 8 stations), and `air-eagle-amraam.fbm` flies it shooting. The two statements are not
  reconciled in this run, and sortie 1.7's F-15C never entered its fight, so this campaign has no
  measurement either way.
- **The MiG-27 substitution was never checked against `make -C sim test-air mig23`**, which prints
  that deck's own anchors. `air-bomber-intercept.fbm` records one of seven outside band (the service
  ceiling). Nothing here depends on it, but nothing here confirms it either.
- **No substitution was cross-checked against a second run.** Every number in §6.1 is one run of one
  geometry. `doc/doctrine-evolution.md`'s spawn-grid work exists precisely because single runs of
  combat missions flip; the delivery numbers (no combat before release) are the trustworthy ones and
  the air-to-air outcomes are not.
- **`sa18` for the SA-9 understates the threat twice over** — two rounds instead of four AND no flare
  resistance — and sortie 1.8 never got close enough for it to fire, so the choice is unmeasured.
- **The 480-round magazine was never emptied.** No sortie in this campaign reached a gun engagement,
  so `set gun_rounds 480` is a declaration nothing has yet exercised.
