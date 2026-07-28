# O1 — Bekaa Valley 1982, the Syrian side (the canonical defeat)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet. It is also **the
yardstick campaign**: the one whose question is not "can the MiG win" but "**what in the doctrine
moves the outcome, and what does not move at all**". That framing is stated once, as a measurable
question, in [`INDEX.md`](INDEX.md) §"Bekaa as the yardstick" and derived here in §Knowledge 3.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Operation Mole Cricket 19, 9 June 1982 | §Knowledge 1, cited and tiered, with the disputes left standing |
| **FlightBox sources** | what a `.fbm` can declare and what the MiG-29 module does | [`../modules/mig29/module.md`](../modules/mig29/module.md), [`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md), [`../modules/mig29/defence-rwr-cm.md`](../modules/mig29/defence-rwr-cm.md), [`../duels.md`](../duels.md), [`../missions/combat.md`](../missions/combat.md) |

Confidence legend and gap IDs `C0…C21`: [`INDEX.md`](INDEX.md).

### Temporal honesty — read this before anything else

**The MiG-29 was not at Bekaa.** The type entered Soviet service in 1983; the Syrian side flew
**MiG-21, MiG-23 and Su-20** on 9 June 1982 [T4]. This campaign therefore treats Bekaa as a
**scenario archetype** — the *situation* is the anchor, not the serial number:

> a ground-controlled fighter force, dependent on a SAM belt it must not fly over and on a controller
> it cannot fight without, is committed piecemeal against a swept, jammed, airborne-directed opponent.

Two consequences must be written into every mission header of this campaign:

1. **The substitution makes the defender STRONGER than history.** The 1982 MiGs had "only nose and
   tail alert radar systems and no side warnings or look-up and look-down systems" [T4]. FlightBox's
   MiG-29 has the SPO-15 with eight azimuth channels, a look-down-capable N019 and an IRST. A
   FlightBox Bekaa that still ends in a rout says something *stronger* than history did; one that ends
   in a draw says less than nothing about 1982.
2. **The decisive Israeli mechanism cannot be modelled at all.** The anchor's own explanation is that
   selective communications jamming cut the MiGs off from their controllers [T4]. FlightBox has no
   jamming (`C13`). The campaign's substitute is to **delete the GCI brief** — a cruder, honest
   stand-in whose difference from jamming is stated in §Knowledge 3.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Date | **9 June 1982**, the opening of the 1982 Lebanon war | [T4] |
| Israeli objective | destroy the Syrian SAM belt in the Bekaa, then defeat the fighter response | [T4] |
| SAM belt | **30 batteries** of SA-2, SA-3 and SA-6; **29 destroyed**, plus 6 more that night and the following day | [T4] |
| Israeli method — step 1 | **RPVs (Tadiran Mastiff, IAI Scout) sent in first** to make the SAM radars radiate by presenting false aircraft; the emissions were relayed to E-2Cs and analysed | [T4] |
| Israeli method — step 2 | **Boeing 707 ECM aircraft** flooded the Syrian command network; "selective airborne communications jamming disrupted the airwaves for the MiG-21s and MiG-23s and **cut them off from ground control**" | [T4] |
| Israeli method — step 3 | **E-2C Hawkeye as airborne battle management** — RPVs over Syrian airfields reported take-offs, and the E-2Cs guided Israeli fighters to attack **from the beam**, where Syrian radar warning had no coverage | [T4] |
| Israeli method — step 4 | Sparrow shots from **22–40 km**, outside Syrian radar range | [T4] |
| Syrian aircraft | MiG-21, MiG-23, Su-20 | [T4] |
| Syrian sensor limitation | nose- and tail-only radar warning; **no side warning**, no look-up/look-down | [T4] |
| Syrian tactic | "do what they can, then run back for cover" inside the protective missile envelope | [T4] |
| Syrian losses | **82–86 aircraft** | [T4] |
| Israeli losses | 2 F-15 damaged; at least 1 RPV lost; **none in air-to-air combat** | [T4] |
| Soviet counter-claim | **67 Israeli aircraft** shot down — **[DISPUTED]**, described by the source as "widely dismissed" | [T4]/[DISPUTED] |
| Anchor region | Bekaa Valley ≈ 33.6–34.2 N 35.8–36.2 E; Rayak ≈ 33.85 N 35.99 E; Damascus/Mezzeh ≈ 33.48 N 36.22 E (**approximate, verify**) | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| **The question is not "does the MiG win"** | it is: *which of the doctrine levers FlightBox can express moves the outcome, by how much, and what remains after the best combination of them.* The residue is the structural part |
| The levers are **mission text**, never code | GCI present/absent (`set brief_gci`), emission policy (`n019_emission illum\|dummy\|off`), commit range (`pilot_lock_nm`), launch doctrine (`pilot_shot_rtr`), altitude band (the `wp` set), formation contract (`brief_sort`), reaction time (`pilot_react_s`). A lever that requires a new class is not a doctrine, it is a rebuild |
| Each mission changes **exactly one** lever from its predecessor | the campaign is a designed experiment, not ten scenarios |
| The verdict is the outcome band, not a win | reported as the tournament reports it: outcome dominates, craft orders within equal outcomes ([`../missions/combat.md`](../missions/combat.md)) |
| **Ground targets in every mission** | the SAM belt is the campaign's real subject; every mission carries the belt as ground units even while they are inert (`C1`), so that the day they become active nothing about the missions has to move |
| The substitution disclosure is part of the mission | every header states the archetype substitution and its direction (defender stronger than history) |

### 3. The ten missions

Blue = Israeli side (F-16 module). Red = Syrian side (MiG-29 module) — **the flying side of this
campaign**.

| # | Mission | Task | Time | Wx | Red (ours) | Blue | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `o1-01-controlled` | GCI-led intercept, everything working | day | calm | 2 MiG-29 (flight), full `brief_gci`, `n019_emission off` until commit | 2 F-16 | 3 `target_soft` (SAM belt) + 1 `target_hard` (site HQ) | `kill team hostile` + both `survive` | **The baseline.** With the architecture intact and the F-16 flown to its own doctrine, what happens? Everything else in the campaign is a delta on this run |
| 2 | `o1-02-no-gci` | mission 1 with `brief_gci` deleted | day | calm | 2 MiG-29, **no GCI** | 2 F-16 | as above | as above | **The campaign's headline experiment.** The stand-in for the 707s' jamming. How much of the outcome is the controller? (`w3-08` asks the same question in a different theatre — the two must agree or one of them is wrong) |
| 3 | `o1-03-beam-attack` | Blue attacks from the beam | day | calm | 2 MiG-29, full GCI | 2 F-16 entering at 90° aspect | as above | as above | The anchor's core tactic. FlightBox's SPO-15 has **eight azimuth channels**, so the beam is *not* a blind spot for it the way it was in 1982 — how much of the Israeli advantage survives an opponent that can hear you from the side? |
| 4 | `o1-04-blind-beam` | mission 3 with `set rwr off` on the Red flight | day | calm | 2 MiG-29, **no RWR** | 2 F-16 beam entry | as above | as above | The crude reconstruction of the 1982 sensor handicap. Does the beam attack become decisive when the warning receiver is gone? |
| 5 | `o1-05-emcon` | Red never radiates | day | calm | 2 MiG-29, `n019_emission off` all run, KOLS armed | 2 F-16 | as above | as above | The MiG-29's own answer to being hunted: see without being seen. Blocked in part by `D3` ([`../duels.md`](../duels.md)) — **the pilot does not use the IRST**, so "silent" today means "silent and blind" |
| 6 | `o1-06-early-launch` | Red shoots at 1.4 × Rtr | day | calm | 2 MiG-29, `set pilot_shot_rtr 1.4`, `pilot_lock_nm 16` | 2 F-16 | as above | as above | The one lever already **measured** to be worth an entire outcome band to this airframe (`duels.md`: `mig_base` −393.7 → `mig_long` +585.0). Does it still pay when the opponent is a flight rather than a single jet? |
| 7 | `o1-07-under-the-belt` | Red stays inside the SAM envelope | day | calm | 2 MiG-29 with a `wp` set confined to the belt's area | 4 F-16 (two flights) | 4 `target_soft` + 1 `target_hard` | Red `survive` + belt intact | The anchor's own Syrian tactic. Today the belt does nothing (`C1`), so this mission measures the **cost** of the tactic with none of its benefit — an honest half-measurement, labelled as such |
| 8 | `o1-08-piecemeal` | Red committed two at a time against four | day | calm | 2 + 2 MiG-29 spawning 90 s apart | 4 F-16 (one flight) | as above | Red `kill team hostile` | Piecemeal commitment is the classic charge against the Syrian conduct of the battle. Is it actually worse than committing four at once — and by how much? Mission 9 is the control |
| 9 | `o1-09-massed` | the same eight aircraft, Red committed together | day | calm | 4 MiG-29 (one flight) | 4 F-16 (one flight) | as above | as above | The control for mission 8. **One number:** Red survivors, massed vs piecemeal, same total force |
| 10 | `o1-10-mole-cricket` | the whole battle | day | `wx fixture` | 8 MiG-29 (two flights), best doctrine found in 1–9 | 8 F-16 (two flights) + 2 "sweep" | 6 `target_soft` (belt) + 2 `target_hard` | reported as an outcome band, not a win | **The yardstick run.** With every lever set to its best measured value, what is the residual? That residue is the campaign's actual answer |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| MiG-29 | flyable module | **yes** | stands in for MiG-21/MiG-23/Su-20 — a **generous** substitution, declared in every header |
| MiG-21 / MiG-23 / Su-20 class | flyable module | **no** (`C7`) | the actual Syrian force; three types, three different handicaps |
| F-16 | flyable module | **yes** | Blue |
| F-15 / F-4 / Kfir / A-4 | flyable module | **no** (`C7`) | the Israeli force was mixed; the AIM-7 shots at 22–40 km were F-15/F-4 |
| SA-2 / SA-3 / SA-6 battery | ground, emitting + shooting | **no** (`C1`) | **29 of them were the target of the whole operation** |
| RPV decoy (Mastiff / Scout class) | air, small, expendable | **no** (`C7`) | the operation's opening move |
| E-2C AEW | air, support, battle management | **no** (`C6`) | the operation's brain |
| Boeing 707 ECM / comms jammer | air, support, jamming | **no** (`C13`) | **the decisive mechanism, and the one that is furthest out of reach** |
| Ground control post / GCI radar | ground, emitting | **no** (`C1`, `C6`) | what the jamming attacked |
| Airfield (Rayak, Mezzeh) | ground complex | **partial** (`target_hard`/`target_soft`) | RPVs watched these for take-offs |

### 5. What must be true before mission 1 can fly

`o1-01`, `o1-02`, `o1-03`, `o1-04`, `o1-06`, `o1-08`, `o1-09` are buildable **today** — seven of
ten, more than any other campaign in the set, because their subject is doctrine and doctrine is
mission text. That is exactly why this is the yardstick campaign.

---

## State

**Nothing built.**

What exists and carries it: the GCI entry chain as three latency-charged command-bus entries; the
SPO-15 with its documented forward blanking while the own radar transmits; the N019's three-position
emission switch; the sixteen `pilot_*` variant keys and `fb_tournament.py`; the measured doctrine
matrix in [`../duels.md`](../duels.md) §Knowledge 4, which already contains this campaign's
missions 1, 2 and 6 in single-ship form.

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C13` | **no jamming of any kind** | **the anchor's decisive mechanism.** Mission 2's deleted `brief_gci` is a stand-in whose difference is spelled out in §Knowledge 3 |
| `C1` | **no active SAM belt** | the operation's target and the Syrian side's only shelter |
| `C7` | **no MiG-21/23/Su-20, no E-2C, no RPV, no 707** | the substitution makes the defender stronger than history, which is stated but not free |
| `C6` | **no live controller** | GCI is briefed text; a controller that re-vectors under pressure — or fails to — cannot be modelled, and "the controller was overwhelmed" is half of what happened |
| `D3` (`duels.md`) | **the pilot does not use the IRST** | mission 5's "silent" is "silent and blind" |
| `C0` | **no campaign layer** | the belt destroyed on mission 1 is intact on mission 2 |
| `C4` | **no terrain masking** | the Bekaa is a valley between two ridges; that is the whole geography |
| `C2` | **no time of day** | |
| `C15` | **no lead tasking**, formation is combat spread only | "committed piecemeal" is expressible only as spawn times, not as a decision |
| `C9` | **the MiG cannot fly `set task attack`** | irrelevant here (Red is defending) but relevant to O3/O5 |

---

## Knowledge

### 1. The anchor with its sources

Primary retrieval: [Operation Mole Cricket 19 (Wikipedia)](https://en.wikipedia.org/wiki/Operation_Mole_Cricket_19)
[T4] — the date; the 30 SAM batteries and 29 destroyed plus 6 more; the aircraft types on both sides;
the RPV baiting of the SAM radars and the relay to the E-2Cs; the 707 ECM aircraft; the "selective
airborne communications jamming … cut them off from ground control" statement; the E-2C battle
management and the beam attacks into the Syrian radar-warning blind arc; the 22–40 km Sparrow shots;
the Syrian nose/tail-only warning with no side coverage and no look-up/look-down; the "do what they
can, then run back for cover" description; the 82–86 Syrian losses; the Israeli losses of two damaged
F-15s and at least one RPV; and the Soviet 67-aircraft counter-claim with the note that it is widely
dismissed.

Corroboration used only for framing, not for numbers:
[Operation Mole Cricket 19: Israel erased Syria's air defenses in 6 hours (MiGFlug)](https://migflug.com/jetflights/operation-mole-cricket-19-israel-erased-syrias-air-defenses-in-6-hours/)
[T4]; [Operation Mole Cricket 19: the SEAD mission that rewrote air warfare (TheDefenseWatch)](https://thedefensewatch.com/policy-strategy/operation-mole-cricket-19-bekaa-valley/)
[T4]; [Bekaa Valley air battle (whatwhen.day)](https://www.whatwhen.day/years/1982/events/bekaa-valley-turkey-shoot-1982) [T4].

### 2. Where the numbers are contested, and they are left contested

| Quantity | Values found | Treatment |
|---|---|---|
| Syrian aircraft lost | **82–86** [T4], sometimes rendered as "82 in the largest dogfight since WWII" | range kept; the campaign never uses a single figure |
| Israeli aircraft lost | **none in air-to-air**; two F-15 damaged; ≥1 RPV [T4] — against the Soviet claim of **67** [T4] | both stated. The Soviet figure is recorded with its own source's judgement ("widely dismissed") and is **not** averaged, discounted or deleted |
| SAM batteries | 30 present / 29 destroyed / +6 later [T4]; other accounts give 17–19 destroyed on the first strike | **the second range was not confirmed on this pass** and is therefore not carried as a number — flagged in [`PROGRESS.md`](PROGRESS.md) |
| Whether the Syrians were badly flown or badly systemised | the sources assert both without separating them | **this is the campaign's whole point** and is left as a question, not resolved by citation |

### 3. Bekaa as a measurable question (the yardstick derivation)

The campaign refuses "who wins". It asks two quantities:

**(a) The doctrine band.** Let `O(v)` be the outcome score of a Red doctrine vector `v` over a fixed
geometry set — the same score the mixed tournament already computes (outcome dominates; craft orders
only within equal outcomes, [`../missions/combat.md`](../missions/combat.md)). The levers are the ones
listed in §Spec 2, all of them mission text. Then

```
band  =  max_v O(v)  −  min_v O(v)          [the doctrine is worth this much]
```

and the campaign reports `band` with the vector that achieved the maximum. Precedent: the same
measurement on a **single** MiG over one geometry already gave a band of **978.7 points**
(`mig_deep` −850.6 → `mig_long` +585.0) and turned six losses into none
([`../duels.md`](../duels.md)) — so the machinery for this exists and has been run.

**(b) The residue.** With `v* = argmax O(v)` held fixed on the Red side and the Blue side flown to
*its* best measured doctrine,

```
residue  =  O_blue(v*_blue)  −  O_red(v*)     [what no doctrine on either side removes]
```

The residue is the part of Bekaa that is **structural** — the weapon obligation, the sensor reach,
the cross-section ratio, the four asymmetries already tabulated in [`../duels.md`](../duels.md)
§Knowledge 1. If the residue is small, FlightBox is saying that 1982 was a doctrine and architecture
defeat that better decisions could have changed. If it is large, FlightBox is saying the Syrian force
was outmatched in materiel before anybody decided anything. **Either answer is publishable; refusing
to compute it is not.**

Two honesty conditions on that number, both non-negotiable:

1. It is a statement about **FlightBox's models**, never about 1982 — the scale is staggered by design
   ([`../vision.md`](../vision.md)) and the flying side is a type that did not exist yet.
2. A Red loss caused by a **pilot-AI defect** is not part of the residue. The duel campaign already
   found three such defects by measurement and fixed each where it belonged; the same rule applies
   here, and a residue reported before that filtering is a bug report wearing a result's clothes.

### 4. What the GCI-deletion stand-in is NOT

Deleting `set brief_gci` removes the controller **before the flight starts**. The 707s removed him
**in the middle of an intercept**, after the aircraft were already committed on his vector. The
difference matters and cannot be hidden:

| | deleted brief (`o1-02`) | the anchor's jamming |
|---|---|---|
| When | at spawn | mid-intercept |
| What the pilot has | never had a vector; searches on its own from the start | has a stale vector and believes it |
| The N019's scan elevation | never set — the radar looks at whatever the default is | set for a geometry that has since changed |
| The failure mode | blindness | **confident** blindness |

The second is the more dangerous state and the more interesting one to model. Getting it needs a
controller that can be silenced during the run (`C6`) — which makes "a GCI channel that can go away
at a declared time" the single cheapest addition that would raise this campaign from a stand-in to
the real experiment.
