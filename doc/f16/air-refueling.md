# F-16C Air-to-Air Refueling

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 17 — Air-to-Air Refueling, pp. 778–791.

## Spec

### Basics
- The F-16C uses a **flying-boom** receptacle (behind the cockpit) — boom refueling only, not probe-and-drogue.
- Boom-capable tanker in DCS: **KC-135** (also KC-135 MPRS = probe-and-drogue via wing pods, not usable by F-16).
- Wake turbulence off the tanker causes wobble in close proximity — fly formation on the tanker, not the boom.

### Procedure

#### Tune tanker TACAN + radio
1. Note tanker TACAN channel (e.g. 14X) + UHF freq from briefing.
2. MIDS LVT — ON; adjust TACAN volume.
3. ICP **T-ILS(1)** → TACAN-ILS DED page.
4. Dobber DOWN to CHAN, keypad channel, ENTR; M-SEL(0)+ENTR for band X/Y.
5. Dobber RIGHT (SEQ) to **TCN A/A TR** (air-to-air transmit/receive).
6. Dobber LEFT (RTN) to CNI; EHSI "M" to TACAN mode.
7. C&I selector to UFC; COM1 → tanker UHF (e.g. 251.00 = "25100").
8. Comms Transmit AFT → contact tanker → "Intent to refuel" → tanker gives join instructions.

#### Configure aircraft
9. Reduce workload: DED Data switch FWD (DED on HUD); LIST → 2 (BNGO) for fuel monitoring.
10. **Open AIR REFUEL trap door.** (With external tanks: open **5–6 min prior** to let them depressurize.)
11. Confirm **RDY** light on (door open).
12. Master Arm — OFF; RF switch — SILENT.

#### Contact
13. Position ~**20 ft below** the boom; be **perfectly trimmed** before approach.
14. Fly formation with the **tanker**, not the boom; gentle small stick inputs, **no rudder**.
15. Let the boom pass just left/right of the canopy, ~2–3 ft above.
16. Move slowly forward aligned with the **yellow stripe** on the tanker belly; use **PDI (Pilot Director)
    lights** to stay in the boom envelope.
    - **PDI lights are directive** ("Go" + direction): light toward **D**=go down, **U**=up, **A**=aft, **F**=forward.
    - **Steady** = substantial correction; **flashing** = small correction.
17. Boom operator flies the boom into your receptacle → "contact" / "you are taking fuel".
18. **AR/NWS** light illuminates; monitor transfer on HUD + BNGO DED page. Hold alignment with tanker
    engines/centerline; correct one axis at a time.

#### Disconnect
19. Complete when **DISC** warning light illuminates. To leave early: press **NWS A/R Disc** button to
    unlatch the boom.
20. **Close AIR REFUEL trap door** and resume. (Leaving it open blocks external-tank fuel use.)

### Related limits (see `aerodynamics-performance.md`)
- Air-refuel door opening/closing: 400 kts / M0.85. Door open: 400 kts / M0.95.
- Opening the refuel door also sets FLCS gains to takeoff & landing mode and disengages autopilot.

---

### ED EA Guide addendum — official procedure detail (pp.157–162)

ED's own AAR chapter cross-validates Chuck's procedure closely and adds the pre/post-refueling emitter-
management checklist and the formal breakaway procedure Chuck's tutorial-style guide doesn't spell out
as a discrete emergency maneuver.

#### Approach and rendezvous
- Approach the tanker **from behind and below**; establish altitude separation before entering the AAR
  track; visual or radar contact required before starting the rendezvous. Progressively reduce closure
  rate — other receivers may be working the same tanker simultaneously.

#### Pre-refueling checklist (emitter/weapon safing — new structured list, not in Chuck)
`MASTER ARM → OFF` · `LASER ARM → OFF` · `CMDS MODE → STBY` · `FCR → STBY` · `ECM PWR → STBY` ·
`TACAN → REC mode` · `RDR ALT → STBY` — **all emitters disabled before reaching Pre-contact position**.
Then: `AIR REFUEL → OPEN` (ED: **3–5 minutes** prior if external tanks fitted, to depressurize them —
Chuck's guide gives **5–6 minutes**; both are the same "several minutes ahead of contact" guidance, not
treated as a hard discrepancy, just slightly different numbers from two tutorial-style sources).
`HOT MIC/CIPHER → HOT MIC`.

#### Contact position — director lights (ED's naming vs. Chuck's "PDI")
ED calls the same tanker-belly light array **"Longitudinal Director Lights"** (right row) and
**"Vertical Director Lights"** (left row) rather than Chuck's "PDI" (Pilot Director Indicator) — same
physical system, different name in the two sources. ED adds a **color** dimension Chuck's guide doesn't
mention: **green = slight correction needed, red = at the limits of boom travel** — this is
*additional* information (color-coded severity) alongside Chuck's flash-state description (steady =
substantial, flashing = small), not a contradiction; a faithful director-light model likely needs both
dimensions (color for severity, flash-state possibly for correction magnitude) if DCS implements both.
Arrow/dash biasing: `D`/`U` (down/up, corrected with stick) and `F`/`A` (forward/aft, corrected with
throttle) — matches Chuck's D/U/A/F scheme exactly.

#### Weight-effect note (new physical detail, not in Chuck)
ED explicitly notes: as the receiver takes on fuel, gross weight increases, the aircraft **"drops away"**
from the tanker (requiring a pitch-attitude correction to hold altitude), which **increases AoA**,
requiring a **throttle increase** to counter the added drag and hold airspeed — a real, continuously-
changing trim condition during the entire refueling contact, not a one-time setup.

#### Breakaway procedure (new — formal emergency maneuver, absent from Chuck's tutorial)
On the call **"Breakaway, breakaway"** from the boom operator (collision-risk closure/distance):
1. Immediately apply **forward stick** to descend away from the tanker.
2. **Retard throttle** to reduce airspeed and gain separation.
3. Deliberate and expeditious, but **not aggressive** — this is a separation maneuver, not an emergency
   dive.
Boom operator clears the receiver back to Pre-contact position before resuming, if refueling continues.

#### Post-refueling checklist
Reverse of pre-refueling: `AIR REFUEL → CLOSE` · `HOT MIC/CIPHER → OFF` · verify fuel quantity/transfer/
balance · confirm all AR Status lights off · restore emitters (`FCR`/`ECM`/`TACAN`/`RDR ALT`) and
`MASTER ARM`/`LASER ARM` as required for the next mission phase.

## State

**Nothing of this file is implemented.** FlightBox has no tanker, no boom, no receptacle, no refuelling
state — and no unit kind that could be one (`FBUnitKind` is Aircraft / Weapon / Ground).

| Item of this reference | FlightBox |
|---|---|
| Fuel quantity as a simulated, changeable state | **built** — `FBFdm::Get/SetFuel*` drives `FGPropulsion` per tank or as a total, distributed proportionally to the model's tank capacities; running dry is JSBSim's own physics ([`../flightbox/sim/fdm.md`](../flightbox/sim/fdm.md) §11) |
| BINGO as a decision quantity | **built** — the warning block carries it, and the intercept pilot reads it before pressing on ([`../flightbox/sim/pilot-ai.md`](../flightbox/sim/pilot-ai.md) §7) |
| Tanker unit, rendezvous, TACAN A/A channel, boom contact, director lights, breakaway | **not implemented** |
| Formation flight on another unit | **not implemented** — the datalink shows other units, no formation law flies against them |

A refuelling round would need three things FlightBox does not have: a tanker unit type, a formation/
station-keeping guidance mode, and a fuel-transfer channel. The fuel channel is the only one that exists.

## Gaps

**Source gaps** (this file vs. its sources)
- The `## Knowledge` section is an explicitly **SHALLOW** research pass (UARRSI/PDI level detail only),
  marked "deepen when in scope".
- Chuck Part 17 (pp.778–791) and ED pp.157–162 are fully processed.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* fuel state and its consequences only.
- *Partially:* nothing.
- *Not at all:* the entire air-to-air refuelling task — tanker, rendezvous, contact, director lights,
  disconnect, breakaway, the pre/post-refuelling emitter checklists.

## Knowledge

**Technical depth (researched — shallow pass — deepen when in scope)**

*Researched engineering depth. Kept separate from the guide distillation in `## Spec`; sources cited at
the end. This pass is explicitly **shallow** — deepen when the subsystem is in scope.*

### Components (LRUs)
- **UARRSI** (Universal Aerial Refueling Receptacle Slipway Installation): the flush **boom receptacle**
  behind the cockpit, with its door and internal latching + signal/slipway lighting.
- **Tanker interface**: the tanker's **flying boom** + **PDI (Pilot Director Indicator) lights**; the
  F-16 is **boom-only** (no probe-and-drogue).

### Functional principle
The receiver flies formation on the tanker and the boom operator flies the telescoping boom into the
UARRSI, which latches it; fuel transfers under tanker pump pressure into the F-16's tanks (external tanks
must be depressurized first). The PDI lights are **directive** (tell the pilot which way to move within the
boom envelope), and disconnect is automatic at limits or manual via the NWS A/R Disc button. Opening the
door forces takeoff/landing FLCS gains and drops the autopilot (`flight-controls-flcs.md`), so refueling is
hand-flown.

### Sources
- USPTO aerial-refueling patents / general references — UARRSI, boom receptacle.
- DCS guide Part 17 (procedure, PDI lights) — cross-referenced above.
- `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — Aerial Refueling chapter p.157–162
  (pre/post-refueling checklists, director-light color coding, weight-effect trim note, breakaway
  procedure).
