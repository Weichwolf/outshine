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

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

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

**Amended by the build (2026-07-29), and the amendment is the point of a Spec that is written first.**
Three of this section's blockers were re-checked against the tree instead of trusted, and every one of
them had been closed by another round in the meantime — so the mission table of §3 was rebuilt around
what the tree can now do rather than around what it could when the spec was written:

| §3/§5 said | Today |
|---|---|
| `C1` blocks the belt; `C22`/`C23` block the net and the layering | **all three CLOSED.** The belt in every O1 mission is five live positions on one declared `net` with a P-18 control node, a transmitted `wcs`, a per-member `autonomy` fallback and three declared `zone` cylinders. The belt is not scenery in a single sortie |
| `C24` (comms jamming) is what would turn mission 2 "from a stand-in into the experiment" | **CLOSED**, and it is O1's sortie 09 — but it reaches the BELT and not the fighters (`C6`), so it did **not** turn mission 2 into anything. What did is below |
| the "confidently blind" case needs a controller that can be silenced during the run (`C6`) | **NOT TRUE, and the spec was wrong about its own mechanism.** `set brief_gci <atS> …` has always carried its own time, so a TRUNCATED brief IS a controller who stops talking at a declared instant with the aircraft already committed. No new capability was needed; the capability was read. It is sortie 03 |
| the ten missions of §3 | **rebuilt as one baseline + six single-lever variants + one controlled pair + one two-step carry chain.** The piecemeal/massed pair (§3 missions 8 and 9) is NOT BUILT and the reason is structural — see §State, "what the ten slots would not hold" |

---

## State

> **RE-MEASURED 2026-07-29, after the ground-launch fix (`fdm/FBFdm`, `fdm/FBFdmBoot`,
> `missions/FBMissionBoot`, `modules/missile/FBMissileGuidance`, `units/FBSimUnit`).** The campaign's
> largest finding — *the SAM rounds destroy their own launchers* — **was a defect on FlightBox's side of
> the seam and it is closed.** Everything below that was measured on the pre-fix binary is kept **with
> its date**, because a measured failure is knowledge; every superseded statement is marked in place and
> the replacement measurement stands in
> [§The ground half, re-measured](#the-ground-half-re-measured-after-the-ground-launch-fix-2026-07-29).
> The air half of the campaign is untouched: of 160 committed missions only 10 changed a byte, and the
> two that belong to O1 are `o1-08-belt-netted` and `o1-10-mole-cricket`.

**BUILT AND FLOWN, 2026-07-29 — the second of the ten campaigns to exist as files.** Ten `.fbm` in
`sim/missions/o1-*.fbm` plus `sim/campaigns/o1-bekaa-1982.fbc`, run as a campaign, replayed step by
step, and measured. **No file under `sim/src/` and none under `sim/assets/` was touched**
(`git status --porcelain sim/assets` empty, `verify-models` green, `verify-layers` unchanged word for
word), so every pre-existing mission is byte-identical by construction rather than by comparison: the
binary is the one that was already there.

### The arena, and the two things about it that are lies by omission

A block of the Bekaa flown as the parallel **33.90 N** between **34.60 E and 36.40 E** `[SET]`, with the
SAM belt's eastern group at 36.20–36.40 E, under `--elev const`. Two disclosures belong in the first
paragraph rather than in the gap table:

1. **There is no valley.** `C4` (no terrain masking) means the Lebanon and Anti-Lebanon ridges — the
   entire geography of the anchor — do not exist. The campaign flies a plane and says so in every header.
2. **The datum is 0 m** where the real valley floor is ~900 m, so every altitude in every file is height
   above a declared datum.

**And one thing the layout itself had to give up.** A FlightBox SAM site has **no IFF interrogator**
(`modules/ground/FBSiteModule`: *"a battery has no IFF interrogator in this tree"*) and an
`FBRadarContact` carries no identity by construction, so a battery engages the nearest firm track in its
envelope whoever it belongs to. **MEASURED on this campaign's first layout:** with the Syrian CAP at
8 000 m over its own belt, `swsa2` put **three V-750 into its own fighters within 7 s**. Every O1
mission is therefore laid out so that no Red aircraft is ever inside a friendly envelope — which
**deletes the anchor's own Syrian tactic**, *"do what they can, then run back for cover"* inside the
protective missile envelope [T4]. The campaign cannot fly the one thing the Syrians actually did.

### The ten sorties, their fingerprints and their answers

Campaign exit **3** (the worst step's; every step is a measuring rig whose own header says the verdict is
the telemetry). Campaign fingerprint under `--elev const`:
`81b549fd04c4591987b9dadf233deffdabbbfb01f9dc89f4f7f0d4486d7bba8e` (pre-fix); after the
ground-launch fix of 2026-07-29 it is `4b9582ac805564cbef8e6991ea76c1f9100cfb1e070d6dc5a50a7819a114af9a`.
**Eight of the ten step fingerprints are unchanged** — only steps 8 and 10, the two that launch from
the ground, moved. That is the fix's blast radius inside this campaign, measured rather than argued.

> **PRE-FIX, 2026-07-29.** The whole table below is the pre-fix binary's. Two of its rows moved with the
> ground-launch fix: **row 8's exit is now 2** (its attacker is shot down and *then* meets the ground) and
> rows **8 and 10** are two of the ten missions whose bytes changed. Both post-fix fingerprints have since
> been read out and stand beside the pre-fix ones in the table; the other eight are byte-for-byte the
> values below. Both determinism criteria were re-measured after the fix and still hold
> (§Both determinism criteria).

| # | Mission | control | exit | fingerprint | The answer to its one tactical question |
|---|---|---|---:|---|---|
| 1 | `o1-01-controlled` | — | 3 | `47d0e7669202edcd` | **The canonical defeat reproduces.** Blue releases at t = 156.6 / 157.0 s, Red at 160.7 — 3.6 s late. Both AIM-120 arrive at **3.38 m** and **3.14 m** against a 10.0 m fuze; **both MiG-29 mission-killed**, Blue untouched, both R-27R never arrive. The belt fires **nothing** — which is what "committed forward of its own umbrella" is as a number |
| 2 | `o1-02-no-gci` | 01 | 3 | `9590e80fe187c392` | Head-on, the controller is worth **one MiG's engagement** — 4 contacts and 1 shot instead of 8 and 2 — and **nothing measurable in the outcome**. One MiG survives, and the cause is read off the log rather than argued: **both AIM-120 went to `hunter2b`** (3.14 m and 2.60 m) where sortie 01's two rounds split one per MiG. The survivor survived because nobody shot at it |
| 3 | `o1-03-gci-cut` | 01 | 3 | `bd8fd8f4581799cf` | **Identical to its control in every measured quantity.** The three deleted calls bought exactly nothing: head-on, the briefed waypoint already points the nose at the raid and the first call's `elDeg = −4.1` stayed inside the ±6.0° RAD bar for the whole closure. **On a collision course, confident blindness and full control are the same state** |
| 4 | `o1-04-beam` | 01 | 3 | `bdb015442e8a2f87` | First contact t = **108.1 s at 26.0 nm, azDeg −48.8** — found exactly where the controller said, outside the uncued sector. Two R-27R away at t = 189.1 / 190.3, neither arrives, **and Blue never fires at all**. Nobody is lost: the 45° entry costs Blue its shot and buys Red one it cannot convert |
| 5 | `o1-05-beam-blind` | 04 | 3 | `9e2488d960888f0a` | **ZERO.** Zero contacts, zero shots, zero detonations, zero losses; the MiGs fly their CAP west to 33.75 E and the package walks past them. Against sortie 04's 9 contacts and 2 shots on the identical geometry, **the controller is worth the entire engagement** — not later, never |
| 6 | `o1-06-blind` | 01 | 3 | `3806c59ea9511976` | **Identical to its control in every measured quantity**: the warning receiver is worth nothing here, and the reason is a clock. Blue releases at t = 156.6 and the round arrives at 169.8, so the SPO-15's warning at t ≈ 157 leaves **13 s** — less than the reaction time plus the first defensive input plus the chaff programme. A receiver that warns you inside your opponent's time of flight is a receiver you cannot spend |
| 7 | `o1-07-early-launch` | 01 | 3 | `99458ae11963c2b1` | **THE ONLY LEVER OF THE SEVEN THAT INVERTS THE BASELINE.** Red releases at t = **153.1** against 160.7 — 7.6 s earlier and **3.5 s before Blue's own release would have fallen**. Blue then never fires at all. Five R-27R away against two; arrivals **2.75 / 1.54 / 2.50 / 10.42 m** against a 13.8 m fuze; **both F-16 mission-killed, both MiGs alive.** 2–0 against becomes 0–2 for, on two lines of mission text |
| 8 | `o1-08-belt-netted` | — (it IS 09's control) | 3 → **2** | `cf0e95b1e1f8f910` → **`fd1375cb9d7f9488`** (post-fix, 2026-07-29) | **[SUPERSEDED 2026-07-29 — see §The ground half, re-measured. Kept as the pre-fix measurement.]** **The netted belt works exactly as specified and destroys itself.** `net JOIN` ×4 at t = 3.9, 79 `net CUE`, first firm track t = **508.5 s at 34.9 km**, **five `site LAUNCH`**. Not one round reached an aircraft: all three V-601 nosed over into the ground **at their own launcher** within 1.6 s, and both 3M9 did the same and **killed the batteries that fired them** (0.375 m and 0.372 m). The strike killed two positions with Mk 82 at 22.87 m and 18.41 m |
| 9 | `o1-09-comms-jam` | 08 | 3 | `00d7bf504f76ab67` | **[The last sentence is SUPERSEDED 2026-07-29 — the jamming now costs the belt everything; see §The ground half, re-measured. Kept as the pre-fix measurement.]** **The anchor's decisive mechanism reproduces perfectly and changes nothing.** Four `net LOST reason=jammed` at t = 244.0 / 284.0 / 288.0 / 319.9 (distM 9 239.7 and 6 418.0 against reachM 23 340.7 — the horizon is not the cause and the log says which is), `net AUTONOMOUS fallback=hold` on all four, all DARK by t = 439.8, `net CUE` **79 → 32**, **`site LAUNCH` 5 → 0**, no firm track at all. **And the strike is byte-identical to its control's**: the same two Mk 82 on the same two batteries at t = 664.9 and 684.7, at 22.87 m and 18.41 m |
| 10 | `o1-10-mole-cricket` | its own standalone run | 3 | `36c498ac10e1ab79` → **`8d68f7975a3ec6ed`** (post-fix, 2026-07-29) | **With every lever at its measured best, Red loses nothing and Blue loses two.** 13 R-27R from eight MiGs against 2 AIM-120 from four escorts; `eagle2` mission-killed by a **2.03 m** arrival at t = 168.5, `eagle3` by 2.56 / 3.63 / 3.25 m at t = 178.7–181.0; **zero Red losses**. The run ends at t = 203.1 on `eagle2`'s ground contact — `FirstFlightKo`, not the clock |

### The ground half, re-measured after the ground-launch fix (2026-07-29)

**The campaign's largest finding was three defects, all of them on FlightBox's side of the seam, and no
deck was touched to close them** (`git status --porcelain sim/assets` still empty, `verify-models`
green). The full build and its derivations live in [`../weapons.md`](../weapons.md) §"Rail launch" and
[`../modules/ground/module.md`](../modules/ground/module.md) §4; only what O1 measures is repeated here.

| # | Defect | The measurement that pinned it |
|---|---|---|
| 1 | `FBFdm::LoadUnguarded` ran `RunIC()` while JSBSim still held **its own default ground datum**; the terrain elevation arrived one call too late, so `FGLGear` resolved a contact **inside the initial condition** | a raw JSBSim probe with no FlightBox guidance in it at all: rail 90° → **0.000 °/s**, 70° → **−79.284 °/s**, 45° → **−114.76 °/s**, 3M9 at 45° → **−179.81 °/s**, each with a ground contact force reported. A 6.09 m V-601 on a 70° rail has its tail structure point (6.09/2)·sin 70° = **2.86 m under the datum** |
| 2 | the motor was **cold for 0.55 s** — an air-launched store drops clear and *then* lights, and a rail launch inherited that slew | at t = 0.51 s the round is still in free fall: **4.98 m/s = 9.81·0.51**. From a 0.5 m launcher height that is ½·9.81·0.55² = **1.48 m of sink through the ground before any thrust exists** |
| 3 | `FBStoreSpec::GatherS` **was read by no line of code** — declared, filled for all six surface rounds, specified in two doc files, never built | the gathering phase existed on paper only; the guidance law steered from tick 1 against an airframe with no dynamic pressure |

**What the V-601 does now, one round, before against after:**

| Quantity | Pre-fix | Post-fix |
|---|---|---|
| pitch, first 1.6 s | +70° → **−41°** | **70.00 / 69.97 / 69.95°** — it flies the rail |
| where it ends | impact **7 m under the ground** | at the end of the 2.5 s gathering phase: **868.8 kt = 447 m/s**, i.e. full fin authority before the first steering command |

**And what that does to sortie 08 against sortie 09 — the pair that exists to price the anchor's own
decisive mechanism:**

| | `o1-08` unjammed (control) | `o1-09` `jam_comm_m = 90 000` |
|---|---|---|
| `site LAUNCH` | **8** | **0** |
| detonations | **7** | **0** |
| ground impacts by a SAM | **0** | — |
| the attackers | `bolt1` **shot down** | both report **objectives met** |
| the belt | **all five ground positions intact** | **two positions destroyed** |

Arrival distances on the attacker: **9.15 / 8.57 / 4.87 m** for the V-601 against its 10 m fuze, and
**0.28 / 4.09 m** for the 3M9 against its 8 m fuze. **No position destroys itself in any mission any
more.**

> **The lever that used to move nothing now moves everything.** The pre-fix pair measured the belt's
> ground damage as *identical to the metre and the tick* with the jammer on and off — which is exactly
> the measurement that made this campaign book the ground half as a defect. It is resolved: turning the
> jammer on now costs the belt every launch it would have made and costs the campaign two positions.

### The carry, where it lands, and what it was worth

`carry units ground stores` — **O1 does not narrow it**, and that is the exact opposite of O4's choice
for the opposite reason: a DACT sortie ends with both aircraft landing at Laage, and Bekaa ends with
82–86 Syrian aircraft not landing [T4]. **Attrition is this campaign's subject.**

Sorties 01–07 are seven pairwise-disjoint casts and carry nothing in or out — a controlled variant that
shared a callsign with its control would inherit that control's expenditure and stop being a control.
The chain is **08 → 10**, with 09 (08's control) sitting between them on a wholly disjoint cast so that
it neither takes from 08 nor gives to 10. Five `campaign CARRY` lines land on step 10:

| Carried | Value |
|---|---|
| `ground` | `bksa6a`, `bksa6b`, `bksa3` **dropped** — destroyed in sortie 08, **two of them by their own rounds** *(pre-fix, 2026-07-29; post-fix no round reaches the ground at all, so the carried set is a re-measure — **TODO**)* |
| `stores` | one `set store … mk82` line dropped from each of `bolt1` and `bolt2` |
| `units` | nothing: sortie 08 has no fighters (see below), and both strikers came home |

**And the honest measurement of what that was worth** *(pre-fix, 2026-07-29 — sortie 10 is one of the
two O1 files whose bytes moved with the ground-launch fix; the standalone-versus-campaign delta below is
**not** re-measured, **TODO**)*. The same file run STANDALONE with no carried
state against the same file as campaign step 10 differs in **exactly one quantity — the belt**:
`site LAUNCH` **5 → 3**, and the belt's self-destructions **2 → 0**, because the two batteries that would
have killed themselves were already dead. **Every air number is unchanged** — 13 R-27R, 2 AIM-120, the
same two escorts lost, the same end tick t = 203.1. The attrition arc is visible, auditable line by
line, and it **moved no outcome**.

Campaign totals: `ATTRITION unitsFriendly=4 unitsHostile=7 groundFriendly=0 groundHostile=5`,
`EXPENDED r27r=24 aim120=10 mk82=8 r73=3`.

### The lever table — what moves the outcome, by how much, and what moves nothing

Every row is one line of mission text against the baseline named in its `control` column. "Lost" counts
aircraft made combat-ineffective.

| Lever | Setting | Red lost | Blue lost | Red contacts | Red shots | Blue shots | **What it moved** |
|---|---|---:|---:|---:|---:|---:|---|
| — | baseline `o1-01` | **2** | 0 | 8 | 2 | 2 | — |
| GCI brief | deleted (`o1-02`) | 1 | 0 | **4** | **1** | 2 | half of Red's engagement; the one-jet outcome change is a **sort artefact**, both rounds on one MiG |
| GCI brief | truncated (`o1-03`) | 2 | 0 | 8 | 2 | 2 | **nothing at all** |
| entry geometry | 45° (`o1-04`) | **0** | 0 | 9 | 2 | **0** | Blue's entire shot |
| GCI brief on 45° | deleted (`o1-05`) | 0 | 0 | **0** | **0** | 0 | **the entire engagement** — 9 → 0 contacts, 2 → 0 shots |
| RWR | off (`o1-06`) | 2 | 0 | 8 | 2 | 2 | **nothing at all** |
| launch doctrine | `shot_rtr 1.4`, `lock_nm 16` (`o1-07`) | **0** | **2** | 10 | **5** | **0** | **the whole battle**, by 3.5 s of tempo |
| comms jamming | `jam_comm_m` 0 → 90 000 (`o1-08`→`o1-09`) | — | — | — | — | — | ~~`site LAUNCH` **5 → 0**, `net CUE` 79 → 32, first firm track 508.5 s → never. **Ground damage: identical to the metre and the tick**~~ **(pre-fix, 2026-07-29 — the last clause is WRONG on the current binary.)** Post-fix: `site LAUNCH` **8 → 0**, detonations **7 → 0**, the attacker `bolt1` shot down → **both attackers meet their objectives**, ground positions lost **0 → 2**. **This lever moves the outcome** |
| the campaign carry | step 10 standalone → in campaign | 0 → 0 | 2 → 2 | — | 13 → 13 | 2 → 2 | `site LAUNCH` **5 → 3**. Every air number unchanged |

**Two levers move the outcome and four move nothing, and the four are the interesting half** — *stated
pre-fix, 2026-07-29. On the current binary it is **three**: comms jamming joined them the moment the
belt got rounds that fly.*

### What is left when nothing moves it

1. **The whole outcome sits inside a 3.5-second launch decision.** Sortie 07 changes when Red pulls the
   trigger and nothing else, and the engagement inverts from 2–0 against to 0–2 for. Every other lever in
   the campaign — the controller, the receiver, the belt, the net, the jamming, the carried attrition —
   is measured against that and is worth less.
2. ~~**The entire ground half of Bekaa is inert in this tree, and the reason is a defect rather than a
   doctrine.** The 2K12's 3M9 and the S-125's V-601 nose over and hit the ground **at their own
   launcher** seconds after release; the 3M9's 59 kg warhead then destroys the battery that fired it.
   That is why the net, the cueing, the emission discipline, the fire-control authority and the anchor's
   own decisive jamming all move mechanisms (5 launches → 0, 79 cues → 32, 76.6 s and 13.0 km of
   earlier warning) and move **no outcome**: a belt whose rounds fall on its own launchers had nothing
   to lose.~~
   **SUPERSEDED 2026-07-29 by the ground-launch fix. The measurement above is kept because it is what
   found the defect** — it was pre-existing and visible in a committed mission (`net-cue.fbm`,
   t = 172.8 s, `monitor KO unit=sam_3m9_1 reason=CFIT` 0.8 s after that battery's own first launch,
   followed by `damage KILL unit=sam reason="structure destroyed"` at rangeM 0.0018), and this campaign
   is what forced it to be read. Its diagnosis, contrary to the sentence "fixing it means touching
   `sim/assets/aircraft/`", cost **no deck change at all**: the cause was three defects on FlightBox's
   own side of the seam (initial condition run against the wrong ground, a motor cold for 0.55 s, and an
   unread `GatherS`). **The corrected statement:** the belt is live, the jamming lever now moves the
   outcome, and what remains inert is only the *air* half's conclusion in point 1. See §The ground half,
   re-measured.
3. **The warning receiver is inside the AMRAAM's time of flight.** 13 s from first warning to impact is
   less than reaction plus the first defensive input plus the chaff programme, so deleting the SPO-15
   changes not one measured number. The anchor's "no side warnings" handicap therefore costs nothing on
   a head-on BVR shot in this tree, and the campaign says so instead of assuming the receiver mattered.
4. **The residue is not a residue of Bekaa, and it must not be reported as one.** With the best doctrine
   measured in 01–07 the defender wins outright. But the doctrine that won is a launch discipline
   available to both sides and given to one; the defender is a type that postdates the battle by a year;
   and §Temporal honesty said in advance that a good Red result says nothing about 1982. **The campaign's
   publishable statement is therefore about FlightBox and not about history:** on this geometry, in this
   tree, `band` is carried by a single trigger parameter and the architecture — control, warning, belt,
   net — contributes measurably nothing to it.

### Both determinism criteria, measured

Under `--elev const`, read out of `campaign-summary.txt` rather than assumed
([`../missions/campaign.md`](../missions/campaign.md) §5):

| # | Criterion | Result |
|---|---|---|
| **1** | 3 repetitions × `--threads 1/2/4` produce one campaign fingerprint | **9 runs, 1 fingerprint** — `81b549fd04c459198…` pre-fix, `4b9582ac805564cbe…` post-fix (both re-measured 2026-07-29) |
| **2** | every step's per-mission fingerprint equals that mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included, on the first attempt — O4's clock hole stayed closed for the first clocked campaign built after it |

**Re-measured after the ground-launch fix, 2026-07-29: both criteria still hold — 9 runs, one campaign
fingerprint, and 10/10 steps replayed individually.** The fingerprint *value* changed (steps 8 and 10
carry new bytes) and is not written down yet — TODO above.

### Conservation

*(This section describes the round that BUILT O1, 2026-07-29. The ground-launch fix that came after it is
a `sim/src/` round with its own conservation argument: 7 files under `sim/src/`, no deck, **10 of 160
missions changed and 150 byte-identical**; `verify-layers` 297 files and 6 registry readers unchanged,
`verify-models` 1 declared delta / 34 FlightBox-own, warning-free build.)*

**Nothing to compare, and that is the strongest form of it.** `git status --porcelain` lists eleven
untracked files and no modified one: ten `sim/missions/o1-*.fbm` and one `sim/campaigns/*.fbc`. No
`sim/src/` file, no tool and no asset was touched, so the binary that flew O1 is the binary that flew
everything before it. Gates re-run all the same: `make core-lib gym native wasm` warning-free,
`verify-layers` *"297 files, 805 internal include(s), 12 layers — no upward include, 3 restricted
header(s) respected, 6 registry reader(s) inside the perception boundary, 284 file(s) in their layer's
namespace (5 C-island file(s) exempt)"*, `verify-models` *"4 upstream-backed model path(s) match
assets/MODEL-DELTAS.md (1 declared delta(s), 34 FlightBox-own)"*, six harnesses rc = 0.

### What the ten slots would not hold, and the rule that falls out

Ten missions hold **one baseline + six single-lever variants + one controlled pair + one carry chain**
and nothing else, because of a constraint the campaign layer creates and cannot enforce:

> **A chain step cannot also be a controlled variant.** The carry is keyed by callsign, so two missions
> that differ by one lever must have disjoint casts — and a mission that inherits a predecessor's state
> differs from any sibling in two things at once.

The spec's **piecemeal-versus-massed pair (§3 missions 8 and 9) is therefore not built.** It was the
lever that lost the slot, and the reason it lost is that `C15` already calls it the weakest instrument
in the set — *"committed piecemeal is expressible only as spawn times, not as a decision"* — and a
`.fbm` has no spawn times, so it would have been a spawn **displacement** (90 s at 450 kt = 20.8 km of
extra ingress). What bought the slot instead was the anchor's own decisive mechanism, as a clean pair.
The eight remaining campaigns should budget their ten slots the same way, in the open, before writing
a file.

### What the tree already had, and this campaign consumed unchanged

| Already built | Where | Used for |
|---|---|---|
| the GCI entry chain: BRAA → range-angle elevation → ZONE → ILLUM, one entry per decision tick, latency-charged, rejectable, supersedable, **timed by `atS`** | `modules/mig29/FBMig29Pilot` | sorties 01–07, and the "confidently blind" case the spec thought needed new code |
| the connected air defence: `net` block, cue, sector, `wcs`, `autonomy`, `net LOST reason=…` | [`../air-defence-network.md`](../air-defence-network.md) | the belt in all ten sorties |
| `zone` + `zone_<name>_in`/`_s`, RESTRICTED to the judge | ″ §4 | the layer cake over the belt |
| `set jam_comm_m` — one published scalar, receiver-side, other teams only, no die | ″ §6 | sortie 09, one number against sortie 08 |
| the `ef111` catalogue row and the `p18`/`sa2`/`sa3`/`sa6` rows | [`../modules/air/catalogue.md`](../modules/air/catalogue.md), [`../modules/ground/catalogue.md`](../modules/ground/catalogue.md) | the 707 stand-in and the belt, with **no new class** |
| the campaign layer, its overlay and its two fingerprints | [`../missions/campaign.md`](../missions/campaign.md) | the chain and both criteria |

---

## Gaps

### The honest headline

**O1 exists, it runs, it replays, and its result is the opposite of the anchor's — for reasons that are
one trigger parameter and one weapon defect.** The campaign reproduces the canonical defeat on its
baseline (both defenders mission-killed, the attacker untouched), then removes it with a single launch
doctrine, and finds that *nothing else it can express reaches the outcome at all*: not the controller
on a head-on merge, not the warning receiver, not the belt, not the net, not the jamming that the
anchor calls decisive, and not the campaign's own attrition. Two of those four nulls are model
properties worth publishing; two are defects, and both are named below with the committed mission that
already showed them.

> **UPDATE 2026-07-29 — the weapon defect is fixed and one of the nulls was it.** The jamming null is
> gone: on the current binary `o1-08`→`o1-09` moves 8 launches to 0, 7 detonations to 0 and 0 lost
> positions to 2. The trigger parameter (sortie 07) is untouched and still the largest single lever.
> Three *new* defects became visible underneath the fixed one; they are rows in the table below.

| ID | What is missing | State here |
|---|---|---|
| ~~**new — the SAM rounds destroy their own launchers**~~ | the 2K12's **3M9** and the S-125's **V-601** pitch over and reach the ground at their launcher's own coordinates within 0.8–1.6 s of release; the 3M9's 59 kg warhead then registers as a `damage KILL` on the battery that fired it | **CLOSED 2026-07-29**, and the diagnosis contradicts this row's own prognosis: **no deck was touched**. Three defects on FlightBox's side of the seam — the IC run against JSBSim's default ground, a motor cold for 0.55 s, and an unread `GatherS`. Post-fix: **0 ground impacts, 0 self-kills, 7 detonations on the attacker in sortie 08**. `C1`+`C22`+`C23`+`C24` are mechanisms **with** consequences now. Build: [`../weapons.md`](../weapons.md) §"Rail launch", [`../modules/ground/module.md`](../modules/ground/module.md) §4 |
| **new — the V-750 cannot fly its own pitch-over** | the round is spawned on an **80°** rail and must be brought down onto a line of sight that can be **2.5°**. Pure proportional navigation plus a gravity bias has no mechanism for a large *commanded* pitch-over; the real S-75 flies a **programmed** one | **MEASURED 2026-07-29.** Flat geometry: the law never asks for more than **0.53 g**, so the round is never brought around and never arrives. Steep geometry: it reaches **−1.23 g**, goes 80° → 42°, and hits. The rail angle and the engagement geometry are therefore coupled through a law that does not know the rail exists. Belongs to `modules/missile` |
| **new — a MANPADS launched with an invalid fire-control state never uncages** | `FBMissileGuidance::Run`'s `if (!HaveTarget_)` early return sits **above** the block that uncages the infrared head, so a shoulder round handed no valid target at separation flies its whole life with a caged seeker | **MEASURED 2026-07-29:** seeker state and lateral acceleration **zero for the entire flight**. It is the exact cause of the older *"MANPADS without a seeker tone"* gap in [`../modules/ground/module.md`](../modules/ground/module.md), which is therefore still **open** — the ground-launch fix gave those rounds a trajectory, not a seeker |
| **new — the shared missile controller gains depart the 9.8 kg shoulder round** | one set of autopilot gains serves every round in the tree, from a 2 300 kg V-750 to a 9.8 kg Igla | **MEASURED 2026-07-29:** loss of control at **5.1 s** of flight time, fins **at the stops**, angle of attack **±4°**. Two mass classes cannot share one gain set; the fix is a per-row schedule and it is not built |
| **new — a SAM battery has no IFF and cannot be given a sector against its own side** | `FBSiteModule` sets `SetIffInterrogator(false)` on both antennas and an `FBRadarContact` has no identity field, so a battery engages the nearest firm track in its envelope whoever it is. A `net … member … sector` gates the CUE, not the member's own autonomous engagement | **DELETES THE ANCHOR'S SYRIAN TACTIC.** MEASURED: three V-750 into its own CAP within 7 s on this campaign's first layout. O1 works around it by geometry — no Red aircraft is ever inside a friendly envelope — and therefore cannot fly "run back for cover inside the missile envelope" at all |
| ~~`C13`~~ / `C24` | **comms jamming** | **CLOSED and consumed**: sortie 09 is `set jam_comm_m` 0 → 90 000 as a clean controlled pair. The RADAR half of `C13` stays wholly open. ~~What the closure bought is measured and is smaller than the spec expected~~ *(pre-fix, 2026-07-29)* — **post-fix the closure buys the whole ground engagement**: 8 launches → 0, 7 detonations → 0, 0 → 2 positions lost. See the lever table |
| ~~`C1`~~ ~~`C22`~~ ~~`C23`~~ | active belt, connected defence, judged belt geometry | **all CLOSED and all consumed** — ~~and all three are, in this campaign, mechanisms without consequences because of the first row of this table~~ *(pre-fix, 2026-07-29; with the first row closed they have consequences: the jamming lever costs the belt 8 launches and the campaign 2 positions)* |
| `C6` | **no live controller, and no aircraft rides a net** | halved rather than closed. The FIGHTER half of "cut off from ground control" is flown as a deleted (`o1-02`, `o1-05`) or truncated (`o1-03`) brief, which is a real mechanism with real timing; the JAMMED half reaches only the belt, because the MiG-29 module has no `FBNetLinkSystem` slot. The two are not the same thing and the mission headers say so |
| `C7` | **no MiG-21/23/Su-20, no E-2C, no RPV** | the substitution stands, declared in all ten headers with its direction. **The `ef111` row closed the 707 for free** — a mover with no weapon but `jam_comm_m`. `mig21` is one of the four `ACCEPTED` decks and would make a substitution-direction control run possible; **it was not built here**, and that is the first thing a second O1 round should add |
| `C4` | **no terrain masking** | the Bekaa is a valley between two ridges and this campaign flies a plane. It is the single largest geographic omission in the set and it is stated in every header rather than in a footnote |
| `C15` | **no lead tasking; and a `.fbm` has no spawn times** | this is why the piecemeal/massed pair is not built. "Committed piecemeal" would have been a spawn DISPLACEMENT (20.8 km = 90 s at 450 kt), and the slot went to the anchor's decisive mechanism instead |
| **new — `FirstFlightKo` ends the whole run** | one flight-monitor K.O. anywhere ends the mission for everybody, so a file holding a fighter engagement AND a SEAD run measures the fighter engagement twice | **MEASURED: t = 237.0 s**, with the strikers 130 km short of the belt, on O1's first layout of sortie 08. It is why sorties 08/09 carry no fighters and sortie 10 puts its strikers past the CAP line. A campaign that wants two engagements in one file must separate them in TIME, not only in space |
| **new — a four-ship AIM-120 salvo departed** | in an 8v4 at a 64.7 km entry, **all four** AIM-120 hit `departure: sustained high multi-axis body rate` ~13 s after launch, at a terminal `losElDeg` of −11.4° against −5.4° in the working case | not present in the built sortie 10 (0 of 2 fired departed at the 106.3 km entry) and therefore not a blocker here, but it is a repeatable geometry and belongs in `weapons.md`'s gaps. Attributed: **not the weather** — the identical file under `wx calm` departs all four at the same ticks |
| `D3` (`duels.md`) | **the pilot does not use the IRST** | untouched and unmeasured here: no O1 sortie flies EMCON, because on this module the third GCI entry IS the radar's on-switch and a deleted brief would delete the controller AND the sensor in one line. `duel-emcon.fbm` remains the tree's only measurement of emission discipline |
| ~~`C0`~~ ~~`C2`~~ | campaign layer, time of day | **CLOSED and consumed**: one `.fbc`, both criteria measured, and the clock is 11:00 Z on 9 June 1982 |
| `C9` | **the MiG cannot fly `set task attack`** | irrelevant here (Red defends throughout) |

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
