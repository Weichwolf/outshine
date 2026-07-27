# Roadmap

The stages, thin on purpose. **What** each stage must achieve lives in the Spec section of its topic
file; this page only orders them and says where the work is described. Direction and scale:
[`vision.md`](vision.md). What already landed: [`journal.md`](journal.md).

## Stages

| # | Stage | Spec / gaps live in | State |
|---|---|---|---|
| R1 | **Comment reduction** in `sim/src/` — derivations live in this documentation, the code carries one-liners plus a reference (proof: `sim/tools/strip_comments.py` hash unchanged) | [`conventions.md`](conventions.md) | **done**, committed as `f77f1cf` |
| R2 | **Weather, server half:** `/wx` on fb-tiles — NOAA GFS 0.25°, one compact global raster per variable (1440×721) instead of tiles; wind at 10 m and 850/700/500/250 hPa, cover per étage, cloud base, visibility. The format **is** the interface. | `world-and-terrain.md` (being written in the same round) | running |
| R3 | **MiG-29 knowledge base** `doc/mig29/` after the pattern of `doc/f16/` plus its new `flight-model.md` (§11 checklist as the template): systems, avionics, procedures from the two DCS manuals + research; `weapons.md` (R-27R/T, R-73, R-60M, GSh-301, air-to-ground); `flight-model-spec.md` as the build order with documented envelope anchors | [`aircraft/mig29.md`](aircraft/mig29.md) | running |
| R4 | **Weather, sim half:** `FBWeatherProvider` (constant / fixed gym dataset from the `/wx` fixture, deterministic / live), wind → JSBSim's `FGWinds` (today: no wiring at all), `.fbm` weather declaration, data interface for the cloud rebuild | `world-and-terrain.md`, [`sim/core.md`](sim/core.md) | after R1 |
| R5 | **Cloud rebuild** — the six `FBCloud*` stages are demolition, not a base | [`render/clouds.md`](render/clouds.md) (Spec) | after R4 |
| R6 | **Asymmetric weapons + RCS:** enemy missile family (R-73/R-27/R-77 class from the MiG-29 base; R-37M class for the long-range threat), AIM-9X, radar cross-section as a unit property. Reason: the duel stalemate is symmetry, not an AI defect — asymmetry turns the coin toss into a decision | [`sim/weapons-and-damage.md`](sim/weapons-and-damage.md), [`aircraft/mig29.md`](aircraft/mig29.md) | after R3 |
| R7 | **Enemy units** in order of buildability: one-way drone (Shahed class) + cruise missile first (the only ones that can be built properly today; real F-16 tasking; they stress the Doppler notch and the gun; `modules/drone` tests the module architecture), then MiG-29 per the R3 spec / Flanker class — explicitly at BVR scale | [`aircraft/mig29.md`](aircraft/mig29.md) | after R6 |
| R8 | **JSBSim MiG-29 model** along the build order of `doc/mig29/flight-model-spec.md`, every step measured against a documented anchor in the gym; bookkeeping under the `MODEL-DELTAS` discipline | [`aircraft/mig29.md`](aircraft/mig29.md), [`aircraft/stores.md`](aircraft/stores.md) | after R3 |
| R9 | **Missions for humans:** a scenario layer over the `.fbm` format | [`vision.md`](vision.md), `doc/mission-format.md` | open |
| R10 | **Translation wave:** `doc/flightbox/` to English (the bodies that today still carry the `> Body still in German` note), then schema alignment of `doc/f16/` and `doc/mig29/` onto the same Spec/State/Gaps/Knowledge form | this file, [`INDEX.md`](INDEX.md) | open |

## Parked

Work that has no home file yet, kept here so it cannot be lost:

| Thing | Waits for |
|---|---|
| DEM cache lies per worker instance (6× redundancy, unmeasured); eviction purely time-based; `kNodeCeil` silently refuses every split (former `TODO.md` §4.7) | the `world/` split — `world-and-terrain.md` is being rewritten in the R2 round and will be split into `world/terrain.md` + `world/weather.md` afterwards |
| Imagery mode (SVS/EVS) not declarable in `.fbm`; TLS not wired in the tile server (former `TODO.md` §4.8) | same |

## How a stage runs

The working rule is in [`conventions.md`](conventions.md) and is binding: change the **Spec** of the
topic file first, build until **State** meets it, then update State and Gaps and add one line to
[`journal.md`](journal.md). Rejected approaches stay in Gaps with their measurements.
