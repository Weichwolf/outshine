# F-16C Air-to-Air Refueling

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 17 — Air-to-Air Refueling, pp. 778–791.

## Basics
- The F-16C uses a **flying-boom** receptacle (behind the cockpit) — boom refueling only, not probe-and-drogue.
- Boom-capable tanker in DCS: **KC-135** (also KC-135 MPRS = probe-and-drogue via wing pods, not usable by F-16).
- Wake turbulence off the tanker causes wobble in close proximity — fly formation on the tanker, not the boom.

## Procedure

### Tune tanker TACAN + radio
1. Note tanker TACAN channel (e.g. 14X) + UHF freq from briefing.
2. MIDS LVT — ON; adjust TACAN volume.
3. ICP **T-ILS(1)** → TACAN-ILS DED page.
4. Dobber DOWN to CHAN, keypad channel, ENTR; M-SEL(0)+ENTR for band X/Y.
5. Dobber RIGHT (SEQ) to **TCN A/A TR** (air-to-air transmit/receive).
6. Dobber LEFT (RTN) to CNI; EHSI "M" to TACAN mode.
7. C&I selector to UFC; COM1 → tanker UHF (e.g. 251.00 = "25100").
8. Comms Transmit AFT → contact tanker → "Intent to refuel" → tanker gives join instructions.

### Configure aircraft
9. Reduce workload: DED Data switch FWD (DED on HUD); LIST → 2 (BNGO) for fuel monitoring.
10. **Open AIR REFUEL trap door.** (With external tanks: open **5–6 min prior** to let them depressurize.)
11. Confirm **RDY** light on (door open).
12. Master Arm — OFF; RF switch — SILENT.

### Contact
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

### Disconnect
19. Complete when **DISC** warning light illuminates. To leave early: press **NWS A/R Disc** button to
    unlatch the boom.
20. **Close AIR REFUEL trap door** and resume. (Leaving it open blocks external-tank fuel use.)

## Related limits (see `aerodynamics-performance.md`)
- Air-refuel door opening/closing: 400 kts / M0.85. Door open: 400 kts / M0.95.
- Opening the refuel door also sets FLCS gains to takeoff & landing mode and disengages autopilot.

---

# Technical depth (researched — shallow pass — deepen when in scope)

## Components (LRUs)
- **UARRSI** (Universal Aerial Refueling Receptacle Slipway Installation): the flush **boom receptacle**
  behind the cockpit, with its door and internal latching + signal/slipway lighting.
- **Tanker interface**: the tanker's **flying boom** + **PDI (Pilot Director Indicator) lights**; the
  F-16 is **boom-only** (no probe-and-drogue).

## Functional principle
The receiver flies formation on the tanker and the boom operator flies the telescoping boom into the
UARRSI, which latches it; fuel transfers under tanker pump pressure into the F-16's tanks (external tanks
must be depressurized first). The PDI lights are **directive** (tell the pilot which way to move within the
boom envelope), and disconnect is automatic at limits or manual via the NWS A/R Disc button. Opening the
door forces takeoff/landing FLCS gains and drops the autopilot (`flight-controls-flcs.md`), so refueling is
hand-flown.

## Sources
- USPTO aerial-refueling patents / general references — UARRSI, boom receptacle.
- DCS guide Part 17 (procedure, PDI lights) — cross-referenced above.
