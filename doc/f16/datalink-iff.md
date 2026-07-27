# F-16C Datalink & IFF

**Sources**:
- **Chuck** = `doc/DCS F-16C Viper Guide.pdf`, Part 13 — Datalink & IFF, pp. 600–648.
- **ED EA Guide** = `doc/DCS F-16C Early Access Guide EN.pdf`, official module manual. Pages used this
  pass: 419–452 (Tactical Net Datalink — network/PPLI/tracks/markpoints/SEAD). Precise on the actual
  **network mechanism** (TDMA slotting, message types, track-file correlation) — the primary source
  for rebuild-grade datalink logic.

This file was **SHALLOW**; this pass raises the TNDL section to full depth. IFF (§2 below) remains at
Chuck's depth — the ED chapter's IFF material overlaps with FCR's AIFF (`radar-sensors.md` §"NCTR &
AIFF") and was not independently re-extracted this pass; flagged as a remaining gap in §3.

---

## Spec

### 1. Datalink — MIDS / Link-16 / TNDL (Chuck, unchanged from previous pass)
- **MIDS** (Multifunctional Information Distribution System) = NATO name for the Link-16 communication
  component; carried by the aircraft's MIDS radios over the Link-16 TADIL network.
- DCS F-16 implementation is **TNDL** (Tactical Network Datalink), formerly "L16"/"Link-16" — same
  symbology, expanded network customization.
- TNDL lets F-16s and F/A-18s exchange data on the same network. Surveillance aircraft (E-3 AWACS)
  appear in the datalink without being added as TNDL donors.
- Datalink contacts + markpoints display on HSD and FCR; datalink markpoints stored in steerpoints
  71–80 (see `navigation-ils.md`).
- HSD datalink controls: XMT OFF / **TNDL**; contact filters FR ON (all friendly) / FL ON (flight
  leaders only) / FR OFF. DL/MAP power switches left OFF at startup (no function).

### 2. IFF — Identify Friend-or-Foe (Chuck, unchanged from previous pass)
Two components: **Interrogator** (broadcasts a coded interrogation) and **Transponder** (replies with
its own coded signal; content depends on selected mode).

| Mode | Type | Description |
|---|---|---|
| 1 | Military | 2-digit 5-bit mission code |
| 2 | Military | 4-digit octal unit code (set on ground for fighters) |
| 3 / A | Civil | 4-digit octal ID code (ATC-assigned, cockpit-set); usually combined with C |
| C | Civil | Pressure altitude; combined with 3/A → "Mode 3 A/C" |
| 4 | Military | 3-pulse encrypted reply (delay from encrypted challenge) — **secure** |
| 5 | Military | Cryptographically secured Mode S + ADS-B GPS position |
| S | Civil | Selective addressing, collision avoidance (TCAS/ACAS II); compatible with A/C SSR |

**Mode 4** is the combat mode: encrypted, undetectable by enemy transponders. A **valid Mode-4 reply
guarantees friendly** (in DCS); lack of reply does **not** guarantee hostile. Modes 1/2/3 are insecure.
**Only Mode 4 is simulated** (per the guide's 2022-01-16 note). IFF Master switch → NORM to power on;
ICP **IFF** button → IFF DED page shows mode codes.

---

### 3. ED EA Guide — TNDL network mechanism (official, primary — ED EA Guide p.419–452)

#### 3.1 Physical/network layer — TDMA (ED p.420–422)
TNDL is documented as a genuine **Time Division Multiple Access (TDMA)** network: every participant is
assigned a synchronized time slot (sync signal sourced from GPS) in which — and *only* in which — it
may transmit; no two participants transmit simultaneously on the same channel. Every participant
carries a **Source Track Number (STN)**: **5 octal digits (each 0–7)**, used by the MMC to identify the
message source and decide whether to retain the data (matches a programmed Flight/Team member or
Donor) or discard it (unrecognized STN + not a PPLI/C2 message type).

#### 3.2 Three channels, three roles (ED p.421–423)
| Channel | Purpose | Participants | Message types received |
|---|---|---|---|
| **Fighter (FC)** | fighter-to-fighter cooperative targeting | up to **4 Flight + 4 Team + 4 Donor** members, all independently configured per-aircraft | Flight: PPLI + Air Target tracks (**with** lock/shot lines) + markpoints + SEAD targets + TDOA. Team: same **minus** lock/shot lines. Donor: PPLI + Air Target tracks (no lock/shot lines) + markpoints. Unlisted participant: PPLI only |
| **Mission (MC)** | surveillance/C2 (e.g. AWACS) | C2 platforms | PPLI of the C2 platform + **Air Surveillance tracks** relayed from its own radar+Mode-4-interrogation picture |
| **Special (SC)** | — | — | **not implemented in DCS** |

**Rebuild-relevant asymmetry**: a Flight member gets richer data (engagement state — who's locked/
shooting at what) than a Team member or Donor from the *same* aircraft, purely based on which STN list
the receiving aircraft programmed it into — i.e. "how much I see of your picture" is a **receiver-side
configuration choice**, not a transmitter-side broadcast tier.

**C2/AWACS relay function** (ED, explicit): even if a friendly aircraft's own PPLI is LOS-blocked from
another participant (terrain masking), the C2 platform can still detect+interrogate it and relay its
position via Air Surveillance tracks on the Mission channel — i.e. TNDL coverage is not purely
point-to-point LOS-limited once a C2 relay is present on the network.

#### 3.3 Message types & their trigger conditions (ED p.424, precise — this is the event model)
| Message | Transmission trigger | Channel |
|---|---|---|
| **PPLI** | automatic, **fixed periodic interval** | Fighter or Mission |
| **Fighter Air Target Track** | automatic, whenever FCR is tracking ≥1 airborne target | Fighter |
| **Air Surveillance Track** | automatic, from C2 platform's own sensor picture | Mission |
| **Markpoint** | pilot-commanded (IFF-IN-style button, held **>0.5 s**), source = selected steerpoint or an actively-tracking sensor (FCR FTT/GMTT or TGP Point/Area/INR track) | Fighter |
| **SEAD Target** | pilot-commanded (throttle control, held >0.5 s) | Fighter |
| **TDOA Ranging** | pilot-commanded (SSC, held >0.5 s), requires an HTS-designated threat on the HAD page as SOI | Fighter |

#### 3.4 System Track File (STF) — the receiving side (ED p.437–441)
- The MMC's STF is **partitioned**: **first 10 slots = onboard FCR-generated tracks**, remaining slots
  = **offboard tracks received via MIDS LVT**. Every track (own-radar or datalink) carries: Track
  Number (TN), 3-D position/altitude, velocity/course (used to **extrapolate** position between
  updates — same dead-reckoning principle as FCR's own TWS track files), aircraft type (if known),
  sovereignty (Friendly/Unknown/Suspect/Hostile), and Mode-4 interrogation status/outcome.
- **Correlation logic**: a new track update is matched to an existing STF slot by TN; if a TN doesn't
  match anything, a new track is created. Independently, **tracks from different sources with matching
  position/altitude/velocity/course are correlated and merged into one slot** — i.e. the same physical
  aircraft seen by two different sensors (own FCR + a wingman's datalink track, or two wingmen's
  datalink tracks) collapses to one STF entry rather than duplicating.
- **PPLI-specific correlation**: if two aircraft ever transmit PPLI with an **identical STN** (a
  data/config error case), the display alternates ("mipples") between their positions each transmit
  cycle — ED documents this as observable behavior, i.e. STN collision is a real, simulated failure
  mode, not just a theoretical one.
- **Track staleness**: **PPLI tracks are extrapolated for 13 seconds, then purged** if no update
  arrives — the *same* 13-second constant used for FCR's own TWS track-file staleness timeout
  (`radar-sensors.md`, TWS section) — i.e. ED applies one shared "13 s without a refresh → drop" rule
  across both the organic-radar and the datalink track-management logic. Worth treating as one shared
  system parameter in a FlightBox implementation, not two coincidentally-equal magic numbers.

#### 3.5 Symbology semantics (ED p.425, precise)
- **PPLI**: always a **circle** (any TNDL transmitter is by definition friendly); **blue** = Flight/Team
  member, **green** = other friendly (Donor/unlisted participant). Wingman-ID glyph distinguishes
  Flight (1–4), Team (5–8), Donor (dot), or unlisted (blank).
- **Air Target / Air Surveillance tracks**: 4-way sovereignty coloring **Friendly/Unknown/Suspect/
  Hostile**. A bugged-by-wingman track gets a **number** (single Flight/Team bugger) or **"M"**
  (multiple buggers) above the symbol on the FCR format, or a **4-character callsign** if bugged by a
  Donor; the HSD format instead shows a **lock line** to the symbol, but **only for Flight members
  (1–4)** — Team/Donor bugs never get a lock line on the HSD, only the FCR-format annotation.
- **Markpoints / SEAD targets**: HSD/HAD-only symbols; get a **lock line** only when transmitted by an
  actively-tracking Flight member (1–4) — same Flight-only lock-line restriction as Air Target tracks.

## State

The datalink is FlightBox's **cooperative** sensor — the deliberate counterpart to the radar: everyone
transmits their own navigation solution and identity, everyone in range receives it. It was also the
first cross-unit perception in the simulator at all.

| Item of this reference | FlightBox | Where |
|---|---|---|
| Cooperative network picture with identity | **built** — `FBDatalinkSystem`: filters by faction, requires a *transmitting* sender, limits to min(terminal range, radio horizon of both altitudes) | [`../flightbox/sim/sensors.md`](../flightbox/sim/sensors.md) §3 |
| Network cycle and track staleness | **built** — the picture updates only on a 1 Hz net cycle and a track that stops being received is held for 3 cycles before it drops; tracks carry an age and are never "live" | same |
| Two switches, because the real terminal has two | **built** — POWER (off = blind *and* mute) and XMT (off = EMCON: still receiving, no longer heard) | same |
| MIDS-LVT terminal range (~300 nm LOS) | **built** in `FBF16Datalink` | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) §5 |
| HSD contact filter FR ON / FL ON / FR OFF | **built** as the F-16 override of `AcceptContact` — but "flight leads only" keeps the *first* unit of the faction: there is no formation concept | same + [`../flightbox/sim/sensors.md`](../flightbox/sim/sensors.md) Gaps 10 |
| IFF Mode 4 | **built, and it is the only identity source a radar contact can ever get** — two-valued: valid reply = friendly, no reply = unknown. `FBIffReply` has no "hostile" value | [`../flightbox/sim/sensors.md`](../flightbox/sim/sensors.md) §1 |
| TDMA slotting, STN assignment, PPLI intervals, message types | **not implemented** — the net cycle is a period, not a slot map | — |
| System Track File correlation, donor/quality logic, markpoints, SEAD messages | **not implemented** — a received track is the sender's own position, not a correlated multi-source file | — |
| Datalink symbology (HSD display) | **not implemented** — no HSD exists | — |
| IFF Modes 1/2/3, interrogation range limits | **not implemented** — Mode 4 only, every firm track in the volume interrogated every 5 s | same, Gaps 9 |
| Radio path obstruction | **not implemented** — the radio horizon is purely geometric, no terrain | same, Gaps 1 |

## Gaps

**Source gaps** (this file vs. its sources)
- **IFF stays at Chuck depth**, stated in the header and in §4: ED's IFF/AIFF material was not
  independently re-extracted, and overlaps with the FCR AIFF section in `radar-sensors.md`.
- ED pp.419–452 (TNDL) are fully processed; the IFF procedure detail is the named unprocessed remainder.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* cooperative track sharing with range, radio horizon, net cycle, staleness and the two
  terminal switches; IFF Mode 4 as the sole identity channel.
- *Partially:* the contact filter (FR/FL/off exists, "flight lead" is a placeholder for a formation
  concept that does not exist).
- *Not at all:* the TDMA/STN transport, PPLI and message-type model, System Track File correlation,
  markpoints/SEAD traffic, datalink display symbology, IFF Modes 1/2/3.

## Knowledge

### 4. Technical depth (researched — deepened this pass for TNDL, IFF remains a gap)

*Section number kept for cross-reference stability.*

- **Link-16 / MIDS-LVT real-world architecture** (T3, unchanged from previous pass — Wikipedia
  *Link-16*/*MIDS*): frequency-hopping, jam-resistant TDMA terminal; TACAN co-hosted in the same LRU.
  ED's TNDL documentation (§3.1–3.2 above) is **consistent** with the real Link-16 TDMA/STN model —
  this is a case where the official DCS chapter and the real-world system description agree closely,
  raising confidence in both.
- **AN/APX-113** (T3, unchanged — militaryaerospace.com/forecastinternational.com): combined
  interrogator/transponder, replaced AN/APX-101 on USAF F-16C/D, supports Modes 1/2/3A/C/4(/5).
- **Gap, not guessed**: this pass did not re-extract ED's IFF/AIFF chapter material beyond what
  `radar-sensors.md`'s "NCTR & AIFF" addendum already captured (the NCTR-trigger-fires-AIFF-LOS-
  interrogation-simultaneously fact). ED's dedicated AIFF procedural detail (interrogation SCAN vs.
  LOS methods, DED page fields) is **not yet distilled to ED-depth** — Chuck's summary (§2 above)
  remains the deepest coverage of IFF procedure specifically. **TODO (future pass)**.
- **Gap**: exact PPLI transmission interval (ED says "fixed periodic interval" but does not give a
  seconds value in the pages extracted this pass) — a genuine doctrine/hardware constant, not
  derivable from physics; mark as a gap rather than guess a Link-16-generic slot-rate figure, since
  DCS's own simulated value may differ from the real system's.

### Sources
- **ED EA Guide** (primary this pass): pp. 419–452 (§3 above).
- **Chuck's Guide**: Part 13, pp. 600–648 (§1–2 above, unchanged from previous pass).
- T3 web research (§4, unchanged from previous pass): Wikipedia *Link-16*/*MIDS*, militaryaerospace.com
  / forecastinternational.com AN/APX-111/113.
