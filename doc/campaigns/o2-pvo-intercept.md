# O2 — PVO intercept exercise (ground control in its pure form)

**What this file is:** a **campaign spec** — ten missions derived from one doctrinal anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet. It is the **eastern half
of the identification argument**; the western half is [`w5-baltic-qra.md`](w5-baltic-qra.md), and the
argument is stated once in [`INDEX.md`](INDEX.md) §"The identification task".

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Soviet PVO / ground-controlled interception practice, and the MiG-29's own documented GCI hardware | §Knowledge 1, cited and tiered |
| **FlightBox sources** | the MiG-29 module's GCI chain, its sensors, and the mission format | [`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md), [`../modules/mig29/radar-sensors.md`](../modules/mig29/radar-sensors.md), [`../modules/mig29/module.md`](../modules/mig29/module.md), [`../missions/sensors.md`](../missions/sensors.md), [`../sensors.md`](../sensors.md) |

Confidence legend and gap IDs `C0…C24`: [`INDEX.md`](INDEX.md).

**Temporal honesty:** this campaign has **no single historical event as its anchor** — it is an
*exercise*, and its anchor is a **doctrine plus a piece of hardware**. That is stated openly rather
than dressed as a battle: the doctrine is the Soviet/PVO ground-controlled intercept model, and the
hardware is the MiG-29's own guidance panel, the one the DCS module labels *"automatic guidance by a
datalink, voice guidance from the command post — currently not implemented"*. The campaign's ten
missions are therefore **built from the documented loop**, not reconstructed from a sortie log.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| The doctrine | PVO interceptors were vectored by ground radar; pilots functioned as "extensions of the ground control network", flying to designated intercept points and following controller instructions, which minimised independent decision-making | [T3]/[T4] |
| The datalink family | **Lazur** — a two-way VHF data link for GCI; transmits guidance commands, allowing engagement **without the pilot switching his own radar on** | [T4] |
| What Lazur-S carries | course, speed and altitude indications, plus discrete messages such as *"afterburner on"* and *"radar on"* | [T4] |
| The later automation | Su-15TM with **Vozdukh-1M** + **SAU-58** could fly a fully automatic, hands-off intercept until the final moments | [T4] |
| On the MiG-29 specifically | a **Command Guidance mode switch**, PU-S31 panel position 5: *"automatic guidance by a datalink, voice guidance from the command post. Currently not implemented."* | [T2] (DCS-EA p.59, via [`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md)) |
| The MiG-29's **documented substitute** loop | voice **BRAA** → pilot types **expected range (km)** and **expected relative altitude (km)** into the radar → the scan elevation is aimed; the **ZONE** switch sets the azimuth third | [T2] (DCS-FM pp. 43–44, 83–84) |
| Range unit | **kilometres if the controller is Russian**, miles if Western | [T2] (DCS-FM pp. 99–100) |
| The controller's vocabulary | BOGEY DOPE (nearest bandit, BRAA + aspect) · PICTURE (all) · *"clean"* · *"merged"* inside 5 nm · vector to home plate / tanker | [T2] (DCS-FM pp. 99–100) |
| Identity, and the trap in it | **IFF gates the lock permission** (the PU-S31 LOCK FOE/FRIEND switch), the **IFF interrogator does not work with the IRST**, and the **RWR warns of every radar, friendly included** | [T2] (DCS-EA p.59, DCS-FM pp. 86, 30–31) |
| ~~Declassified primary material exists~~ | ~~CIA reading-room holdings include…~~ **SUPERSEDED 2026-07-29: both documents were retrieved and READ.** Their content is the block below and §Knowledge 1 | [T1] |

**The [T1] rows, added 2026-07-29 when the two CIA reading-room documents were finally read.** They
are the reason this campaign's doctrine half is no longer the thinnest thing in the directory, and
every one of them is used by a mission or a header rather than quoted for decoration.

| Fact | Value | Tier |
|---|---|---|
| Cooperation has exactly **two** forms | **separate zones** (simple, safe, and it "precludes the possibility of concentrating the efforts of fighter aviation and surface-to-air missile units against grouped air targets") or the **same zone** (concentrating, and dangerous) | [T1] Mitronin |
| Same-zone cooperation is allocated in **five currencies** | altitude · axis (area, sector) · control line · time · target. "The upper flight echelons are preferably assigned to the fighter aircraft" | [T1] Mitronin |
| **The fratricide problem is the document's own central problem** | same-zone work "demands … the availability of reliable means of control **and identification**; otherwise, the firing capability of the surface-to-air missile units might be restricted due to the danger of hitting our own aircraft" | [T1] Mitronin |
| A missile/AAA unit **without** an identification system is a documented case, not a simplification | fighter action inside the firing zones of regimental air defence is "inadvisable, since these air defense means **frequently do not have identification systems** and, as a result, cannot fully guarantee the safety of the flights of our aircraft" | [T1] Mitronin |
| The altitude answer to it | assign fighters a band **above the upper limit of the SAM kill zone**; and where the air-defence troops cannot engage at all (relocating, out of missiles, equipment defective) **lower the fighters' unrestricted band in those zones to 3 000 m** | [T1] Mitronin |
| **Corridors** | established to guarantee the safety of one's own flights; several of them; **"the enemy may discover and exploit the corridors to his own advantage"**; must be "reliably covered by air defense units and subunits **with identification means** and well-organized control" | [T1] Mitronin |
| The control organs | a fighter-aviation combat control centre (**TsBU IA**) at the *front*'s air-defence command post; the air army's **TsBU VA** = 2–3 **guidance and target-designation posts (PNT)** + a combat control group, the PNTs **colocated with the SAM units' command posts** and able to guide fighter crews onto enemy aircraft | [T1] Mitronin |
| The one number in either document, and it is about **latency** | at exercise **DRUZHBA-76** it took **10 to 15 minutes** for a division's air-defence chief to pass own-aircraft data down to the antiaircraft gunners, "because they were transmitted according to a code table" | [T1] Mitronin |
| Alert states are named | sorties are flown from **"airfield alert"** and **"airborne alert"** | [T1] Pstygo et al. |
| Ground-control radar is a **contended** resource | fighter-bomber command posts "cannot guarantee the guidance of their own aircraft to enemy ground targets … because the radars will, as a rule, **be operated to control the fighters carrying on combat with the air enemy**" | [T1] Pstygo et al. |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| **The subject is the loop, not the shot** | every mission reports the time from controller call to radiating radar, and the time from radiating to first firm contact. A mission that only reports a kill has measured nothing about GCI |
| Each entry is **charged** | the GCI brief goes over the command bus in its documented three entries with their latency classes; a rejected entry is retried, and the retry is part of the measurement ([`../duels.md`](../duels.md) defect **M1** exists precisely because it was not) |
| The entries can be **wrong** | at least three missions declare a brief that does not match the real geometry. The pilot must fly the brief it was given, discover the error through its own sensors, and recover |
| **Identity is the second subject** | on this aircraft identity has exactly one source, and using it *radiates*. Missions 6–9 are built around that trade |
| **The anti-cheat requirement of `w5-03` applies here too** | any mission with a non-hostile contact is run twice, `team neutral` vs `team hostile`, and the interceptor's telemetry must be byte-identical up to the first sensor discriminator |
| **Ground targets in every mission** | the interceptor is defending something: an early-warning site, a command post, an airfield. They are the reason the vector exists |
| The verdict is machine-read where it can be | `kill unit` where a shot is authorised, telemetry-read otherwise (`C12`) |

### 3. The ten missions

Red/ours = MiG-29. Blue = the intruder.

| # | Mission | Task | Time | Wx | Ours | Intruder | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `o2-01-vector` | textbook GCI intercept | day | calm | 1 MiG-29, `brief_gci` correct, `n019_emission off` at spawn | 1 F-16, straight and level | 1 `target_soft` (EW site being defended) | contact acquired within a declared time of the call | **The loop, timed.** How many seconds from the controller's call to a firm track — and how much of that is the three typed entries? (`mig29-intercept.fbm` already measured 8.0 s call→radiating) |
| 2 | `o2-02-wrong-altitude` | the brief has the wrong altitude band | day | calm | 1 MiG-29, brief 5 km high | 1 F-16 low | as above | contact acquired at all | The N019's elevation bar is **±6°** ([`../duels.md`](../duels.md) §Knowledge 1). A wrong altitude entry does not degrade the search — it deletes it. How long before the pilot notices and re-enters? |
| 3 | `o2-03-wrong-zone` | the brief has the wrong azimuth third | day | calm | 1 MiG-29, `n019_zone` wrong | 1 F-16 | as above | as above | The ZONE switch is **discrete, not a slew** — a continuous value is clamped and acknowledged as such. Does the pilot re-enter, or search inside the wrong third forever? |
| 4 | `o2-04-silent-run` | intercept without ever radiating | day | calm | 1 MiG-29, `n019_emission off` throughout, `kols_mode ir` | 1 F-16 | as above | close to a declared range without emitting | The doctrine's own promise: Lazur exists so the fighter need not switch on its radar. **Blocked in part by `D3`** ([`../duels.md`](../duels.md)) — the pilot does not consume the IRST block, so "silent" is presently "blind" |
| 5 | `o2-05-late-radar` | radiate only inside a declared range | day | calm | 1 MiG-29, ILLUM commanded late | 1 F-16 with RWR on | as above | contact + the intruder's RWR must stay quiet until the declared range | The whole point of a late emission is measured **on the other jet's telemetry**. Cross-unit measurement is the campaign's method here, not its problem |
| 6 | `o2-06-identify` | identify a non-cooperating contact | day | calm | 1 MiG-29, `iff_interrogator on` | 1 F-16, `team neutral`, `iff_xpdr off` | 1 `target_soft` + 1 `target_hard` | identification attempted, **no weapon event** | **The identity trap.** On this aircraft interrogating requires the radar, and the radar warns the target. Identity therefore *costs* surprise — the exact inverse of the F-16's datalink picture |
| 7 | `o2-07-identify-friendly` | the same contact answers | day | calm | 1 MiG-29 | 1 MiG-29, `team friendly`, `iff_xpdr on` | as above | identified friendly, no weapon event | The two-valued reply's easy half. Baseline for mission 8 |
| 8 | `o2-08-team-swap` | mission 6 with the intruder's `team` changed and nothing else | day | calm | 1 MiG-29 | 1 F-16, **`team hostile`**, `iff_xpdr off` | as above | **telemetry must match mission 6** up to the first sensor discriminator | **The anti-cheat test on the eastern side.** Harder than `w5-03`, because this pilot has an IRST that produces contacts with **no IFF at all** — a channel that can see but structurally cannot identify |
| 9 | `o2-09-two-contacts` | one friendly and one not, close together | day | calm | 2 MiG-29 (flight), no cooperative terminal | 1 friendly + 1 neutral, 5 km apart | as above | correct discrimination, no weapon event on the friendly | The MiG has **no cooperative track list** ([`../formation.md`](../formation.md) F3): its flight can only apply a briefed contract. With two contacts and one interrogator, what does the pair do? |
| 10 | `o2-10-pvo-exercise` | the full exercise | **night** | `wx fixture` | 4 MiG-29 (two flights), staggered GCI briefs | 1 "bomber" + 2 escorts (F-16 stand-ins), one squawking | 2 `target_soft` + 1 `target_hard` (the defended complex) | correct identification of all three + intercept of the non-cooperating one | Four interceptors on four separate ground-controlled vectors against a formation containing one friendly. **Does the ground picture and the aircraft picture ever agree** — and when they disagree, which one does the pilot fly? |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| MiG-29 | flyable module | **yes** | ours; the documented GCI entry loop is already implemented as three latency-charged bus entries |
| Ground control post (a controller that **reacts**) | infrastructure / unit | **no** (`C6`) | today `set brief_gci` is static text fixed before the run. A controller that updates a vector when the target turns does not exist |
| Early-warning radar | ground, emitting | **`target_soft` only** (`C1`) | the thing that generates the vector |
| Intruder: strategic bomber (Tu-95 / B-52 class) | flyable module | **no** (`C7`) | the classic PVO subject: large, slow, high, unmanoeuvring — a completely different intercept from a fighter |
| Intruder: ELINT/recon (RC-135 / Il-20 class) | flyable module | **no** (`C7`) | the second classic subject |
| Escort fighter | flyable module | **yes** (F-16) | |
| `target_soft` / `target_hard` | ground | **yes** | the defended complex |
| Cruise missile | air, one-way | **no** (roadmap R7) | the late-Soviet PVO's actual worry |

### 5. What must be true before mission 1 can fly

`o2-01`, `o2-02`, `o2-03`, `o2-05`, `o2-06`, `o2-07`, `o2-08` are buildable **today** — the GCI chain,
the ZONE switch, the antenna elevation entry, the emission switch and the interrogator are all real.
**Missions 6 and 8 are the pair to build first**, for the same reason `w5-03` is: they cost nothing
and they test the boundary.

> **SUPERSEDED 2026-07-29 by the build. Ten of ten were built and ten ran, and the ten slots hold
> something slightly different from the spec's list:** one baseline + four single-lever variants + one
> controlled pair + **that pair's own control run** + a two-step chain. Three corrections the build
> forced, each named in the `.fbc` header rather than discovered missing:
>
> 1. **Mission 4 (the silent run) and the no-controller experiment are the SAME FILE**, and that is a
>    finding rather than a saving. The spec treats them as separate because it assumed the radar would
>    be on; it is not — `n019_emission` powers up `off` by doctrine and the GCI's third entry is the
>    only thing that turns it on. Deleting the brief therefore IS the silent run.
> 2. **Mission 9 (two contacts, one interrogator) is not a file of its own.** Its geometry is what
>    sorties 09 and 10 fly: a raid with one aircraft of ours inside it, transponder on. Building it
>    twice would have cost the chain a step.
> 3. **The pair to build first was three files, not two.** `o2-06`/`o2-08` come out byte-identical, and
>    a byte-identical pair has two possible causes; `o2-07` — the same contact, answering — is the
>    control that separates them. `w5-03` should budget the same third slot.
>
> And one sentence of §3 did not survive contact: mission 3's question is only ASKABLE on an off-nose
> geometry. The three ZONE sectors overlap (left −60…0, centre −30…+30, right 0…+60), so a target dead
> ahead lies on the shared edge of all three and a wrong third would cost exactly nothing. The baseline
> is therefore laid out with the raid 20° off the nose on a constant-bearing course, and says so.

---

## State

**BUILT AND FLOWN, 2026-07-29 — the fourth of the ten campaigns to exist as files.** Ten `.fbm` in
`mods/f16/src/missions/o2-*.fbm` plus `mods/f16/src/campaigns/o2-pvo-intercept.fbc`, run as a campaign, replayed step by
step, and measured. **No file under `sim/src/`, `sim/tools/` or `mods/f16/src/` was touched**
(`git status --porcelain` lists eleven new untracked files and **no modified one**), so the **173** pre-existing
`mods/f16/src/missions/*.fbm` are byte-identical **by construction rather than by comparison**: the binary that
flew O2 is the binary that flew everything before it.

### The two unread sources were read, and the doctrine half is no longer the thinnest in the directory

`INDEX.md` and `PROGRESS.md` both named the same pair as *"the highest-value unread source in the
directory"*. **Both were retrieved and read on this round.** The `cia.gov/readingroom/docs/…` path is
Akamai-blocked to automated retrieval (HTTP 302 → *Access Denied*); the Wayback Machine's captures of
the same two URLs are not, and both PDFs came back complete (11 and 13 pages). Their content is §1's
new [T1] block and §Knowledge 1; four of the facts changed a mission or a header rather than decorating
one, and one of them — **DRUZHBA-76's 10-to-15-minute ground picture** — is the number that puts this
campaign's own headline result in proportion.

### The arena, and the disclosure that belongs in the first paragraph

An exercise has no battlefield, so the arena is the Soviet air-defence firing range at **Ashuluk**,
Astrakhan oblast [T4], taken as ~47.30 N 47.90 E `[SET]` and not improved on. `--elev const`, 0 m
datum, no terrain masking (`C4`). Scale: 1° of longitude = 75 490 m [DERIVED]. The defended complex is
a command post and a vehicle park at 47.675 / 48.53; the early-warning set is a **P-18 that really
radiates** at 47.55 / 48.20 — `C1` was re-checked against the tree rather than trusted, and the spec's
*"the EW radar that generates the vector is inert"* is no longer true.

**The disclosure: the defect this campaign was written about is the one standing in its way.**
`FBMig29Pilot` posts the GCI's **world-frame** scan elevation into a **body-frame** antenna command
(`doc/pilot.md` 2.15, found by O5, still open on the tree O2 flew against). Every interceptor in all ten
files is therefore laid out **level and co-altitude**, and what that costs is measured below rather than
argued.

### The ten sorties, their fingerprints and their answers

Campaign exit **3** (the worst step's; nine of the ten are measuring rigs whose own header says the
verdict is the telemetry). Campaign fingerprint under `--elev const`:
`93b5869298b6b8a59248a1b906447e27a183b526c745959b6edbdce98bea43bc`. **The FRAME round of 2026-07-29 moved it again** — the spawn state is now the trimmed airframe's own rather than position only, so the first 0.01 s of guidance moved in every mission with an airframe (`sensors.md` §10, item 24). Post-frame-round value, both criteria re-measured and still holding: `b582a3694f36b33837168b5ecdb2275ed993eaad10eafe7babcdfc80de5544de`.

| # | Mission | ctrl | exit | fingerprint | The answer to its one tactical question |
|---|---|---|---:|---|---|
| 1 | `o2-01-vector` | — | 3 | `7b48d2816b676d16` | **The loop is 11.0 s and it splits 8.0 + 3.0.** `gci BRAA` t = 19.9 → `ENTER_ELEV` (−0.03°) → `ENTER_ZONE` t = 21.9 (−20.05° → `zone=left`) → `RADAR_ILLUM` t = 23.9 → `n019 EMISSION` t = 27.9 → first firm track t = 30.9 at **16.08 nm**. Call → radiating **8.0 s** (reproducing `mig29-intercept.fbm` on a different geometry, so it is a property of the entry chain); radiating → firm track **3.0 s** (one RAD frame). The price, on the other jet: `rwr THREAT_NEW … kind=fire-control` at t = **28.0**, 0.1 s after the emission. **And an intercept that connects is not an intercept that kills:** both R-27R away at t = 45.9 onto the same target, arrivals **4.85 m** and **4.75 m** against a 13.8 m fuze, both in the FORWARD zone — twelve avionics boxes failed, engine/controls/structure only degraded, `CombatEffective` holds, `kill team friendly` UNMET, and the intruder finishes its route blind and unarmed |
| 2 | `o2-02-wrong-altitude` | 01 | 0 | `786907782ec13245` | **The entry deletes the search, and the recovery is a dead-band accident.** Entered `elDeg=7.4712`. The LEAD: **zero radar contacts in 400 s** — its uncued law had cached −2.0038 at t = 3.5 and never drifted the 2.0° that would make it re-command. The WINGMAN: cached 0.0, drifted past the band at t = 55.9, pulled the antenna to −2.0001 and found the raid at t = 60.9 at 8.56 nm — **30.0 s and 7.5 nm** worse than its control. **The intruder was not even warned:** first `kind=fire-control` at t = 56.0 against 28.0, because a mis-aimed beam does not illuminate what it is not pointed at. **And the outcome inverted, which is reported as chaos and not as doctrine:** one R-27R arrived 2.48 m out and produced the `damage KILL` the control never got out of two hits at under 5 m |
| 3 | `o2-03-wrong-zone` | 01 | 3 | `25e345045aa3b3b3` | **The wrong third is worse than the wrong band, and the overlaps forgive nothing.** `offDeg=19.9496` → `n019 ZONE zone=right`, and **zero contacts on the raid in 400 s** (the file's one `RADAR_CONTACT` is the wingman at 0.62 nm). Against sortie 02's four, the azimuth error costs more, and it costs it to **both** aircraft — there is no dead-band accident in azimuth, because the switch is discrete and nothing drifts. Zero shots; the intruder is never warned at all |
| 4 | `o2-04-no-gci` | 01 | 3 | `41eba66be05f75df` | **Silent is not blind. Silent is UNREAD.** Zero `gci` lines, zero `n019 EMISSION`, zero contacts, zero shots — deleting the controller here does not degrade the search, it deletes the radar, because the emission switch powers up `off` and the third typed entry is the only thing that turns it on. **And the doctrine's own promise, priced:** the wingman's KOLS held the raid from t = **105.0 to 195.0**, ninety seconds of passive, un-warning contact at brg 241.8 — the identical window sortie 03 measured — and nothing read it (`D3`) |
| 5 | `o2-05-late-radar` | 01 | 3 | `9e637d7baa2a6c13` | **The late commit buys 45.3 s and costs the whole engagement.** `BRAA` t = 64.9 → `EMISSION` t = 73.2 (8.3 s) → first firm track t = 76.3 at **3.68 nm**. The purchase, on the intruder: first `kind=fire-control` at t = 73.3 against 28.0 — **45.3 s** in which it heard only the ground. The price, on the interceptor: 6.82 km instead of 29.78 km, **77 % of the detection range**, and **zero shots** against two hits |
| 6 | `o2-06-identify` | — | 0 | `179512110a91db5c` | **The answer costs 0.1 s of surprise and arrives 3.0 s later — and the target learns first.** `EMISSION` t = 27.9 → the subject's RWR at t = **28.0** → the interceptor's own first contact and `IFF_REPLY … reply=none` at t = **30.9**. `mission IDENTIFIED` t = 133.9, 658.8 m inside a 2 000 m box, `heldS=30.1`; `no_fire` met. **The two channels that cannot identify saw it anyway:** the KOLS at t = **10.0** — 17.9 s before the aircraft radiated at all — and the eye at t = 122.0, `vis RECOGNISED … type=f16`, a registry key that reads the same for a friend and an enemy |
| 7 | `o2-07-identify-friendly` | 06 | 0 | `acb39b903ff817f7` | **The channel works, and that is all this file had to show.** `reply=friendly` at the identical t = 30.9, track number and range. Normalised against 06, `events.log` differs in **exactly two lines**: that reply and the judge's own `team=` field. The interceptor's telemetry is **byte-identical** — the answer changed and no metre of flight path did, which is the honest statement of what this pilot does with an identity today |
| 8 | `o2-08-team-swap` | 06 | 0 | `d82f850dd43d9ef3` | **Byte-identical for the whole run, in every channel.** 5 of 5 `telemetry*.csv` byte-identical to 06's; `events.log` differs in **1 line of 53** — `team=neutral` vs `team=friendly` in the RUNNER's own `UNIT_RESULT`, which no simulated system can read. Four channels live (N019 + interrogator, KOLS, eye, SPO-15), 300 s, and nothing moved. **The strong form holds** — the criterion asks for identity up to the first discriminator, and there is none |
| 9 | `o2-09-exercise-one` | — | 3 | `e915620d3bd44811` | **CHAIN HEAD. The corridor survived and the raid was denied by a hit that did not kill.** Two vectors, staggered: sok1 finds at t = 30.9, sok2 — briefed at 58 km, **beyond the N019's 50 km gate** — at t = 45.9, and both pay the identical 8.0 + 3.0 s. 10 releases, 4 detonations, no kill; `w1a` took three arrivals and its **`stores` system failed at t = 128.3**, so it reached its aim point and released nothing: `deny release team friendly` MET on all four, earned inside the simulation. **`w1c`, the corridor aircraft, answered `friendly` to three of the four interceptors at 34–48 km and no round was fired at it** |
| 10 | `o2-10-exercise-two` | — | 3 | `09e71efe2a0791ec` | **CHAIN TERMINUS. The magazine decides the weapon and the weapon decides the range.** Standalone: 7 releases, first kill t = **80.6** by a 3.46 m R-27R at BVR, run 328.1 s. As step 10: 4 releases, first kill t = **182.3** by a **1.37 m R-73 at the merge**, run 304.6 s. Same brief, same geometry, same controller — **101.7 s later and one weapon class down**, because that is what was left on the rails |

### The carry, where it lands, and what it was worth

`carry units ground stores` — **not narrowed.** It lands on **one** chain (09 → 10) and nowhere else,
because that is the only place the campaign has a question about it: *is the controller worth the same
to a force that has already paid?* Sorties 01–05 and 06–08 are eight pairwise-disjoint casts —
aircraft **and** ground — that carry nothing in or out.

Entering step 10: **10 × `campaign CARRY … action=stores`** and nothing else. `pvo1` enters with 1
R-27R, `pvo2` with 0, `pvo3` with 0 R-27R + 1 R-73, `pvo4` with **0 and 0** — an empty aeroplane on a
correct vector. **`units` and `ground` carried nothing, because wave one lost nobody and the complex was
never touched, and that is stated rather than dressed up as an attrition arc.**

Campaign totals: `ATTRITION unitsFriendly=2 unitsHostile=0 groundFriendly=0 groundHostile=0`,
`EXPENDED r27r=13 r73=6`.

**The answer, and its limit:** the controller's own worth did **not** change — both waves take the
identical 8.0 s to radiate and reach a firm track the identical 3.0 s later. What changed is what the
force could DO with his vector. **Two points are a difference and never a trend**, the `.fbc` header
says so, and this campaign therefore publishes no force-state curve.

### When is the ground controller worth anything — the campaign's own question, answered

Three campaigns had measured him at nothing (`o1-02` head-on, `o1-03` truncated mid-intercept, `o5-03`
defender 4 000 m high). The reason they could is that in all three the MiG's radar was **already
radiating**: deleting the brief left it badly aimed. O2 flies the doctrinal power-up state instead —
`n019_emission off`, the position both manuals document — and the answer inverts.

| Condition | First firm track | Shots | The intruder's first fighter-radar warning |
|---|---|---|---|
| correct brief (`o2-01`) | t = 30.9, **29.78 km** | 2, both hit | t = 28.0 |
| altitude band wrong (`o2-02`) | t = 60.9, 15.86 km, **lead never** | 1, hit | t = 56.0 |
| azimuth third wrong (`o2-03`) | **never** | 0 | **never** |
| controller deleted (`o2-04`) | **never** | 0 | **never** |
| controller late (`o2-05`) | t = 76.3, **6.82 km** | 0 | t = 73.3 |

**The controller is worth the entire intercept — on an aircraft that starts silent.** He is worth
nothing on one that is already radiating, which is what the three earlier campaigns measured and which
remains true. The finding is therefore not *"O2 disagrees with O1 and O5"*: it is that **the quantity
those three measured was the AIMING of a radar and the quantity this one measures is its EXISTENCE**,
and the deciding line is `set n019_emission`, not `set brief_gci`.

**And the honest counterweight, from the [T1] source read this round:** the loop this campaign times is
the **cockpit** half. Mitronin's DRUZHBA-76 figure for the *ground* half of the same loop is **10 to 15
minutes** to move own-aircraft data down a code table. FlightBox's controller has **no latency at all**
(`C6`), so an 8.0 s call-to-radiating is measured against a ground picture that is, in the anchor,
between 75 and 113 times slower than the whole engagement.

### The identification counter-check, measured rather than argued

The requirement (`INDEX.md`, "The identification task"): build the mission twice, identical but for the
subject's `team`, and require byte-identity up to the first sensor discriminator.

| Comparison | Telemetry | `events.log` (normalised for callsign prefix and mission name) |
|---|---|---|
| **06 vs 08** — `team neutral` → `team friendly`, one token | **5 of 5 byte-identical** | **1 differing line of 53**: `mission UNIT_RESULT … team=`, written by the RUNNER |
| **06 vs 07** — the control: the subject answers | interceptor's **byte-identical**; the subject's differs in exactly **one column**, `iff_xpdr` | **2 differing lines**: `radar IFF_REPLY … reply=none` → `friendly` at t = 30.9, plus the same `team=` field |

**07 is why 08 means anything.** A byte-identical pair has two possible causes — no leak, or a dead
channel — and rule 4 says that gets a control run rather than an argument. The channel fires, on the
same tick, when there is something to discriminate.

Three further properties fell out of the same three runs and none of them was arranged:

- **`fcr_iff` is 0 for all three runs and that is not a hole.** The column reports the LOCKED track's
  reply and an identification pass never locks, so the discriminator lives only in `events.log` —
  which is exactly where the diff looks.
- **`vis RECOGNISED … type=f16` at t = 122.0 in all three.** The visual channel's type name is the
  module registry key (`C3`'s own design), so recognition produces the identical string for a friend
  and for a stranger. Recognition is not identification, and this is where the difference is visible.
- **The KOLS had the subject at t = 10.0**, 17.9 s before the aircraft radiated, and it has no IFF at
  all. A channel that sees earlier, for free, and structurally cannot identify is the eastern half of
  the asymmetry stated once in `INDEX.md`.

### The GCI elevation-frame defect, measured instead of worked around

`doc/pilot.md` 2.15 is open and it is this campaign's own subject. Three attribution runs, none of them
one of the ten, take `o2-01` and put the pair in a climb (spawn 2 000 m, briefed level 8 000 m):

| Run | Pitch at the first call | GCI posts | The body-frame command would be | Contacts on the raid |
|---|---|---|---|---|
| `o2-01` baseline (level) | +0.0…0.5° | −0.025° | −0.025° | 10 |
| **a** — climbing, 450 kt | **+5.40°** (5.36…5.89 over the run) | **+8.303°** | **+2.90°** | **7**, first at t = 30.9 |
| **c** — climbing, 350 kt | +5.61° (5.36…5.88) | +8.270° | +2.66° | 6, first at t = 30.9 |
| **b** — climbing, brief deleted | ″ | — | — | **0** |

**The error is exactly the aircraft's pitch, and on this arena it does not bite — by 0.11 to 0.64
degrees.** `FBAutopilot` holds ~25 m/s of climb at either speed, so the pitch band is a narrow
5.36…5.89°, and the N019's RAD bar is ±6.0°. O5's scramble bit because the raid ALSO sat 3–4° low in
the body frame and the two errors added; the bite condition is therefore

```
|pitch| + |target's body-frame elevation offset| > 6.0°
```

and O2 measured the margin on the other side of it. Run **b** reproduces O5's control: deleting the
controller does not recover a climbing interceptor either.

### What this campaign found while building, none of it fixed here

Rule: *the defect sits in the seam you did not look at.* O2 found three, and only the first is in the
subsystem the campaign was written about.

| # | Finding | The measurement that pinned it |
|---|---|---|
| **1** | **A wrong brief has no deliberate recovery, and what looks like one is a dead-band accident.** `FBMig29Pilot` types what it was given; `FBPilot`'s uncued search law re-commands only when ITS OWN wanted angle has moved more than `kInterceptElDeadDeg` = 2.0° from ITS OWN last command, and that cache is per aircraft | sortie 02: the wingman's cache sat at 0.0, drifted past the band at t = 55.9 and recovered the search 28 s late; the leader's sat at −2.0038 from t = 3.5, never drifted, and never looked at the raid again. **One aircraft of two, by arithmetic** |
| **2** | **`D3` is not an omission in a report — it is a sensor wired to nothing, and the price is now a byte count.** Adding `set kols_mode ir` to sorties 01–05 changed **exactly 4 of 184 telemetry columns** (`blk_irst`, `irst_on`, `irst_mode`, `irst_contacts`) and **not one metre of any trajectory, not one shot, not one other event** | sorties 03 and 04: the wingman's KOLS held the raid for **90 s** (t = 105.0 → 195.0) while the radar was aimed at the wrong third resp. switched off, and nothing consumed it |
| **3** | **An R-27R inside its own fuze is not a kill, and the deciding variable is the damage ZONE and not the miss distance.** Sortie 01's two arrivals at 4.85 m and 4.75 m entered the FORWARD zone and left the target combat-effective with every avionic box dead; sortie 02's single arrival at 2.48 m killed. The campaign's outcome axis is therefore **not** ordered by the quality of its briefs | 01 (2 hits, no kill, `kill team` unmet) against 02 (1 hit, `damage KILL` at t = 78.8) — and 09, where three arrivals on `w1a` failed its `stores` system and produced the DENIAL instead of the kill |

### Both determinism criteria, measured

Under `--elev const`, read out of `campaign-summary.txt` rather than assumed:

| # | Criterion | Result |
|---|---|---|
| **1** | 3 repetitions × `--threads 1/2/4` produce one campaign fingerprint | **9 runs, 1 fingerprint** `93b5869298b6b8a59248a1b906447e27a183b526c745959b6edbdce98bea43bc`, exit 3 in all nine; **re-measured after the frame round of 2026-07-29: 9 runs, 1 fingerprint** `b582a3694f36b3383…`, exit 3 in all nine |
| **2** | every step's per-mission fingerprint equals that mission run STANDALONE with step *k−1*'s state | **10/10 MATCH**, exit codes included, on the first attempt; **10/10 again after the frame round of 2026-07-29** |
| **1 — re-run 2026-07-30** | the same criterion under the branch-order change of `b433950` ([`../pilot.md`](../pilot.md) §7.4a) | **9 runs, 1 fingerprint** `f2fbb47e952f778ae06167bb976189d3fe9511b1faffd5389af744bcf612533f`, `--elev const`. **The value above is kept with its date; this is the current one.** Step exits `3 0 3 3 3 0 0 0 3 3` — unchanged |
| **2 — re-run 2026-07-30** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 | | `632fd1ad050f4749 0d6fcd7ba16bbad2 bae528e309e7f62d 4c11b3535a82e060 0170cac196ed5d2d d8debf56655d3c52 e884dc90411d51ae 460c72d66e7a046b 8e65b7f0bbfbdc13 f157f44c740bfdc0` |
| **1 — re-run 2026-07-30 (`E6`)** | the same criterion after the judge-completion fix of [`../doctrine-evolution.md`](../doctrine-evolution.md) X-1 | **9 runs, 1 fingerprint** `f2fbb47e952f778ae06167bb976189d3fe9511b1faffd5389af744bcf612533f`, `--elev const`. **The rows above are kept with their dates; this is the current one.** Step exits `3 0 3 3 3 0 0 0 3 3` — unchanged. **Unchanged, byte for byte** — campaign fingerprint and all ten step fingerprints. No mission of this campaign ends before its judges are finished, so none of them gains a line. |
| **2 — re-run 2026-07-30 (`E6`)** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Per-step fingerprints, 2026-07-30 (`E6`) | | `632fd1ad050f4749 0d6fcd7ba16bbad2 bae528e309e7f62d 4c11b3535a82e060 0170cac196ed5d2d d8debf56655d3c52 e884dc90411d51ae 460c72d66e7a046b 8e65b7f0bbfbdc13 f157f44c740bfdc0` |

**One process deviation, stated, and it is the same one O5 confessed:** `INDEX.md`'s rule 5 says run
`fb_campaign_verify.py replay` after the FIRST mission. It was run after all ten. It passed 10/10 on the
first attempt, so nothing was lost — and the rule exists to bound the damage when it does not.

### Conservation, and the gates

`git status --porcelain` lists **eleven new untracked files and no modified one**: ten
`mods/f16/src/missions/o2-*.fbm` and one `mods/f16/src/campaigns/*.fbc`. Gates: `make core-lib gym native wasm`
warning-free; `verify-layers` *"300 files, 826 internal include(s), 12 layers — no upward include, 3
restricted header(s) respected, 6 registry reader(s) inside the perception boundary, 287 file(s) in
their layer's namespace (5 C-island file(s) exempt)"*; `verify-models` *"4 upstream-backed model
path(s) match assets/MODEL-DELTAS.md (1 declared delta(s), 34 FlightBox-own)"*; eight harnesses rc = 0
(`test-monitor` `test-fdm` `test-corner` `test-gun` `test-missile` `test-weather` `test-mig29`
`test-air`). Annotating the ten files with their MEASURED blocks after the runs left all ten
per-mission fingerprints and the campaign fingerprint unchanged, which is the check that a comment is a
comment.

---

## Gaps

**Re-checked against the tree on 2026-07-29, not trusted** (`INDEX.md`'s inherited rule 7). Five of the
ten rows below had been closed by other rounds between the spec and the build.

| ID | What is missing | Blocks here |
|---|---|---|
| ~~`C12`~~ | **CLOSED and flown.** `identify unit … range … hold …` and `no_fire` carry sorties 06–08; `deny release team` carries 09 and 10 | — |
| ~~`C2`~~ / ~~`C0`~~ | **CLOSED and flown.** Ten declared clocks, one `.fbc`, both determinism criteria | — |
| ~~`C1`~~ | **CLOSED and flown, and the spec's own line about it is now false.** The defended complex carries a **P-18 that radiates**, is heard on the intruder's RWR as `kind=surface-search` from t = 0.1, builds its own tracks and can be killed | — |
| ~~`C3`~~ | **CLOSED and flown**, and it did more than expected: the eye RECOGNISED the subject's type at t = 122.0 in every identification sortie, 11.9 s before the geometric verdict latched, with the identical string on both sides of the faction swap | — |
| `C6` | **no live controller** — `set brief_gci` is text fixed at spawn. Every wrong brief here is wrong FROM THE START, which is the easy case, and no mission can measure a controller who goes silent or corrects himself mid-run | **still the campaign's subject.** And the [T1] source read this round sharpens it into a number: the anchor's ground picture takes **10–15 minutes** and FlightBox's takes **zero** |
| **NEW — no deliberate recovery from a wrong brief** | the pilot cannot doubt, re-request or re-enter a call; what recovery exists is a 2.0° dead-band artefact of `FBPilot`'s own search law (§State, finding 1) | sortie 02's second half, and every mission `C6` would enable |
| `C7` | **no bomber, no ELINT aircraft** — and the catalogue is not a way round it: `tu95` is a kinematic mover, and `ACCEPTED` means accepted as a FLIGHT MODEL | the PVO's real subject is not a fighter. Declared in every file, with its direction: the substitution makes the intercept HARDER than history |
| `D3` (`duels.md`) | **the pilot does not consume the IRST block** | mission 4's whole subject, and now priced: a 90 s passive contact that changed **4 of 184 columns and nothing else** (§State, finding 2) |
| `C19` | **no ROE / weapons-hold state** | "identify, do not engage" is `objective no_fire` in the verdict and a pilot hook nowhere — sorties 06–08 hold their fire because they carry no stores, not because they were told to |
| `C18` | **no radio between units** | the whole loop is a voice loop in reality; FlightBox models the aircraft end as typed entries, which is the right abstraction for the cockpit and the wrong one for the controller |
| **the runner's first-wreck rule** | not a gap ID, but the constraint that shaped 09 and 10: `FirstFlightKo` ends the run at the first K.O., so two vectors in one file are separated in TIME as far as staggered briefs allow and no further | sorties 09 and 10 are each measured for as long as their first casualty takes to fall |
| **the ZONE has no fourth position** | `n019_zone` is left/centre/right and the three overlap at the boresight, so a wrong third is only measurable on an OFF-NOSE geometry — which is why the baseline is laid out 20° off the nose and says so | nothing, once it is declared; it is a mission-design constraint, not a hole |

### The honest headline

**The controller was worth nothing in three campaigns because all three had already switched the radar
on.** O2 flies the documented power-up state instead — `n019_emission off` — and on that aircraft the
brief is not an aid, it is the precondition: delete it and there is no radar, get its azimuth wrong and
there are no contacts at all, get its altitude wrong and one aircraft of two recovers by accident. That
is the campaign's answer and it is a real one.

What it does **not** answer is the question `C6` owns. Every brief here is right or wrong **from the
first second**, and the [T1] document read on this round is the reason that limitation is now
quantified rather than merely admitted: the real loop's ground half took ten to fifteen minutes, and
FlightBox's takes none. **This campaign measures a cockpit with a stopwatch and a command post with
nothing.**

---

## Knowledge

### 1. The anchor with its sources

- **The doctrine.** [Unveiling Soviet interceptor doctrine in the Cold War (In The War Room)](https://www.inthewarroom.com/unveiling-soviet-interceptor-doctrine-in-the-cold-war/)
  [T4] — pilots as "extensions of the ground control network", flying to designated intercept points
  and minimising independent decision-making. Corroborating discussion:
  [Soviet air combat tactics in the Cold War (Secret Projects Forum)](https://www.secretprojects.co.uk/threads/soviet-air-combat-tactics-in-the-cold-war.37055/)
  [T4].
- **The hardware.** [Lazur (Grokipedia)](https://grokipedia.com/page/lazur) [T4] — a two-way VHF data
  link for GCI, transmitting guidance commands and radar video, enabling engagement without the
  aircraft's own radar; Lazur-S decoding course/speed/altitude plus discrete commands such as
  "afterburner on" and "radar on". [Sukhoi Su-15 (MILAVIA)](https://www.milavia.net/aircraft/su-15/su-15.htm)
  [T4] and [Uragan Soviet automatic air defense interception system (Wikipedia)](https://en.wikipedia.org/wiki/Uragan_Soviet_automatic_air_defense_interception_system)
  [T4] — the Vozdukh-1M/SAU-58 hands-off intercept, and the pattern of the ground station commanding
  heading, speed and altitude and telling the pilot when he may switch his radar on.
  [Ground controlled interception & integrated air defense (archive.org PDF)](https://archive.org/download/history-of-the-electro-optical-guided-missiles/IADS,%20GCI.pdf)
  [T3] — general GCI/IADS treatment, **not retrieved in full on this pass**.
- **Declassified primary material — ~~NOT read~~ READ IN FULL, 2026-07-29.** Both CIA reading-room
  documents were retrieved and read; §1's second table is what they say. The direct
  `cia.gov/readingroom/docs/…` path is **Akamai-blocked to automated retrieval** (HTTP 302 →
  *"Access Denied"*), and the route that works is the Wayback Machine's capture of the identical URL:
  - **Mitronin, General-Mayor of Artillery V.**, *"The Organization of Cooperation Between Air Defense
    Troops of the Ground Forces and Fighter Aviation"*, Warsaw Pact journal *Information Collection of
    the Headquarters and the Technical Committee of the Combined Armed Forces*, **issue 12, 1976**;
    CIA *Intelligence Information Special Report*, **16 February 1979**, TS #798039, 11 pages,
    declassified 18 June 2012.
    [1979-02-16A](https://cia.gov/readingroom/docs/1979-02-16A.pdf) ·
    [Wayback capture 2025-04-03](http://web.archive.org/web/20250403130054/https://www.cia.gov/readingroom/docs/1979-02-16A.pdf)
    [T1]. **This is the document that carries the campaign**: the two forms of cooperation, the five
    allocation currencies, the identification/fratricide argument, the corridors, the control-organ
    structure (TsBU IA, TsBU VA, PNT, GBU) and the DRUZHBA-76 latency figure.
  - **Pstygo, I. / Ganichev, N. / Reshetnikov, N.**, *"Air Support of Ground Forces and Control of
    Combat Actions of Front Aviation"*, USSR Ministry of Defence journal *Military Thought*, **issue
    5 (66), 1962**; CIA translation **12 October 1976**, 13 pages, released December 2004.
    [1976-10-12a](https://www.cia.gov/readingroom/docs/1976-10-12a.pdf) ·
    [Wayback capture 2025-04-04](http://web.archive.org/web/20250404124145/https://www.cia.gov/readingroom/docs/1976-10-12a.pdf)
    [T1]. **It is about fighter-BOMBER control and not about interception**, which is stated rather
    than stretched: what it contributes to O2 is two things and no more — the two named alert states
    ("airfield alert" / "airborne alert") and the statement that ground-control radar is a **contended**
    resource, prioritised to the air battle over air support. Everything else in it — the T/O
    operations groups, the coded map tables, the mobile command posts on ZIL-157 chassis — is outside
    this campaign's subject and is not used here.
- **The MiG-29's own half** is not researched here at all: it is already distilled, with page
  citations, in [`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md) §1–§3 and
  [`../modules/mig29/radar-sensors.md`](../modules/mig29/radar-sensors.md). Every [T2] row in §1 above
  points there rather than restating a manual.

### 2. Where the sourcing is weakest, and it is stated

| Thing | Status |
|---|---|
| Whether a MiG-29 in PVO/Frontal Aviation service actually flew Lazur-class command guidance, and with which ground system | **not established.** The DCS manual documents the *switch* and says the function is not implemented; research puts the 9-12 in the ALM-1/ALM-4 family ([`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md) §7). No message set, no frequency, no range was found |
| PVO exercise structure — what an intercept exercise actually consisted of, how it was scored | **still not sourced, and the [T1] round did not fix it.** Mitronin names DRUZHBA-76 and says what was *learnt* at it, never how it was run or scored. The ten missions are a **designed ladder** `[SET]`, built from the documented loop, not a reconstruction of a syllabus |
| Reaction times, alert states, scramble procedure | **partly sourced as of 2026-07-29, and the part that landed is the smallest one.** Pstygo et al. name the two alert states — **"airfield alert"** and **"airborne alert"** — and no time for either. The one interval either document gives is a **ground-picture** latency (10–15 min at DRUZHBA-76), not a scramble. So the campaign now knows what the two postures are CALLED and still cannot put a second on either, which is exactly the hole O5 booked as "the alert scramble is not expressible" |
| The arena | **`[SET]` and unsourced.** Ashuluk is [T4] as *a* Soviet air-defence firing range; nothing was found that ties a MiG-29 GCI exercise to it, and the campaign uses the coordinates as a flat, declared stage rather than as a claim |

### 3. Why this campaign and W5 must be read together

They are the same task from the two sides of the perception boundary, and the asymmetry is exact:

| | F-16 (W5) | MiG-29 (O2) |
|---|---|---|
| Identity channels | IFF interrogator **and** a cooperative datalink that supplies a friendly picture | **IFF interrogator only** |
| Cost of identifying | the datalink is passive; the interrogator is a radar action | identifying **always** means radiating |
| The passive sensor | none | KOLS IRST — sees, but has **no IFF at all** ([`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md) §3) |
| The receiver | ALR-56M: 360° azimuth, ±45° elevation, no IFF | SPO-15: warns of **every** radar including friendly ones, and is **blind forward while the own radar transmits** |
| Consequence for an AI | it can build a picture and stay quiet | **every increment of certainty is paid for in surprise** |

That difference is not a balance decision — it falls out of two documented aircraft — and it is why
the identification task is the sharpest test of the anti-cheat boundary in the whole set: on the
eastern side, an AI that "just knows" would be visible not only in its shooting but in its **silence**.
