# Roadmap

The stages, thin on purpose. **What** each stage must achieve lives in the Spec section of its topic
file; this page only orders them and says where the work is described. Direction and scale:
[`vision.md`](vision.md). What already landed: [`journal.md`](journal.md).

## Stages

| # | Stage | Spec / gaps live in | State |
|---|---|---|---|
| R1 | **Comment reduction** in `sim/src/` — derivations live in this documentation, the code carries one-liners plus a reference (proof: `sim/tools/strip_comments.py` hash unchanged) | [`conventions.md`](conventions.md) | **done**, committed as `f77f1cf` |
| R2 | **Weather, server half:** `/wx` on fb-tiles — NOAA GFS 0.25°, one compact global raster per variable (1440×721) instead of tiles; wind at 10 m and 850/700/500/250 hPa, cover per étage, cloud base, visibility. The format **is** the interface. | [`world/weather.md`](world/weather.md) | running |
| R3 | **MiG-29 knowledge base** `doc/modules/mig29/` after the pattern of `doc/modules/f16/` plus its new `flight-model.md` (§11 checklist as the template): systems, avionics, procedures from the two DCS manuals + research; `weapons.md` (R-27R/T, R-73, R-60M, GSh-301, air-to-ground); `flight-model-spec.md` as the build order with documented envelope anchors | [`modules/mig29/module.md`](modules/mig29/module.md) | running |
| R4 | **Weather, sim half:** `FBWeatherProvider` (constant / fixed gym dataset from the `/wx` fixture, deterministic / live), wind → JSBSim's `FGWinds` (today: no wiring at all), `.fbm` weather declaration, data interface for the cloud rebuild | [`missions/weather.md`](missions/weather.md), [`core.md`](core.md) | after R1 |
| R5 | **Cloud rebuild** — the six `FBCloud*` stages are demolition, not a base | [`render/clouds.md`](render/clouds.md) (Spec) | **running** (worktree, parallel to the src refactor) |
| R6 | **The MiG-29 line — owner goal, set 2026-07-28.** The MiG-29 becomes FlightBox's second full jet and F-16 vs MiG-29 the core of the game. Five stages, each measured in the gym loop: **(1)** the JSBSim model along `doc/modules/mig29/flight-model-spec.md`'s 10-step build order, every step accepted against its documented envelope anchor; **(2)** the module with real systems as slot overrides — N019 with the quantified Doppler notch, SPO-15 with its documented failure modes, KOLS IRST as a NEW passive sensor slot (seeing without radiating, but no IFF), R-27R with the SARH support obligation to impact, R-73 as the first IR seeker (which makes flares real and gives the F-16 the AIM-9), GSh-301, GCI guidance as command-bus inputs with latency; **(3)** E2E solo — the MiG flies takeoff/route/landing like the Payerne class, proving the module architecture carries a second jet; **(4)** the asymmetric 1v1 BVR duel with radar cross-section as a unit property — active seeker vs illumination obligation, EMCON vs IRST, decided outcomes at last instead of the mirror stalemate; **(5)** flights 2v2/4v4, for which the formation concept becomes real (flight lead, target sorting, mutual support) and the tournament infrastructure sharpens section tactics evolutionarily. Spec-first, no cheating, every number with provenance. Terrain masking follows once the duel stands. | [`modules/mig29/module.md`](modules/mig29/module.md), [`weapons.md`](weapons.md), [`sim/sensors.md`](sensors.md) | **stages 1–5 DONE** (`b411b2b`…this round): model built (10 anchors hit/in-band, 4 missed with diagnosis), module with N019/SPO-15/KOLS-IRST/GCI + R-27R/R-73/GSh-301/RCS, E2E solo flies, the asymmetric duel measured — **the launch DOCTRINE decides, not the airframe** — and now the FLIGHT: roles as mission data, station keeping, target sort and a cover rule that is nearly free for the AIM-120 (0.3 s of binding), expensive for the R-27R (17.3 s) and unavailable to the MiG, which has no channel to carry it ([`formation.md`](formation.md)). Terrain masking next. |
| R7 | One-way drone (Shahed class) + cruise missile — real F-16 tasking, stresses notch and gun; deliberately AFTER the MiG line per owner priority (the R-73's IR seeker makes an AIM-9 drone hunt near-trivial later) | [`modules/mig29/module.md`](modules/mig29/module.md) | after R6 |
| R8 | *(merged into R6 stage 1)* | — | — |
| R9 | **Missions for humans:** a scenario layer over the `.fbm` format | [`vision.md`](vision.md), [`missions/INDEX.md`](missions/INDEX.md) | open |
| R11 | **The campaign foundation — owner goal, step 1, set 2026-07-28.** Four contracts the ten campaign specs cannot be flown without, specified before any code: **`C2`** the mission clock (`time`, Zulu, mission-wide, all three clients) · **`C12`** four more objective kinds (`identify`, `protect`, `no_fire`, `deny release`) · **`C3`** visual acquisition as a passive sensor channel inside the perception boundary · **`C0`** the campaign layer (`.fbc`, three carried facts, a campaign fingerprint). Build order: `C2` → `C12` → `C3` (needs `C2`) → `C0`. Acceptance for `C2` and `C12` is the regression gate at full strength — all 84 `sim/missions/*.fbm` byte-identical; for `C3` it is weaker by exactly the appended columns and says so. Step 2 of the same goal is `C1`, the active surface-to-air threat, whose bounded gap entry lives in [`weapons.md`](weapons.md). | [`missions/syntax.md`](missions/syntax.md), [`missions/verdict.md`](missions/verdict.md), [`sensors.md`](sensors.md) §9, [`missions/campaign.md`](missions/campaign.md), [`clients/clients.md`](clients/clients.md) | **specs written** (foundation round, 2026-07-28); nothing built |
| R10 | **Translation wave:** `doc/` to English, then schema alignment of `doc/modules/f16/` and `doc/modules/mig29/` onto the same Spec/State/Gaps/Knowledge form | this file, [`INDEX.md`](INDEX.md) | **done** — the last two German bodies (`mission-format.md`, `world-and-terrain.md`) were translated in the phase-3 split; no German prose remains in `doc/` |

## Parked

Work that has no home file yet, kept here so it cannot be lost:

| Thing | Waits for |
|---|---|
| *(empty)* | The two former entries — DEM cache per worker instance / time-based eviction / `kNodeCeil`, and imagery mode not declarable in `.fbm` / TLS not wired — **got their home file in the phase-3 split** and now live in [`world/terrain.md`](world/terrain.md)'s Gaps (the picture-mode half also in [`missions/syntax.md`](missions/syntax.md)'s Gaps). Nothing is parked. |

## How a stage runs

The working rule is in [`conventions.md`](conventions.md) and is binding: change the **Spec** of the
topic file first, build until **State** meets it, then update State and Gaps and add one line to
[`journal.md`](journal.md). Rejected approaches stay in Gaps with their measurements.
