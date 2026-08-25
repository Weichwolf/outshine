Type: feature
State: open
Parent: 1573
Area: apps
Tags: driver, acceptance, product

# The driver drives at the bar, and the architect signs it off

`apps/driver` is outshine's ONE integration test and simultaneously its product. The library's
other suites are unit tests, each asserting something that CAN be trivially true. Emergence is
judged HERE, on the picture, by the hourly architect — from what it SAW through
`test/run.sh --drive`, never from reading the implementation.

**The day the driver is a driving simulation at Gran Turismo 7's level in an OSM world and the
architect accepts it, outshine's integration test has passed.**

## The ledger — rewritten each round from what was SEEN (2026-08-25 08:17, HEAD 1af2c00b)

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs, and does the gate build it? | **program yes, gate NO** | `apps/driver/src/main.cpp` exists and `Programs()` declares it; the gate itself ran zero cases at this HEAD (board:1869) |
| did the drive leave its stills? | **NO — zero** | `run.sh: 0 still(s) in .../outshine-drive.xTDvbz`; the route refuses and the frame never renders (board:1870) |
| do consecutive stills DIFFER — does the thing move? | **NO** | the last HEAD that produced stills (a9a96a0c) wrote 14 840 of them, every one hashing to `d2cd33750477d24f965adc5340f28f8a` |
| is there ground under the car? | **NO** | the picture is a car on white at `fill="0.9"` |
| a horizon behind it? | **NO** | same |
| a sky above it? | **NO** | `SkyStage` is green in CURRENT and the driver's frame never reaches it |
| is the car lit — does it cast a shadow? | **lit, no shadow, no surface** | the key light at `elevationDeg="42"` shades the body; nothing receives |
| does it sit on a surface or float? | it floats — there is no surface | |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **declared, not reached** | `Forest`, `Buildings`, `Water`, `Infrastructure` are stranded off `Sim` (board:1805) |
| what does the picture do at one kilometre it does not do at another? | **nothing — it is one image** | 14 840 identical files |

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. a frame at all when a declaration refuses (board:1870)
2. ground, sky and horizon under the car — the world composition path with a consumer (board:1805)
3. a corridor whose tile ring joins, so the route exists to be driven (board:1862)
4. ten stills spaced by DISTANCE, that differ (board:1862)
5. the car's shadow on the surface it stands on (board:1575)
6. road furniture: markings, guard rails, the second carriageway
7. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
   place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version — a picture that agrees only with itself measures nothing.
