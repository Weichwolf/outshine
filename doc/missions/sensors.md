# Mission data — sensors: datalink, radar/IFF, RWR and countermeasures

**Source of this file:** the former `doc/mission-format.md` (split in the Phase-3 mirror rebuild), sections
"Datalink — was eine Einheit von den anderen sieht", "Radar (FCR) & IFF — was eine Einheit selbst
FINDET" and "RWR & Gegenmaßnahmen — wer schaut MICH an, und was werfe ich". Translated 1:1 from the
German original; no revision of content.

This file is the **mission-author's view**: which `set` keys exist, which rules a mission must know
when building a sensor scenario, and what becomes observable. The implementation contract of the
sensor slots themselves is in [`../sensors.md`](../sensors.md); the real jet's boxes are in
[`../modules/f16/datalink-iff.md`](../modules/f16/datalink-iff.md),
[`../modules/f16/radar-sensors.md`](../modules/f16/radar-sensors.md) and
[`../modules/f16/defence-rwr-cm.md`](../modules/f16/defence-rwr-cm.md).

---

## Spec

### Datalink — what a unit sees of the others

A unit perceives other units EXCLUSIVELY through simulated systems (CLAUDE.md "Kein Cheaten"). The
cooperative net is `sensors/FBDatalinkSystem` (MIDS/Link-16, F-16 derivation
`modules/f16/FBF16Datalink`); the active radar is `sensors/FBRadarSystem` (below). Both read the unit
registry (`units/FBUnitRegistry` — the snapshots of the last completed tick) and write contacts from
it into `FBState`; the pilot only reads FBState, never the registry.

Rules a mission must know when building one:

- **own team only** (`team`) — a cooperative net knows no enemies. Enemy reconnaissance is radar, not
  this.
- **the SENDER must transmit.** `set datalink off` switches the terminal off: the unit is blind AND
  mute. `set datalink_xmt off` is EMCON: it still RECEIVES the whole picture but is no longer carried
  by anybody (the normal case in training air combat).
- **Range** = min(terminal range, radio horizon of both altitudes ≈ 1.23·(√h₁[ft]+√h₂[ft]) nm).
  F-16 default 300 nm (MIDS-LVT/Link-16 LOS); `set datalink_range_nm` configures it (a mission that
  wants to show the range loss sets it small — `missions/payerne-pair-datalink.fbm`).
- **1 Hz net cycle, no live picture.** Between two cycles the reported position stands still and its
  AGE counts up (0..1 s); a track no longer received is held for 3 cycles and then deleted.
- **Contact filter** (F-16, HSD): `fr` all friendlies (default), `fl` flight leads only (for lack of a
  real formation structure: the FIRST `unit` of that team in the file), `off` none.

| `set` key | Values | Effect |
|---|---|---|
| `datalink` | `on`/`off` | terminal power — `off` is blind AND mute |
| `datalink_xmt` | `on`/`off` | XMT/EMCON — still receives, is no longer heard |
| `datalink_filter` | `fr`/`fl`/`off` | HSD contact filter |
| `datalink_range_nm` | nm | terminal range |

Observable in both channels: `events.log` carries `datalink TRACK_GAINED` / `TRACK_LOST` (discrete
events), `telemetry.csv` the five columns `dl_on`, `dl_xmt`, `dl_tracks`, `dl_near` (nm to the nearest
track), `dl_age` (s since its report).

### Radar (FCR) and IFF — what a unit FINDS by itself

The active counterpart to the datalink: `sensors/FBRadarSystem` (generic default), F-16 derivation
`modules/f16/FBF16Fcr` (AN/APG-68). The datalink gets identity as a gift, the radar gets an echo — the
rules a mission must know:

- **Scan volume instead of range.** A contact only comes into being if the target lies inside the
  volume of the active mode: azimuth × elevation RELATIVE TO THE NOSE (the volume rolls and pitches
  with the jet) plus a range gate. Mode table (`set fcr_mode`):

  | Mode | Azimuth | Elevation | Range | Frame | Auto-lock |
  |---|---|---|---|---|---|
  | `off` | — | — | — | — | — (does not radiate) |
  | `crm` (default, power-up) | ±60° | ±10.5° | 40 nm | 4.0 s | no |
  | `acm_hud` (30×20 box) | ±15° | ±10° | 10 nm | 1.0 s | yes |
  | `acm_bore` (~10° cone) | ±5° | ±5° | 10 nm | 0.3 s | yes |
  | `acm_vert` (narrow-tall) | ±5° | −13°…+47° | 10 nm | 1.2 s | yes |
  | `acm_slew` (20×20, `fcr_slew_*`) | ±10° around cursor | ±10° around cursor | 10 nm | 0.8 s | yes |
  | (locked = STT) | ±60° | ±60° | 40 nm | 0.1 s | single-target |

- **Contacts come into being and pass away in TIME.** Two consecutive looks (`kHitsToFirm`) turn an
  echo into a track — the build-up time is therefore one frame long. A track that is no longer seen is
  held for `max(1 s, 3 frames)` (coast, frozen geometry, `fcr_lock_age` counting up) and then deleted.
- **ACM locks by itself.** Every ACM sub-mode locks the NEAREST firm track without any input — that is
  the purpose of those modes. The lock is STT: the antenna leaves the small box, follows the target to
  the gimbal limit (±60°) and then sees ONLY this one target (all other track files run out).
- **CRM NEVER locks by itself — the pilot designates.** The command bus carries `Designate` for that
  (TMS forward): value = the published track NUMBER of the contact (i.e. exactly the anonymous handle
  the pilot reads from the bus), value 0 = break lock (TMS aft). A number without a firm track is
  rejected as `out_of_context` — the reflector was gone before the hand was finished. A DESIGNATED
  lock that is lost falls back into search and does NOT grab the next contact; auto-reacquire belongs
  to the ACM modes that asked for it. Events: `radar RADAR_DESIGNATE` resp. `radar RADAR_BREAK`
  (separate from `RADAR_LOCK`, because one is a decision and the other an automatism).
- **`fcr_slew_el` is the antenna-elevation control, not just the ACM cursor.** CRM's elevation CENTRE
  follows it too (±10.5° around the set height): a mechanically scanning radar covers only a few
  thousand feet at BVR range, and setting the antenna to the wrong altitude band is the classic way to
  fly past a target the radar could easily have seen. Default 0 → unchanged behaviour for every
  mission that does not set the key.
- **A contact is ANONYMOUS.** `FBRadarContact` (core/) carries range/bearing/elevation angle/az/el/
  closure/track number — no id, no callsign, no team. That is the fidelity boundary and at the same
  time the anti-cheat point. (Bearing and elevation angle are WORLD-referenced and stand beside the
  body-fixed az/el pair: the radar knows its own attitude at the moment of the look, its consumer does
  not — without that, every consumer would have to rotate a look-old body vector back through a
  now-current attitude and would smear its own roll motion into the target geometry.)
- **IFF is the ONLY source of identity** (Mode 4): valid reply = `friendly`; NO reply = **unknown**,
  never "hostile". A reply is only valid if the target's transponder is on AND the target belongs to
  the same team (crypto). An enemy with the transponder on and a friend with it off yield the same
  result: unknown. Switches: `set iff_xpdr` (what OTHERS get back from me, in the unit's emission
  snapshot) and `set iff_interrogator` (whether I ask at all).
- **Terrain masking is NOT modelled** (deliberately, documented in the header): for air-to-air between
  two flying units, volume and range decide; masking would need a DEM raymarch per contact and per
  look.

| `set` key | Values | Effect |
|---|---|---|
| `fcr_mode` | `off`/`crm`/`acm_hud`/`acm_bore`/`acm_vert`/`acm_slew` | the active mode = the active scan volume |
| `fcr_range_nm` | nm | overrides the range of EVERY mode |
| `fcr_slew_az` / `fcr_slew_el` | degrees | cursor of the slewable box; `_el` is also the CRM antenna elevation |
| `iff_xpdr` | `on`/`off` | own transponder |
| `iff_interrogator` | `on`/`off` | own interrogator |

Observable in both channels: `events.log` carries `radar RADAR_CONTACT` / `RADAR_DROP` (track build-up
and deletion), `radar RADAR_LOCK` / `RADAR_LOST` (STT) and `radar IFF_REPLY`; `telemetry.csv` the
eleven columns `fcr_on`, `fcr_mode` (ordinal of the table above), `fcr_contacts`, `fcr_lock` (track
number, 0 = no lock), `fcr_lock_nm`, `fcr_lock_az`, `fcr_lock_el`, `fcr_lock_clos` (kt, + =
closing), `fcr_lock_age` (s since the last look — > 0 means coast), `fcr_iff` (0 = not interrogated,
1 = unknown, 2 = friendly), `iff_xpdr`.

**No HUD symbology:** [`../modules/f16/hud-symbology.md`](../modules/f16/hud-symbology.md) documents
neither a target-designator box nor a locked-target symbol (the radar-adjacent entry, HMC, is a
markpoint cursor). The lock therefore stays in FBState/telemetry/events until the symbology reference
covers it — nothing is invented.

### RWR and countermeasures — who is looking at ME, and what do I throw

The other side of the sensor picture: `sensors/FBRwrSystem` (F-16: `modules/f16/FBF16Rwr`, AN/ALR-56M)
listens, `sensors/FBCountermeasureSystem` (F-16: `modules/f16/FBF16Cmds`, AN/ALE-47) answers.

**What a radar RADIATES is observable state** (`core/FBEmitter.h`), published at the same tick barrier
as pose and datalink switches: mode, emitter kind, the body-fixed beam window and the range gate.
Three signals, and the difference is the tactical core:

| Emission | Beam | What it means |
|---|---|---|
| **Search** | the whole scan volume of the mode (the beam sweeps over it once per frame) | somebody is searching — information |
| **Track** | ±3° PENCIL on exactly one contact (STT) | he has ME — warning |
| **Guidance** | the same pencil beam while the shooter supports a weapon | a missile is flying — alarm |
| **Missile seeker** | the missile's own beam (`modules/missile/FBMissileSeeker`) | terminal phase — alarm |

**The RWR is a geometrically limited RECEIVER, not a threat oracle.** It reports what it HEARS:

- **The beam must hit.** The window is body-fixed AT THE SENDER, is rotated with its published
  attitude and only then tested — the same transformation the sender uses for its own detection. A
  searching radar illuminates everything in its volume; a tracking one EXACTLY ONE aircraft.
- **The own antenna must be able to hear it.** 360° azimuth, but only **±45° elevation** (ALR-56M) —
  above and below that lies a real **blind zone** which one's own manoeuvring tears open and which
  makes an already existing lock or launch warning disappear SILENTLY (`rwr THREAT_BLIND`).
- **Hearing range = sender gate · 2** (`kBeamRangeFactor`): the receiver sits in the ONE-WAY path, the
  sender needs the way out and back. One is warned before one is detected.
- **No range.** An RWR measures received power, never range. Published are relative bearing,
  elevation, signal strength and a lethality (ring position), never metres.

**CMDS: programs as data, the mode knob as a state machine.** A program is the ALE-47's DED parameter
schema: per type burst quantity (BQ 0–99), burst interval (BI 0.020–10 s), salvo quantity (SQ 0–99),
salvo interval (SI 0.50–150 s); BQ or SQ = 0 removes that type from the program. The six F-16 programs
(`modules/f16/FBF16Cmds`, values [SET], schema documented):

| PRGM | Chaff | Flares | for what |
|---|---|---|---|
| 1 BREAK LOCK | 2 × 0.10 s, 2 salvos at 1.00 s | — | the dense reflex answer, and what AUTO throws against a MISSILE |
| 2 MIXED | 2 × 0.10 s, 2 salvos at 2.00 s | 1, 2 salvos | unknown threat |
| 3 FLARE | — | 2 × 0.10 s, 4 salvos | IR only (counted, ineffective: there is no IR seeker) |
| 4 SUSTAINED | 2 × 0.10 s, 4 salvos at 4.00 s | — | against a mere TRACK, and what AUTO then repeats |
| 5 SLAP | 1 | 1 | the panel button |
| 6 BYPASS | 1 | 1 | the documented emergency dispense |

Modes: `off`/`stby` (nothing; only in STBY may one reprogram) · `man` (CMS forward throws the PRGM
program) · `semi` (the system CHOOSES, but every dispense needs consent) · `auto` (chooses AND
repeats, consent applies from the mode change onwards) · `byp` (exactly 1 chaff + 1 flare). Automatic
dispenses stop at chaff BINGO. **In SEMI/AUTO the set triggers on the RWR BLOCK** — that is, on the
warning and not on the truth: what stands in the blind zone is not answered.

| `set` key | Values | Effect |
|---|---|---|
| `rwr` | `on`/`off` | power of the warning receiver |
| `rwr_display` | `priority`/`open` | TWP MODE display cap |
| `rwr_search` | `on`/`off` | SEARCH filter |
| `cmds_mode` | `off`/`stby`/`man`/`semi`/`auto`/`byp` | the mode knob |
| `cmds_program` | 1..6 | PRGM knob |
| `cmds_chaff` / `cmds_flare` | count per type, together ≤ 120 | magazine load |

**The effect — Doppler, not dice** (`sensors/FBRadarSystem`, a model decision with a derivation): a
chaff cloud loses the aircraft's velocity within one second and afterwards lies in the clutter filter
which a pulse-Doppler seeker discards — **unless** the tracked aircraft lies with its OWN radial
velocity in the same filter, which is exactly the case when it flies across the line of sight. Then
the processor cannot separate the two echoes and takes the strongest (RCS/r⁴, with the cloud's age
curve). Set parameters: notch half-width **40 m/s**, measurement dwell **0.2 s**, bloom **0.3 s**,
cloud lifetime **8 s**, RWR hearing factor **2.0**. All deterministic — no randomness anywhere.

Observable: `events.log` carries `rwr THREAT_NEW` / `THREAT_MODE` / `THREAT_BLIND` / `THREAT_DROP`,
`cmds PROGRAM_START` / `SALVO` / `PROGRAM_END` / `MAGAZINE_EMPTY` and `radar CHAFF_SEDUCED` /
`CHAFF_RESOLVED` (with the two measured quantities the decision came from); `telemetry.csv` at the end
of the line the ten RWR columns `blk_rwr`, `rwr_on`, `rwr_threats`, `rwr_mode` (−1 = nothing,
0 = search, 1 = track, 2 = missile), `rwr_brg`, `rwr_el`, `rwr_leth`, `rwr_new`, `rwr_launch`,
`rwr_act` and the eleven CMDS columns `blk_cmds`, `cm_mode`, `cm_status`, `cm_prog`, `cm_chaff`,
`cm_flare`, `cm_lo`, `cm_disp`, `cm_out_chaff`, `cm_out_flare`, `cm_clouds`.

Reference runs: `missions/rwr-spike.fbm` (search → track → missile seeker out of ONE geometry, and the
disappearance of all three), `missions/rwr-blindzone.fbm` (the ride through the ±45° boundary while
the sender demonstrably keeps locking) and the 2×2 board `cm-straight-clean` / `cm-chaff-straight` /
`cm-beam-only` / `cm-chaff-beam` (chaff alone ineffective, manoeuvre alone almost ineffective, both
together decisive).

## State

| Item | State |
|---|---|
| Datalink | built; four `set` keys, 1 Hz net cycle, 3-cycle hold, five telemetry columns |
| FCR / IFF | built; six modes plus STT, five `set` keys, eleven telemetry columns |
| RWR | built; ALR-56M geometry (360° az, ±45° el), PRIORITY 5 / OPEN 16 display cap, three `set` keys |
| CMDS | built; six programs, six modes, four `set` keys, eleven telemetry columns |
| Chaff effect | built as a Doppler-notch model, deterministic |
| Flare effect | **counted only** — there is no IR seeker to defeat |

## Gaps

| Gap | Detail |
|---|---|
| No terrain masking | air-to-air line of sight is free; masking would need a DEM raymarch per contact per look. The hook (`const FBWorld*`) exists, the computation does not. |
| Flares have no effect | no IR seeker exists; the count is honest bookkeeping, not a defence |
| No radar HUD symbology | the symbology reference documents neither a TD box nor a locked-target symbol, so none is drawn |
| Datalink flight leads are positional | for lack of a real formation structure, "flight lead" = the FIRST unit of that team in the file |
| CMDS program values are `[SET]` | the schema (BQ/BI/SQ/SI and their ranges) is sourced, the six programs' numbers are set by us |

## Knowledge

- **Why the datalink and the radar are opposites.** A cooperative net hands identity over for free but
  only within its own team and only if the sender transmits; an active radar finds anybody but gets
  only an echo, pays with a warning to the target, and can be deceived. Both write into `FBState` —
  what the pilot knows about other units is exactly what the sensors put there, with their range,
  their scan volume, their net cycle and their age.
- **Why a contact carries no identity.** The registry knows who is flying there; the radar must not
  pass it on. The only legitimate source of identity is IFF Mode 4, and it is two-valued — `FBIffReply`
  has no value "hostile" at all.
- **Why the RWR publishes no range.** It measures received power. Deriving a range from that would
  require assuming the emitter's transmit power, which is exactly the kind of invented number the
  conventions forbid.
- **Why hearing range is twice the sender's gate.** The receiver sits in a one-way path, the radar in a
  two-way path. The factor is the model's way of stating the physical asymmetry: one is warned before
  one is detected.
- **Why chaff is a Doppler question and not a probability.** A cloud without own velocity lies in the
  clutter filter and is discarded. It only works when the target itself is in the same filter — i.e.
  flying across the line of sight. Measured from OWN quantities (own velocity projected on the line of
  sight minus measured closure, over a dwell), never from the target's truth: deterministic, no dice.
