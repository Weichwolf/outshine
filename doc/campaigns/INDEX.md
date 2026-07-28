# Campaigns — the ten scenario specifications

**Status: spec only. Nothing is built.** No `.fbm` file of any campaign exists; no line of `sim/` was
touched to write this directory. It exists because the missions must not be invented: a campaign
without a cited anchor is a mood, and a mood cannot be measured.

This directory is **step 4 of the owner goal** and its specification comes first, per
[`../conventions.md`](../conventions.md)'s spec-first rule: *change the Spec of the topic file first;
if a round cannot say what the contract becomes, it is not ready to start.*

---

## The ten campaigns

Five in which the **F-16** flies, five in which the **MiG-29** flies. Ten missions each.

| # | Campaign | Anchor | The hook |
|---|---|---|---|
| **W1** | [Red Flag / Nellis](w1-red-flag.md) | the USAF aggressor enterprise, 1975– | the training ladder — and the inversion that at Nellis the "MiG-29" is an F-16 pretending, while here it is the real module |
| **W2** | [Osirak 1981](w2-osirak.md) | Operation Opera, 7 June 1981 | reach, not combat: 1,600 km, a 30 m ingress and tanks that ran dry — **the one campaign whose subject FlightBox cannot express at all** |
| **W3** | [Desert Storm, the first nights](w3-desert-storm.md) | 17 January 1991 + Package Q, 19 January | a package against an integrated air defence, and the three named ways Package Q came apart |
| **W4** | [Allied Force 1999](w4-allied-force.md) | 24 March – 10 June 1999 | mountains, cloud and an air defence that refuses to emit — the campaign the weather hook was built for |
| **W5** | [Baltic Air Policing / QRA](w5-baltic-qra.md) | NATO air policing, 30 March 2004– | **identification as the task**, and the sharpest anti-cheat test in the set |
| **O1** | [Bekaa 1982, the Syrian side](o1-bekaa-1982.md) | Operation Mole Cricket 19, 9 June 1982 | the canonical defeat, reframed as a measurable question: **what in the doctrine moves the outcome, and what is left when nothing does** |
| **O2** | [PVO intercept exercise](o2-pvo-intercept.md) | Soviet GCI doctrine + the MiG-29's own guidance panel | ground control in its pure form, and identity that always costs surprise |
| **O3** | [Yom Kippur 1973](o3-yom-kippur-1973.md) | the opening strikes, 6 October 1973 | ground attack under a friendly SAM umbrella — **the only campaign with zero runnable missions**, and the requirement that says why |
| **O4** | [GAF MiG-29G DACT](o4-gaf-mig29g-dact.md) | JG 73 Laage, 1991–2003 | the one campaign in which **both** FlightBox airframes really flew against each other — and the cheapest to build, because half of it is already measured |
| **O5** | [Airfield defence](o5-airfield-defence.md) | Batajnica 24–26 March 1999, with Iraq 1991 as the parallel | a defender whose success is something **not happening** — which the verdict vocabulary has no word for |

---

## How to read a campaign file

Every file carries the tree's four sections, mapped onto a campaign:

| Section | Content here |
|---|---|
| `## Spec` | the anchor in one table · the campaign contract · **the ten missions** · **the cast** · what must be true before mission 1 can fly |
| `## State` | what is built (today: nothing) and which existing FlightBox pieces the campaign would consume unchanged |
| `## Gaps` | **what FlightBox cannot do for this campaign**, with the shared gap IDs — plus one "honest headline" naming the single worst hole |
| `## Knowledge` | the anchor **with its sources**, the disputes left standing, where the sourcing is thin, and the derivations |

### Four rules that hold in all ten files

1. **Every number carries a source and a confidence tier.** Nothing is averaged, smoothed or inferred
   into existence. Where two sources disagree the file says so and carries **both**.
2. **Where the flying jet does not match the historical operation, the file says so in its own
   header** and treats the anchor as a **scenario archetype** — *the situation is the anchor, not the
   serial number.* It also states the **direction** of the substitution: for O1 and O3 the MiG-29
   makes the defender materially **stronger** than history, which means a FlightBox result that still
   goes badly says something stronger than the record did, and one that goes well says nothing.
3. **Ground targets appear in every mission of every campaign**, not only the strike ones — including
   the pure air-to-air rides. A fight with nothing on the ground has no reason to be where it is.
4. **Each mission states exactly one tactical question**, and a run whose telemetry cannot answer that
   question is a badly built mission rather than a result.

### Confidence tiers (identical in all ten files)

| Tag | Meaning |
|---|---|
| **[T1]** | official / government / service document (NATO, USAF, CIA reading room, service histories) |
| **[T2]** | manufacturer or service publication — here chiefly the two DCS manuals via `doc/modules/mig29/` |
| **[T3]** | established literature and specialist press (RAND, Osprey, Air & Space Forces Magazine, Smithsonian) |
| **[T4]** | encyclopaedic / community consensus (Wikipedia, enthusiast press, forums) |
| **[DISPUTED]** | sources conflict; both values carried, neither preferred |
| **[SET]** | a FlightBox setting chosen in this directory, not sourced |
| **[DERIVED]** | computed from a named relation |

**The honest overall picture of the sourcing:** the western campaigns rest largely on [T3]/[T4] with
some [T1] (NATO and AFHSD pages for Allied Force, a Marine Corps study for 1973); the eastern
campaigns are markedly thinner, and O2's doctrine half is the thinnest thing in the directory — the
[T1] material that would fix it (two CIA reading-room documents) is identified and **unread**. That is
the same sourcing asymmetry `doc/modules/mig29/` already declares for the aircraft itself, and it is
declared again rather than hidden.

---

## What is buildable today

**50 of the 100 missions.** Per campaign:

| Campaign | Runnable today | Blocked | The first pair to build |
|---|---:|---:|---|
| O4 GAF DACT | **8** | 2 | `o4-04` / `o4-05` — the entry-range sweep at 10 and 5 nm |
| O1 Bekaa | **7** | 3 | `o1-01` / `o1-02` — the GCI-deletion experiment |
| O2 PVO | **7** | 3 | `o2-06` / `o2-08` — the identification anti-cheat pair |
| O5 Airfield defence | **7** | 3 | `o5-01` / `o5-02` — alert versus CAP, one `spawn` line apart |
| W3 Desert Storm | **5** | 5 | `w3-07` / `w3-08` — the same GCI experiment, another theatre |
| W1 Red Flag | **4** | 6 | `w1-01` … `w1-04`, the ladder |
| W2 Osirak | **4** | 6 | `w2-03` / `w2-04` — combat radius clean and loaded |
| W4 Allied Force | **4** | 6 | `w4-01` / `w4-02` — one `wx` line apart |
| W5 Baltic QRA | **4** | 6 | `w5-02` / `w5-03` — the `team`-swap anti-cheat pair |
| **O3 Yom Kippur** | **0** | 10 | — blocked at the module (`C9`) |

Four of the ten "first pairs" are **one-line experiments**: `o1-01/02`, `w3-07/08`, `w4-01/02`,
`w5-02/03`. That is the pattern this directory was written to produce.

---

## The aggregated cast — the build order for units

Every type named in any campaign's cast table, with the number of campaigns that need it. **This is
the input to the next build step.** Ordered by frequency, then by how much the campaign loses without
it.

| Type / class | Campaigns | Exists | Note |
|---|---:|---|---|
| **F-16C** | 10 | **yes** (`f16`) | flown by W1–W5, opposed by O1–O5 |
| **MiG-29 (9-12)** | 10 | **yes** (`mig29`) | flown by O1–O5, opposed by W1, W3, W4, W5 |
| **`target_soft`** | 10 | **yes** | vehicle parks, radar vans, artillery positions, coastal sites, range pits |
| **`target_hard`** | 10 | **yes** | bunkers, shelters, bridges, a reactor dome, a runway |
| **Ground radar / EW / GCI site as an EMITTER** | 7 | **no — specified** | today `target_soft` that does not radiate; needed by W2, W3, W4, W5, O1, O2, O5. Row `p18` in [`../modules/ground/catalogue.md`](../modules/ground/catalogue.md). **As a row it only radiates and dies; what makes it worth attacking is `C22`** ([`../air-defence-network.md`](../air-defence-network.md)) |
| **AAA / short-range and man-portable air defence** | 6 | **no — specified** | the reason for every altitude decision in W2, W3, W4, O3, O5, W1. Rows `zsu23` `zu23` `sa7` `sa18` |
| **SAM battery, fixed (SA-2 / SA-3 class)** | 5 | **no — specified** | W3, W4, O1, O3, O5. Rows `sa2` `sa3` |
| **SAM battery, mobile (SA-6 / SA-8 class)** | 5 | **no — specified** | W3, W4, O1, O3 — and in O3 it is **ours**, which the design must allow (it does: a site declares its own `team` like any unit). Rows `sa6` `sa8`; mobility is expressed in TIME (`set scoot_s`), not in space — `C14` stays open |
| **AEW aircraft (E-3 / E-2C class)** | 4 | **no** | W1, W3, W4, O1 |
| **F-15 class** | 4 | **no** | escort in W2/W3, opposition in O1/O5 — the aircraft that historically shot down every MiG in O5's two anchors |
| **Period Soviet types (MiG-21 / MiG-23 / MiG-25 / MiG-17 / Su-7 / Su-20)** | 3 | **no** | W3, O1, O3 — one module family would serve all three |
| **Tanker (KC-135 class) + a boom** | 3 | **no** | W1, W3, W4 |
| **Cruise missile / one-way vehicle** | 3 | **no** | W3, O5, O2 — already on the roadmap as R7 |
| **Runway / airfield as a STATEFUL, closable object** | 3 | **no** | W1, W3, O5 |
| **Large subject aircraft (bomber, ELINT, transport)** | 2 | **no** | W5, O2 — the actual subject of a real intercept, and a completely different intercept geometry from a fighter |
| **Anti-radiation shooter (F-4G / F-16CJ with HARM)** | 2 | **module yes, weapon no** | W3, W4 |
| **Jammer aircraft (EF-111 / Boeing 707 class)** | 2 | **no — but no longer needs an airframe** | W3, O1 — in O1 it is the **decisive** mechanism of the whole battle. `C24` makes comms jamming a published **scalar on any unit** (`set jam_comm_m`), so an F-16 stands in for the 707 without `C7`; the radar-jamming half still has no representation |
| **Moving ground column** | 2 | **no** | W4, O3 |
| **Ships** | 2 | **no** | W5, W3 |
| **Helicopter (Mi-8 / AH-64 class)** | 2 | **no** | W3, O3 |
| **Radar decoy (ground, emitting, not lethal)** | 1 | **no — specified** | W4 — named by the anchor as a decisive Serbian measure. Costs one catalogue row: the `p18` row with `rounds 0` and a small range gate ([`../modules/ground/cast.md`](../modules/ground/cast.md)) |
| **RPV / expendable decoy air vehicle** | 1 | **no** | O1 — the operation's opening move |

**Reading of that table.** After the two flyable jets and the two ground-target kinds — all of which
exist — the next four rows are the same thing four times: **something on the ground that emits and
shoots.** Nothing else in the list appears in more than five campaigns. The unit build order is
therefore not a list of aircraft; it is one system.

---

## The aggregated capability gaps — the build order for the engine

The shared catalogue used by all ten files. Ordered by **blocking degree**: first how many campaigns
it *blocks* (a mission cannot run or cannot be read), then how many it *degrades*.

**Home files.** Five of these got one in the foundation round (2026-07-28), and four of the five are now
CLOSED: `C2`, `C12`, `C3` and `C0` were specified and then built, each against its own file's Spec.
`C1` got its own round the same day and is now **SPECIFIED, not built**: the contract, the nine sourced
catalogue rows and the rest of the cast live in [`../modules/ground/`](../modules/ground/INDEX.md). It
stays open in this table until it is measured against its own ten acceptance criteria.

**The net above the positions.** `C1` specifies **one** position and is deliberately silent about what
happens between positions — its own catalogue says a search radar "cues nobody … a target and a warning,
not a network". That layer is [`../air-defence-network.md`](../air-defence-network.md), specified
2026-07-28: the cue that aims a fire unit's antenna, the declared and judged belt, the fire-control
authority and what remains when the net is taken away, plus the bounded jamming model that makes the
last question measurable. It books `C22`/`C23`/`C24` below and splits `C13`.

| ID | Gap | Blocks | Degrades | Verdict |
|---|---|---:|---:|---|
| `C1` | **No active surface-to-air threat.** Ground units are inert: nothing emits, nothing launches, nothing shoots. `target_soft`/`target_hard` have only a structure state. **SPECIFIED 2026-07-28, NOT BUILT — home: [`../modules/ground/`](../modules/ground/INDEX.md)** (contract, nine sourced catalogue rows, and the rest of the cast at four quantities per type); the entry in [`../weapons.md`](../weapons.md) is now a pointer plus the two collisions the spec found in that file's own contracts. **The layer above it is `C22`** ([`../air-defence-network.md`](../air-defence-network.md)) | **6** | 3 | **the single most blocking gap in the set.** Six campaigns lose named missions; every campaign loses the *reason* for its altitude and route decisions |
| ~~`C12`~~ | **CLOSED 2026-07-28.** The vocabulary is eight kinds: `identify`, `protect`, `no_fire` and `deny release` are built beside the original four — [`../missions/verdict.md`](../missions/verdict.md), grammar in [`../missions/syntax.md`](../missions/syntax.md). What stays refused (a general `deny`, `escort`, time windows, target value) is listed there with a reason each | — | — | W5/O2 can now declare the identification pass and the weapons hold, O5 the denial. O5's timing half remains a telemetry read, as its own spec says |
| `C7` | **Only two flyable modules.** Every other aircraft in every cast list is absent | **1** (O3's period force) | 9 | blocks nothing outright because substitutions exist — but every substitution changes the answer, and each is declared in its mission header |
| ~~`C2`~~ | **CLOSED 2026-07-28.** `time <ISO8601 Z>` is mission data, the clock binds all three clients, `FBEphemeris` sits in `core/` and `fb-gym` publishes `FBEnvironmentBlock` — [`../missions/syntax.md`](../missions/syntax.md), [`../clients/clients.md`](../clients/clients.md) | — | — | a night mission can now say so |
| `C6` | **No live controller.** GCI is `set brief_gci`, static text fixed before the run: nothing re-vectors, nothing goes silent mid-intercept, nothing is wrong *halfway through*. **Its GROUND half is specified in [`../air-defence-network.md`](../air-defence-network.md)** — a node that can be killed, jammed or fall out of range mid-run; the AIRBORNE half (a jet subscribing to a controller) is untouched and is that file's §2 design B | **1** (O2's subject) | 6 | the difference between "blind" and **"confidently blind"** — see [`o1-bekaa-1982.md`](o1-bekaa-1982.md) §Knowledge 4, the cheapest addition that would raise O1 from a stand-in to the real experiment |
| `C9` | **The MiG-29 module cannot fly `set task attack`** — no CCIP/CCRP block, because the real aircraft's unguided delivery is a *director*, not a release cue | **1** (all of O3) | 2 | **the only gap that zeroes an entire campaign** |
| `C5` | **No aerial refuelling, no external fuel tank in the store catalogue** | **1** (W2's subject) | 3 | W2's defining constraint is exactly the thing that cannot be expressed |
| ~~`C3`~~ | **CLOSED 2026-07-28.** `sensors/FBVisualSystem` is built and is the SIXTH registry reader, declared in advance and paid for in five currencies — [`../sensors.md`](../sensors.md) §9, mission switches in [`../missions/sensors.md`](../missions/sensors.md). Recognition is the resolution test it was specified to be: measured, a beam-on F-16 is detected at 3 784 m, recognised at ~950 m and identified at ~590 m, and the name it gains is the module registry key. **`w5-03`/`o2-08` survive by measurement, not by argument**: two runs differing only in the target's `team` produce byte-identical telemetry | — | — | what is NOT closed: nothing consumes the block yet (deliberate, its own round in [`../pilot.md`](../pilot.md)), and the channel contributes nothing at night because nothing in the tree emits light — so W5's and O5's night merges are measured to be eyeless |
| `C8` | **Store catalogue is Mk-82 / AIM-120 / AIM-9 / R-73 / R-27R.** No HARM, no LGB, no Mk-84, no cluster, no rocket pod, no FAB-class bomb | **2** (W3/W4 SEAD, O3 stores) | 2 | there is no such thing as a suppression element in the tree |
| `C15` | **No package coordination** — no time-on-target, no deconfliction, no lead tasking; formation is combat spread only, no rejoin | 0 | **7** | the *definition* of a package, and every large-force mission is affected |
| ~~`C0`~~ | **CLOSED 2026-07-28.** The `.fbc` file, the three carried facts and the aggregating runner are built — [`../missions/campaign.md`](../missions/campaign.md), driven by `fb-gym --campaign`. Both determinism criteria measured on both ground bases: 9 runs one campaign fingerprint, and all 4 steps of `sim/campaigns/viper-attrition.fbc` reproduce standalone from their state file plus the ground the run recorded | — | — | a sequence of missions is now a campaign. What is NOT closed: the ten campaigns still have no `.fbm` files, damage and fuel stay refused with a reason, and there is no campaign-scope objective |
| `C4` | **No terrain masking.** The hook (`const FBWorld*`) reaches every sensor slot; the computation does not exist | 0 | 5 | already named as next in [`../roadmap.md`](../roadmap.md) R6. W4 and O1 are its acceptance tests |
| `C14` | **No moving ground units and no ships** | 0 | 4 | W4's armour hunt, O3's column, W5's Baltic |
| `C18` | **No radio between units.** Only the datalink PPLI and typed GCI entries. The **ground-to-ground** half is specified as `C22`; what stays open is the VOICE net — call volume, a channel that saturates | 0 | 4 | Package Q's third failure mode was a radio net collapsing under 80 % of the calls; there is no such net to collapse |
| `C22` | **No connected air defence.** No cue from an early-warning set to a fire unit, no sector responsibility, no fire-control authority, no control node — and therefore no experiment in which a defence loses its net. **SPECIFIED 2026-07-28, NOT BUILT — home: [`../air-defence-network.md`](../air-defence-network.md)** | 0 **today** (`C1` blocks first); **5 behind it** | 5 | *Is killing an early-warning radar worth a sortie?* Today the answer is structurally **no** — a `p18` beside an `sa3` does not make the `sa3` smarter — and that is a model artefact, not a finding. It is the reason `w3-01` (the opening move of the whole war) currently measures nothing |
| `C23` | **No declared, judged threat geometry.** A mission cannot say what its belt is, and a verdict cannot say whether the flight went through it or round it. **SPECIFIED — home: [`../air-defence-network.md`](../air-defence-network.md)** §4 (`zone` lines, per-unit dwell, one new objective kind `avoid zone`) | 0 | 5 | the 15,000 ft floor (W4), the low ingress (W2/O3) and "under the belt" (O1) are one decision seen from four campaigns, and today a run can report a kill and a loss but never **where in the layer cake** either happened |
| `C24` | **No communications jamming** — the bounded subset of `C13` the O1 anchor actually names. **SPECIFIED — home: [`../air-defence-network.md`](../air-defence-network.md)** §6: one declared radius, receiver-side, other teams only, no sensor touched | **1** (O1's mechanism, jointly with `C13`) | 1 | it turns `o1-02` from a stand-in into the experiment: the present mechanism deletes the controller **at spawn**, the anchor removed him **mid-intercept**, and [`o1-bekaa-1982.md`](o1-bekaa-1982.md) §Knowledge 4 already tabulates the two as different failure modes |
| `C10` | **No dive or pop-up delivery.** The attack phase is a level laydown, by design and for a stated reason | **1** (W2's profile) | 3 | |
| `C17` | **One runway per mission; no divert field; a runway cannot be cratered or closed and an airfield has no state** | **1** (O5-07) | 1 | |
| `C13` | **No jamming of any kind** — neither self-protection nor communications jamming. **SPLIT 2026-07-28:** the communications half is `C24`; what stays wholly open here is the RADAR half — noise, deception, range-gate pull-off, angle-of-jam, burn-through, home-on-jam | **1** (O1's mechanism) | 1 | the decisive Israeli move at Bekaa is the one thing in the set with no substitute at all |
| `C16` | **Cloud affects only the IRST** — not radar, not weapon delivery, not a visual pickup; the cloud rebuild is unbuilt | **1** (W4-08) | 1 | roadmap R5 |
| `C19` | **No rules-of-engagement state** — nothing expresses weapons hold / tight / free. The vocabulary is now defined and gated **for ground units** in [`../air-defence-network.md`](../air-defence-network.md) §5; aircraft still have none | **2** (W5, O2) | 0 | |
| `C20` | **No terrain-following guidance.** `Direct` holds an ASL altitude, so a 30 m AGL ingress over varying terrain is not flyable | **1** (W2-02) | 1 | |
| `C11` | **No strafing** — gun bundles are not resolved against ground targets | 0 | 2 | |
| `C21` | **No declarable initial damage** — a jet can be switched off, not broken | 0 | 2 | |

### The foundation round (2026-07-28) — what it decided

Step 1 of the owner goal was a **spec round, no code**: the contracts for `C2`, `C3`, `C12` and `C0`
were written into the files that own them, and `C1` was given a home with a boundary. The decisions
worth knowing before reading them:

| Contract | The decision |
|---|---|
| `C2` | **Zulu only, and the default is *no clock at all*.** The absent-value case touches no channel, which is what makes the 84 existing missions byte-identical by construction rather than by hope. A client flag that contradicts a `time` line is a **boot error**, not a precedence |
| `C3` | **Recognition is a resolution test, not a lookup.** One quantity (presented extent over range) with Johnson N50 multiples for detect/recognise/identify; the type name is the module registry key, so two MiG-29s on opposite teams produce the identical string, and the anti-cheat pair survives. **Built and measured the same day; the two places the contract was wrong are corrected in [`../sensors.md`](../sensors.md) §9 rather than quietly satisfied** |
| `C12` | four kinds — `identify`, `protect`, `no_fire`, `deny release` — costing **one monotone bit and one float** on the roster. `identify` measures the **geometry**, not a sensor event, because the judge measures what the aircraft DID and never what it knew. A general `deny` and `escort` are refused, each with a reason |
| `C0` | a campaign carries **three** facts (units, ground targets, stores) and refuses the rest by a three-part test. The overlay may delete a line, never add one. Determinism is proven twice: one fingerprint over 9 runs, and each step re-runnable **standalone**. **Built the same day; the overlay ended up narrower than the contract allowed — it only ever deletes** |
| `C1` | **one data-driven class, nine catalogue rows** — not nine classes and not a module. It sees through **derivations** of bases that already read the registry, so the gate stays at six files; it **uses** `FBSystemHealth` unchanged rather than inheriting anything; it asks for exactly **one** core change (two emitter beams per unit, because a battery is two antennas) and **one** new enum value on each of `FBSeekerKind` (command guidance) and `FBEmitterKind` (surface search / surface fire control). `emcon hold` is a **passive receiver**, never a timer or a range trigger |

### The four things to build first, and why in this order

1. **`C1` — an emitting, shooting ground unit. SPECIFIED 2026-07-28 in
   [`../modules/ground/`](../modules/ground/INDEX.md), not built.** It blocks six campaigns, it is the top
   four rows of the cast table collapsed into one system, and it is the only gap whose absence removes the
   *reason* for tactical decisions rather than the ability to make them. Without it W2, W3, W4, O1, O3 and
   O5 are air-to-air campaigns wearing strike names. **The spec's own honest headline:** a site can be
   *heard* and not *seen* (no air-to-ground radar mode, no HARM — `C8`), so `C1` gives the ground the
   ability to shoot back long before it gives the air the ability to shoot first. That is the right order
   to build in and it is not a SEAD capability.
2. ~~**`C12` — three more objective kinds.**~~ **BUILT 2026-07-28** (four kinds). Small, self-contained,
   and it makes five campaigns *readable*. `deny`/`protect` is checked against the roster the `kill`
   objectives already use; `identify` needed a definition, and W5/O2 supplied one.
3. **`C2` — a mission clock.** One line in the format. It mislabels more artefacts than anything else
   in the list, and the renderer already has the ephemeris to consume it.
4. **`C6` — a controller that can change or vanish during a run.** It converts O1's stand-in into the
   real experiment, gives O2 its subject, and is the shared mechanism behind the one experiment three
   separate campaigns want to run (below).

---

## The identification task (W5 and O2)

Two campaigns have a task with **no weapon in the success condition**: put eyes on an unknown
aircraft and say what it is. They are here because identification is the sharpest available test of
the anti-cheat boundary — and, unusually, a test that costs almost nothing to run.

### Why it is sharp

An AI that cheats about *shooting* is hard to catch: a fast, accurate kill can be good flying. An AI
that cheats about *identity* is caught in one diff, because the correct behaviour toward an unknown
contact is **the same regardless of what it turns out to be**, right up to the moment a sensor
discriminates. Any earlier divergence has exactly one possible source.

The tree's own restrictions make the test possible, and every one of them helps:

| Restriction | Where enforced | Consequence for the test |
|---|---|---|
| A radar contact carries range, bearing, azimuth, elevation, closure and a radar-local track number — **no unit id, no callsign, no team** | `core/FBRadarContact` | there is no legitimate path from a contact to an identity |
| IFF Mode 4 is **two-valued**: friendly, or no reply. There is no "hostile" value | `core/FBIffReply` | **silence is ambiguity**, and ambiguity is the whole subject |
| The unit registry reaches exactly six files in `systems/`+`modules/`, all of them sensors | grep-checked and pinned in `tools/verify_layers.py`, [`../sensors.md`](../sensors.md) §1.2 | the pilot has no path to the truth even if it wanted one |
| The cooperative datalink is faction-filtered and carries a **point**, never an identity | [`../formation.md`](../formation.md) §Knowledge 2 | a mate cannot leak an identity either |
| Runs are deterministic to one fingerprint over `--threads 1/2/4` × 3 repeats | [`../missions/runtime.md`](../missions/runtime.md) | a behavioural comparison is a **byte comparison**, not a judgement |

### The test, stated as an acceptance criterion

> Build each identification mission **twice**, identical in every line except the subject's `team`
> (`neutral` in one, `hostile` in the other). Require the interceptor's own telemetry to be
> **byte-identical up to the first tick at which one of its own sensors discriminates** between the
> two cases. A divergence before that tick names the tick and the column, and it is a defect of the
> same class the include graph already forbids structurally.

`w5-03` and `o2-08` are that experiment, on the two airframes. They need **nothing that does not
exist**, which is why both campaigns nominate them as the first pair to build.

### What mission data must carry, and what it must not

| Must be declarable | Must NOT be declarable |
|---|---|
| the subject's `team` (already: `friendly` / `hostile` / `neutral`) | anything the pilot can read that names the subject |
| the subject's transponder state (`set iff_xpdr on\|off`) and its emission state | a "this one is the bad guy" hint of any kind, including a callsign convention |
| the interceptor's interrogator state (`set iff_interrogator on\|off`) | |
| a **geometry** the interceptor must reach and hold (the abeam box) — `[SET]`, declared per mission | |
| a rules-of-engagement state (needs `C19`) | |

### What the victory condition became (`C12`, built)

| Objective | Fulfilled when | Violated when |
|---|---|---|
| `identify unit <callsign> range <m> hold <s>` | this unit held the declared geometry for the declared cumulative duration | — |
| `no_fire` | no release and no burst by this unit for the whole run | any release or burst → immediate FAIL |

The sensor half was **deliberately dropped from the verdict**: producing "a discriminating sensor event
on unit N" means correlating an anonymous contact back to a unit, and that function's existence is the
identity leak these two campaigns exist to test. The IFF reply is therefore read out of `events.log`
(`radar IFF_REPLY … reply=none`) beside the verdict rather than inside it — the full argument, and the
price ("a pilot that flies the box with its eyes shut still scores"), is in
[`../missions/verdict.md`](../missions/verdict.md). `sim/missions/qra-identify.fbm` is the shape both
campaigns can now be written against; `qra-weapons-hold.fbm` is the same intercept with the hold broken.

### The eastern version is the harder one

On the MiG-29, identity has exactly **one** source and using it radiates
([`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md) §3): the IFF interrogator
works through the radar, the IRST has no IFF at all, and the SPO-15 warns of every radar including
friendly ones. So on the eastern side an AI that "just knows" is visible not only in its shooting but
**in its silence** — a MiG that never interrogates and is nonetheless always right is caught by the
same diff.

---

## Bekaa as the yardstick (O1)

The Bekaa campaign is the one that must not ask "does the MiG win". It asks two quantities, and the
machinery to compute both already exists and has been run once at single-ship scale.

**(a) The doctrine band.** With `O(v)` the outcome score of a Red doctrine vector `v` over a fixed
geometry set — the same score `fb_tournament.py` already computes, outcome dominating and craft only
ordering within equal outcomes:

```
band = max_v O(v) − min_v O(v)
```

The levers of `v` are **mission text only**: GCI present/absent, emission policy, commit range,
launch doctrine, altitude band, formation contract, reaction time. A lever that needs a new class is
not a doctrine, it is a rebuild. Precedent: the same measurement on a single MiG over one geometry
already produced a band of **978.7 points** and turned six losses into none
([`../duels.md`](../duels.md)).

**(b) The residue.** With both sides held at their best measured doctrine,

```
residue = O_blue(v*_blue) − O_red(v*_red)
```

The residue is what **no decision on either side removes** — the weapon obligation, the sensor reach,
the cross-section ratio, the four asymmetries already tabulated in [`../duels.md`](../duels.md)
§Knowledge 1. A small residue says the defeat was doctrinal and architectural; a large one says the
force was outmatched in materiel before anybody decided anything. **Both answers are publishable.
Refusing to compute the number is not.**

Three conditions on it, all non-negotiable:

1. It is a statement about **FlightBox's models**, never about 1982. The scale is staggered by design
   ([`../vision.md`](../vision.md)) and the flying type did not exist yet.
2. A loss caused by a **pilot-AI defect** is not part of the residue. The duel campaign found three
   such defects by measurement and fixed each where it belonged; a residue reported before that
   filtering is a bug report wearing a result's clothes.
3. The substitution direction is stated with the number. FlightBox's MiG-29 has a better warning
   receiver, a look-down radar and an IRST that the 1982 Syrian force did not — so a bad FlightBox
   result is a **stronger** statement than the record, and a good one is not a rehabilitation of
   anybody.

### The one experiment three campaigns share

`o1-02`, `w3-08` and `o5-04` are the **same single deleted line** (`set brief_gci`) in three
geometries whose anchors independently name ground control as the missing ingredient. FlightBox's
mechanism makes it unusually clean: without the brief the N019 never receives its scan elevation or
its ZONE third, so the aircraft is not "less informed" — it is pointing its radar at nothing in
particular, and the measured quantity is a **detection time**, comparable across all three campaigns
with no normalisation.

**If the three disagree, the answer is a property of the geometry rather than of the doctrine, and
that is itself the finding.**

---

## Related

| Place | Relationship |
|---|---|
| [`../vision.md`](../vision.md) | the staggered scale and why anti-cheat is a game decision — the ground both cross-cutting sections above stand on |
| [`../duels.md`](../duels.md) | the 1v1 measurement campaign; O4 is largely its re-framing, and O1's yardstick reuses its tournament |
| [`../formation.md`](../formation.md) | the flight as a fighting unit; every campaign with more than two aircraft a side depends on it |
| [`../missions/`](../missions/INDEX.md) | everything a `.fbm` can declare — the source of every "can" and "cannot" in the gap tables |
| [`../modules/f16/module.md`](../modules/f16/module.md), [`../modules/mig29/module.md`](../modules/mig29/module.md) | what the two flyable jets can actually do |
| [`../roadmap.md`](../roadmap.md) | R7 (one-way vehicles), R9 (missions for humans — the nearest existing home for `C0`) |
| [`PROGRESS.md`](PROGRESS.md) | the source-coverage ledger: which anchors are researched, which sources were identified but not read |
