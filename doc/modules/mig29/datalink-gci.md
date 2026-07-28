# MiG-29A (9-12) — GCI Guidance (Lazur/Biryuza), IFF, and the Doctrine Contrast to Link-16

**Variant scope**: izdeliye **9-12** (Fulcrum-A). Deltas in §7.

**Sources**: **DCS-EA** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (printed page == PDF page);
pages used: 38, 48–49, 51, 59, 113. **DCS-FM** = `doc/DCS MIG-29 Flight Manual EN.pdf` (**printed page
= PDF page − 6**); pages used: 12–13, 30–31, 43–44, 83–86, 99–100.
Research in **§4 Technical depth**, tiered **T1** > **T2** > **T3** > **T4**.

**Depth declaration**: **SHALLOW on the hardware** — this is the thinnest-sourced file in
`doc/modules/mig29/`, and honestly so: **the DCS module does not implement the guidance datalink at all**, and
both manuals say so explicitly. **MEDIUM on the doctrine and the human-in-the-loop mechanism** — the
FC3 manual documents, in detail, exactly *how* GCI data enters the aircraft when the datalink is absent
(§2.2), which is the mechanism a FlightBox pilot module should implement first. **§5 is the design
argument**, kept separate from the sourced facts.

> **The single most important line in this file**: the MiG-29 was designed to be **flown to the merge by
> someone else**. Its own long-range picture is poor by Western standards, its radar warns the enemy the
> moment it is used, and the compensating asset is a **ground-controlled intercept network that hands the
> fighter a vector**. That is the exact inverse of `doc/modules/f16/datalink-iff.md`, where the aircraft is a peer
> node in a self-organising, aircraft-only network.

---

## Spec

### 1. What the aircraft actually carries

| Item | Evidence | Status |
|---|---|---|
| **"Birjuza" (Biryuza) radio link, onboard part** | listed in the airframe avionics inventory, `DCS-FM p.12` | present in the real jet |
| **Guidance system panel**, right console position 8 | `DCS-EA p.38` | **"Not available"** in DCS |
| "Control panels for the state identification system **and guidance**" | `DCS-EA p.49` (figure, callouts uncaptioned) | panel exists in the cockpit model, unlabelled |
| **Command Guidance mode switch**, PU-S31 weapon control panel, position 5 | `DCS-EA p.59` | *"Position up – **automatic guidance by a datalink, voice guidance from the command post**. Currently not implemented."* |
| **SO-69 ATC transponder** | `DCS-FM p.13` | present in the real jet |
| **IFF transponder** panel, right console position 16 | `DCS-EA p.39, 48` | **"Not implemented yet"** |
| **ID index coder** ("sets the board identifier"), right console position 3 | `DCS-EA p.38, 48` | **"Not implemented"** |
| **IFF power switch** on the System Power Panel | `DCS-EA p.51` | switch exists |
| **GCI** as a defined term | `DCS-EA p.113` abbreviation list | — |

**That is the complete hardware evidence in both manuals.** No frequencies, no message set, no
symbology, no ranges. Everything else in §4 is research.

---

### 2. How GCI information actually reaches the pilot in the simulated aircraft

Since the datalink is not implemented, the documented path is **voice → pilot → manual data entry**,
and the manuals are unusually specific about it. **This is directly implementable today.**

#### 2.1 The voice request/response protocol (`DCS-FM p.99–100`)
| Request | Response format |
|---|---|
| **BOGEY DOPE** — bearing, range, altitude and aspect of the **nearest** enemy aircraft | *"(callsign), (controller), **bandits bearing (xx) for (yyy). (altitude) (aspect)**"*. **Range in kilometres if the controller is Russian**, miles if Western |
| No enemy contact | *"(callsign), (controller), **clean**"* |
| Enemy **within five miles** of the player | *"(callsign), (controller), **merged**"* |
| **PICTURE** — same data for **all** enemy aircraft in the zone | same format, repeated |
| **Vector to Home Plate** | *"Home bearing (xx) for (yyy)"* |
| **Vector to Tanker** | *"Tanker bearing (xx) for (yyy)"*, or *"No tanker available"* |

**Rebuild-relevant properties**: the controller reports **BRAA + aspect**, in **kilometres**, and has a
**"merged" state at 5 nm** beyond which it stops being useful. It reports **nearest** or **all**, never
a tracked, correlated, persistent track file. There is **no altitude of ownship in the loop** — the
pilot must convert the controller's absolute target altitude into a **relative** one himself (§2.2).

#### 2.2 The manual data-entry mechanism — **the important part**
`DCS-FM p.43–44` and `p.83–84`: the radar's **elevation scan coverage is computed from two numbers the
pilot types in**:
1. **Expected range to target, in kilometres** — *"often derived from AWACS and GCI data"*.
2. **Expected target altitude relative to own aircraft, in kilometres.**

The manual's own worked example:
> *"If your fighter is at an altitude of 5 km and AWACS reports a target at range 80 km and altitude
> 10 km, you should turn your aircraft towards the target, then enter the range of 80 km and relative
> altitude 5 km into the radar. The radar scan zone would then be correctly aimed at the expected target
> elevation."*

The entered range appears **under the azimuth coverage bar** on the HUD; the entered relative altitude
appears **next to the elevation coverage bar**. The **azimuth** side is separately set by the discrete
**ZONE** switch (left / centre / right — `radar-sensors.md` §2.2).

So the *real* GCI loop, as documented, is:

```
 ground controller ──voice BRAA──▶ pilot ──manual entry──▶ radar scan-elevation solution
                                     │
                                     └──manual ZONE selection──▶ radar scan azimuth
```

**This is a command/acknowledge channel with human latency in it** — exactly the shape of FlightBox's
`FBCommandBus`, and exactly the kind of thing `doc/modules/f16/controls-commands.md` §5 calls a *head-down
latency class*. A MiG-29 `FBPilot` that receives a GCI vector must **spend real seconds** entering it,
and can enter it **wrong**.

#### 2.3 Range-only cueing under jamming
The same manual mechanism is reused when the radar cannot measure range at all: under an AOJ (angle-of-
jam) lock, *"the target range displayed in the HUD … is **not measured by the radar but rather provided
by the fighter pilot (e.g. according to instructions received by radio)**, with the default value 10 km"*
(`DCS-FM p.52`). **The GCI channel is the fallback ranging source.**

---

### 3. IFF — what identity the MiG-29 can and cannot obtain

| Fact | Source |
|---|---|
| **Radar** returns a **double row of dots** for a target that answers IFF as friendly; hostile/unknown returns a **single row**. On the HDD, friendly = a **circular** mark | `DCS-FM p.43, 84` |
| A **"LOCK" switch (FOE / FRIEND)** on PU-S31 *"defines if radar can lock on the targets being detected by IFF as friendly"* — i.e. IFF gates the **lock permission**, not just the display | `DCS-EA p.59` |
| **The IFF interrogator does not operate with the IRST** — *"be absolutely sure that the target is an enemy aircraft before attacking"* | `DCS-FM p.86` |
| The **RWR has no IFF capability** — *"the system warns of every radar, both adversarial and friendly"* | `DCS-FM p.30–31` |
| SPO-15 threat-programme collisions can make a **friendly radar** be recognised as a threat type | `DCS-EA p.108` |

**Consequence, and it is a strong one**: on a MiG-29, **identity is only available through the active
radar**. Going passive (IRST, §6 of `radar-sensors.md`) buys stealth at the cost of **losing IFF
entirely**. There is no equivalent of the F-16's datalink-supplied friendly picture
(`doc/modules/f16/datalink-iff.md`) — the *only* cooperative identity channel is the radar interrogator, and
using it means radiating.

This maps cleanly onto FlightBox's existing anti-cheat contract (`CLAUDE.md`, "Kein Cheaten"):
`FBRadarContact` is anonymous, and `FBIffReply` is two-valued (friendly / no-reply). The MiG-29 simply
**cannot** obtain the third channel the F-16 has.

---

### 7. Variant notes
- **9-13 (MiG-29S)**: same guidance family; research (§4.2) puts both 9.12 and 9.13 in the
  **ALM-1/ALM-4** group, with ALM-4 the Rubezh-capable/Su-27-interoperable set.
- **Export aircraft**: reported to receive **the downgraded Lazur/ALM-1** rather than ALM-4 — relevant
  if a mission wants to model a coalition MiG-29 without full GCI integration.
- **MiG-29SMT/MiG-35**: modern digital datalinks; nothing here applies.

---

## State

**Nothing in this file is implemented.** FlightBox has no MiG-29 module, no
`sim/src/modules/mig29/` and no JSBSim MiG-29 model. The airframe exists only as a **spec-first
contract** — [`module.md`](module.md), whose own status
line reads *"spec only. Nothing is built."* Everything below is therefore a **forward commitment**,
not a description of code.

| Roadmap stage | What it will take from this file |
|---|---|
| **R3** — knowledge base | *running*: this file is the R3 deliverable for GCI doctrine and IFF |
| **R6** — asymmetric weapons | nothing directly |
| **R7** — enemy units at BVR scale | the decisive row: a MiG-29 opponent is **ground-controlled by design**. The compensating asset for a short-legged radar is a controller on the ground, and the FlightBox `FBDatalinkSystem` slot fits it **without new architecture** — but the payload is a **steering vector with human entry latency**, not a track list (§5). The voice-BRAA → manual-radar-cueing loop of §2.2 is implementable today; the hardware datalink can wait |
| **R8** — JSBSim model | nothing directly |

**The scale caveat that governs every row** (from the module file): the MiG-29 is a
**BVR-scale** opponent — what has to be right is what he can reach, how fast he gets there, what he
can see and what he can shoot. A failing knife-fight comparison is not a defect of the model; a wrong
envelope is.

Roadmap chain: [`../flightbox/roadmap.md`](../../roadmap.md) — **R3** (this knowledge base,
running) → **R6** (asymmetric weapons + RCS) → **R7** (enemy units, MiG-29 at BVR scale) → **R8**
(the JSBSim MiG-29 model). Nothing after R3 has begun.

---

## Gaps

**Source gaps** — this file has the most, and says so: the Lazur/Biryuza hardware is
documented in both manuals only as *"not implemented"*, so §1 is honestly **SHALLOW**. The file's own
itemised list follows, section number unchanged.

**Implementation gaps** — none statable yet: nothing is built (see State).

### 6. Open gaps (honest — this file has the most)
1. **The entire message set of Lazur/Biryuza** — command vocabulary, update rate, data format,
   frequencies, ranges. Nothing usable found.
2. **Whether the guidance couples to the SAU-451** (§5.3) — unresolved.
3. **The HUD symbology of the guidance mode** — no source describes what the pilot sees, apart from the
   unverified "range setting 9–9.5 km / 32 km" note (§4.2).
4. **IFF Mode/system designation and interrogation geometry** — the MiG-29's interrogator is never named
   in either manual (`DCS-EA` calls it "IFF transponder, not implemented yet"), and no research source
   gave the system (Kremniy-2 / Parol were the era's systems, **but no source in this pass confirmed the
   9-12's fit — deliberately not asserted here**).
5. **SO-69 transponder behaviour** — listed once, never described.
6. **The ground station's own detection performance**, which is what actually bounds a GCI intercept —
   entirely out of scope of both manuals and unreached by research.
7. **secretprojects.co.uk is 403 to automated fetching** and appears to hold the deepest public treatment
   of Soviet GCI datalinks. **TODO**: retrieve those two threads by another route — they are the highest-
   value single target for the next research pass on this file.

---

---

## Knowledge

§4 is the researched hardware depth; **§5 is a design argument, i.e. reasoning
rather than source**, and was already marked as such in place. Both sit here so that no reader can
mistake either for a documented aircraft property.

### 4. Technical depth (researched) — the real Lazur/Biryuza system

All **T4** (community/encyclopaedic; no T1–T3 source was reachable this pass — `secretprojects.co.uk`,
which appears to hold the deepest treatment, returns HTTP 403 to automated fetches).

#### 4.1 What Lazur is
- **Lazur** (NATO reporting name **"Markham"**) is a Soviet-era **two-way VHF data link for
  ground-controlled interception**: it carries **radar video and guidance commands** between the ground
  control station and the fighter, *"allowing pilots to engage targets **without activating onboard
  radars** to maintain stealth"*.
- Transmission is by **burst data on the VHF and UHF AM air bands**; **no voice transmission is
  necessary unless the data link fails**.
- Source: Grokipedia "Lazur" entry; radioreference.com "Historic GCI data links" thread.

#### 4.2 The MiG-29 fit
- The MiG-29 Lazur installation is described as comprising: **SAU-451-04** automatic control system ·
  **E502-20 / E502-20/04** airborne guidance system · **R-862** radio · **A-611** marker receiver ·
  **SO-69** ATC responder with the UNN block / K-42E kit · **ARK-19** radio compass ·
  **TESTER-UZ/LK** flight data recorder · **ALMAZ-UP** information reporting system.
- **E502-20 "Biryuza" (Turquoise)** and **E502-20/04** were the sets used on the MiG-29 in the 1980s —
  which corroborates `DCS-FM p.12`'s "onboard part of the radio link **Birjuza**".
- **Guidance information is displayed on the HUD**, with the display **range setting 9–9.5 km in
  Lazur-M mode and 32 km in Biryuza and SPK-75 modes**.
- Soviet MiG-29 **9.12 and 9.13** are described as having **ALM-1 / ALM-4** datalink reception for the
  **Vozdukh-1M** and **Rubezh** command systems; **ALM-1 and Lazur went to export/downgraded fits**,
  while **ALM-4** was more capable and interoperated with **Rubezh** ground stations and the **Su-27**
  data links.
- Sources: secretprojects.co.uk "Soviet GCI Command & Datalinks" (via search summary only, page not
  fetchable), Grokipedia, radioreference.com — **T4**.

⚠️ **Do not over-trust §4.2.** The component list is plausible and internally consistent (it re-uses the
same SAU-451 block family, the same R-862 radio and the same ARK-19 that `DCS-FM p.13` independently
lists), but it comes from a single forum lineage. **The one number in it — the 9–9.5 km / 32 km HUD
range scales — is the kind of detail that should be corroborated before it is modelled.**

#### 4.3 The ground side
- **Vozdukh-1M** is described as an automated **fighter direction post** for automated guidance of both
  the direction post and the fighters.
- **Rubezh** is the more modern command system the ALM-4-equipped MiG-29s worked with.
- Neither has a public message-set specification reachable this pass.

---

### 5. Design argument for FlightBox (**clearly separated — this is reasoning, not source**)

#### 5.1 The contrast, stated plainly
| Property | **F-16 / Link-16 (MIDS-LVT)** | **MiG-29 / Lazur-Biryuza** |
|---|---|---|
| Topology | **peer-to-peer TDMA network**, every participant a node | **star**: one ground station, one (or a few) controlled fighters |
| Payload | each node's **own nav solution + identity** → a shared track picture | **steering commands** (and radar video) → a vector to fly |
| Who holds the picture | **every aircraft** | **the ground station only** |
| Aircraft role | sensor **and** shooter **and** relay | **effector** |
| Identity source | datalink track + IFF | **IFF via own radar only** (§3) |
| Emission cost of using it | ownship **transmits** on the net (EMCON-relevant) | ownship can **receive silently**; the *ground* radiates |
| Failure mode | graceful — the picture degrades | **total** — without the controller the fighter is alone with a short-legged radar |
| Range | ~300 nm line-of-sight terminal range (`doc/modules/f16/datalink-iff.md`) | bounded by the **ground radar's** coverage, not the aircraft's |

#### 5.2 Proposed FlightBox mapping
`sensors/FBDatalinkSystem` already has the right shape and needs **no new architecture** — only a
different override, in the same slot the F-16 uses for `FBF16Datalink`:

- **`FBMig29Guidance : FBDatalinkSystem`** with a network of exactly **one sender**, which is **not an
  aircraft**. It publishes into `FBState` **not a track list** but a **steering vector** — the natural
  block already exists conceptually (Nav/Cruise), and the honest representation is
  *{commanded heading, commanded altitude, expected target range, expected relative altitude, age}*.
- **The two "expected" fields are the documented radar-cueing inputs (§2.2)** — so the guidance block
  feeds the *radar*, not the autopilot, exactly as the real loop does.
- **Power/XMT split still applies**: receiving is silent; the aircraft's **reply/reporting** (ALMAZ-UP)
  would be the transmitting half. Model receive-only by default → **the MiG-29 is quieter than an F-16
  on the net, by design.**
- **Latency and fallibility**: when the datalink is *not* modelled (the DCS-faithful case), the same
  block is filled by the **voice + manual entry** path of §2.2, and then it **must** carry pilot
  reaction time and be routed through `FBCommandBus` — a GCI call the pilot has not yet typed in is not
  yet knowledge.
- **Doctrine for `FBPilot`'s intercept phase**: a MiG-29 pilot module should be able to run an entire
  BVR approach with **the radar in DUMMY/OFF**, flying the controller's vector, and bring the radar to
  **ILLUM only inside a briefed range** — the mirror image of the F-16 module's "lock as late as
  possible", one step further back: **radiate as late as possible**.

#### 5.3 What must NOT be invented
- A track list. The sources describe **vectors and commands**, never a correlated multi-track picture in
  the cockpit.
- An identity feed. §3 is explicit: **no cooperative identity except via the radar interrogator.**
- Coupling to the autopilot. The SAU-451-04 is *named* in the Lazur fit (§4.2), which suggests the
  guidance could drive the AFCS — but **no source in this pass states that it does**, and the DCS
  manuals describe the guidance switch as offering *"automatic guidance by a datalink, **voice guidance
  from the command post**"* as a single position, which reads as a mode selector rather than a coupled
  autopilot. **Leave it as a HUD/director cue until better-sourced.**

---
