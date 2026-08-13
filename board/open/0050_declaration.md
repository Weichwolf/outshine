Type: feature
Area: world
Tags: perf

**I.4 Declaration**

- [x] JSON reader in `core` (`core/Json.h`)
- [x] A scenario is a declared world: place, clock, weather, what runs (`mods/*/mod.json`, four of them)
- [ ] JSON schema check of a scenario before it is used, with the failing path named
- [ ] A standpoint the tile scheme cannot carry is **refused by name**, never mapped to a tile — the **declaration** now is, at the earliest possible place and before anything streams (`clients/Scene.cpp:69-78`, verified: `fb_world` at 85.0525 / 85.0530 / 85.0531 / 86.0 N all exit 2 with the latitude and the bound in the message and zero HTTP requests; the 78.2 N control exits 0), and so are the two former `osmmesh_geo_to_tile` call sites, now `TileIndex::Of` with the indices unreachable from the refusal (`world/OsmField.cpp:38`, `world/World.cpp:493`). **The point query is not**: the deleted world entry point still answers 86 N and 89.9 N with `groundResolved=1 groundAslM=-3448.27`, the DEM row at 85.0511 N, because the deleted tile server clamps (the bug tasks in `board/`). The window in which the old code fabricated tile (0,0) rather than refusing for an unrelated reason was **85.051128779807 < lat ≤ 85.053023927135, 211.7 m** — bounded above by `Schedule::Widest` finding no in-grid row at `RadiusRegions = 1`, not by the projection. `CLAUDE.md`'s *every point on Earth is a valid start* is a claim the tile scheme itself does not hold: closing the gap the other way — a polar scheme beside Mercator — is a second line and the owner's call, not this one
- [ ] A gate that fails the build on `getenv` outside `clients/` — six live variables change the picture or disarm a pass, and the layering targets cannot see them
- [ ] `scenarios/` as the decided directory name — the tree still says `mods/`
- [ ] Declared body format: segments, joints, contacts, force sources, medium, model, materials, brain
- [ ] **Scenario: an RC aeroplane circling one position.** The repository's own first flying thing (`539aebd`, the deleted X-Plane bridge): a level 15° bank held about a home point, altitude constant, reverting to return-to-home beyond a declared radius. The smallest complete airborne scenario there is — one body, one propulsion, one control law, no destination — and therefore the cheapest test of flight that is not a camera on rails
- [ ] **Scenario: a take-off and a landing in Switzerland.** Payerne (LSMP) RWY23, threshold 46.84335 / 6.91523 at 441 m, from `payerne-full.fbm`: a ground start with brakes set on runway heading, a climbing turn over the Broye valley and the Jura foothills, a descending leg onto the extended final, then approach, flare and rollout to a full stop. It exercises ground contact, propulsion, a control law, terrain over a real valley and a runway that exists — and its verdict is decidable, because the aircraft either stops on the runway or does not
- [ ] Declared entity catalogue a generator can fill without editing a closed enum
- [ ] Declared entity catalogue a generator can fill without editing a closed enum
- [ ] Declared capability surface an LLM calls into
- [ ] Declared strata list per ground class, with no global default, so an unclassified place grows nothing
- [x] Declared vegetation class table with per-class densities (`assets/world/vegetation.json`)
- [x] Declared ground-material table with sourced albedo and roughness (`assets/world/ground-materials.json`)
- [x] Declared species files, one per species (`assets/world/species/*.json`)
- [ ] Declared material table for built surfaces with a derivation beside every number
- [ ] Declared environment track over the day (keyed tone shoulder, fog lobe, weather transition length only)
- [ ] Declared weather state with a blend interval
- [ ] Epoch index (three) threaded to every material, vegetation density, building state and road surface
- [ ] Decay index (three) on the same path
- [ ] Epoch and decay as a selection, never an interpolation
- [ ] Epoch and decay reaching geometry or identity — REFUSED: the same dataset must stay the same dataset or the claim is untestable
- [ ] A scripting language for mechanics — REFUSED: function calling over a declared surface, or nothing
- [ ] Quality levels or graphics presets — REFUSED: there is one version during basic development
