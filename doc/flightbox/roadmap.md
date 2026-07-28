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
| R5 | **Cloud rebuild** — the six `FBCloud*` stages are demolition, not a base | [`render/clouds.md`](render/clouds.md) (Spec) | **running** (worktree, parallel to the src refactor) |
| R6 | **The MiG-29 line — owner goal, set 2026-07-28.** The MiG-29 becomes FlightBox's second full jet and F-16 vs MiG-29 the core of the game. Five stages, each measured in the gym loop: **(1)** the JSBSim model along `doc/mig29/flight-model-spec.md`'s 10-step build order, every step accepted against its documented envelope anchor; **(2)** the module with real systems as slot overrides — N019 with the quantified Doppler notch, SPO-15 with its documented failure modes, KOLS IRST as a NEW passive sensor slot (seeing without radiating, but no IFF), R-27R with the SARH support obligation to impact, R-73 as the first IR seeker (which makes flares real and gives the F-16 the AIM-9), GSh-301, GCI guidance as command-bus inputs with latency; **(3)** E2E solo — the MiG flies takeoff/route/landing like the Payerne class, proving the module architecture carries a second jet; **(4)** the asymmetric 1v1 BVR duel with radar cross-section as a unit property — active seeker vs illumination obligation, EMCON vs IRST, decided outcomes at last instead of the mirror stalemate; **(5)** flights 2v2/4v4, for which the formation concept becomes real (flight lead, target sorting, mutual support) and the tournament infrastructure sharpens section tactics evolutionarily. Spec-first, no cheating, every number with provenance. Terrain masking follows once the duel stands. | [`aircraft/mig29.md`](aircraft/mig29.md), [`sim/weapons-and-damage.md`](sim/weapons-and-damage.md), [`sim/sensors.md`](sim/sensors.md) | after phase 3 + flaperon round |
| R7 | One-way drone (Shahed class) + cruise missile — real F-16 tasking, stresses notch and gun; deliberately AFTER the MiG line per owner priority (the R-73's IR seeker makes an AIM-9 drone hunt near-trivial later) | [`aircraft/mig29.md`](aircraft/mig29.md) | after R6 |
| R8 | *(merged into R6 stage 1)* | — | — |
| R9 | **Missions for humans:** a scenario layer over the `.fbm` format | [`vision.md`](vision.md), `doc/mission-format.md` | open |
| R10 | **Translation wave:** `doc/flightbox/` to English (the bodies that today still carry the `> Body still in German` note), then schema alignment of `doc/f16/` and `doc/mig29/` onto the same Spec/State/Gaps/Knowledge form | this file, [`INDEX.md`](INDEX.md) | **done** — except `world-and-terrain.md`, which stays German until its split into `world/` (phase 3 of the mirror refactor); the seven `sim/` files still cite pre-refactor `sim/src` paths, updated in phase 3 |

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
