# O2 — PVO intercept exercise (ground control in its pure form)

**What this file is:** a **campaign spec** — ten missions derived from one doctrinal anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet. It is the **eastern half
of the identification argument**; the western half is [`w5-baltic-qra.md`](w5-baltic-qra.md), and the
argument is stated once in [`INDEX.md`](INDEX.md) §"The identification task".

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Soviet PVO / ground-controlled interception practice, and the MiG-29's own documented GCI hardware | §Knowledge 1, cited and tiered |
| **FlightBox sources** | the MiG-29 module's GCI chain, its sensors, and the mission format | [`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md), [`../modules/mig29/radar-sensors.md`](../modules/mig29/radar-sensors.md), [`../modules/mig29/module.md`](../modules/mig29/module.md), [`../missions/sensors.md`](../missions/sensors.md), [`../sensors.md`](../sensors.md) |

Confidence legend and gap IDs `C0…C21`: [`INDEX.md`](INDEX.md).

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
| Declassified primary material exists | CIA reading-room holdings include a 1976 report on the organisation of cooperation between PVO troops and fighter aviation, and a 1976 memorandum on control of front aviation's combat actions | [T1] |

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

---

## State

**Nothing built.**

What exists and carries this campaign, in unusual completeness for a spec-only campaign:

| Piece | Where |
|---|---|
| The three-entry GCI brief with per-entry latency and retry-on-rejection | `modules/mig29/FBMig29Pilot::BriefGci`, [`../modules/mig29/module.md`](../modules/mig29/module.md) |
| The measured loop time: **8.0 s** from call to radiating radar | `sim/missions/mig29-intercept.fbm` |
| The N019 as a mode set with a ±6° elevation bar, a 3.0 s frame and 6 s of inertial coast | [`../sensors.md`](../sensors.md) §4.9 |
| The ZONE switch as a discrete three-position control that **clamps** a continuous command | [`../missions/sensors.md`](../missions/sensors.md) |
| The emission switch (`illum`/`dummy`/`off`) powering up **off** by doctrine | ″ |
| The SPO-15's forward blanking while the own radar transmits — measured in `mig29-rwr-blind.fbm` | [`../modules/mig29/defence-rwr-cm.md`](../modules/mig29/defence-rwr-cm.md) |
| IFF as two-valued, contacts as anonymous, the registry reaching only sensor slots | [`../sensors.md`](../sensors.md) |

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C6` | **no live controller** — `set brief_gci` is text fixed at spawn; nothing re-vectors, nothing goes silent mid-run, nothing is ever *wrong on purpose halfway through* | **the campaign's subject.** Missions 2 and 3 can only give a brief that was wrong from the start, which is the easy case |
| `C12` | **no `identify` objective** | missions 6–9 have no declarable verdict |
| `C7` | **no bomber, no ELINT aircraft** | the PVO's real subject is not a fighter; substituting one changes the closure, the aspect, the RCS and the tactical meaning of everything |
| `D3` (`duels.md`) | **the pilot does not consume the IRST block** | mission 4 — the doctrine's central promise (intercept without radiating) — cannot be flown as intended |
| `C3` | **no visual acquisition** | a PVO intercept classically ended with a visual pass, often specifically to identify |
| `C19` | **no ROE / weapons-hold state** | "identify, do not engage" is a pilot hook, not a declaration |
| `C2` | **no time of day** | mission 10 |
| `C1` | **defended sites do not emit** | the EW radar that generates the vector is inert |
| `C0` | **no campaign layer** | an exercise is a sequence with a score |
| `C18` | **no radio** | the whole loop is a voice loop in reality; FlightBox models it as typed entries, which is the *right* abstraction for the aircraft and the wrong one for the controller |

### The honest headline

**This is the campaign FlightBox is best equipped to fly and least equipped to make interesting.**
The aircraft side of the loop is genuinely built — three typed entries, real latency, real rejection,
a radar whose scan volume depends on what was typed. The *ground* side is a string in a mission file.
Until `C6` gives the controller a voice that can change during a run, eight of these ten missions
measure a pilot obeying a brief nobody can revise.

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
- **Declassified primary material that exists and was NOT read.** The CIA reading room holds a
  February 1976 intelligence information special report on the organisation of cooperation between
  air-defence troops and fighter aviation, and an October 1976 memorandum on the control of front
  aviation's combat actions, both derived from the Soviet *Military Thought* collection:
  [1979-02-16A](https://cia.gov/readingroom/docs/1979-02-16A.pdf) and
  [1976-10-12a](https://www.cia.gov/readingroom/docs/1976-10-12a.pdf) [T1].
  **They are the T1 upgrade path for this entire campaign** and are flagged as unread in
  [`PROGRESS.md`](PROGRESS.md).
- **The MiG-29's own half** is not researched here at all: it is already distilled, with page
  citations, in [`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md) §1–§3 and
  [`../modules/mig29/radar-sensors.md`](../modules/mig29/radar-sensors.md). Every [T2] row in §1 above
  points there rather than restating a manual.

### 2. Where the sourcing is weakest, and it is stated

| Thing | Status |
|---|---|
| Whether a MiG-29 in PVO/Frontal Aviation service actually flew Lazur-class command guidance, and with which ground system | **not established.** The DCS manual documents the *switch* and says the function is not implemented; research puts the 9-12 in the ALM-1/ALM-4 family ([`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md) §7). No message set, no frequency, no range was found |
| PVO exercise structure — what an intercept exercise actually consisted of, how it was scored | **not sourced.** The ten missions above are a **designed ladder** `[SET]`, built from the documented loop, not a reconstruction of a syllabus |
| Reaction times, alert states, scramble procedure | **not sourced** |

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
