# The air-to-ground half — beating an air defence down

**Subject:** `C8`/`C25`/`C26`/`C27` — **what the air can do to the ground.** The weapon that homes on a
transmitter, the five other stores that decide a strike, what a fighter radar honestly does against dirt,
the difference between *suppressed* and *destroyed*, and the one pre-existing defect that has to die
first.

**Status: BUILT 2026-07-28, except §3.5 (the rocket pod) and §4 (air-to-ground ranging).**

**Delimitation, and it is the whole reason this file exists separately.**
[`modules/ground/module.md`](modules/ground/module.md) specifies the position and
[`air-defence-network.md`](air-defence-network.md) the net above it. Both end at the same sentence, and
they say it themselves:

> *"`C1` gives the ground the ability to shoot back long before it gives the air the ability to shoot
> first. That asymmetry is real, it is the correct order to build in, and it is not a SEAD capability."*
> — [`modules/ground/module.md`](modules/ground/module.md), Gaps
>
> *"this file makes the ground better at it. The counterweight (HARM, terrain, a SEAD element that
> suppresses) is `C8` and `C4`, and neither is in this round."*
> — [`air-defence-network.md`](air-defence-network.md), Gaps

This file is that counterweight. **A position is HEARD, not SEEN** — and everything below follows from
taking that sentence seriously instead of repairing it with a radar mode that would delete it.

**Why it is outside the `sim/src/` mirror.** Like [`duels.md`](duels.md) (a PAIRING),
[`formation.md`](formation.md) (a FLIGHT) and [`air-defence-network.md`](air-defence-network.md) (a NET),
this is an ENGAGEMENT of one thing by another and not a directory: it cuts through `core/` (two seeker
kinds, N catalogue rows, one objective kind, two floats on a release record, one roster bool),
`sensors/` (one seeker derivation, one radar function), `weapons/` (one resolution boundary widened),
`pilot/` (one release cue), `missions/` (grammar and judge), and three module directories at once
(`ground/`, `stores/`, `f16/`). Putting it in [`weapons.md`](weapons.md) would bury the sensor half and
the verdict half; putting it in `modules/ground/` would put the attacker inside the defender's file;
putting it in `campaigns/` would turn a capability into a scenario. It is the **fourth** deliberate
exception to the mirror rule and [`INDEX.md`](INDEX.md) names it as one.

| Source class | What it is |
|---|---|
| **In-tree primary** | [`modules/f16/weapons.md`](modules/f16/weapons.md) §2.6/§3 (ED EA Guide p.549–632, p.34–42 — HARM/LGB/cluster/rocket employment and specs) · [`modules/mig29/weapons.md`](modules/mig29/weapons.md) §5 (DCS MiG-29 manuals — the FAB/RBK/S-8/S-24 family with its release envelopes) |
| **In-tree contracts consumed** | [`weapons.md`](weapons.md) (weapon-as-unit, the three resolution boundaries, the damage model) · [`sensors.md`](sensors.md) §1/§5 (the perception boundary, the RWR) · [`missions/verdict.md`](missions/verdict.md) (what a judge may measure) · [`modules/ground/module.md`](modules/ground/module.md) + [`catalogue.md`](modules/ground/catalogue.md) (the target) · [`air-defence-network.md`](air-defence-network.md) (the net that keeps it alive) · [`pilot.md`](pilot.md) §4 (the attack phase that exists) |
| **Requirement sources** | [`campaigns/w3-desert-storm.md`](campaigns/w3-desert-storm.md) (`w3-04`/`w3-05`: a suppression element with nothing to suppress with) · [`campaigns/w4-allied-force.md`](campaigns/w4-allied-force.md) (`w4-04`/`w4-05`/`w4-07`: *"the campaign's defining aircraft cannot carry its defining weapon"*) |
| **External** | cited per row in §Knowledge 6, tiered |

Marking: `[T1]`…`[T4]` as [`campaigns/INDEX.md`](campaigns/INDEX.md) · `[SET]` = a FlightBox setting with
its one-sentence reason · `[DERIVED]` = computed from a named relation · `[MESS]` = measured in this tree ·
`[DISPUTED]` = sources conflict, both carried · `[TODO]` = open.

---

## Spec

### 0. The five capabilities, and the question each one unblocks

| # | Capability | The question that stays **unanswerable** without it | Asked in |
|---|---|---|---|
| 1 | **A weapon that homes on a transmitter** | *Is emission discipline worth anything?* Today it is worth **nothing measurable**: nothing punishes a radiating site, so `emcon free` and `emcon hold` differ only in when the site sees you. The whole doctrine of the `C1` round is untested | `w3-04`, `w3-05`, `w4-04`, `w4-05`, `w4-07`, `w4-10` |
| 2 | **Stores that are not one 500 lb bomb** | *Which weapon does this target want?* With one warhead mass in the tree, every strike question is the same question. A hardened target, a dispersed vehicle park and a radar van are three different problems and one answer | every strike mission of W2/W3/W4/O3 |
| 3 | **An honest answer about the radar against the ground** | *Can he find it, or only hear it?* Unstated, the question decays into "did we accidentally give him a threat-ring display". The two radars in the tree are **very** unequal here and the inequality is a result | `w4-04`, `w4-05`, all of O3 |
| 4 | **Suppressed against destroyed** | *Did the SEAD element do anything?* `CombatEffective()` on a site is `Structure` alone, so today the two words are one word and a suppression sortie can only be scored as a failed kill | `w3-04`, `w3-05`, `w4-07`, `o5-04` |
| 5 | **A pilot that can take the shot** | *…* — nothing, and deliberately so. The weapon is measurable before the pilot learns anything (§7), and the pilot's own round is the one after this | — |

### 1. The contract

| Contract | Acceptance / measurement anchor |
|---|---|
| **The anti-radiation seeker is the warning receiver, and nothing else** | `FBMissileArSeeker : sensors/FBRwrSystem`, the same relationship `FBMissileIrSeeker` has to `FBIrstSystem`. It measures a bearing, an elevation and a received power; **it never measures a range**, and `msl_range` is −1 for the whole flight |
| **The registry reader list does not grow** | the RWR is already one of the six ([`sensors.md`](sensors.md) §1.2); a derivation adds no `#include`. Acceptance: `make -C sim verify-layers` prints *6 registry reader(s) inside the perception boundary* after the round exactly as before it |
| **A silent transmitter is a lost target, and the round's memory is a RATE, never a POINT** | §2.2. The escape window is not a rule and not a setting: it is the zero-effort-miss law of the guidance law already in the tree (§Knowledge 1), so *when* a crew goes dark decides the outcome continuously |
| **An anti-radiation round radiates nothing and warns nobody** | its `Emission()` is `None`. The site under attack gets no warning of any kind — consistent with the tree (no MWS, [`sensors.md`](sensors.md) gap 8) and with the weapon |
| **A guided BOMB binds its shooter; a free-fall bomb does not** | the laser-guided round is a semi-active weapon whose illuminator is the shooter: `FBSeekerHandoverS = -1`, the same support obligation the R-27R already imposes ([`weapons.md`](weapons.md) §10.2) — one hook, no new architecture |
| **The radar never FINDS a ground unit** | `FBRadarSystem`'s `FBUnitKind::Aircraft` filter is **unchanged**. What the air-to-ground function adds is a *measured range to where the antenna is already pointed*, never a contact, never a track, never a symbol (§4) |
| **Suppression is a BEHAVIOUR, not a damage state** | it is scored by an objective kind against an observed *radiating* bit, not by a new `FBSystemId` and not by widening `CombatEffective()` — which stays a statement about airframes ([`weapons.md`](weapons.md) §7.5) |
| **A weapon sees through its seeker or not at all** | unchanged. No air-to-ground weapon in this file is ever told where its target is by anything except (a) its own seeker, (b) the shooter's published illumination, or (c) a briefed point the mission author wrote — the same three sources every existing round has |
| **A mission that declares none of the new stores behaves byte-identically** | the conservation rule of `C2`/`C12`/`C23`. **With one declared, named exception**: the B1 fix of §6, whose breakage is predicted in advance and is exactly two files |
| Nothing here is random | no die anywhere: the seeker is a geometry test, the memory is a clock, the footprint is an area, the damage is the existing deterministic model. One fingerprint over `--threads 1/2/4` × 3 repeats |

---

### 2. The anti-radiation weapon

#### 2.1 The seeker is the RWR — which is why it is cheap

| Property | Value | Provenance |
|---|---|---|
| Slot | `FBMissileArSeeker : sensors/FBRwrSystem` | the `FBMissileIrSeeker` precedent verbatim |
| Seeker kind | `FBSeekerKind::AntiRadiation`, appended | one enum value |
| What it measures | arrival **bearing**, arrival **elevation**, **received power** (`SignalNorm`) | the RWR's own three outputs ([`sensors.md`](sensors.md) §5.3) |
| What it can never measure | **range, closure, identity, team** | `FBRwrThreat` has no field for any of them, and the missile adds none |
| Field of regard | forward cone, `SeekerFovDeg` half-angle, via the base's existing `Blanked(rxAz)` hook | **no new hook**: `FBMig29Rwr` already uses that hook to blank a hemisphere; the missile uses it to keep one |
| Look rate | **continuous** — no frame raster | a wideband passive receiver has no antenna to sweep. It is the one seeker in the tree that looks every tick, and that is physics, not a favour |
| Guidance law | **pure PN**, `a = N·V_own·(Ω × û)` + 1 g bias, `N = 4` | identical to the infrared round, and for the identical reason: angles only, no closure to put into `N·Vc·Ω` ([`weapons.md`](weapons.md) §10.2) |
| Emission | **none** | it is a receiver. `Emission()` returns `Mode::None`, so no RWR, no ESM and no site hears the shot |
| IFF | **none, and it homes on friendly emitters** | `FBRwrSystem` stores `SelfTeam_` and deliberately never reads it ([`sensors.md`](sensors.md) §5.1). An anti-radiation round has no way to ask, and neither does this one. Named, not fixed |
| Target selection | **latches the first acquired emitter of the programmed class and never re-targets** | a real HARM is handed a threat type and follows what it locked. A "strongest wins" re-target rule would make the round a lottery over whichever site is loudest this tick |
| Target class filter | `FBArTargetClass { AnySurface, SurfaceFireControl, SurfaceEarlyWarning }` | the honest coarsening of the ALIC threat table [T2]. **Band letters, PRF and pulse width are refused** — `FBEmitterSignature` carries none of them, and inventing them is the same refusal [`sensors.md`](sensors.md) §5.6 already makes for the SPO-15's threat letters |

**Why this is not a seventh reader, spelled out:** `FBRwrSystem.cpp` holds `#include "FBUnitRegistry.h"`
today and reads exactly one thing from a foreign unit — its published `FBUnitSignature::Radar[]` array.
The derivation overrides two virtuals and adds no include. The gate is a diff, and this round produces
none.

#### 2.2 What happens when the transmitter stops — the load-bearing decision

Two designs exist. They are not equivalent, and the difference is the entire tactical value of the
previous round.

| | **(A) DIRECTION memory — recommended** | (B) POINT memory |
|---|---|---|
| What the round holds after the last look | the last measured **LOS rate**, for `SeekerMemoryS`; then nothing | a **geodetic point**, held to impact |
| Where the point would come from | — | (i) the shooter hands one over (POS/EOM), or (ii) the round intersects its own LOS with the terrain: `range = Δh / sin(depression)` |
| Behaviour after the memory expires | `Ω = 0` → PN commands nothing lateral. The **1 g gravity bias survives**, because it comes from an accelerometer the round owns; the PN term does not, because it needs a measurement the round no longer has. Result: a straight, non-drooping coast | true PN against a stationary point — it arrives, whatever the crew does |
| Does emission discipline work? | **yes, and continuously** — the miss is `ZEM(0)·(t_go/t_f)^N` (§Knowledge 1) | **no.** After one good look the site cannot escape. `emcon`, `scoot_s` and the whole `C1` doctrine become decoration |
| What it needs that the tree does not have | nothing | (i) a **range to a ground emitter** — the RWR refuses to produce one by contract, and an HTS-style geolocation would triangulate *exactly* because the tree has no bearing error ([`sensors.md`](sensors.md) gap 4): a truth read wearing a sensor's clothes. (ii) a per-tick terrain walk inside a weapon module, whose accuracy would then be a property of `--elev const` vs `--elev tiles` rather than of the weapon |
| Fidelity | the documented **HAS / self-detect** case: bearing only, no range, no loft [T2] — and the documented **AGM-45 Shrike** case: *"the Shrike will lose its lock if the radar ceases to radiate"* [T3] | the documented **AGM-88 memory mode**: *"remembers the last known location and continues towards that location"* [T4] |

**RECOMMENDED: A, with a per-row `SeekerMemoryS` that generalises the existing `kLosRateHoldS`.**

The recommendation is not a preference; it is the only one of the two the tree can produce honestly, and
the reason is a chain of three contracts it would otherwise have to break — the RWR's *no range*, the
sensor layer's *no measurement error*, and the weapon layer's *never invent a number*. Design B's second
route (terrain intersection) is admissible in principle and is refused for a second, independent reason:
it deletes the countermeasure. That is a *modelled* outcome and not a physical one — the real weapon's
memory is bounded by inertial drift, seeker bias and by the site being able to **move**, and FlightBox
models none of the three (`C14`, [`modules/ground/module.md`](modules/ground/module.md) G7).

**The direction of the error is stated with the decision:** FlightBox's anti-radiation round is
**weaker than an AGM-88C against a late shutdown** and about right for a Shrike-class round or a HARM
employed in HAS. `SeekerMemoryS` is the one number that moves it, it is logged, and a mission may argue
about it. It is `[SET]` per row and it is the *only* setting in this weapon that decides a shot.

**Re-acquisition is allowed, and it is the third distinct behaviour in the seeker table:**

| Seeker | Loses the target when | Can it come back? |
|---|---|---|
| `ActiveRadar` | never (own transmitter) | — |
| `Infrared` | the mark leaves the field | yes, if it re-enters |
| `SemiActiveRadar` | the shooter stops illuminating | **never** — there is no transmitter of its own to switch on |
| `AntiRadiation` | **the target stops transmitting** | **yes** — the round switched nothing off, and neither did the shooter. A crew that blinks back on inside the seeker's cone is re-acquired |

That row is the doctrine in one line: **go dark, and stay dark until it is over.**

#### 2.3 The round — `agm88`

| Field | Value | Provenance |
|---|---|---|
| `Key` / `Seeker` | `agm88` / `AntiRadiation` | — |
| `MassLbs` | 780 (355 kg) | [T4] Wikipedia, AGM-88 |
| `WarheadKg` | **66.0** (WDU-21/B blast-fragmentation, ~25,000 steel fragments) | [T4]. It is the ONE store-side number the damage model reads |
| `FuzeRadiusM` | **5.0** | **[SET]**. The weapon has a *laser* proximity fuze [T2, ED p.598–611]; no radius is published. 5 m is the smallest gate in the tree above the MANPADS rows, chosen because a HARM's lethality is its fragment pattern on a soft van and not a direct hit — and because the fuze radius is a *did-it-detonate* gate while the **mass** carries the lethality ([`weapons.md`](weapons.md) §6.1) |
| `RequiresLock` | **false** | its precondition is a seeker cue, not a fire-control lock (§Gaps collision 3). A wasted round is the pilot's problem, which is exactly HAS employment |
| Max speed | **Mach 2.9** [T4] vs **Mach 1.84** [T2, ED p.34–42] | **[DISPUTED]**, both carried. The deck is sized against the [T2] figure, because the ED number is the one the rest of `modules/f16/weapons.md` is built from and mixing sources inside one deck is worse than being slow |
| Range | 25 km low / 80 km medium / 148 km stand-off [T4]; ≈80 nm [T2] | **not a catalogue field.** FlightBox declares no `Rmax` for this round: it has no DLZ (`RequiresLock` false), so its reach is whatever its own deck produces, exactly as `weapons.md` §4.2 demands. The published band is documentation and a sanity check on the deck |
| `SeekerFovDeg` | **40** (half-angle) | **[SET]**. The HAS FOV options (CTR/LT/RT/WIDE) are documented and their angles are not [T2]. The value must exceed the off-boresight angle a pilot can produce at launch, or the round would lose at separation the emitter it was slaved to — and it has a measurable consequence: a shot taken further off the nose than this never acquires |
| `SeekerMemoryS` | **4.0** | **[SET]**, and the one that decides shots. §Knowledge 1 gives what it is worth: it shifts the effective shutdown one memory-length later, so on a 20 km shot (≈29 s of flight at a ~700 m/s mean) it takes 4 s off the crew's escape window |
| Deck | FlightBox's own, by the slender-body recipe of [`weapons.md`](weapons.md) §10.2, sized from ⌀254 mm / 355 kg / the [T2] terminal Mach | `[DERIVED]` |
| Emission | none | §2.1 |

#### 2.4 What the anti-radiation weapon is expressly NOT

| Not modelled | Consequence, stated |
|---|---|
| **HTS / emitter geolocation, and therefore POS/EOM/PB employment** | the only mode is self-detect. Refused with a reason, not for scope: geolocation by parallax over a moving platform, in a tree with **no bearing error**, returns the exact truth. It becomes admissible the day `sensors.md` gap 4 closes, and not before |
| Lofting | inherited from the AIM-120 ([`weapons.md`](weapons.md) Gaps). ED states HAS does not loft anyway, so the omission and the mode agree for once |
| The ALIC tables, threat letters, band/PRF discrimination | the filter is three values over `FBEmitterKind` |
| Scan-cycle time as a function of the active threat set | [T2] documents that narrowing the set speeds detection. There is nothing to narrow: the receiver is continuous |
| A launch warning of any kind at the target | there is none, by construction |
| Home-on-jam | the comms jammer publishes no signature ([`air-defence-network.md`](air-defence-network.md) §6); nothing to home on |

---

### 3. The other five stores — each with the one quantity that decides it

| Store | Class | **The one quantity that decides it** | Binds the shooter? |
|---|---|---|---|
| `mk84` | 2,000 lb free-fall | `WarheadKg` **428.6** → lethal radius **100 m** against `target_soft`, **17.7 m** against `target_hard` (§Knowledge 2) | no |
| `gbu12` | 500 lb laser-guided | the **designation obligation**: `FBSeekerHandoverS = -1`, illumination to impact | **yes, to impact** |
| `cbu87` | cluster | the **footprint** 200 × 400 m — the only weapon that turns an aim error from a distance into a binary | no |
| `fab250` / `fab500` | Soviet free-fall | the **release envelope** 500–1,000 km/h — the first store in the tree whose carriage refuses a release | no |
| `hydra70` / `s8` in a pod | rocket | the **magazine**: N rounds on one station, one per pickle | no |

#### 3.1 `mk84` — the free-fall bomb that is not an Mk-82

| Field | Value | Provenance |
|---|---|---|
| `MassLbs` | 2,039 (925 kg) nominal; 1,972–2,083 lb depending on fin/fuze | [T4] Wikipedia, Mark 84 |
| `WarheadKg` | **428.6** (945 lb Tritonal) | [T4], corroborated by [T2] `modules/f16/weapons.md` §3 |
| `DragAreaFt2`, `Perf.*` | from its own deck, by the Mk-82's rule (§`core.md` 7.1) | `[DERIVED]` |
| Deck | FlightBox's own, scaled from the `mk82` deck | **and it inherits that deck's fidelity caveat in full** ([`modules/stores.md`](modules/stores.md) Gaps 1): the delivery error against it is fidelity to the MODEL, never to a real release |

**Employment:** identical to the Mk-82 — CCIP/CCRP, one release path, no new mode. What changes is the
release *altitude* the crew can survive: a 100 m lethal radius on a soft target is a fragment envelope the
releasing aircraft flies through at a level laydown, and [`weapons.md`](weapons.md) §5.4 deliberately does
**not** resolve a ground burst against aircraft. Named here because the Mk-84 is where that omission first
becomes a lie the mission author can trip over.

#### 3.2 `gbu12` — the laser-guided bomb, and the reason it binds

**The decision: an LGB is a SEMI-ACTIVE weapon whose illuminator is the shooter.** That is not an analogy —
it is the same mechanism with a different wavelength, and the tree already built it in stage 2c.

| Element | How it is expressed | Cost |
|---|---|---|
| Seeker kind | `FBSeekerKind::SemiActiveLaser`, appended | one enum value |
| Alive while | the shooter publishes an active designation whose age is under `kUplinkTimeoutS` | the `SemiActiveRadar` gate verbatim |
| Lost when | the shooter stops designating (phase, damage, its own `FireControl` id, or a turn that breaks the run) | — |
| **Comes back?** | **yes**, unlike the R-27R — a Paveway seeker is a passive energy detector and reacquires a spot that returns. This is the one place the two semi-active kinds differ, and it is a one-line difference in the gate | — |
| Shooter's obligation | `FBSeekerHandoverS = -1` → `pilot/FBEngagement`'s support state runs to predicted **impact**, and `FBPilot::SupportInhibitsDefend` applies | three hooks that exist |
| What is designated | the **steerpoint** — the same briefed point CCRP already aims at | **no new knowledge**: the mission author declared it, exactly as a `wp` line is declared. No targeting pod, no laser spot tracker, no sensor |
| Publication | `FBUnitSignature::Designation` — one bool plus a point, published at the barrier beside `Uplink` | one field |
| Guidance law | **NOT proportional navigation.** The documented Paveway law is *align the velocity vector with the instantaneous LOS to the spot; when aligned, the canards trail and the weapon flies ballistically, gravity-biased* [T2, ED p.572–579]. That is a pursuit law with a **bang-bang** actuator, and it is one branch in `FBMissileGuidance` | one branch |
| Three phases | ballistic (release → acquisition) · transition (align) · terminal (hold) | maps onto the existing `INERTIAL`/`MIDCOURSE`/`TERMINAL` ordinals without renaming a column |
| Accuracy | Paveway II CEP **~6 m** in good conditions [T4]; a second [T4] source gives **9 m** for the GBU-10. Unguided comparison from the manufacturer's own sheet: **94 m** CEP [T4] | **[DISPUTED]**, both carried, and **neither is a FlightBox setting** — the miss is whatever the law produces and the published band is what it is measured against |

**Why the bang-bang detail is worth a line:** it is the reason an LGB's terminal path is sinusoidal and
its miss is bounded rather than asymptotic, and it means the round must **not** be given a proportional
autopilot. Modelling it as PN would produce a better weapon than the real one and hide the delivery
condition that matters — the ballistic phase's velocity at acquisition, which the release attitude sets.

#### 3.3 `cbu87` — the cluster, and the one currency decision it forces

| Field | Value | Provenance |
|---|---|---|
| Canister | SUU-65/B, 950 lb (430 kg) | [T3]/[T4] |
| Submunitions | **202 × BLU-97/B**, 1.5 kg each | [T4] |
| Footprint | **≈200 × 400 m** [T2 Chuck / T4] vs **800 ft × 400 ft = 244 × 122 m** [T3 GlobalSecurity] | **[DISPUTED]**, both carried; FlightBox takes the larger (200 × 400 m) because it is the more conservative reading of the weapon's lethality |
| Function | proximity fuze at a burst altitude; footprint size set by canister speed and altitude at dispersal | [T2]/[T4]. FlightBox models a **fixed** footprint per row and says so |

**The model, and it reuses the second input of the damage model rather than inventing a third.** A cluster
is not a point source: it is an **areal energy density** over a declared footprint, which is exactly the
currency `FBDamageModel::ApplyKinetic` already takes from the gun (`FBGunFluxJm2`, J/m²).

```
flux = N_sub · (kCaseFraction · m_sub) · ½·(kFragSpeedMs² + v_impact²) / (footprint area)     [J/m²]
```

inside the footprint, and **zero outside it**. Worked in §Knowledge 3: 202 bomblets over 200 × 400 m give
**3.13e3 J/m²** — just over `target_soft`'s 2.8e3 failure threshold and far under `target_hard`'s 9.0e4.
So a CBU-87 kills soft things anywhere under it and does nothing to a bunker, **out of the constants that
were already in the tree and without one of them being touched**.

**The declared modelling decision, because it is a third mechanism on one currency:**
[`weapons.md`](weapons.md) §6.3 already states that sharing J/m² between warhead fragments and 20 mm
impacts *"is this simulator's choice, not a statement of equivalence"*. A BLU-97 is a shaped charge **and**
a fragmentation case **and** an incendiary ring; expressing it as one areal density is coarser than either
existing use. **This is the weakest model in the file** and is marked as such rather than discovered in a
result — see §Knowledge 3 for the sensitivity, which is 12 % from flipping the verdict.

#### 3.4 `fab250` / `fab500` — and the rejection nobody has written yet

| Field | Value | Provenance |
|---|---|---|
| FAB-500 M-62 | 500 kg class, 2,470 × 400 mm, **filling 201 kg** | [T3] via [`modules/mig29/weapons.md`](modules/mig29/weapons.md) §5.1 |
| FAB-250 M-62 | 250 kg class, **filling 100 kg** | [T3] ibid. |
| **Release envelope** | **500–1,000 km/h** | [T2] `DCS-FM p.75`, and the MiG-29 base already rules it: *"a rejectable condition (`out_of_context`), not a guideline"* |

**The envelope is the new mechanism, and it is two fields plus one check.** `FBStoreSpec` gains
`ReleaseMinKt`/`ReleaseMaxKt` (0 = no limit, so every existing row is unchanged by construction), and
`FBStoresSystem::Release` gains **check 8** — after the two hardware interlocks and after the fire
control's answer, because it is a property of the STORE and not of the airframe or of the computer:

| # | Condition | Outcome | Reason |
|---|---|---|---|
| 8 | CAS outside `[ReleaseMinKt, ReleaseMaxKt]` | Rejected | `out_of_context`, detail `"store release envelope"` |

**Blocked at the delivery end, and it must be said:** the MiG-29 cannot fly `set task attack` at all
(`C9` — the real jet's unguided delivery is a **director**, not a release cue). The FAB rows are therefore
**carriage and briefed-release only** until `C9` closes, and they are specified here because the store
catalogue is this file's subject and the delivery mode is not.

#### 3.5 The rocket pod — one station, N pickles

Three designs, and the pool arithmetic decides between them.

| Design | Verdict |
|---|---|
| A: a rocket is a **gun row** — `FBGunProjectiles` retires at 3 s / 3000 m, which fits both rockets exactly (Hydra 70 at 700 m/s covers 2,100 m in 3 s; the S-8's documented effective range is 2 km) | **rejected.** The gun path applies **kinetic** energy. A rocket is a fuzed bursting warhead (M151: 3.9 kg loaded, 1.04 kg Comp B4 [T4]; S-8KOM: 3.6 kg warhead [T3]), and the same argument that pushes 57/100 mm AAA onto the store path ([`modules/ground/module.md`](modules/ground/module.md) §Knowledge 4) pushes a rocket there too |
| B: a salvo is **N units** | **rejected, with its price.** 19 rounds per press, 38 for a pair salvo; `kMaxPendingReleases == kMaxStations` is **9**, so the release queue cannot even hold one salvo, and the actor list grows by 19 in one tick |
| **C: the pod is a MAGAZINE on one station** — `PodRounds` on the row, one round released per pickle, `RippleS` minimum spacing | **RECOMMENDED.** Every existing mechanism is untouched: one release, one unit, one queue slot. A salvo becomes repeated `set brief_release_s` lines, which is how every other measured release in the tree is authored |

**What C costs in fidelity, stated:** a real rocket attack is a simultaneous salvo whose product is a
*pattern*; FlightBox produces a string of single impacts. The dispersion figure that would build the
pattern exists and is the best-sourced dispersion number in the tree — **0.3 % of range** [T2
`DCS-FM p.78–79`], i.e. a 6 m CEP at 2 km — and it is **not consumed** by design C. It is recorded here so
the round that wants the pattern has its number.

| Row | Value | Provenance |
|---|---|---|
| `hydra70` (M151) | warhead 3.9 kg loaded / 1.04 kg Comp B4; muzzle 700 m/s; LAU-131 **7** tubes | [T4] |
| `s8` (S-8KOM) | 11.3 kg round, **3.6 kg** warhead, 610 m/s, B-8M1 **20** rounds, motor burn 0.69 s | [T3]/[T2] via `modules/mig29/weapons.md` §5.2 |
| In-range cue | slant range < **8,000 ft** | [T2] ED p.555–557 — documentation; nothing in FlightBox reads it yet |

---

### 4. The radar against the ground — what the two sets honestly do

#### 4.1 The two are unequal, and the inequality is the result

| | **APG-68 (F-16C Block 50)** | **N019 Rubin (MiG-29 9-12)** |
|---|---|---|
| Air-to-ground modes | GM (real-beam ground map), GMT (moving target), DBS/patch expand, SEA, FTT, air-to-ground ranging | **none** |
| How the jet finds a ground target | radar map, TGP, HTS, steerpoint | the **optical sight** and the pilot's eyes; the LRF for range |
| Delivery computation | CCIP / CCRP / post-designate, all radar- or SPI-fed | OPT / TOSS / RETICLE / BS — a **director**, and a correction table read off a grid [T2 `DCS-EA p.99–105`] |
| In FlightBox after this round | **air-to-ground RANGING only** (§4.2) | **nothing, and nothing is claimed** |

**This inequality is not a defect to be equalised.** It is the single largest capability gap between the
two airframes in the tree and it is the reason O3 (ground attack under a friendly umbrella) is blocked at
the module (`C9`) rather than at the weapon.

#### 4.2 What IS modelled — one function, and it is a measurement

**Air-to-ground ranging: the set reports the slant range and the geodetic point at which the commanded
antenna line meets the terrain.** Nothing else.

| Property | Rule |
|---|---|
| Input | the antenna's commanded az/el (the existing `fcr_slew_*` channel) and the elevation provider |
| Output | one range, one lat/lon/elev — into `FBFireControlBlock` beside the existing air-to-ground fields |
| Limits it obeys | the set's own volume, its own range gate, its own frame availability, and `FBSystemId::Radar` (failed → the block is `Invalid` → the fire control falls back to the steerpoint plane, and the HUD dashes) |
| **It radiates** | `FBEmitterKind::AirborneFireControl`, unchanged — so **mapping wakes an `emcon hold` site**, because the site's ESM hears exactly this. Ranging costs silence, and the trade falls out for free |
| It never produces | a contact, a track, a track number, a symbol, an identity, a detection of any kind |

**Why it needs a sensor at all, when the fire control already reads the provider.** Today the FCC samples
the elevation provider **at the steerpoint** — legitimate, because a steerpoint is a briefed point with a
briefed elevation ([`weapons.md`](weapons.md) §4.2, provider letter `B`). Sampling that provider *anywhere
else* is a jet reading terrain height it never measured. **The radar mode is what licenses the second
sample**, and that is the whole reason this function is specified as a radar function instead of as a
better fire-control computation.

**Conservation, by construction:** with `--elev const` the terrain is a flat plane and the ranged point is
the steerpoint plane, to the metre. **Every mission in the tree that runs `--elev const` is byte-identical;
only `--elev tiles` missions can move.** That is the acceptance criterion, and it is also the measurement:
the difference between the two samples IS the terrain-relief error that a level laydown over real ground
carries today and nobody has quantified.

#### 4.3 What is expressly NOT modelled

| Not modelled | Why |
|---|---|
| The ground **map** — real beam, DBS 8:1/64:1, patch, SEA | it is an image, and the tree has no clutter, no terrain return, no ground RCS and no display for one. A "ground contact at truth range" would be a number posing as physics |
| **GMT** (ground moving target) | nothing on the ground moves (`C14`) |
| **FTT** (fixed target track) | it is a track on an image feature; see above |
| A radar cross-section for ground objects | there is none in the tree and inventing one would set every detection range in the campaign |
| HTS emitter geolocation | §2.4 — it needs measurement error first |
| Targeting pod, laser spot tracker, IR video | `C7`-class equipment. The LGB's designation is the steerpoint (§3.2) |
| **Any change to `FBRadarSystem`'s `FBUnitKind::Aircraft` filter** | it stays. *A site is heard, not seen* remains true after this round, and that is deliberate |

---

### 5. Suppressed against destroyed

[`modules/ground/module.md`](modules/ground/module.md) G2 states the problem exactly:
`CombatEffective()` on a site is `Structure` alone, *"a battery with a dead radar and dead rails still
scores as effective, and SEAD is exactly that outcome"*. Two halves are needed, and only one of them is a
verdict.

#### 5.1 The half that is a MECHANISM: a crew that goes off the air

Today a site can go dark for exactly three reasons: `emcon hold` before its cue, `scoot_s` after a launch,
and `Radar` failed. **None of them is a reaction to being attacked**, so nothing in the tree can suppress
anything short of killing it.

| Candidate cue for "we are under attack" | Verdict |
|---|---|
| The inbound anti-radiation round | **impossible, and it must stay impossible.** The round radiates nothing (§2.1) and there is no MWS ([`sensors.md`](sensors.md) gap 8). A site that reacted to it would be reading the world |
| A detonation nearby | there is no acoustic or seismic channel and inventing one is inventing a sensor |
| The attacker's own fire-control radar | **already exists** — it is the `emcon hold` cue, and it is the same signal |
| **The position's own damage** | **RECOMMENDED.** A module may READ its own health register ([`weapons.md`](weapons.md) §7.1: read, never written). `Degraded(Radar)` or any new hit is a fact the site owns about itself, needs no sensor and cannot leak anything |

**The change: `set emcon` gains a third value, `react`.** `free | hold` → `free | hold | react`.

```
react  ≡  behave as `free`, and go dark for ScootS the moment this position's own health register
          reports a change (any newly degraded or failed system)
```

**It adds no seventh `set` key** — the `C1` contract's six author-facing keys are untouched; one of them
gains a value. It is an interface change to
[`modules/ground/module.md`](modules/ground/module.md) §Spec 5/§Spec 9 and is booked in §Gaps as a
declared dependency, the same way the net round booked its cue.

**What is still missing, and it is the honest limit:** the crew reacts to *damage*, i.e. **after** the
round arrives. A real crew goes off the air on a warning, and FlightBox has no warning to give them. So
FlightBox's anti-radiation weapon is **more lethal than history against an attentive crew** and correctly
lethal against an inattentive one. Direction of the error, stated once, here.

#### 5.2 The half that is a VERDICT: `objective suppress`

```
objective suppress unit <callsign> [emitting <s>]      # default 0
objective suppress team <team>     [emitting <s>]
```

| | Rule |
|---|---|
| Fulfilled when | the named unit's **cumulative radiating time over the run** is ≤ `<s>` |
| Violated when | — (it is not a FAIL condition; it is simply unmet) |
| Decided | in `Finalize` — **deferred**, because a site can still come back on. It joins `HasDeferredObjective()` and moves nothing for a mission that does not declare one |
| `FBObjectiveCovers` | **false**, like every non-`kill` kind. Wanting a unit quiet is not declaring that it should die |
| Roster price | **one bool, `Emitting`** on `FBUnitObservation` — filled by the OWNER from the signature it publishes at the barrier (`Sig_.Radar[b].Mode != None`), the identical construction `CombatEffective` and `ReleasedWeapon` already use. It is the first **non-monotone** roster field; the judge's accumulator is monotone, which is what the rule needs |
| Telemetry | **none new.** The site already publishes `site_beam0`/`site_beam1`; the dwell is reconstructible from them. Events: `mission SUPPRESSED` / `mission SUPPRESSION_LOST` |

**It passes [`missions/verdict.md`](missions/verdict.md)'s own test** — *a judge measures what the aircraft
DID, never what it knew*: radiating-or-not is an observed fact about a published signature, the same class
of fact as a health bit, and the declaring unit is never asked what it heard.

**Destroyed implies suppressed, and it is not a special case:** `Radar` failed → the block goes `Invalid`
→ `Emission()` returns `None` → the bit is false forever. The implication falls out of
[`weapons.md`](weapons.md) §8's coupling.

**The false positive, named rather than hidden:** a site that was never woken scores a suppression nobody
earned — the exact dual of `deny release` scoring a jettisoning striker as denied. The reading rule is the
mission's: **`suppress` is only a result when it is paired with something the attacker did** (a
`kill unit`, an `avoid zone`, or a strike objective on the same run), and the mission header says so.

#### 5.3 What is refused

| Candidate | Verdict |
|---|---|
| Extending `CombatEffective()` to ask about `Radar`/`Stores` on a site | **refused.** It is a statement about airframes ([`weapons.md`](weapons.md) §7.5), it is read by the mission monitor for every unit in the tree, and changing it moves every existing verdict. The conservation argument alone kills it |
| A new `FBSystemId` for "suppressed" | it is not a system and it is not monotone. `FBSystemHealth` is monotone by contract |
| A `mission_kill` objective | it is `kill` with a different threshold — and the threshold would be the thing invented |
| Time windows (`suppress … between t0 t1`) | still deferred, for [`missions/verdict.md`](missions/verdict.md)'s own reason: a window is a modifier on **every** kind and multiplies the grammar. `emitting <s>` is a cumulative allowance, not a window |

---

### 6. The pre-existing defect `B1`, and exactly which missions it breaks

**The defect** ([`air-defence-network.md`](air-defence-network.md) Gaps B1, and re-verified in the source
while writing this file):

```
modules/ground/FBGroundModule.h : FBGroundModule(...)  { Rwr_.SetPowered(false); Visual_.SetPowered(false); }
modules/stores/FBStoreModule.h  : FBStoreModule(...)   { Rwr_.SetPowered(false); Visual_.SetPowered(false); }
```

Both unpower the receiver slots and **neither unpowers `Radar_`**, whose default is `Powered_ = true`
(`sensors/FBRadarSystem.h:232`). `units/FBSimUnit.cpp:53` publishes `Radar().Emission()` unconditionally.
So **a bunker and a falling Mk-82 each put an `AirborneFireControl` beam on the air.** Visible in the
committed baseline: `sam-radar-kill.fbm`, `t=41.1 rwr THREAT_NEW unit=ew … kind=fire-control elDeg=40.56`
— that is the bomb, heard by the early-warning site's ESM at 40.6° of elevation.

**The fix is one word in each of two constructors:** `Radar_.SetPowered(false);`. It is an omission and
not a decision — the same constructor already unpowers two other slots for the identical stated reason
(*"nothing it holds can be mistaken for a picture"*).

**And there is a sibling, found while checking it. `B1b`:**

```
units/FBSimUnit.cpp:45 :  Sig_.IffXpdr = Module_->Radar().IffTransponder();
sensors/FBRadarSystem.h:234 :  bool IffXpdr_ = true;
```

The transponder is published **outside the power gate**, so `SetPowered(false)` does not silence it:
after the fix above, **every ground target and every released store still answers IFF Mode 4 as
friendly.** It bites nothing today, because only `Aircraft` are tracked and therefore only `Aircraft` are
ever interrogated — which is precisely why it should be fixed in the same round: `SetIffTransponder(false)`
in both constructors costs **zero** conservation, and leaving a bunker that answers Mode 4 in a tree whose
sharpest anti-cheat test is identification (W5/O2) is asking for it later.

#### The predicted breakage — named before, not explained after

The break is exactly: *a listener that today hears a `target_soft`/`target_hard` or a falling unguided
store*. A listener is an aircraft with `set rwr on` or a site with `FBSiteEsm`. The two sets, taken from
the committed missions:

| Set | Missions |
|---|---|
| has a `target_soft`/`target_hard` | `attack-ccip` `attack-ccrp` `attack-hardened` `attack-late` `escort-protect` `escort-protect-lost` `wx-ccrp-wind` |
| releases an unguided store (`mk82`) | the seven above minus `attack-hardened`'s target, plus `deny-release` `deny-release-broken` `mk82-drop` `mk82-safe` `mk82-carriage-loaded` `objective-covers-none` `sam-radar-kill` `net-blind-cue` |
| **has a listener** (`set rwr on` or a site) | 35 missions, **none of which contains a ground TARGET** |
| **intersection — the prediction** | **`sam-radar-kill.fbm` and `net-blind-cue.fbm`. Exactly two.** |

**Per mission, what changes:**

| Mission | Predicted change | Behaviour change? |
|---|---|---|
| `sam-radar-kill.fbm` | the `ew` site's `rwr THREAT_NEW`/`THREAT_DROP` pair for the falling bomb disappears from `events.log`; its `site_cue` column goes 0 where it was 1. The viper's own `rwr_*` columns may also move (it can hear its own released store — a different unit) | **no.** `ew` is `set emcon free`, so the cue never gated anything, and a `p18` has no weapon |
| `net-blind-cue.fbm` | the same lines for `ew`, **and** the `sam` (SA-6) loses the cue it currently gets from the bomb. Its `site_cue`/`site_state` columns move | **possibly yes**, and the mission says so itself: line 67 reads `set alert cold  # 60 s of warm-up: blind while the bomb is on its way` — a workaround written *for this defect*. After the fix the workaround is unnecessary; the comment is wrong and must be corrected in the same commit, or the file lies about why it is shaped the way it is |
| every other mission | **unchanged, byte for byte** | — |

**The prediction is itself the acceptance criterion — and it was WRONG, in two independent directions.**
Measured (2026-07-28, the fix applied alone, nothing else changed):

| | Predicted | Measured |
|---|---|---|
| `events.log` files that differ | **2** (`sam-radar-kill`, `net-blind-cue`) | **5**: those two plus `deny-release-broken`, `escort-protect`, `escort-protect-lost` |
| `telemetry*.csv` files that differ | **0 outside those two missions** | **15**, across **8** missions |
| behaviour changes anywhere | none in `sam-radar-kill`, "possibly yes" in `net-blind-cue` | **none anywhere**. Every one of the five diffs is a PURE DELETION of phantom `rwr THREAT_NEW`/`THREAT_DROP`/`THREAT_BLIND`/`site CUE` lines; no timing moves, no exit code changes |

**The three extra event logs are exactly the case §6's own residual-uncertainty paragraph named:** the
F-16's RWR is powered by default, so `set rwr on` is not the discriminator. `escort-protect`,
`escort-protect-lost` and `deny-release-broken` each carry an aircraft that hears a falling Mk-82.
The paragraph's fallback estimate of **nine** was wrong too, and in the other direction: having a
listener AND a ground target is necessary but not sufficient — the emitter must also fall inside the
receiver's coverage, and in the four `attack-*` files plus `wx-ccrp-wind` no aircraft ever hears one.

**The fifteen telemetry files are a class the prediction did not consider at all.** They are not
listener effects: they are the two columns the EMITTER publishes about ITSELF — `fcr_on` (the radar
slot's powered bit) and `iff_xpdr` — on every `target_soft`/`target_hard` and every released `mk82`.
There is no way to fix the defect without those two columns moving, because they are the defect. Union
of moved columns over all 15 files: `fcr_on`, `iff_xpdr` and nothing else.

**And the `net-blind-cue` workaround was not a workaround.** §6 predicted that `set alert cold` becomes
unnecessary after the fix. Measured with the line removed: the SA-6 acquires a **firm track** and the
file produces one `site TRACK` line, against a reading rule whose only passing value is zero. The line
is LOAD-BEARING, its comment has been corrected to say so, and the prediction that it was a defect
workaround is withdrawn.

**Residual uncertainty, stated:** the prediction rests on the RWR being unpowered unless a mission writes
`set rwr on` — evidenced by every RWR mission in the tree writing that line explicitly, and by the
`C1`/`C22` rounds recording this defect as visible in *one* baseline file rather than in the seven
`attack-*`/`escort-*` files. If the F-16's RWR is in fact powered by default, the seven ground-target
missions join the list and the count becomes nine. The gate answers it in one run, and the answer is
cheap either way — nothing in those missions consumes an RWR warning.

---

### 7. What the pilot needs — and nothing beyond

**The minimum is one gate, not a phase.** `pilot/FBPilot`'s `Attack` phase is already *"the only phase
whose decision is a MOMENT"* ([`pilot.md`](pilot.md) §4), and a suppression pass is the same shape: run-in
on the briefed leg, one pickle, egress. What differs is only **which instrument supplies the cue**.

| Piece | Change | Why it is the minimum |
|---|---|---|
| `set attack_mode arm` | a third value beside `ccip`/`ccrp` | `attack_mode` is already the switch that selects the cue. A new phase would duplicate the run-in, the egress, the release-latency lead and the SMS-counter watch for one changed comparison |
| The gate | `state.Rwr.H.Readable()` ∧ a threat of the declared class exists ∧ `\|threat.BearingDeg\|` ≤ `SeekerFovDeg` at the predicted moment of release | reads the RWR **block** like every other instrument, exactly as the CCIP gate reads the FireControl block. The pilot learns no range, because there is none to learn |
| The release lead | **unchanged** — `FBCommandBus::LatencyS(WeaponRelease) + DecisionDtS_` | the same statement about his own hands |
| The launch programming | the shooter's own RWR bearing/elevation travels with the round: `FBStoreRelease` gains `CueAzDeg`, `CueElDeg`, `CueValid` | **two floats and a bool.** A position cannot travel, because there is none; the seeker is slaved with the existing `SlewTo(losAz, losEl)` mechanism, which is what the infrared round already does |
| The moment | `set brief_release_s`, as today | **the first proof missions need no pilot change at all** — the mission briefs when, the gate decides whether. The weapon is measurable before the pilot learns anything, which is the whole reason this section is short |

**What the pilot must NOT get this round, listed so the next round knows what it owns:**

| Not now | Whose round |
|---|---|
| Turning toward a threat bearing to put it inside the seeker cone | the pilot's SEAD round |
| Any defensive reaction to a SAM launch, a track warning or a launch light | [`modules/ground/module.md`](modules/ground/module.md) G11 — the same `D3` precedent the eye set |
| Choosing between an ARM and a bomb, or between two threats | weapon-selection logic; needs a threat-value model that does not exist |
| Re-attack, shot-quality judgement, or waiting for a site to come up | all three need a memory of a threat across time, i.e. `FBBfmTrack::Datum` pointed at the ground |
| Route or altitude discipline against a belt | `C23` gives the belt; consuming it is `N10` |

---

### 8. Mission grammar

**Actor scope** — the store rows are ordinary `set store` values, so the grammar is unchanged:

```
unit weasel
  module f16
  set store 3 agm88                # the new rows: agm88 mk84 gbu12 cbu87 fab250 fab500 hydra70 s8
  set store 7 agm88
  set arm_class fire_control       # AnySurface | fire_control | early_warning     [SET] per mission
  set brief_master_arm arm
  set task attack
  set attack_mode arm              # the third cue
  set brief_release_s 120
  objective suppress unit sam emitting 30
  objective survive
```

| New key | Values | Effect |
|---|---|---|
| `arm_class` | `any` \| `fire_control` \| `early_warning` | the anti-radiation seeker's target-class filter. `[SET]` per mission; the coarse form of the ALIC table |
| `attack_mode arm` | — | the release cue is the RWR bearing instead of a ballistic countdown |

**On the defender, one value on an existing key** (§5.1):

```
  set emcon react                  # free | hold | react
```

**One objective kind** (§5.2), mission scope unchanged:

```
objective suppress unit <callsign> [emitting <s>]
objective suppress team <team>     [emitting <s>]
```

Parse rules follow the tree: an unknown store key is a runtime FAIL at spawn; an unknown `arm_class` value
is a `SET_REJECTED` FAIL; `suppress` naming a unit that does not exist is a parse error (the `objective`
rule).

---

### 9. Observable

| Channel | Content |
|---|---|
| Telemetry, `msl_*` (existing columns, reused) | `msl_range` = **−1** for an anti-radiation round throughout, exactly as for an infrared one — the column says what the seeker cannot measure. `msl_seeker` 0/1/2, `msl_tgt_age` = time since the last real look (**this is the memory clock**), `msl_losrate`, `msl_los_az`/`_el` |
| Telemetry, `msl_sig` (**one new column**) | the seeker's received power, 0..1 — the only proximity hint the round has, and the one number that says why it locked what it locked |
| Telemetry, fire control | one new air-to-ground ranging pair (`fc_ag_rng_m`, `fc_ag_elev_m`), invalid when the mode is off or `Radar` is failed |
| Events, `missile` | `PROGRAMMED` (with the angular cue instead of a target state), `SEEKER_ACTIVE`, **`EMITTER_LOST`** (time of flight, range-to-go unknown, memory remaining), **`EMITTER_REACQUIRED`**, `PHASE` |
| Events, `sms` | `RELEASE_REJECTED … reason=out_of_context detail="store release envelope"` (the FAB check) |
| Events, `site` | `GO_DARK reason=damage` (the `react` value) |
| Events, `mission` | `SUPPRESSED` / `SUPPRESSION_LOST` (cumulative emitting seconds, allowance) |
| Reused unchanged | `stores IMPACT` / `DELIVERY` / `DETONATION`, `damage *`, every `site *`, `UNIT_RESULT` |

---

### 10. Acceptance criteria

Measured, not argued.

| # | Criterion | Measurement |
|---|---|---|
| 1 | **The gate did not widen** | `verify-layers` prints *6 registry reader(s) inside the perception boundary*, and the `units/FBUnitRegistry.h` includer tuple is byte-compared before and after |
| 2 | **Existing missions untouched — except two, named in advance** | every `telemetry*.csv` byte-identical; every `events.log` identical modulo `wallS`/`speedup`/path **except `sam-radar-kill` and `net-blind-cue`** (§6). A third difference is a finding |
| 3 | **The transmitter decides the shot** | one geometry, three runs differing only in when the site goes dark (`set scoot_s`): shutdown at ~30 %, ~60 % and ~85 % of the round's time of flight. The three miss distances must follow the `(t_go/t_f)^4` law of §Knowledge 1 to within the tick, and exactly the last one may be a hit |
| 4 | **A shot from further off the nose is easier to escape** | the same site, two launches at ~5° and ~35° off boresight with the identical shutdown time: the second misses and the first does not |
| 5 | **The round is silent** | the target site's own ESM produces **zero** `site CUE` lines attributable to the inbound round, and its RWR-equivalent table never contains it |
| 6 | **It has no IFF** | one mission, one friendly emitter inside the cone: the round homes on it. Ugly, correct, and it is the acceptance criterion for "no identity" |
| 7 | **A laser-guided bomb binds its shooter** | two runs: designation held to impact → hit; designation broken mid-fall → the bomb goes ballistic and misses; designation restored before impact → **it reacquires and hits**. Three branches, because the third is what distinguishes it from the R-27R |
| 8 | **The cluster is an area and the Mk-84 is a radius** | one `target_soft` displaced 90 m from the aim point: the Mk-84 kills it (100 m radius, §Knowledge 2) and the Mk-82 does not (45 m). One `target_soft` at 150 m: the CBU-87 kills it (inside the 200 × 400 m footprint) and the Mk-84 does not |
| 9 | **The envelope refuses** | one FAB release at 1,100 km/h → `RELEASE_REJECTED`; the same file at 900 km/h → accepted |
| 10 | **Ranging changes nothing on flat ground** | every `--elev const` mission byte-identical with the air-to-ground mode on; one `--elev tiles` mission over relief shows a non-zero difference between the ranged elevation and the steerpoint elevation |
| 11 | **Mapping costs silence** | two runs against an `emcon hold` site, differing only in whether the attacker uses the air-to-ground mode: the mapping run produces `site CUE` and `site RADIATE`, the silent run produces neither |
| 12 | **Suppressed is not destroyed** | one run in which the site survives structurally and stops radiating: `objective suppress` MET, `objective kill unit` UNMET, `UNIT_RESULT … INTACT`. One run in which it dies: both met |
| 13 | Determinism | one fingerprint over `--threads 1/2/4` × 3 repeats |

---

### 11. The anti-cheat argument, in full

| Candidate path | Verdict |
|---|---|
| The anti-radiation seeker | **not a seventh reader.** `FBRwrSystem.cpp` already holds the include; the derivation overrides `Blanked()` and the target filter and adds none |
| The air-to-ground radar function | reads the **elevation provider**, which is not the registry, and is licensed by a sensor precisely so the fire control does not read terrain it never measured (§4.2) |
| The laser designation | **published**, not read: `FBUnitSignature::Designation` at the barrier, like `Uplink` and the chaff clouds. The bomb takes the point by value from the ONE unit whose id is its launcher — the `FBMissileUplink` rule verbatim, and it is already one of the six |
| The designated POINT itself | is the **steerpoint**, i.e. mission-author knowledge, the same class as a `wp` line. A round that flies to a briefed point learns nothing about the world |
| The `suppress` judge | reads the owner-filled roster. `FBMissionMonitor` is allowed the truth by construction; no module is asked and none can answer |
| A HARM that is "told" where the site is | **refused** (§2.4). It is the one thing in this file that would have been easy and it is the thing the whole file is shaped to prevent |

**And the one sentence that carries it:** every new weapon here is aimed by something that already exists —
its own passive seeker, its shooter's published illumination, or a point the mission author wrote down.
**Nothing in this file gives a pilot, a module or a round a fact it did not measure or was not briefed.**

---

## State

**BUILT.** `C8` closes except the rocket pod, `C26` and `C27` close, `C25` is untouched.

### What exists now

| Piece | Where | Proof |
|---|---|---|
| **The anti-radiation round** `agm88` — a bearing, an elevation and a received power, never a range | `core/FBStore.h` (row) · `modules/missile/FBMissileArSeeker` (`: sensors/FBRwrSystem`, two existing hooks overridden, **no new include**) · `FBMissileGuidance::AntiRadiationCommand` | `arm-escape-frontal` `arm-escape-offset` `arm-shutdown-late` `arm-shutdown-early` `arm-reacquire` `arm-pilot-cue` |
| **The RATE memory**, `SeekerMemoryS` 4.0: after the last reception the measured LOS rate is held, then ZERO — PN commands nothing lateral, the accelerometer's 1 g bias survives, the round coasts STRAIGHT | `FBMissileGuidance::AntiRadiationCommand` | the four `arm-*` escape files |
| **`FBSeekerKind::SemiActiveLaser`** and the Paveway PURSUIT law with a rate-stabilised bang-bang relay — expressly not proportional navigation | `FBMissileGuidance::LaserCommand` · `assets/aircraft/gbu12/` | `lgb-designate` (3.9 m) `lgb-lase-broken` (1 895 m) `lgb-lase-restored` (9.1 m) |
| **The designation as a published STATE**, beside the midcourse uplink and read the same way | `FBLaserDesignation` in `core/FBStore.h` · `FBUnitSignature::Designation` · `FBStoresSystem::Designation()` · `FBMissileUplink` | the three `lgb-*` files |
| **The shooter binds itself**: while its own SMS reports `Designating`, the attack pass does not start the egress | `FBStoresBlock::Designating` · `FBPilot::AttackCommands`, one instrument read | `lgb-designate` vs `lgb-lase-broken` |
| **Mk-84**, a factor of 2.2 in lethal radius over the Mk-82, out of the untouched fragment law | `core/FBStore.h` · `assets/aircraft/mk84/` | `mk84-radius` vs `mk82-radius` |
| **CBU-87**, an AREA and not a radius — the areal-energy-density input the damage model already took from the gun | `FBMissionRunner::ResolveClusterBurst` | `cbu87-footprint` |
| **The release envelope**, `FBStoresSystem::Release` check 8, after both hardware interlocks and the fire control | `core/FBStore.h` (`ReleaseMinKt`/`MaxKt`) · `weapons/FBStoresSystem.cpp` | `fab-envelope-reject` vs `fab-envelope-ok` |
| **The air-burst resolution path**, §Gaps collision 2: a store with `FuzeRadiusM > 0` now resolves its burst against `Ground` units at closest approach too, the launcher excluded | `FBMissionRunner`, the fuze loop | every `arm-*` hit |
| **`set emcon react`** — the crew goes dark for `scoot_s` on its own health register's next hit | `FBSiteFireControl` · `FBModule::OwnHits()` | `sam-emcon-react` |
| **`objective suppress unit\|team [emitting <s>]`**, deferred, judged on a monotone accumulator over a non-monotone roster bit | `core/FBObjective.h` · `FBMissionMonitor::NoteEmitting` | `suppress-quiet` vs `suppress-killed` |
| **`set attack_mode arm` + `set arm_class`** — the release cue is the RWR's bearing inside the round's own cone | `FBPilot::AttackCommands` · `FBF16Module` | `arm-pilot-cue` |
| **Re-acquisition** — a latched symbol that leaves the receiver's table is a transmitter that stopped, and the seeker goes back to looking | `FBMissileGuidance::UpdateTarget` | `arm-reacquire` (dark 20 s, re-taken under a NEW symbol, site destroyed) |
| **The B1/B1b fix** — a bunker and a falling bomb no longer radiate and no longer answer IFF Mode 4 | `FBGroundModule` · `FBStoreModule` constructors | §6, measured below |

### The gates, measured

| Gate | Result |
|---|---|
| `verify-layers` | *285 files, 765 internal include(s), 12 layers — no upward include, 3 restricted header(s) respected, **6 registry reader(s) inside the perception boundary**, 272 file(s) in their layer's namespace (5 C-island file(s) exempt)* — the reader list is unchanged |
| `verify-models` | *4 upstream-backed model path(s) match assets/MODEL-DELTAS.md (1 declared delta(s), 17 FlightBox-own)* |
| `core-lib` `gym` `native` `wasm` | all clean under `-Wall -Wextra -Wpedantic -Werror`; `nm build/fb-gym` = **0** Dawn/WebGPU symbols |
| Nine harnesses | rc = 0 |
| Conservation | after the B1 fix, **all 113 remaining missions byte-identical** — telemetry AND `events.log` — for everything in this file |
| Determinism | the whole set identical over `--threads 1 / 2 / 4` |

---

---

## Gaps

### What the contract did not carry, measured

Six places where building it produced a different answer than writing it did. Every one is a
MEASUREMENT and none of them was tuned away.

| # | The contract said | The build measured | What it means |
|---|---|---|---|
| **F1** | the escape boundary at 20 km sits at **61 %** of the flight for a 5° shot and **76 %** for a 35° shot (§Knowledge 1) | **85.0 %** and **88.1 %** (bisected on `UNIT_RESULT unit=sam`, `arm-escape-*` geometry, release at t = 1.5 s, t_f 62.24 s / 68.50 s) | the crew has 9–13 percentage points MORE time than predicted. **The qualitative claim holds exactly**: the frontal shot is the harder one to escape (85.0 < 88.1) and it falls out of the geometry. The quantitative one is optimistic for the attacker, for three named reasons below |
| **F1a** | `ZEM(0) = R·sin(θ_offset)` | that is the LATERAL half only | the shots are flown from 4 000 m against a site at ~450 m over 20 km, i.e. an 11.3° DEPRESSION whose vertical aim error is 3.92 km — larger than the 5° shot's 1.74 km lateral one. With `ZEM(0) = 4.29 km` / `12.1 km` the predicted boundaries become 68.7 % / 75.9 % |
| **F1b** | the miss decays as `(t_go/t_f)⁴` | it decays as **`(t_go/t_f)^2.3`** — measured 490 m at 60 %, 266 at 70 %, 98 at 80 %, 2.4 at 90 % on the 5° shot | the law's `N = 4` assumes a constant closing speed and a linearised geometry; this round decelerates from ~500 to 280 m/s over its last third, so the effective navigation gain falls. This is the dominant residual |
| **F1c** | `SeekerMemoryS` "shifts the effective shutdown one memory-length later" | it does, and it is worth **+6.4 pp** (5°) and **+5.8 pp** (35°) | the contract said so; it is quantified here. F1a + F1c together predict 75.1 % / 81.7 % against 85.0 / 88.1 measured, and F1b is the rest |
| **F2** | the B1 fix breaks **exactly two** missions | it breaks **five** `events.log` files and **fifteen** telemetry files across **eight** missions | see §6 below |
| **F3** | §9: `msl_sig`, "one new column" · §10 criterion 2: every `telemetry*.csv` byte-identical | **the two cannot both hold.** `msl_*` is not the last source on a round's bus, so a column added there shifts every column right of it in 95 already-measured files | resolved in favour of the append rule (`units/FBSimUnit.cpp`): **no `msl_sig` column**. The number is not lost — this round's head IS a warning receiver, so it publishes the whole `rwr_*` group on the same trace and the power is exactly `(rwr_leth − base(rwr_mode)) / 0.15`; the events carry it verbatim (`SEEKER_ACTIVE … signal=`, `EMITTER_LOST … lastSignal=`) |
| **F4** | §3.2: the shooter's obligation runs "to predicted **impact**", broken by "a turn that breaks the run" | **an F-16 with a nose-referenced designator cannot lase to impact from a level pass at all.** An unpowered bomb covers ground at `v·cos θ < v`, so the jet ALWAYS reaches the target first and the spot goes behind it (measured: 5.6 km standoff at 4 000 m / 450 kt, spot lost 5.7 s before impact, 229 m short) | it is only reachable where the bomb's ground speed exceeds the jet's — measured at **250 kt / 4 000 m**, where the shooter holds the spot all the way and the miss is 3.9 m. Stated rather than repaired: §4.3 refuses a targeting pod, and without one this is what the geometry gives |
| **F5** | §Gaps collision 2: widen the burst to `Ground` at closest approach | doing that ALSO moved `stores MISS` in `net-belt-high` (a V-750 recorded a 410 m pass over a bunker instead of a 9 525 m pass at the aircraft) | `stores MISS` stays a record about AIRCRAFT; the fuze reach is widened and the launcher is excluded from it. Where a round came down against a POSITION is `stores IMPACT`'s `crossLat`/`crossLon` |
| **F6** | §10 criterion 3: three runs "differing only in when the site goes dark (`set scoot_s`)" | **`scoot_s` cannot be placed in time** — it needs a launch, and `react` needs a hit; neither is a clock | `set emcon` gained an optional briefed PLAN (`free <offS> [<onS>]`) — one value on an existing key, the C1 contract's six author-facing keys still six. Without it the escape window is unmeasurable, which is the whole subject of this file |

### The honest headline

**This file removes the asymmetry `C1` and `C22` declared, and it introduces a smaller one in the other
direction.** After it, the air can punish a transmitter — but the ground's only defence against that
punishment is to have gone dark *before* the round arrives, and the only cue it has for doing so is its own
damage (§5.1) or a doctrine it declared before the run. **FlightBox's anti-radiation weapon is therefore
harder to defeat than the real one**, and the two things that would balance it are named and not built:
a warning the crew can act on (which needs a channel that does not exist) and a battery that can **move**
(`C14`).

Second, and it must be said as plainly as the ground file said its version: **a site is still heard and
not seen.** §4 deliberately does not change that. The air gains a weapon that needs no sight; it does not
gain sight.

### New gaps, booked here

| ID | Gap | Home |
|---|---|---|
| `C8` | **The store catalogue — BUILT except the rocket pod.** `agm88` `mk84` `gbu12` `cbu87` `fab250` `fab500` fly; both new `FBSeekerKind` values and the release check exist. `hydra70`/`s8` and the `PodRounds`/`RippleS` magazine group are **NOT built**: design C costs a field group, an FDM model with a motor a guidance-free module must light, and a mission grammar for a ripple, and none of it is on the acceptance path of this round | this file, §3.5 |
| `C25` | **NOT BUILT. No air-to-ground radar function.** Specified here as **ranging only** (§4.2) with an exhaustive refusal list (§4.3). [`sensors.md`](sensors.md) gap 3 is its symptom and keeps its wording; the ID and the contract live here | this file, §4 |
| ~~`C26`~~ | **CLOSED.** `set emcon react` + `objective suppress`, `CombatEffective` untouched, roster price one bool. [`modules/ground/module.md`](modules/ground/module.md) G2's second option is answered | this file, §5 |
| ~~`C27`~~ | **CLOSED.** One gate, one `attack_mode` value, `set arm_class` beside it. Everything above it is still the pilot's own round | this file, §7 |

### Existing gaps this file touches

| ID | What changes | What does not |
|---|---|---|
| `C1` | `set emcon` gains the value `react`; the six author-facing keys stay six | the five-state machine, the two beams, the nine rows |
| `C4` | **nothing.** Air-to-ground ranging samples an elevation provider along one commanded line; it is not terrain masking and does not pretend to be | radar, RWR, IRST and the eye still see through mountains |
| `C9` | the FAB rows exist; the MiG-29 still cannot deliver them | the director-based delivery is untouched and is that module's round |
| `C11` | **nothing.** No strafing: the gun pool still gives up at 3 s / 3000 m and ground targets still present zero area | — |
| `C13` | **nothing.** The radar half stays wholly open, and §2.4 explains why home-on-jam cannot exist without it | — |
| `C14` | unchanged, and it is now load-bearing: a battery that cannot move cannot use the one countermeasure history gave it | — |

### Named, quantified, refused for a reason

| # | Gap | Detail |
|---|---|---|
| N1 | **No emitter geolocation (HTS/POS/EOM)** | refused until measurement error exists ([`sensors.md`](sensors.md) gap 4). With exact bearings, triangulation returns the exact truth — a registry read with extra steps. This is the single largest capability the real F-16CJ has and FlightBox does not, and `w4`'s SEAD jet is that aircraft |
| N2 | **A cluster footprint is fixed** | the real footprint is set by canister speed and altitude at dispersal [T2]. FlightBox declares one per row. A release from 1,000 m and one from 5,000 m produce the same pattern |
| N3 | **The cluster's J/m² is the coarsest use of that currency in the tree** | §Knowledge 3: the verdict against `target_soft` sits 12 % above the failure threshold, so `kCaseFraction` and `kFragSpeedMs` — two declared `[SET]`s — decide it. Any future calibration of either moves this row first |
| N4 | **A rocket salvo is a string, not a pattern** | design C, §3.5. The 0.3 %-of-range dispersion [T2] is the best-sourced dispersion figure in the tree and is not consumed |
| N5 | **No station compatibility** | `FBStoresSystem::Load` refuses only unknown or occupied stations, so a mission may hang an Mk-84 on a wingtip rail. Pre-existing; it becomes visible the moment the catalogue has stores that differ by a factor of four in mass |
| N6 | **No fuzing options** | NOSE/TAIL/NSTL, air-burst versus impact versus delay, cluster burst altitude, JDAM's arming-delay list — all documented [T2], none modelled. `ArmingS` is the only fuze quantity in the tree |
| N7 | **No LGB laser codes and no designator other than the shooter** | buddy-lasing, a ground designator and a PRF-code mismatch are all one field away and all need a second aircraft's behaviour, i.e. `C15` |
| N8 | **The anti-radiation round cannot be given a shot-quality answer** | with no range it has no DLZ, so `sms LAUNCH_OUT_OF_ZONE` can never fire for it and a wasted round produces no rejection. That is faithful and it means a mission cannot measure "he shot from too far" except by counting misses |
| N9 | **Nothing measures suppression from the attacker's side** | `objective suppress` is scored on the target's published bit. The attacker's own belief — "I think I put him off the air" — has no representation, and building one would need the RWR history the pilot does not keep |
| N10 | **The Mk-84's own fragment envelope is not resolved against the releasing aircraft** | [`weapons.md`](weapons.md) §5.4 stands, and with a 100 m soft-kill radius a level laydown now flies through an envelope the model says nothing about. The omission was defensible with an Mk-82 and is merely *stated* with an Mk-84 |

### Collisions with the existing tree — the three places it is actively in the way

| # | Collision | Detail | Resolution proposed |
|---|---|---|---|
| 1 | **The warning receiver refuses to produce a range, so the real weapon's memory cannot be built** | [`sensors.md`](sensors.md) §5.3: *"nothing downstream can accidentally fly a range solution from a warning receiver"* — written to protect the pilot, and it binds the seeker derived from that class too. The documented AGM-88 memory mode needs a position; the tree's only honest routes to one are a shooter handover (needs HTS, needs measurement error) or a terrain intersection (makes weapon accuracy a function of `--elev`) | **none, deliberately.** The contract wins and the weapon is specified as the bearing-only round FlightBox can build honestly (§2.2), with the direction of the resulting error stated. This is the collision that shaped the whole file |
| 2 | **A proximity fuze is resolved against `Aircraft` only, so no air-to-ground weapon can burst above its target** | `weapons.md` §5.1 gates on `FBUnitKind::Aircraft`; §5.3 resolves a store only where it crosses the ground. A HARM with a 5 m laser fuze, a cluster functioning at altitude and every future air-burst weapon therefore have **no resolution path at all** — the boundary was written for the other direction | widen `ResolveGroundBurst`'s trigger: a store with `FuzeRadiusM > 0` resolves its burst against `Ground` units on closest approach, with the identical CPA machinery, in addition to the ground crossing. **§5.4's refusal is untouched** — that one forbids a ground burst against *aircraft*, for want of a fragment-against-airframe geometry, and this is its mirror image, not its exception |
| 3 | **The release interlocks ask the FIRE CONTROL, and this weapon's precondition is a SEEKER** | `Release()` checks 5–7 read the cached `FBFireControlBlock` (lock, DLZ, envelope). An anti-radiation round needs none of the three and needs one thing the tree does not have: **a pre-launch seeker state on a store still on the rail** — the identical defect [`modules/ground/module.md`](modules/ground/module.md) B1 measured on MANPADS, where the gunner has a tone and FlightBox does not | `RequiresLock = false`, and the cue travels as three fields on `FBStoreRelease` (§7) taken from the **shooter's own** receiver — the same construction a guided round's `Target` uses. The round acquires after separation or does not, and the wasted round is a real outcome. **The general fix (a rail-borne seeker that can report before launch) stays open and is now a defect of two weapon families rather than one** |

---

## Knowledge

### 1. The escape window is a derivation, not a rule

The question *"what happens between launch and impact when the transmitter stops"* has a closed answer,
and it comes out of the guidance law the tree already runs rather than out of anything written for this
weapon.

For linearised proportional navigation with navigation constant `N` against a non-manoeuvring target, the
**zero-effort miss** — literally *"the miss you get if you stop steering now"* — evolves as

```
ZEM(t) / ZEM(0)  =  (t_go / t_f)^N
```

Design A's coast (§2.2) IS "stop steering now": after `SeekerMemoryS` the LOS rate is zero, PN commands
nothing lateral, and the surviving 1 g bias makes the coast a straight line rather than a ballistic arc.
So the miss produced by a shutdown at time `t` is exactly `ZEM(t)`, with `N = kNavConstant = 4`
([`weapons.md`](weapons.md) §10.2).

**`ZEM(0)` is the launch aim error**, and for a rail-launched round it is set by the off-boresight angle:
`ZEM(0) ≈ R · sin(θ_offset)`. Two worked shots at 20 km, tabulated as the **pure law** — the memory is
not in the table, because its whole effect is to shift the *effective* shutdown one `SeekerMemoryS`
later, i.e. it moves a crew one row down:

| Elapsed at shutdown | `t_go/t_f` | `(·)⁴` | Miss, 35° off-boresight shot (`ZEM₀` = 11.5 km) | Miss, 5° shot (`ZEM₀` = 1.74 km) |
|---|---|---|---|---|
| 25 % | 0.75 | 0.316 | 3 630 m | 550 m |
| 50 % | 0.50 | 0.0625 | 717 m | 109 m |
| 60 % | 0.40 | 0.0256 | 294 m | 44.6 m |
| 70 % | 0.30 | 0.0081 | 93 m | 14.1 m |
| 80 % | 0.20 | 0.0016 | 18.4 m | 2.8 m |
| 90 % | 0.10 | 1.0e−4 | 1.15 m | 0.17 m |

**Against what does one read those numbers?** Not the fuze radius — a 66 kg warhead's *lethality* radius
against an unprotected position, from the tree's own model (§Knowledge 2 arithmetic with `WarheadKg` 66
and an arrival speed of ~600 m/s): `flux(r) = 4.73e6 / r²`, hence

```
target_soft  fails at 2.8e3 J/m²  →  r = 41 m
target_hard  fails at 9.0e4 J/m²  →  r =  7 m
```

**The crew's boundary is therefore ~76 % of the round's flight for a 35° shot and ~61 % for a 5° shot**
(elapsed = 1 − `(41/ZEM₀)^0.25`: 0.244 and 0.392 of the time of flight remaining, respectively). Three consequences fall out and none was put in by hand:

1. **The shot taken straight ahead is the hard one to escape.** A pilot who turns to put the threat on the
   nose buys a smaller `ZEM(0)` and therefore a shorter window for the crew — the geometry decides, not a
   weapon parameter.
2. **`SeekerMemoryS` is worth exactly its own seconds of time-to-go**, which is why it is the one setting
   in this weapon that is allowed to decide a shot and why it is logged.
3. **A large `ZEM(0)` may not be flyable at all.** Pulling out an 11 km zero-effort miss demands lateral
   acceleration the airframe may not have at that dynamic pressure (`kMaxCommandG` 25 is a *command*
   ceiling, not a capability). Where the fins saturate, the round misses even without a shutdown — and the
   table above is then an upper bound on the weapon rather than a description of it.

### 2. What a bigger bomb buys, in the model's own currency

[`weapons.md`](weapons.md) §9.2's Mk-82 relation, re-derived for the new rows (`kCaseFraction` 0.5,
`kFragSpeedMs` 1800, arrival speed as noted):

```
m_frag = kCaseFraction · WarheadKg          rho_A = m_frag / (4π r²)
flux(r) = ½ · rho_A · (kFragSpeedMs² + v_impact²)
```

| Round | `WarheadKg` | `v_impact` | `flux(r)` | `target_soft` fail (2.8e3) | `target_soft` degrade (1.2e3) | `target_hard` fail (9.0e4) |
|---|---|---|---|---|---|---|
| Mk-82 | 87 | 245 m/s | 5.71e6 / r² | **45 m** | 69 m | 8 m |
| **Mk-84** | **428.6** | 245 m/s | **2.81e7 / r²** | **100 m** | **153 m** | **17.7 m** |
| **AGM-88** | **66** | ~600 m/s | **4.73e6 / r²** | **41 m** | 63 m | 7.2 m |
| GBU-12 | 87 | 245 m/s | as Mk-82 | 45 m | 69 m | 8 m |

**Reading:** the Mk-84 is worth a factor of **2.2 in radius** against both classes — which is the entire
reason to carry one instead of two Mk-82s, and it is why it is the store that makes a `target_hard`
attackable at all with an unguided delivery (17.7 m against a measured 22 m CCIP/CCRP error is a coin
toss; 8 m is not). The GBU-12 changes **nothing** about lethality and everything about whether the
delivery error is inside it — the two halves of a strike, cleanly separated.

### 3. The cluster, and where its number is soft

```
E_bomblet = kCaseFraction · m_sub · ½ · (kFragSpeedMs² + v_impact²)
          = 0.5 · 1.5 kg · ½ · (1800² + 250²)  =  1.238e6 J
flux      = 202 · 1.238e6 / (200 · 400 m²)     =  3.13e3 J/m²
```

| Target | Threshold | Verdict |
|---|---|---|
| `target_soft` | fail 2.8e3 | **destroyed anywhere under the footprint** — by a margin of **12 %** |
| `target_hard` | degrade 2.5e4 | **untouched**, by a factor of 8 |
| the smaller disputed footprint (244 × 122 m) | — | 8.4e3 J/m², i.e. 3× the soft threshold — the same verdict with room |

**The 12 % is the honest headline of this row.** Two `[SET]` constants (`kCaseFraction`, `kFragSpeedMs`)
and one `[DISPUTED]` footprint decide whether a CBU-87 kills a soft position or merely degrades it. It is
recorded as N3 rather than tuned, and if either constant is ever calibrated this is the first row to
re-derive.

### 4. Why the laser-guided bomb is not flown with proportional navigation

PN drives the line-of-sight **rate** to zero and needs a lateral acceleration proportional to it. A
Paveway II has neither the sensor for a rate nor the actuator for a proportional command: the CCG's four
canards are **bang-bang** — full deflection or trailing, nothing between [T2, ED p.572–579] — and the
documented law is *align the velocity vector with the instantaneous LOS to the spot; when aligned, trail
the canards and fall ballistically, gravity-biased toward the target.*

That is a **pursuit** law with a dead band, and modelling it as PN would produce a materially better weapon
than the real one: PN leads the target and arrives with its turn done, while a pursuit law chases and pays
for it in energy — which is exactly why the ED text warns that velocity lost in the ballistic phase
*"directly reduces terminal maneuverability"*. The delivery condition (release attitude and speed) is
therefore a real property of an LGB shot and not of a JDAM shot, and it is the thing the mission measures.

### 5. Rejected designs, with the reason each was rejected

| Design | Why not |
|---|---|
| A ground-map mode that produces `FBRadarContact`s for ground units | it would give the pilot a **range** to a site, delete the RWR's whole point, and require a ground RCS model, clutter and terrain returns — none of which exist. It is `air-defence-network.md` §Knowledge 1's counterfactual, committed one layer over |
| An anti-radiation round with a POINT memory | §2.2 design B: needs a range the tree refuses to produce, and deletes the countermeasure the previous round built |
| HTS geolocation | exact bearings triangulate exactly; the capability's own accuracy would be the *absence* of sensor noise |
| A `suppressed` flag on `FBSystemHealth` | the register is monotone by contract, and suppression is not |
| Widening `CombatEffective()` on a site | it is read for every unit in the tree; the conservation argument alone kills it |
| A rocket salvo as N units | 19 units per trigger against a 9-deep release queue |
| A cluster as 202 units | the same argument, eleven times worse |
| A separate `Suppress` pilot phase | it duplicates run-in, egress and release-latency lead for one changed comparison |
| Modelling the LGB with PN | §Knowledge 4 — it would be a better weapon than the real one |

### 6. Where the numbers live, and which of them are settings

| Kind of number | Home | Status |
|---|---|---|
| warhead masses, launch masses, footprints, submunition counts, release envelopes, muzzle velocities | this file, §§2–3, and [`modules/f16/weapons.md`](modules/f16/weapons.md) §3 / [`modules/mig29/weapons.md`](modules/mig29/weapons.md) §5 | **sourced**, tiered, disputes carried both ways |
| `SeekerFovDeg`, `SeekerMemoryS`, `FuzeRadiusM`, `arm_class`, the cluster footprint choice | this file | **`[SET]`**, each with one sentence of reason; `SeekerMemoryS` is the only one that decides a shot |
| fragility thresholds, `kCaseFraction`, `kFragSpeedMs`, `kNavConstant` | [`weapons.md`](weapons.md) §6/§9/§10 | **unchanged and untouched** — every number in §Knowledge 2/3 is derived from them, which is why the new rows produce verdicts rather than assertions |
| aerodynamic decks | `sim/assets/aircraft/<key>/`, by the slender-body recipe | **`[DERIVED]`**; the bomb decks inherit the Mk-82's declared fidelity caveat |
| the ZEM law | §Knowledge 1 | **`[DERIVED]`** from the tree's own `kNavConstant` |

**External sources, tiered:**

| Tier | Document |
|---|---|
| [T2] | DCS *ED Early Access Guide* and *Chuck's Guide* via [`modules/f16/weapons.md`](modules/f16/weapons.md) §2.6/§3 (HARM HAS/POS/ALIC, Paveway II CCG and the three phases, WCMD/JDAM profile pages, rockets, strafe) and [`modules/mig29/weapons.md`](modules/mig29/weapons.md) §5 (FAB/RBK/KMGU/S-8/S-24 and the OPT/TOSS/RETICLE/BS delivery modes) |
| [T3] | [designation-systems.net — AGM-45 Shrike](https://www.designation-systems.net/dusrm/m-45.html) · [AGM-88 HARM](https://www.designation-systems.net/dusrm/m-88.html) · [GlobalSecurity — AGM-88 specifications](https://www.globalsecurity.org/military/systems/munitions/agm-88-specs.htm) · [GlobalSecurity — CBU-87 CEM](https://www.globalsecurity.org/military/systems/munitions/cbu-87.htm) · [Air & Space Forces Magazine — GBU-10/12/49 Paveway II](https://www.airandspaceforces.com/weapons/gbu-10-12-49-paveway-ii/) |
| [T4] | [Wikipedia — AGM-88 HARM](https://en.wikipedia.org/wiki/AGM-88_HARM) · [Mark 84 bomb](https://en.wikipedia.org/wiki/Mark_84_bomb) · [GBU-10 Paveway II](https://en.wikipedia.org/wiki/GBU-10_Paveway_II) · [GBU-12 Paveway II](https://en.wikipedia.org/wiki/GBU-12_Paveway_II) · [CBU-87 Combined Effects Munition](https://en.wikipedia.org/wiki/CBU-87_Combined_Effects_Munition) · [Hydra 70](https://en.wikipedia.org/wiki/Hydra_70) · [FAB-500](https://en.wikipedia.org/wiki/FAB-500) · [FAB-250](https://en.wikipedia.org/wiki/FAB-250) |

**Identified and NOT read** — the [T1] material that would move the load-bearing numbers: USAF technical
orders and the *Joint Munitions Effectiveness Manual* (JMEM) series, which is the one document class that
would replace the tree's `[SET]` fragility thresholds and the cluster's areal model with sourced
effectiveness data; and the AGM-88 NTSP document already surfaced but not retrieved
([GlobalSecurity mirror](https://www.globalsecurity.org/military/library/policy/navy/ntsp/agm-88-d_2002.pdf)),
which is the nearest thing to a [T1] statement about the memory mode this file's central decision turns on.

---

## Related

| Place | Relationship |
|---|---|
| [`modules/ground/module.md`](modules/ground/module.md) | **the target.** Its G2 is answered in §5, its `set emcon` gains one value, its B1 (a rail-borne seeker with no pre-launch state) is confirmed as a defect of two weapon families |
| [`modules/ground/catalogue.md`](modules/ground/catalogue.md) | the nine rows the weapon is aimed at, and their emitter kinds — which is what `arm_class` filters on |
| [`air-defence-network.md`](air-defence-network.md) | **the file this one answers.** Its B1 is fixed in §6; its net makes a shut-down position recoverable, which is what gives §2.2's escape window somewhere to lead |
| [`weapons.md`](weapons.md) | weapon-as-unit, the release path, the damage model, the three resolution boundaries — one of which is widened (§Gaps collision 2) and the rest consumed unchanged |
| [`sensors.md`](sensors.md) | the perception boundary, the RWR the seeker is, and the `Aircraft` filter §4 deliberately leaves alone |
| [`missions/verdict.md`](missions/verdict.md) | the rule `objective suppress` had to pass, and the roster whose price it pays in one bool |
| [`pilot.md`](pilot.md) | the attack phase that gains one cue and nothing else |
| [`modules/f16/weapons.md`](modules/f16/weapons.md) · [`modules/mig29/weapons.md`](modules/mig29/weapons.md) | the two reference bases every store number in §§2–3 is read out of |
| [`campaigns/w3-desert-storm.md`](campaigns/w3-desert-storm.md) · [`campaigns/w4-allied-force.md`](campaigns/w4-allied-force.md) | the two campaigns that ask for this, and the two that measure it |
