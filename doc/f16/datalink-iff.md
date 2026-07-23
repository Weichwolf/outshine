# F-16C Datalink & IFF

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 13 — Datalink & IFF, pp. 600–648.

## Datalink — MIDS / Link-16 / TNDL

- **MIDS** (Multifunctional Information Distribution System) = NATO name for the Link-16 communication
  component; carried by the aircraft's MIDS radios over the Link-16 TADIL network.
- DCS F-16 implementation is **TNDL** (Tactical Network Datalink), formerly "L16"/"Link-16" — same
  symbology, expanded network customization.
- TNDL lets F-16s and F/A-18s exchange data on the same network. Surveillance aircraft (E-3 AWACS) appear
  in the datalink without being added as TNDL donors.
- Datalink contacts + markpoints display on HSD and FCR; datalink markpoints stored in steerpoints 71–80
  (see `navigation-ils.md`).
- HSD datalink controls: XMT OFF / **TNDL**; contact filters FR ON (all friendly) / FL ON (flight leaders
  only) / FR OFF. DL/MAP power switches left OFF at startup (no function).
- Config sub-topics in the guide: TNDL properties, MIDS network setup, donors.

## IFF — Identify Friend-or-Foe

Two components:
- **Interrogator**: broadcasts a coded interrogation (pulse frequency).
- **Transponder**: replies with its own coded signal; reply content depends on the selected mode. Own
  interrogator matches interrogation vs reply codes to declare friendly.
- Wrong transponder code → friendlies may fail to identify you.

### Modes (pulse-spacing determined)
| Mode | Type | Description |
|---|---|---|
| 1 | Military | 2-digit 5-bit mission code |
| 2 | Military | 4-digit octal unit code (set on ground for fighters) |
| 3 / A | Civil | 4-digit octal ID code (ATC-assigned, cockpit-set); usually combined with C |
| C | Civil | Pressure altitude; combined with 3/A → "Mode 3 A/C" |
| 4 | Military | 3-pulse encrypted reply (delay from encrypted challenge) — **secure** |
| 5 | Military | Cryptographically secured Mode S + ADS-B GPS position |
| S | Civil | Selective addressing, collision avoidance (TCAS/ACAS II); compatible with A/C SSR |

Key takeaways:
- **Mode 4** is the combat mode: encrypted, undetectable by enemy transponders. A **valid Mode-4 reply
  guarantees friendly** (in DCS); lack of reply does **not** guarantee hostile.
- Modes 1/2/3 are insecure — an opponent can copy your interrogator code, spoof friendly, and your
  transponder replies give away position.
- **Only Mode 4 is simulated** (as of the guide's 2022-01-16 note).

### Controls & setup
- IFF Master switch → **NORM** to power on (startup step 46).
- ICP **IFF** button → IFF DED page shows mode codes; defaults usually preset.
- Tutorial covers Mode-4 interrogation via **SCAN** and **LOS (Line of Sight)** methods.
- IFF IDENT light/button: initiates IFF response to an ATC/interrogation request (see `cockpit-displays.md`).

---

# Technical depth (researched — shallow pass — deepen when in scope)

> Datalink/IFF is outside the current rebuild scope (flight + rendering). LRU/principle stub only.

## Components (LRUs)
- **Datalink**: **MIDS-LVT** (Multifunctional Information Distribution System – Low Volume Terminal) —
  the Link-16 radio; TACAN is hosted in the same terminal (guide).
- **IFF**: **AN/APX-113** combined interrogator/transponder (replaced the AN/APX-101 transponder on USAF
  F-16C/D); supports Modes 1/2/3A/C/4(/5).

## Functional principle
MIDS-LVT is a frequency-hopping, TDMA, jam-resistant Link-16 terminal: each participant transmits in
assigned time slots, so many aircraft share one network picture (positions, tracks, IFF) without a central
node. The APX-113 both interrogates unknown contacts and answers interrogations; Mode 4/5 use crypto so a
valid reply positively identifies a friendly while resisting spoofing (see IFF-modes table above). Both are
LRUs on the 1553 bus; the MMC fuses datalink tracks onto the HSD/FCR.

## Sources
- militaryaerospace.com / forecastinternational.com — AN/APX-111/113 combined interrogator/transponder,
  replaces APX-101.
- Wikipedia *Link-16* / *MIDS* — TDMA/frequency-hopping principle.
- DCS guide Part 13 (TNDL, IFF modes) — cross-referenced above.
