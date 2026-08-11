# Requirements — the scope ledger

**What this file is.** The whole surface Outshine has to carry, one line per feature, each with a box.
It answers *how much is there*. `doc/todo.md` answers *what next* and carries the acceptance numbers; a
line here may correspond to a todo entry and that is fine.

**It exists by the owner's decision**, which overrides `CLAUDE.md`'s "`doc/` holds three files and gets
no fourth". The override is recorded, not assumed.

**How a line is read.**

| Mark | Meaning |
|---|---|
| `- [ ]` | not built |
| `- [x]` | built — checked in the tree in this round, not remembered |
| `NO SUBSTITUTE` | the reference gets it from an authored asset and no function replaces it. Naming it is worth more than padding the count |
| `REFUSED` | the reference has it and we will not copy it; the reason stands in the same line |
| `TILE` | blocked on data the served vector schema does not carry — a tile-server change, with its cost |
| `TOOL` | the number is missing because the instrument is missing. Effort, not a limit |
| `UNSURE` | I could not confirm the reference has it, and say so rather than assert |

**Order is the argument.** Five bands — engine, world, vegetation, buildings, vehicles — and inside a
band a line stands after the line it depends on. Nothing is sorted by importance; importance is
`todo.md`'s job.

**References.** Kingdom Come: Deliverance for terrain, vegetation and light — a temperate
central-European vegetation picture on a known budget. GTA 5 for the built world and the verbs, taken
for its **range and construction** and never for its era or its city. The setting is post-scarcity:
modern infrastructure, lush nature.

**Botanical scope is central-European temperate**, because the acceptance places are on the Weser
(Hameln / Emmerthal / Grohnde). A form belonging to another biome says so on its own line.

**Sources for the enumerations.** CryEngine V manual (vegetation, merged meshes, touch bending, road
tool, river tool, water volume, time of day, environment editor, fog volumes, decals) · Warhorse's
published KCD material and console variables (`e_vegetationUseTerrainColorDistance`,
`e_UberlodDistanceRatio`) · Shortbread vector tile schema 1.0 as served by tiles.versatiles.org, which
`tiles/src/tilesrc.c` fetches · RAGE `handling.meta` field groups for vehicle construction · GTA V's 23
vehicle classes · Destatis field-crop acreages for which crops a German field actually is · the
Galio odorati-Fagetum and Arrhenatheretum elatioris species lists for the herb and grass layers.

---

## Band I — Engine

### I.1 The machine and the process

- [x] wasm32 + WebGPU as the fixed target — one feature set, no vendor extensions
- [x] Native translation as frame oracle (`make walk`) and browser translation (`make wasm`) from one source list
- [x] One object owns world and renderer and is the only thing that builds a scene (`clients/Outshine`)
- [x] A client is `main()` plus an output medium and nothing else
- [x] Server target that links no `render/` and needs no device (`make world`, `clients/WorldMain.cpp`)
- [x] Layering enforced by targets that stop building, never by a checker (`verify-generators`, `verify-world`, `verify-clients`, `verify-types`)
- [x] `core/` is I/O-free by directory; `generators/` cannot spell renderer, world or log
- [x] Declared internal render resolution 1280×720; the canvas only scales it
- [ ] Aspect-preserving letterboxing on a canvas of another shape — declared in `architecture.md`, not found in `PresentStage`
- [x] Bring-up phases as an enumeration rather than booleans
- [ ] Fallible asynchronous bring-up completed outside a constructor, everywhere (`C.41`) — partially held, not audited
- [ ] Frame loop that survives a device loss and re-creates the swap chain
- [ ] Pause / resume without the world losing residency

### I.2 Memory

- [x] Fixed heap, forced by the graphics API refusing a resizable buffer as an argument source
- [x] Heap probe reporting bytes (`core/io/HeapProbe`)
- [x] Stack probe per thread (`core/io/StackProbe`)
- [ ] Every pool reports its bytes — partial: `BuildingField`, `WaterField`, `OsmField` report and nothing acts
- [ ] Byte budget on the streamer, with eviction against it rather than entry counts
- [ ] Eviction path for building prints and verts, water surfaces, courses and levels — monotone growth today is a maximum walk length
- [ ] A failed allocation on an elastic path evicts, retries once, then refuses that piece of world
- [ ] A failed allocation anywhere else aborts loudly, naming the item and the bytes
- [ ] Toolchain's silent-null allocation behaviour turned off, so null checks are not dead code that looks like handling
- [x] Device-resident picture data with a handle and a time-to-live on the processor, never a second copy
- [ ] Device memory accounting probe — whether processor and device allocations charge one budget is unverified on the ceiling machine
- [ ] Per-thread stack sizes set per purpose rather than one default for a network thread and a mesher

### I.3 Threads and work

- [x] Worker pool where a pthread is a Web Worker, so a synchronous fetch blocks nothing (`world/TilePool`)
- [x] Fetch, decode, mesh and cluster-DAG build off the frame thread
- [ ] Every long-lived thread created at bring-up before the frame loop, with runtime creation a hard failure
- [ ] Thread count taken from what the runtime reports, never from the developer machine
- [ ] Dedicated non-computing threads sized by the protocol's connection limit per origin
- [x] Request-level timeout; no timeout on the load as a whole
- [ ] Audio worklet thread that neither blocks nor allocates in its callback

### I.4 Declaration

- [x] JSON reader in `core` (`core/Json.h`)
- [x] A scenario is a declared world: place, clock, weather, what runs (`mods/*/mod.json`, four of them)
- [ ] JSON schema check of a scenario before it is used, with the failing path named
- [ ] `scenarios/` as the decided directory name — the tree still says `mods/`
- [ ] Declared body format: segments, joints, contacts, force sources, medium, model, materials, brain
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

### I.5 Numbers, units, coordinates

- [x] `float64` ECEF as the truth, `float32` camera-relative, one late conversion (`core/Geodesy.h`)
- [x] Geodetic ↔ ECEF, tile addressing, slippy scheme
- [x] Metres as the only length unit at an interface (`core/Units.h`)
- [x] `uv` in metres, never 0..1, for every mesh that carries one
- [x] Ephemeris for sun and moon at true altitude and azimuth
- [x] Civil time with a declared instant per scene
- [ ] Calendar with a day-of-year that anything seasonal reads
- [x] Keyframe evaluator that knows none of its consumers (`core/Keyframes.h`)
- [x] Determinism: seed derived from the region key, so placement is a property of place (`gate/SameRegionSamePlacement.cpp`)
- [ ] Determinism across tile arrival order — a pinned binary does not reproduce its still today
- [ ] `FB_TAU` read from the environment removed — the picture must not depend on an undeclared variable

### I.6 Streaming and loading

- [x] Nothing preloaded; every tile on demand; every point on Earth a valid start
- [x] Loading as an application phase with a progress fraction, never a renderer state (`ProgressStage`)
- [x] The renderer runs at full rate during loading
- [ ] Upload per frame as a declared byte budget
- [ ] A tile becomes visible only when complete — verified for terrain, unverified for building and water fields
- [ ] No hitch on stream-in, proven on a moving capture rather than asserted
- [x] No ceiling and no timeout on the initial load

### I.7 Spatial index and level of detail

- [x] One quadtree over the sphere with a vertical extent per node; it answers *where* and owns nothing
- [ ] Vertical split only where content demands it
- [ ] Tile centre at the node's real ECEF origin everywhere — `World::Center` still puts it at `alt = 0`, so a pedestrian at altitude gets a coarser tile under his feet
- [ ] Split metric whose focal length is the projection's, and which is distance-free under an orthographic camera
- [x] One screen-space-error ladder for every instanced model (`core/ClusterDag.h`, `core/ModelLadder.h`)
- [x] Cluster DAG with model-space error per level
- [x] Impostor rung above the mesh levels, its error anchored on the atlas cell texel
- [ ] Measured screen-space error: render the chosen cut against the finest and difference the silhouette — TOOL, two renders and a difference
- [ ] A stand appears in exactly one rank per frame, counted exactly rather than statistically
- [ ] Hysteresis on a rank switch, a minimum observer movement before anything updates, and a per-frame update budget

### I.8 Geometry contract

- [x] Core-defined vertex layout `pos3 @0 · uv2 @12 · nrm3 @20`, 32 B, `static_assert`ed
- [x] Declared second layout `pos3 · nrm3`, 24 B, for the water surface
- [x] Prototype plus instances, never geometry per instance
- [x] Positions as ECEF offsets from a declared anchor
- [x] Crack-freedom within a generator's own soup
- [ ] Winding declared once at registration instead of hard-coded at seven sites
- [ ] Mesh invariant check: unit normals, sign agreement with winding, angle agreement with the geometric normal
- [ ] Mesh invariant check: welding, with a split vertex legitimate only where a seam is declared
- [ ] Mesh invariant check: closure, as a declared property of the yield
- [ ] Mesh invariant check: degeneracy — zero area, NaN, index past the end, winding flip within a surface

### I.9 Generator contract

- [x] A generator is a pure `const` function `(Region, Ground) -> Yield` (`generators/Generator.h`)
- [x] `Ground` carries height, slope, class, edge distance, runner-up, source feature and ring, water level, declared tables
- [x] `Ground` carries no camera, frustum, frame index, clock, LOD level, renderer, device, sun or weather — unspellable, not forbidden
- [x] Three products: occupancy, draw, point query
- [x] Occupancy carries bounds, substitute contact body, mass, contact material — never triangles, material or kind
- [x] Draw carries clusters with model-space error, instances, material row — never bounds or mass
- [x] Region pool and schedule, N concurrent regions without a lock
- [x] The engine knows only physics: a trunk is a cylinder; no content taxonomy exists in it
- [x] A generator runs continuously per region, not once at load
- [ ] Actor spawner sharing the region key and handing seed to an entity store — actors are not generators
- [ ] `DrawSink` truncation reported rather than silent (`ForestDraw.cpp:18`)
- [ ] `RegionPool::Extent::Reached` read by the thing whose budget it claims to bound

### I.10 Render frame

- [x] Forward scene pass; no G-buffer
- [x] As few passes as possible: a pass must beat its own base price of 0.35–0.5 ms before it exists
- [x] A generator's material is a row of numbers with no field that can switch pipeline state
- [x] Core derives discard, two-sided lighting, transmission, blending and emission from what the generator declares
- [ ] Blended transparency ordered back to front inside the scene pass, with a declared budget of blended clusters
- [x] No pipeline creation while playing
- [ ] A title's own entity shader compiled during loading
- [x] Shadow pass, ambient-occlusion pass, exposure pass, temporal pass, present pass
- [ ] The tone-mapping slot in the pass enumeration is empty since the fold — a dead slot is where a pass hides
- [ ] `GpuTimer::Pass::Cloud` is a dead slot

### I.11 Instruments

- [x] Frame telemetry as a time series with scenario, scene, wasm hash and browser version on every line
- [x] Per-pass GPU timestamp pairs — and the published statement that they must not be summed
- [ ] `gpuFrameMs`, one pair spanning the whole encoder, so `Σpass / gpuFrameMs` says whether attribution is even allowed — TOOL, two query slots
- [ ] `frameMs − Σ(spans)` published as its own column, so "unattributed" is measured rather than subtracted by hand
- [ ] Per-pass telemetry published as a distribution instead of a mean (`FrameTelemetry.cpp:66-72`)
- [ ] Overdraw: fragments shaded per output pixel — TOOL, and it is where a forest actually costs
- [ ] Triangle size distribution in projected pixels, p50/p95 — TOOL
- [ ] Culling yield per stage: submitted against visible — TOOL
- [ ] Early rejection count — TOOL
- [x] Memory telemetry per pool
- [x] Stream telemetry: fetch, decode, upload, residency, evictions
- [x] Run identity on every line
- [x] Readback of colour and depth, PNG writer, artefacts posted to `fb-sim`
- [x] `SceneRunner` executing a declared `runs` block natively and writing still and depth
- [ ] The same `runs` block executed by the wasm client, returning still and depth over HTTP — TOOL, a readback and a POST; the sink already exists
- [ ] Cross-client picture comparison on sky/not-sky coverage against a mask frozen on one side, with the self-noise floor published first — TOOL
- [ ] Randomised order within a measurement block — the counterbalanced ABBA design aliases curved drift into the treatment at unity gain and is not an instrument
- [ ] Frame-index-matched comparison on a declared path, instead of a run's p50 as the statistic
- [x] Bench as a layer over the system, never a mode inside it (`WalkBench`, `SubjectBench`, `TreeBench`)
- [ ] `verify-types`' negative gate asserting *why* it fails — any compile error passes it today

### I.12 Physics — one system for walking, driving, flying, swimming

- [x] Substitute contact body as a cylinder with radius, height, mass, contact material (`generators/Body.h`)
- [x] Occupancy claimed through a sink, so a proposal and a placement are one type
- [ ] Rigid-body state: position, orientation, linear and angular velocity, inertia tensor
- [ ] Integrator with a fixed timestep and an interpolated render pose
- [ ] Broad phase over the one spatial index, never a second index
- [ ] Narrow phase: sphere, capsule, box, convex hull, triangle soup (Ericson, ch. 4–5)
- [ ] Contact manifold generation and persistent contact caching
- [ ] Contact solver: restitution, friction with a declared material pair table
- [ ] Joints: hinge, ball, slider, fixed, motorised, with limits
- [ ] Force sources as a declared list, so a wheel, a propeller and a muscle are the same kind of thing
- [ ] Medium: air and water with density, and a body that knows which it is in
- [ ] Buoyancy from displaced volume against the core's water level
- [ ] Aerodynamic and hydrodynamic coefficients per body, not a table lookup
- [ ] Character controller: capsule, gravity, step height, slope limit, ground snap
- [ ] Ragdoll transition from a driven body
- [ ] Sleeping and islanding, so a parked world costs nothing
- [ ] Deterministic solver ordering, because pace deciding the result is a bug (principle 7)
- [ ] Terrain collision against the drawn surface, not against a second heightfield
- [ ] Building collision — `Buildings` deliberately claims no occupancy today, because a cylinder cuts a terrace's neighbours

### I.13 Actors, brains, sensors

- [ ] Entity store with a stable identity, spawned from a region seed
- [ ] A brain that is handed a sensor view and has no name for the world — a type, not a rule
- [ ] Sensor channels: visual contact carrying a TYPE only once angular size gives it away, never a distance or an identity
- [ ] Acoustic sensor
- [ ] A system whose only mutating verb takes a force
- [ ] Whatever builds a prompt cannot read the entity registry — the concrete leak to close
- [ ] LLM function calling over the declared capability surface
- [ ] Regulator brains for the cheap classes
- [ ] Brains only where they are looked at; knowledge never observer-dependent
- [ ] Goals and inner state that survive the world being left and re-entered
- [ ] Animation driven by locomotion rather than leading it, for a human
- [ ] Crowd: pedestrians on the pavement network
- [ ] Fauna: birds, insects, deer, livestock in a field — the reference's meadow has movement in it

### I.14 Input, camera, verbs

- [x] Free camera with a declared stance, eye riding the DEM (`Sim::Look`)
- [x] Orthographic camera for a bird's eye (`demo/ortho`)
- [ ] `Sim &Simulation()`'s non-const overload dropped — it makes moving the eye without the camera basis spellable
- [ ] Walk, with the character controller under it
- [ ] Run, crouch, jump, climb, vault
- [ ] Swim, with the medium under it
- [ ] Drive, fly, ride — one physics system, three propulsion declarations
- [ ] First and third person
- [ ] Footstep response to the contact material under the foot
- [ ] Interaction: open, carry, use, sit
- [ ] Input rebinding as a declaration

### I.15 Audio

- [ ] Audio worklet with a handed stack, no allocation in its callback
- [ ] Positional mixing with distance attenuation and occlusion
- [ ] Wind in a canopy as a function of the declared wind field, not a loop
- [ ] Water at a weir, rain on a surface, footfall by material
- [ ] Vehicle engine as a function of load and revolutions
- [ ] Reverb from enclosure — depends on whatever answers enclosure for the picture (band II, occlusion)

### I.16 The tile server

- [x] `fb-tiles` serving DEM, OSM vectors, imagery, weather and stars over HTTP, and nothing else
- [x] `fb-sim` hosting `web/` and collecting log and telemetry, so a run is reconstructible
- [x] Terrarium DEM tiles with a shared decoder used by both the client and `tiles/`
- [x] Shortbread vector tiles from tiles.versatiles.org
- [x] Aerial imagery tiles served (`tiles/src/tilemap.c`, Esri World Imagery)
- [ ] Imagery consumed by the engine — served and cached, nothing reads it
- [x] GRIB2 weather ingest (`tiles/src/grib2.c`)
- [x] Star catalogue served
- [x] Peaks endpoint
- [ ] `pois` layer fetched — five layers are fetched today; POIs carry amenity, shop, tourism, man_made, name and housenumber and nothing uses them
- [ ] `addresses` and label layers fetched
- [ ] `boundaries` layer fetched
- [ ] Zoom above 14 for terrain — `/t/terrain/15/…` returns non-PNG, so z14 may be the finest served; unresolved

---

## Band II — World

### II.1 Elevation and terrain

- [x] DEM tile fetch, decode and stitch
- [x] Terrain mesh per quadtree node, LOD by screen-space error
- [x] Height at a point on the CPU with no device present
- [x] The height oracle answers on the *drawn* surface, so physics and picture cannot disagree
- [x] `GroundSample` as a tri-state return type — Resolved, Pending, Hole — so a caller cannot place on a sentinel
- [ ] Slope and aspect published as first-class ground quantities everywhere they are used
- [ ] Curvature, for a convex ridge to read differently from a hollow
- [ ] Vertical accuracy of the source stated per place — the chain is faithful; Badwater is 10.9 m off on flat ground and that is the DEM's error
- [ ] Hydro-flattening: a lake polygon carved to a constant elevation at or just below the surrounding terrain
- [ ] A river polygon carrying a monotone downstream gradient, as the engine already enforces for water lines
- [ ] Terrain carved under a road so the carriageway does not ride a raw DEM ripple
- [ ] Terrain carved for a building pad, so a house does not float or bury
- [ ] Cliff and overhang — a heightfield cannot carry one; a declared vertical face is the substitute
- [ ] Cave and tunnel volume — REFUSED as terrain, owed to a declared mesh volume instead
- [ ] Erosion as a function over the DEM — Ebert/Musgrave et al. ch. on terrain; the reference paints this by hand and we cannot

### II.2 Classification

- [x] Class grid from OSM vectors, arbitrated in a declared order (`world/ClassBuilder`)
- [x] Edge distance to the nearest boundary of the winning class
- [x] Runner-up class at a point, so a boundary knows what it blends towards
- [x] Class as a state, not a default: `no row` where OSM has no datum (`generators/Cover`)
- [x] Unmapped substrate that is drawn and grows nothing — the retired global `meadow` default is now unspellable
- [x] Twelve declared land templates plus the unmapped substrate row
- [x] Way width per street kind, 1.5 m path to 45 m
- [x] One predicate, two evaluators: the edge test a fragment runs is the edge test a CPU query runs
- [x] Three tiers over the vectors: AABB on the CPU, source polygon on the CPU, refinement on the GPU one-way
- [ ] Runner-up and edge distance consumed for a height-driven layer blend — available, nothing reads them for this
- [ ] Per-place default where OSM is silent, which needs a climate model this engine does not have
- [ ] OSM layer names spelled once rather than in three files

### II.3 Ground materials and surface

- [x] Seventeen ground materials with linear albedo whose chromaticity is sourced (Munsell renotation, ECOSTRESS spectra) and whose luminance is locked to a broadband value
- [x] Roughness, specular scale, grain size, height amplitude, coarse and fine detail scale per material
- [x] Litter class per material, overridable per template — beech litter under spruce is a defect the botanist calls
- [x] Litter coverage, contrast, edge reach, constructed-edge flag
- [x] Slope maximum per material, so a class cannot sit on a wall
- [x] Sward closure folding the grass colour into the terrain albedo beyond the blade fade
- [x] Alpine limit: a rock template selected by slope band and elevation
- [ ] High-frequency detail as a noise function, explicitly greyscale, cut at a declared range — the reference's rule, and the only legal form a detail map takes here
- [ ] Height-driven blend between classes so pebbles poke through dirt instead of cross-dissolving
- [ ] Class-boundary mixing width measured in pixels at the comparison rung
- [ ] Near-ground luminance variance off the floor
- [ ] Wetness as a material state — darkening, specular rise, puddles in depressions
- [ ] Snow cover as a material state with a slope and aspect mask
- [ ] Frost, ice, mud, ruts, trampled paths
- [ ] Tracks and desire lines where things walk repeatedly
- [ ] White limestone and rock patches reading as snow at 36 N in August — a tonal defect in the existing table
- [ ] Deferred decals for local dressing — REFUSED in the reference's form (authored textures); the procedural substitute is a material row plus a noise function

### II.4 Water

- [x] Water polygons with a level per ring (`world/WaterField`)
- [x] Water surface tessellated at level + 0.15 m over a declared 24 B layout
- [x] Water lines with a monotone downstream gradient
- [x] Water depth at a point as a type that cannot be negative (`core/WaterDepth.h`)
- [x] Depth derived analytically from water level minus ground height — no blended fragment, no separate pass
- [ ] Level model that does not put nine of nine outlines under their own ground — the fifth percentile of a ring under 22 points *is* the minimum
- [ ] Body colour by depth with a declared extinction per wavelength
- [ ] Surface normal perturbation from a wind-driven wave function
- [ ] Fresnel reflection of the sky LUT
- [ ] Reflection of the shore — the reference uses a screen-space term; UNSURE which
- [ ] Refraction of the bed at shallow depth
- [ ] Caustics — the reference ships water-volume caustics from an authored texture; NO SUBSTITUTE is false here, a function reaches it, but nothing is built
- [ ] Foam at a shore line, driven by depth and slope
- [ ] Foam and turbulence at a weir or a rapid
- [ ] Flow direction and speed on a watercourse, visible in the surface
- [ ] Waterfall — the reference's river tool cannot make one either, and says so
- [ ] Shoreline wetting band, darker than the dry bank
- [ ] Floating debris, leaves, ice
- [ ] Ocean with a swell spectrum — out of scope for the acceptance place, named so it is not an oversight
- [ ] Boats displacing water and leaving a wake (band V depends on this)
- [ ] Rain rings on a still surface
- [ ] Underwater view: extinction, god rays, surface from below

### II.5 Atmosphere and sky

- [x] Bruneton transmittance LUT (`TransmittanceStage`)
- [x] Multiple-scattering LUT (`MultiScatterStage`)
- [x] Sky-view LUT (`SkyViewStage`)
- [x] Sky draw from the LUTs (`SkyStage`)
- [x] Irradiance readback that is the scale for everything lit (`IrradianceStage`)
- [x] Aerial perspective / haze along the view ray, Koschmieder-derived (`AtmoHaze.h`)
- [x] Sun disc with limb (`SunStage`)
- [x] Moon as a lit sphere with a phase, over the NASA LROC albedo — measured data that is a raster by nature, principle 2 admissible
- [x] Stars at true altitude and azimuth from the HYG catalogue, magnitude-sorted, with B−V colour
- [ ] Star magnitudes that do not clip at the display white — `maxY ≈ 1.0` on every night frame
- [ ] Airglow and zodiacal light
- [ ] Milky Way band — needs a source that is measured raster rather than authored; UNSURE whether HYG suffices
- [ ] Moon glow and its halo around the disc
- [ ] Horizon lift at night
- [ ] Mesopic response, so a night is not a dark day
- [ ] Ozone absorption band separated in the model — UNSURE whether the current Bruneton parameterisation carries it
- [ ] Rainbow, halo, sun dog — the reference has none of these either
- [ ] Crepuscular rays through a cloud break
- [ ] Volumetric fog with shadowing (`e_VolumetricFog` + `r_FogShadows` is the reference's; ours must fit inside a stage that already reads the HDR target or it does not get built)
- [ ] Ground fog in a valley at dawn, driven by the terrain's own hollows
- [ ] Fog volumes as declared local shapes — the reference's boxes and ellipsoids; ours would be a function of place instead

### II.6 Clouds

- [x] Cloud density as one function with two evaluators, C++ and a WGSL transliteration whose constants are emitted from the same place
- [x] Per-deck separable model: wind-advected 2-D coverage FBM × an analytic vertical profile − 3-D erosion
- [ ] Anything that draws a cloud — `Renderer::CreateClouds()` is an empty function and there is no cloud stage
- [ ] Cloud shadow on the ground
- [ ] Cloud lighting: forward scattering, silver lining, powder term
- [ ] Cloud base from the weather ceiling rather than a constant
- [ ] Three decks — low, mid, high — driven by the four GFS cover diagnostics that the provider already carries
- [ ] Cirrus fibres sheared along the wind (the constants exist; nothing draws them)
- [ ] Contrails
- [ ] Storm cell with anvil
- [ ] Cloud advection consistent with the declared wind, so a shadow moves at the right speed

### II.7 Weather

- [x] Weather provider as an injected seam, with a data-local default and a live implementation
- [x] Wind as the air mass's own NED velocity at altitude, interpolated over pressure levels
- [x] Cloud cover per deck plus a ceiling that can legitimately be absent
- [x] Visibility, with an "unlimited" value outside the format's own window
- [ ] Precipitation: rain intensity, snow, sleet, hail
- [ ] Rain as particles or as a screen-space function — the reference uses particles; ours is undecided
- [ ] Wet-surface response coupled to precipitation history rather than to the current rate
- [ ] Puddles filling and drying
- [ ] Wind gusts as a time series rather than a constant
- [ ] Temperature and humidity fields, because snow line and fog need them
- [ ] Lightning as a light source
- [ ] Weather state blending over a declared interval, reproducibly
- [ ] Weather preset picked every four hours — REFUSED in that form: keying the sky's radiance would make us less physical than we already are. Only the tone shoulder, the fog lobe and the transition length are keyable

### II.8 Light, shadow, occlusion

- [x] Sun as the directional source, its radiance from the atmosphere model
- [x] Sky as an area source through the irradiance LUT
- [x] Four cascaded shadow maps
- [x] Screen-space ambient occlusion at a 0.9 m radius, half resolution
- [x] One lighting model spliced into every lit surface (`SurfaceLight.h`), so a second one cannot appear
- [x] Auto exposure from measured irradiance, with gain and white point read by the tone chain
- [x] ACES-Narkowicz tone curve with no free parameter
- [x] Temporal antialiasing with a Halton(2,3) jitter
- [ ] Nothing in the frame occludes between 1 m and 20 m — the whole of a tree. Cascade 3 is 1.2 m per texel and SSAO reaches 1 m
- [ ] Coarse world-space sky visibility over the cluster DAG's own bounds, per vertex — the cheap candidate, no new pass
- [ ] Voxel cone tracing in the AO pass's existing slot, only if the cheap candidate demonstrably cannot produce the term
- [ ] Sky visibility at 1.5 m under a closed canopy inside the band an LAI of 4.5–5.1 implies
- [ ] Ambient specular in an enclosed place — NO SUBSTITUTE: under a canopy, in a gorge, indoors, the reference hand-places a baked probe, which is measured appearance of an authored scene and principle 2 forbids it. In the open the sky LUT is the correct substitute and is better founded
- [ ] Baked environment probes — REFUSED, principle 2
- [ ] Baked lightmaps — REFUSED, same
- [ ] Point and spot lights as a list the core lights from
- [ ] Emissive surfaces contributing to that list — `Material` has the field and `SurfaceState::Emits()` derives from it; nothing emits
- [ ] Shadow from a point light
- [ ] Contact hardening on a shadow
- [ ] Shadow proxy: a cheap single-material representation per caster — free, because the LOD ladder already produces one
- [ ] CPU coverage-buffer occlusion culling with authored occluder meshes — REFUSED: authored *and* CPU-bound, the wrong direction on wasm32
- [ ] GPU occlusion culling against the depth of the previous frame
- [ ] Vegetation tinted toward the ground class colour with range — the single mechanism that makes a distant foliage field read as one mass; the reference ships it at 50…80 m

### II.9 Night

- [ ] It is not a night today: ground lit by a constant display crutch in `SurfaceLight.h`, `skyRGB = 0,0,0`, trunks bright grey, road legible, sky pure black
- [ ] Moon as a light source with a phase-dependent illuminance
- [ ] Moon shadow
- [ ] Night sky radiance that is not a constant
- [ ] Street lamp emission on placed geometry — no new pass needed
- [ ] Window light with a plausible duty cycle per building
- [ ] Vehicle lights (band V)
- [ ] Skyglow on a cloud base over a settlement
- [ ] Aviation warning lights on masts and turbines

### II.10 Season, and what changes with it

- [ ] Day-of-year reaching anything at all — no `season` in the tree
- [ ] Leaf-on / leaf-off state per species with its own phenology
- [ ] Autumn colour per species, with the sequence right (ash early and dull, beech copper, larch late gold)
- [ ] Leaf fall and a litter layer that thickens
- [ ] Bare-crown silhouette with branch structure legible — the crown geometry already exists, so this is cheap
- [ ] Spring flush with a lighter, yellower leaf
- [ ] Snow lying on branches, roofs and the ground with a slope mask
- [ ] Crop calendar: sown, green, eared, ripe, harvested, stubble, ploughed
- [ ] Meadow cut state: standing, mown, windrowed, baled, regrown
- [ ] Grass senescence — the dry fraction exists per template and is not driven by a season
- [ ] Water level and flow varying by season
- [ ] Ice on a pond

---

## Band III — Vegetation

*The reference is a vegetation picture: canopy plus undergrowth plus grass, superposed from one
declared preset, plus mushrooms and herbs. Ours is one stem class and a stands-per-m².*

**Form before species, and the split is the band's whole argument.** A **growth form** is a shape the
generator must be able to make at all; a **species** is a declaration carried by a form. A species line
is cheap once its form exists and impossible before it, so forms stand first and every species section
below names the form it rides. Where a species needs a form nothing else uses, the line says so — that
is the expensive kind.

**Measured state, and it is smaller than a species count suggests:** sixteen species files exist and
**all sixteen are the same growth form**, a single-stemmed forest tree. The generator cannot currently
produce a different *shape*, only a different tree of one shape.

### III.1 Placement machinery

- [x] Stands per square metre per ground class (`trees.perM2`, twelve templates)
- [x] Species mix per class as declared weights (seven species in mixed broadleaf, four in conifer)
- [x] One cell of the region's own lattice proposes one stand, so a border is exact and no stand can double
- [x] Every refusal has a name, so the counts partition the region and a missing tree is attributable
- [x] Height drawn per stand with the species' own sigma, triangular
- [x] Yaw per stand, uniform over the circle
- [x] Alpine limit refusing a stand above the rock band
- [ ] Placement per **stratum** rather than one scatter — canopy, understorey, field layer, ground layer
- [ ] Strata declared per class with no global default, so an unclassified place grows nothing
- [ ] Slope and aspect biasing the mix — a north-facing scree does not carry a beech stand
- [ ] Soil moisture proxy from distance to water, so alder and willow sit where they belong
- [ ] Clumping rather than a Poisson scatter — a natural stand is patchy and a uniform scatter reads as a plantation
- [ ] Gap dynamics: a clearing, a windthrow patch, a young cohort
- [ ] Age structure within a stand rather than one draw per stand
- [ ] Forest edge: a shrub mantle and a herb seam, denser and lower than the interior
- [ ] Hedgerow placement along a field boundary or a way
- [ ] Avenue placement along a street centreline
- [ ] Riparian gallery along a watercourse — the `riverbank` template exists and no OSM rule selects it
- [ ] The `conifer_forest` template exists and no OSM rule selects it — the served schema has one `forest` kind and cannot tell the two apart. TILE, or an inference from elevation and region
- [ ] Vegetation cleared under a power line right-of-way
- [ ] Vegetation refused on a road, a rail bed and a building footprint — held by the class grid, unverified against the drawn geometry
- [ ] Mown state inside a park, a garden and a cemetery
- [ ] Ground-cover stratum at all — the near-field ground is a shader and nothing else
- [ ] Clutter density per class is declared (`clutter.perM2`, 0.01…1.2) and **nothing reads it**

### III.2 Growth forms — what the generator must be able to shape

*One line per form. `[x]` means the grower can produce that shape today, not that a species using it is
declared.*

- [x] Single-stem tree — one leader, a clear bole, a crown above it. Sixteen species ride this and it is the only form that exists
- [ ] `habit` in a species file is a **prose sentence for a human** and nothing reads it — the grower works from numbers, so a form written there cannot reach the geometry. Either it becomes parameters or it goes
- [ ] Crown shape as a declared envelope — conical, columnar, ovoid, domed, vase, weeping, umbrella, flat-topped. The prose already distinguishes eight and the numbers distinguish none
- [ ] Multi-stem tree — several leaders from one base, common in ash, lime and maple on an edge
- [ ] Multi-stem shrub — no bole, stems from the ground, crown to the ground
- [ ] Bush — a low rounded multi-stem form under about 2 m
- [ ] Dwarf shrub — woody, under 0.5 m, the heath and bilberry form
- [ ] Thicket — a colony spreading by suckers, with no individual outline (blackthorn, bramble)
- [ ] Hedge — a managed linear form with a cut section, not a row of shrubs
- [ ] Coppice stool — many stems of one age from a cut base
- [ ] Pollard — a bolling with a rod crown at head height
- [ ] Trained orchard form — central leader, spindle, bush, on a post and wire
- [ ] Espalier and cordon — a plane trained against a wall or a wire
- [ ] Vine on a trellis — a stock, a cordon and annual canes
- [ ] Climber and liana — a form that needs a **host** to grow on, which nothing in the contract supplies
- [ ] Creeping and mat-forming — a form with no vertical axis at all
- [ ] Rosette — a basal leaf whorl with a bare flowering stem, and it is the commonest meadow herb shape
- [ ] Erect leafy forb — a stem with leaves along it and a terminal inflorescence
- [ ] Umbel — the tall flat-topped form of hogweed and cow parsley, and it is a silhouette on its own
- [ ] Tussock — a dense basal clump with arching leaves, the grass form that reads at distance
- [ ] Turf-forming graminoid — rhizomatous, no clump, the closed sward
- [ ] Cane and reed — an unbranched vertical stem in a dense stand
- [ ] Bulb and geophyte — a short-lived spring form, which forces a phenology the engine has no clock for
- [ ] Fern crown — a shuttlecock of pinnate fronds, a form nothing else uses
- [ ] Frond mat — bracken, a continuous stand rather than individuals
- [ ] Cushion — alpine, a compact hemispherical mass
- [ ] Moss carpet and turf — a surface form, closer to a material than to a plant
- [ ] Lichen crust and foliose thallus — a surface form on bark, stone and roof
- [ ] Floating-leaf aquatic — leaves on the water plane, a form nothing else uses and one that needs the water surface as its datum
- [ ] Submerged aquatic — a form that streams with a current
- [ ] Emergent aquatic — rooted in the bed, standing above the surface
- [ ] Fungal fruiting body — cap and stipe, and the bracket form beside it
- [ ] Sapling and juvenile stage of every woody form above — a young beech is not a small beech
- [ ] Standing dead trunk — a form with no foliage and a decaying outline
- [ ] Snag — a broken top
- [ ] Stump — cut or broken, with and without resprouts
- [ ] Fallen log — a horizontal woody body
- [ ] Root plate — a windthrow's vertical disc of roots and soil
- [ ] Crop row form — a field of one form with a row rhythm, not a scatter of individuals

### III.3 Representation and level of detail

- [x] Procedural growth: trunk, taper, minimum radius, twig radius, branch chance, branch angle and its variance, order length and radius, wander, leader bias, branch up-bias, conical bias, whorl count and spacing, terminal fork, bare steps, crown base, shade prune
- [x] Trunk sides as a declared polygon count
- [x] Bark colour, darkening, frequency, ridge and style
- [x] Leaf kinds: broad, needle, palmate, pinnate, palmate compound
- [x] Leaf blade: segments, length, width, widest point, base fill, base skew, tip, lobes, lobe depth, serration, fold, curve, leaflets, palmate lobes and spread
- [x] Needles: width, length, forward rake, droop
- [x] Leaf cards per point and a card budget per prototype
- [x] Leaf angle distribution as a declared population
- [x] Four mesh LOD levels plus an impostor rung, one ladder, model-space error as a fraction of height
- [x] Instanced sheets standing for sixteen quad elements each
- [x] Octahedral impostor atlas baked at runtime from our own grown prototype — the cache of a computable function, principle 2 admissible
- [x] Sixteen species measured by a bench (`make treebench`)
- [ ] A grower that takes a **form** as an input rather than assuming one
- [ ] Impostor cells that are never sampled without a bake — counted, not assumed
- [ ] A far rank that is one plane per stand, merged per fixed spatial cell into a single draw, corners expanded in the vertex shader so each element faces the camera individually — the reference's UBERLOD, minus its offline bake and minus its single view, because we have a bird's eye
- [ ] Crowns that are not bow-ties: the cross must never survive to the range where its own geometry is legible
- [ ] Stands that do not vanish seen from directly above — 15 995 of them do
- [ ] Crown self-shadowing, so a crown reads as one mass with a lit top and a shadowed underside
- [ ] Leaf albedo at the top comparison rung — NO SUBSTITUTE for now: theirs is an authored alpha and colour; ours is geometry plus a colour, which is enough at 320×180 and unsolved where venation and translucency variation speak
- [ ] Two-sided transmission through a leaf, driven by the material declaration rather than a per-leaf shader
- [ ] Bark normal detail at the near rung, as a function
- [ ] Root flare, so a trunk meets the ground instead of intersecting it
- [ ] Buttress roots on a mature beech
- [ ] Lean and sweep, so a stand is not a set of verticals
- [ ] Damage forms: broken leader, forked stem, lightning scar, browsing line
- [ ] Epiphytes on a host: ivy on the trunk, moss on the north side, lichen on the bark
- [ ] Merged-mesh treatment for a dense low stratum, batched into fixed cells that LOD by removing items — the reference's answer for grass fields
- [ ] Flower and fruit as declared elements — rape's yellow, a cherry in blossom and a rowan's berries are all colour at distance

### III.4 Wind and interaction

- [x] Declared wind field: log profile to canopy top, honami wave at the stand's eigenfrequency, phase speed at canopy-top wind, local speed at a place and time
- [x] Per-species wind amplitude and frequency
- [ ] Response as the closed solution of the plant's own bending equation, driven rather than animated — the field publishes the Cauchy number and nothing consumes it for a tree
- [ ] Detail bending on foliage, distinct from trunk sway
- [ ] Touch bending: a body walking through a bush displaces it — the reference has this for bushes, ferns and trees
- [ ] Breeze generation: local gust sources rather than one global vector
- [ ] Gust visible as a wave crossing a field, not as a phase everywhere at once

### III.5 Species on the single-stem tree form — broadleaf

*The only form that exists, so every line here is cheap. The twelve declared botanical names are what
the files actually say, and three of them are not what a reader would assume.*

- [x] Fagus sylvatica — common beech
- [x] Quercus robur — pedunculate oak
- [x] Carpinus betulus — hornbeam
- [x] Fraxinus excelsior — ash
- [x] Acer pseudoplatanus — sycamore maple
- [x] Tilia cordata — small-leaved lime
- [x] Betula pendula — silver birch
- [x] Ulmus minor — field elm, and not the wych elm a "elm" in a beech forest would be
- [x] Populus nigra 'Italica' — Lombardy poplar, a **columnar cultivar**, not a floodplain black poplar
- [x] Salix × sepulcralis — weeping willow, a **garden hybrid**; the `riverbank` template's declared 50 % willow is therefore an ornamental where a floodplain wants *Salix alba*
- [x] Sorbus aucuparia — rowan
- [x] Aesculus hippocastanum — horse chestnut
- [ ] Quercus petraea — sessile oak
- [ ] Acer platanoides — Norway maple
- [ ] Acer campestre — field maple
- [ ] Tilia platyphyllos — large-leaved lime
- [ ] Betula pubescens — downy birch
- [ ] Alnus glutinosa — black alder, the floodplain's own tree and absent
- [ ] Alnus incana — grey alder
- [ ] Populus tremula — aspen, whose leaf tremor is its recognisable property
- [ ] Populus nigra — black poplar, the species rather than the cultivar
- [ ] Populus alba — white poplar, with its two-tone leaf
- [ ] Salix alba — white willow
- [ ] Salix fragilis — crack willow
- [ ] Ulmus glabra — wych elm
- [ ] Ulmus laevis — European white elm
- [ ] Prunus avium — wild cherry
- [ ] Prunus padus — bird cherry
- [ ] Malus sylvestris — crab apple
- [ ] Pyrus pyraster — wild pear
- [ ] Sorbus aria — whitebeam
- [ ] Sorbus torminalis — wild service tree
- [ ] Sorbus domestica — service tree
- [ ] Juglans regia — walnut
- [ ] Castanea sativa — sweet chestnut
- [ ] Robinia pseudoacacia — black locust, naturalised and common on poor ground
- [ ] Platanus × hispanica — London plane, the urban street tree with its flaking bark
- [ ] Ailanthus altissima — tree of heaven, the urban invader
- [ ] Quercus rubra — red oak, planted
- [ ] Corylus colurna — Turkish hazel, a modern street tree
- [ ] Gleditsia triacanthos — honey locust, a modern street tree
- [ ] Fraxinus ornus — manna ash; southern, named as out of the acceptance region

### III.6 Species on the single-stem tree form — conifers

- [x] Picea abies — Norway spruce
- [x] Abies alba — silver fir
- [x] Pinus sylvestris — Scots pine
- [x] Taxus baccata — yew, and it is declared here although its natural habit is multi-stemmed and often shrubby
- [ ] Larix decidua — European larch, the only deciduous conifer here, and it needs the seasonal state band II.10 owes
- [ ] Pinus nigra — black pine
- [ ] Pseudotsuga menziesii — Douglas fir, planted and now common
- [ ] Picea pungens — blue spruce, garden
- [ ] Pinus cembra — Swiss stone pine; montane
- [ ] Pinus mugo — dwarf mountain pine — needs the **krummholz** form, which is not the single-stem tree
- [ ] Juniperus communis — juniper; the tree form here, the shrub form in III.8
- [ ] Thuja / Chamaecyparis — garden conifers, whose real use is the hedge form
- [ ] Plantation stand: even-aged, even-spaced, no understorey — a placement form rather than a species, and OSM does not distinguish it

### III.7 Species needing the multi-stem shrub, bush and thicket forms

*None of these three forms exists, so every line here is blocked on III.2.*

- [ ] Corylus avellana — hazel, multi-stem
- [ ] Crataegus monogyna — hawthorn
- [ ] Crataegus laevigata — midland hawthorn
- [ ] Prunus spinosa — blackthorn — needs the **thicket** form; an individual outline is wrong for it
- [ ] Sambucus nigra — elder
- [ ] Sambucus racemosa — red elder
- [ ] Viburnum opulus — guelder rose
- [ ] Viburnum lantana — wayfaring tree
- [ ] Cornus sanguinea — dogwood, with red winter stems
- [ ] Cornus mas — cornelian cherry
- [ ] Euonymus europaeus — spindle
- [ ] Ligustrum vulgare — wild privet
- [ ] Rhamnus cathartica — buckthorn
- [ ] Frangula alnus — alder buckthorn
- [ ] Rosa canina — dog rose — an arching cane form between shrub and climber
- [ ] Rubus fruticosus agg. — bramble — needs the **thicket** form and it is the commonest thing on a forest edge
- [ ] Rubus idaeus — raspberry, a cane thicket
- [ ] Ribes rubrum / uva-crispa — currant, gooseberry
- [ ] Lonicera xylosteum — fly honeysuckle
- [ ] Salix cinerea — grey willow, wet
- [ ] Salix viminalis — osier, and its real form is the pollard
- [ ] Cytisus scoparius — broom, a broom-like stem bundle with almost no leaf
- [ ] Ulex europaeus — gorse; western and atlantic, named as marginal here
- [ ] Genista tinctoria — dyer's greenweed
- [ ] Hippophae rhamnoides — sea buckthorn, coastal and gravel
- [ ] Buxus sempervirens — box, garden and churchyard
- [ ] Berberis — barberry, garden
- [ ] Forsythia, Syringa, Philadelphus, Hydrangea — the suburban garden set, and their flowering mass is the point
- [ ] Rhododendron — garden and, as *R. ferrugineum*, alpine

### III.8 Species needing the dwarf shrub form

- [ ] Calluna vulgaris — heather, the heath's defining form and a continuous mass rather than individuals
- [ ] Erica tetralix — cross-leaved heath, bog
- [ ] Vaccinium myrtillus — bilberry, the acid forest floor
- [ ] Vaccinium vitis-idaea — cowberry
- [ ] Juniperus communis in its prostrate montane form
- [ ] Empetrum nigrum — crowberry
- [ ] Thymus and Helianthemum — the dry-slope mats, which double as the creeping form

### III.9 Species needing the hedge, coppice and pollard forms

- [ ] Managed field hedge with a flat-topped section — the **hedge** form, not a row of shrubs
- [ ] Species-rich hedgerow with standards left to grow through it
- [ ] Garden hedge: privet, beech, hornbeam, conifer
- [ ] Windbreak row
- [ ] Coppice stool with multiple stems of one age — hazel, hornbeam, sweet chestnut
- [ ] Pollard with a bolling and a rod crown — the willow along a Weser ditch, and it is the region's signature form
- [ ] Laid and staked hedge
- [ ] Clipped topiary and a hedge cut to a rectangle — the same form with a declared cut section
- [ ] Woodland mantle: the graded height profile from the field to the canopy
- [ ] Herb seam at the mantle's foot
- [ ] Field margin strip, uncultivated
- [ ] Ruderal strip along a road or a rail line

### III.10 Species needing the climber form

*A climber needs a **host**, and the generator contract has no way to say "this grows on that". That is
a contract change, not a species.*

- [ ] Hedera helix — ivy, climbing on a trunk and a wall, and creeping as a ground carpet
- [ ] Clematis vitalba — old man's beard, and it drapes a whole hedge
- [ ] Humulus lupulus — hop, wild on a riverbank and trained in a hop garden
- [ ] Lonicera periclymenum — honeysuckle
- [ ] Vitis vinifera — grape on a trellis, the **vine** form
- [ ] Parthenocissus — Virginia creeper on a wall, and its autumn red is a façade's colour
- [ ] Wisteria, Rosa (climbing) — garden
- [ ] Convolvulus / Calystegia — bindweed on a fence

### III.11 Species needing the rosette, erect forb and umbel forms — forest floor

- [ ] Anemone nemorosa — wood anemone, a spring carpet, which means phenology as well as a form
- [ ] Galium odoratum — sweet woodruff, the association's name-bearer
- [ ] Mercurialis perennis — dog's mercury, in dense masses
- [ ] Allium ursinum — ramsons, a carpet and a **geophyte**
- [ ] Oxalis acetosella — wood sorrel, creeping
- [ ] Hepatica nobilis — liverleaf
- [ ] Corydalis cava — hollowroot, geophyte
- [ ] Ficaria verna — lesser celandine, geophyte
- [ ] Arum maculatum — lords-and-ladies
- [ ] Lamium galeobdolon — yellow archangel
- [ ] Asarum europaeum — asarabacca, creeping
- [ ] Paris quadrifolia — herb Paris, a whorl form nothing else uses
- [ ] Convallaria majalis — lily of the valley
- [ ] Polygonatum multiflorum — Solomon's seal, an arching stem form
- [ ] Maianthemum bifolium — may lily
- [ ] Circaea lutetiana — enchanter's nightshade
- [ ] Stachys sylvatica — hedge woundwort
- [ ] Impatiens noli-tangere and I. parviflora — balsams
- [ ] Urtica dioica — nettle, on nutrient-rich ground and in dense stands
- [ ] Aegopodium podagraria — ground elder, a carpet
- [ ] Geum urbanum — wood avens
- [ ] Vinca minor — periwinkle, naturalised near settlement

### III.12 Species needing the rosette, erect forb and umbel forms — meadow, pasture, ruderal

- [ ] Leucanthemum vulgare — oxeye daisy
- [ ] Achillea millefolium — yarrow
- [ ] Trifolium pratense — red clover
- [ ] Trifolium repens — white clover, creeping
- [ ] Lotus corniculatus — bird's-foot trefoil
- [ ] Ranunculus acris — meadow buttercup
- [ ] Taraxacum officinale agg. — dandelion, in flower and in seed head, a rosette
- [ ] Plantago lanceolata — ribwort plantain, a rosette
- [ ] Plantago major — greater plantain, on a trodden edge
- [ ] Rumex acetosa — common sorrel, and its red flowering haze is a meadow's colour in June
- [ ] Rumex obtusifolius — broad-leaved dock
- [ ] Knautia arvensis — field scabious
- [ ] Centaurea jacea — brown knapweed
- [ ] Campanula patula — spreading bellflower
- [ ] Campanula rotundifolia — harebell
- [ ] Salvia pratensis — meadow clary
- [ ] Geranium pratense — meadow crane's-bill
- [ ] Heracleum sphondylium — hogweed, the **umbel** form
- [ ] Anthriscus sylvestris — cow parsley, the May roadside, umbel
- [ ] Daucus carota — wild carrot, umbel
- [ ] Pastinaca sativa — wild parsnip, umbel
- [ ] Hypericum perforatum — St John's wort
- [ ] Vicia cracca — tufted vetch, a scrambler
- [ ] Medicago lupulina — black medick
- [ ] Primula veris — cowslip
- [ ] Bellis perennis — daisy, on a lawn
- [ ] Veronica chamaedrys — germander speedwell
- [ ] Ajuga reptans — bugle, creeping
- [ ] Colchicum autumnale — autumn crocus, geophyte, and it is an autumn meadow's only flower
- [ ] Narcissus pseudonarcissus — wild daffodil, geophyte
- [ ] Cardamine pratensis — cuckoo flower, damp meadow
- [ ] Silene flos-cuculi — ragged robin, damp meadow
- [ ] Caltha palustris — marsh marigold
- [ ] Filipendula ulmaria — meadowsweet
- [ ] Lythrum salicaria — purple loosestrife, bank
- [ ] Cirsium arvense — creeping thistle
- [ ] Cirsium vulgare — spear thistle
- [ ] Jacobaea vulgaris — ragwort
- [ ] Solidago canadensis — Canadian goldenrod, the invader on every fallow strip
- [ ] Verbascum — mullein, a tall spike on rubble
- [ ] Echium vulgare — viper's bugloss, on gravel
- [ ] Origanum vulgare — marjoram, on a dry slope
- [ ] Thymus pulegioides — thyme, a mat
- [ ] Papaver rhoeas — corn poppy, arable weed
- [ ] Centaurea cyanus — cornflower, arable weed
- [ ] Matricaria chamomilla — scented mayweed, arable weed
- [ ] Chenopodium album, Amaranthus — the stubble weeds
- [ ] Artemisia vulgaris — mugwort, ruderal
- [ ] Tanacetum vulgare — tansy, ruderal
- [ ] Reynoutria japonica — Japanese knotweed, riparian invader, a cane thicket

### III.13 Species needing the tussock and turf graminoid forms

- [x] One aggregate blade class (`graminoid`), green and dry, as a ground-shader term with **no geometry**
- [x] Blades per square metre per class, 0 to 1165
- [x] Sward height, height jitter, blade width, dry fraction
- [x] Leaf area index derived from blades/m² × width × the population mean of the tangent's vertical component
- [x] Canopy top as a three-scale ladder — stand, patch, tussock
- [x] Senescence from the tip down, with a whole-dry fraction
- [ ] A tussock as **geometry** at close range — the aggregate is correct beyond the fade and there is nothing behind it
- [ ] Arrhenatherum elatius — false oat-grass, the hay meadow's dominant
- [ ] Dactylis glomerata — cocksfoot, and its tussock is a distinct silhouette
- [ ] Festuca pratensis — meadow fescue
- [ ] Festuca rubra — red fescue
- [ ] Festuca ovina — sheep's fescue, dry
- [ ] Poa pratensis — smooth meadow-grass
- [ ] Poa trivialis — rough meadow-grass
- [ ] Poa annua — annual meadow-grass, trodden ground
- [ ] Lolium perenne — perennial ryegrass, the intensively managed sward and the lawn
- [ ] Trisetum flavescens — yellow oat-grass
- [ ] Anthoxanthum odoratum — sweet vernal grass
- [ ] Holcus lanatus — Yorkshire fog, with its grey-green haze
- [ ] Avenula pubescens — downy oat-grass
- [ ] Bromus hordeaceus — soft brome
- [ ] Bromus erectus — upright brome, calcareous grassland
- [ ] Phleum pratense — timothy, and its cylindrical head
- [ ] Alopecurus pratensis — meadow foxtail
- [ ] Agrostis capillaris / stolonifera — bents
- [ ] Briza media — quaking grass
- [ ] Nardus stricta — mat-grass; montane pasture
- [ ] Molinia caerulea — purple moor-grass; bog and damp heath, a strong tussock
- [ ] Deschampsia cespitosa — tufted hair-grass, a strong tussock
- [ ] Deschampsia flexuosa — wavy hair-grass, acid forest floor
- [ ] Calamagrostis epigejos — wood small-reed, disturbed ground
- [ ] Milium effusum — wood millet, forest
- [ ] Melica uniflora — wood melick, beech forest
- [ ] Brachypodium sylvaticum — false brome, forest edge
- [ ] Carex sylvatica — wood sedge
- [ ] Carex acutiformis / riparia — the bank sedges
- [ ] Carex elata — tussock sedge, and the tussock *is* the form
- [ ] Carex nigra — common sedge, bog
- [ ] Juncus effusus — soft rush, and it marks wet ground in a pasture
- [ ] Luzula luzuloides / pilosa — woodrushes
- [ ] Eriophorum angustifolium — cotton grass; bog, and the white heads are the whole picture there

### III.14 Species needing the cane, emergent and aquatic forms

- [ ] Phragmites australis — common reed, the **cane** form, and a reed bed is a landscape element on its own
- [ ] Typha latifolia / angustifolia — bulrush, emergent
- [ ] Glyceria maxima — reed sweet-grass
- [ ] Phalaris arundinacea — reed canary grass
- [ ] Schoenoplectus lacustris — club-rush
- [ ] Sparganium erectum — branched bur-reed
- [ ] Iris pseudacorus — yellow flag
- [ ] Butomus umbellatus — flowering rush
- [ ] Alisma plantago-aquatica — water plantain
- [ ] Nuphar lutea — yellow water-lily — the **floating-leaf** form, whose datum is the water surface
- [ ] Nymphaea alba — white water-lily
- [ ] Potamogeton natans — broad-leaved pondweed
- [ ] Ranunculus fluitans — river water-crowfoot, **submerged**, streaming with the current
- [ ] Myriophyllum / Ceratophyllum — submerged, visible only in clear shallow water
- [ ] Elodea canadensis — waterweed
- [ ] Lemna minor — duckweed as a surface film, which changes the water's colour entirely
- [ ] Nasturtium officinale — watercress at a spring
- [ ] Algal bloom as a surface state
- [ ] Bank zonation: open water, floating, emergent, reed, sedge, willow — the sequence, not the species

### III.15 Species needing the fern, moss, lichen and fungal forms

- [ ] Dryopteris filix-mas — male fern, the **fern crown**
- [ ] Dryopteris dilatata — broad buckler fern
- [ ] Athyrium filix-femina — lady fern
- [ ] Polystichum aculeatum — hard shield fern
- [ ] Pteridium aquilinum — bracken — the **frond mat**, a continuous stand rather than individuals
- [ ] Blechnum spicant — hard fern, acid
- [ ] Asplenium scolopendrium — hart's tongue, shaded rock, a strap-leaf form
- [ ] Asplenium trichomanes / ruta-muraria — the wall ferns, and they need a wall as a host
- [ ] Polypodium vulgare — polypody, on a bank or a branch
- [ ] Pleurozium schreberi, Hylocomium splendens, Dicranum scoparium — the conifer floor moss carpet
- [ ] Thuidium tamariscinum — on a broadleaf floor
- [ ] Polytrichum commune — haircap moss
- [ ] Sphagnum spp. — bog moss, and the hummock-hollow pattern is the form
- [ ] Brachythecium / Hypnum on stone, wall and roof tile
- [ ] Marchantia — liverwort on wet bare soil
- [ ] Cladonia rangiferina — reindeer lichen, heath and dune
- [ ] Xanthoria parietina — the yellow lichen on a roof tile and a wayside tree
- [ ] Parmelia / Hypogymnia — grey bark lichen
- [ ] Green algal film on the north side of a trunk, a post and a wall
- [ ] Amanita muscaria — fly agaric
- [ ] Boletus edulis — cep
- [ ] Cantharellus cibarius — chanterelle
- [ ] Russula, Lactarius, Amanita — the common floor set
- [ ] Macrolepiota procera — parasol
- [ ] Fomes fomentarius — hoof fungus on beech, a **bracket** on a standing trunk
- [ ] Ganoderma / Trametes versicolor — brackets on deadwood
- [ ] Mycena / Armillaria — the small ones on a stump
- [ ] Fairy ring in a pasture as a growth pattern rather than an object

### III.16 The deadwood forms and what rides them

- [ ] Standing dead trunk with bark falling off
- [ ] Snag with a broken top
- [ ] Windthrow with a raised root plate and a pit beside it
- [ ] Fallen log, decayed in stages
- [ ] Stump, cut flat, with saw marks
- [ ] Stump with resprouts — a coppice stool's first year
- [ ] Brash pile from thinning
- [ ] Log stack at a forest road, which is a strong human signal
- [ ] Branch litter on the floor
- [ ] Woodpecker holes and bark beetle galleries
- [ ] Charcoal and burnt ground after a fire
- [ ] Habitat pile, deliberately left

### III.17 The crop row form and the crops that ride it

*Ranked by German acreage (Destatis 2024): wheat 2.62 Mha, barley 1.66 Mha, oilseeds 1.15 Mha, rye
0.54 Mha, grain maize 0.50 Mha, sugar beet 0.44 Mha, potato 0.28 Mha, oats 0.16 Mha.*

- [ ] Crop row form: a field of one plant with a row rhythm, a row direction and a drill spacing
- [ ] Winter wheat — the dominant field, and its colour sequence from green to gold is the summer landscape
- [ ] Winter barley, with its awned, nodding head
- [ ] Spring barley
- [ ] Rye, taller than wheat, greyer
- [ ] Triticale
- [ ] Oats, a panicle rather than a spike
- [ ] Grain maize, over two metres, rows legible from the air
- [ ] Silage maize, cut earlier and shorter
- [ ] Oilseed rape in flower — the single most recognisable field colour in a German April
- [ ] Rape after flowering, grey-green and pod-heavy
- [ ] Sugar beet, a low dense canopy with a distinct blue-green
- [ ] Potato, in ridges with a visible furrow rhythm
- [ ] Field bean, pea, lupin
- [ ] Sunflower
- [ ] Soy
- [ ] Hemp, flax
- [ ] Lucerne / clover ley
- [ ] Mustard or phacelia as a cover crop, sown after harvest
- [ ] Flower strip on a margin, deliberately sown
- [ ] Set-aside and fallow
- [ ] Row direction per field, constant within it, varying between neighbours — this is what makes an agricultural landscape read
- [ ] Tramlines from the sprayer, at the machine's own working width
- [ ] Headland, worked across the rows
- [ ] Stubble after harvest
- [ ] Ploughed bare soil, with the furrow direction
- [ ] Harrowed and drilled seedbed
- [ ] Crop lodging patches after a storm
- [ ] Irrigation reel and its wet arc
- [ ] Round bales left on a field
- [ ] Square bales stacked
- [ ] Wrapped silage bales, white and shiny
- [ ] Hay windrows before baling
- [ ] Slurry application darkening a field
- [ ] Bird scarer, kite, gas gun
- [ ] Field boundary stone, marker post
- [ ] Game cover strip and a raised hide

### III.18 The trained forms and the plantings that ride them

- [ ] Streuobstwiese — standard fruit trees on tall stems over a grazed or cut meadow, and it is the form the Weser valley actually has
- [ ] Modern dwarf apple orchard: the **spindle on post and wire** form, with hail netting
- [ ] Cherry orchard
- [ ] Plum and pear orchard
- [ ] Walnut in a field corner
- [ ] Vineyard in rows on a slope, the **vine on trellis** form with posts and wires
- [ ] Terraced vineyard on drystone walls
- [ ] Individual-stake vine training, distinct from the trellis row
- [ ] Espalier fruit against a wall
- [ ] Hop garden with its high wire framework
- [ ] Asparagus ridges under film
- [ ] Strawberry rows under a tunnel
- [ ] Field vegetable rows: cabbage, onion, carrot, leek
- [ ] Nursery rows of young trees
- [ ] Christmas tree plantation
- [ ] Short-rotation poplar or willow coppice
- [ ] Allotment garden: plot grid, sheds, fruit trees, vegetable beds, hedges
- [ ] Domestic garden: lawn, border, ornamental shrub, hedge, terrace, tree
- [ ] Park: mown lawn, specimen tree, avenue, shrub block, bedding
- [ ] Cemetery: clipped hedges, grave plantings, yew and thuja, mown grass
- [ ] Green roof and façade planting — the setting is post-scarcity, so this is not decoration
- [ ] Street tree in a pit with a grate, and a young one with a stake and tie
- [ ] Planter and container planting
- [ ] Sports turf, marked

### III.19 Forms belonging to other biomes, named so they are not mistaken for oversights

- [ ] Krummholz — a wind-formed woody mass, and it is a **form** before it is *Pinus mugo*
- [ ] Alpine dwarf shrub heath — Rhododendron ferrugineum, Vaccinium
- [ ] Alpine mat — Carex curvula, Festuca, the turf form at 2 500 m
- [ ] Cushion — Silene acaulis, Saxifraga, a form nothing temperate uses
- [ ] Scree pioneers
- [ ] Snowbed community
- [ ] Timberline transition, as a density and height ramp rather than a line
- [ ] Mediterranean maquis and garrigue — holm oak, cistus, rosemary
- [ ] Olive, umbrella pine, cypress — three distinct crown envelopes
- [ ] Arid: creosote bush, saltbush, yucca, cactus — the **succulent** form, which nothing here has
- [ ] Arid: ephemeral bloom after rain
- [ ] Arid: desert pavement with no plants at all — the `badwater` scenario exists and there is still no arid template among the thirteen
- [ ] Boreal: spruce–birch taiga with a lichen ground layer
- [ ] Coastal: dune grass (Ammophila), salt marsh (Salicornia, Spartina)
- [ ] Palm — a form nothing else uses, and named for completeness
- [ ] Tropical forms — out of scope, named so they are not an oversight

## Band IV — Buildings, structures and infrastructure

*The reference for this half is GTA 5, for range and construction only. KCD's built world does not
transfer: a Bohemian village is not modern infrastructure. Infrastructure comes first inside the band
because the street network is what buildings, vehicles and lighting all hang off.*

### IV.1 Data prerequisites

- [x] Building footprints from the served vector tiles, kept as rings with an index rather than re-parsed
- [x] A `height` attribute where the provider carries one
- [x] The provider's 5.0 m fill detected and named rather than trusted (`kFillHeightM`, 1634 of the Hameln tile)
- [x] Base elevation per footprint from the ring's own lowest corner
- [ ] One base per building — `FeatureTop` takes the ring's lowest corner, `Buildings::At` derives it from the bbox centre, so a queried prism floats ≈1.5 m against the drawn one on a 10 % slope
- [ ] `building:levels` — TILE: the Shortbread buildings layer carries no attributes at all beyond the provider's height extension
- [ ] Building use / kind (house, church, industrial, retail) — TILE, same reason; the `pois` layer is the only place a use is spelled and it is not fetched
- [ ] `roof:shape`, `roof:levels`, `roof:material`, `building:material`, `building:colour` — TILE
- [ ] Address and house number — in the `pois`/`addresses` layers, not fetched
- [ ] Storey count inferred from height when no level count is served, with the inference stated
- [ ] Building age or period inferred, which the epoch dial needs

### IV.2 Mass and footprint

- [x] Footprint extruded to a prism
- [x] Wall vertices carrying run-along-the-wall and height-above-base in metres, so floor lines and window grids are functions of two numbers
- [ ] Multi-part mass: a main block plus a lower wing, rather than one prism per ring
- [ ] Courtyard buildings as several rings resolved as one structure
- [ ] Terrace: a row of prisms sharing walls, recognised as a row
- [ ] Setback on an upper storey
- [ ] Overhang and cantilever
- [ ] Building on a slope: a stepped base rather than a floating or buried plinth
- [ ] Plinth and base course as a distinct band
- [ ] Party wall exposed above a lower neighbour
- [ ] Attached garage, porch, conservatory, extension
- [ ] Building contact body for physics — deliberately none today, and a wrong body would be worse than none

### IV.3 Roofs

- [ ] Flat roof with a parapet
- [ ] Monopitch / shed roof
- [ ] Gable roof — the default for the region, and the one that must exist first
- [ ] Hip roof
- [ ] Half-hip (Krüppelwalm)
- [ ] Mansard roof
- [ ] Gambrel roof
- [ ] Pyramidal roof
- [ ] Conical roof on a round tower
- [ ] Dome
- [ ] Barrel vault
- [ ] Sawtooth / north-light roof, the industrial hall
- [ ] Butterfly roof
- [ ] Folded-plate and shell roofs
- [ ] Roof pitch as a function of the footprint's proportions and the declared epoch
- [ ] Ridge running along the long axis by default, with the exceptions declared
- [ ] Roof over an L-shaped or T-shaped footprint, with valleys resolved
- [ ] Eaves overhang, fascia, soffit
- [ ] Verge and bargeboard
- [ ] Gutter, hopper, downpipe, and a downpipe that reaches the ground
- [ ] Ridge tiles and hip rolls
- [ ] Chimney stack, with pots or a metal flue
- [ ] Roof vent, extract cowl
- [ ] Dormer: gable, hip, shed, eyebrow
- [ ] Roof window flush in the plane
- [ ] Roof lantern and skylight strip
- [ ] Roof terrace with a railing
- [ ] Plant room, lift overrun, stair head
- [ ] Rooftop HVAC units and ducting — the flat-roofed commercial building's whole silhouette
- [ ] Rooftop water tank
- [ ] Photovoltaic array, and in a post-scarcity setting it is the default rather than the exception
- [ ] Solar thermal panel
- [ ] Green roof, planted
- [ ] Aerial, satellite dish, lightning conductor
- [ ] Snow guard
- [ ] Roof covering: clay pantile, plain tile, concrete tile, slate, shingle, corrugated metal, standing seam, bitumen felt, gravel ballast, membrane
- [ ] Thatch — heritage, epoch 1 only
- [ ] Moss and lichen on the north pitch, and it is one of the strongest ageing signals on a roof
- [ ] Missing tiles, sagging ridge, collapsed section — decay dial

### IV.4 Façade and openings

- [ ] Storey division derived from height, so a window grid has a rhythm
- [ ] Window rhythm: bay spacing, alignment between storeys, a wider or narrower ground floor
- [ ] Window as an opening with a reveal depth, not a decal
- [ ] Window frame, mullion, transom, glazing bars
- [ ] Sill and lintel
- [ ] Casement, sliding, fixed, tilt-and-turn
- [ ] French window and door to a balcony
- [ ] Bay window and oriel
- [ ] Arched, round and porthole openings
- [ ] Shop window at ground floor, full height
- [ ] Roller shutter, louvre shutter, folding shutter
- [ ] Blind or curtain visible behind the glass
- [ ] Glass: reflectance and a dark interior at the comparison rung; a lit room only at night
- [ ] Entrance door, double door, revolving door
- [ ] Garage door: up-and-over, sectional, roller
- [ ] Loading dock door and a dock leveller
- [ ] Gateway passage through a perimeter block
- [ ] Ventilation grille, air brick, meter box
- [ ] Balcony: cantilevered slab, recessed loggia, French balconet
- [ ] Balcony railing: steel, glass, masonry, and planting on it
- [ ] External staircase and fire escape
- [ ] Ramp and handrail
- [ ] Porch and canopy over an entrance
- [ ] Awning, retractable
- [ ] Pergola and terrace
- [ ] Buttress and pilaster
- [ ] Cornice, string course, quoins, lesenes
- [ ] Gable ornament and finial
- [ ] Timber framing (Fachwerk) as a visible structural grid — Hameln's old town is the epoch 1 anchor and it is exactly this
- [ ] Surface finish: render, exposed brick, stone ashlar, rubble stone, board cladding, fibre cement, metal panel, precast concrete, curtain wall, EIFS
- [ ] Brick bond and course height as geometry rather than a texture
- [ ] Weathering: rain streaks below sills, splash zone at the base, efflorescence, algae on the shaded side
- [ ] Shop signage lettering — NO SUBSTITUTE: a typeface is authored appearance, and a shop that reads as a shop needs one. A procedural pseudo-glyph is legible as noise at the top rung
- [ ] Advertising imagery and posters — NO SUBSTITUTE, same reason, and REFUSED as an asset
- [ ] Graffiti — NO SUBSTITUTE: a mark made with intent. A statistical smear is not the same thing
- [ ] Stained glass — NO SUBSTITUTE, authored imagery
- [ ] Company logos and liveries — NO SUBSTITUTE, and legally distinct from the above

### IV.5 Interiors

- [ ] A dark room box behind the glass, so a window is not a hole into the world
- [ ] Interior wall plane at a declared depth, lit only by what comes through the window
- [ ] Lit interior at night with a per-room duty cycle
- [ ] Curtain or blind plane
- [ ] Enterable ground-floor shop
- [ ] Enterable dwelling: hall, room, stair
- [ ] Stair core and lift shaft as geometry
- [ ] Floor plan generated from the footprint and the storey count
- [ ] Furniture as declared bodies — the body format already has to carry furniture
- [ ] Interior lighting as a light list contribution
- [ ] Portal or occlusion boundary at a door, so an interior does not cost the exterior
- [ ] Basement and cellar
- [ ] Loft space under a pitched roof
- [ ] GTA 5's hand-modelled interiors — REFUSED as a method; the substitute is a generated plan, and where it does not reach, a closed door

### IV.6 Building types by use

- [ ] Detached house
- [ ] Semi-detached pair
- [ ] Terrace / row house
- [ ] Apartment block, four to six storeys
- [ ] Slab block and tower block
- [ ] Perimeter block with an inner courtyard
- [ ] Villa in a garden
- [ ] Farmhouse
- [ ] Barn
- [ ] Stable and livestock shed
- [ ] Silo, tower and clamp
- [ ] Greenhouse, glass and film
- [ ] Warehouse and distribution shed
- [ ] Factory hall
- [ ] Workshop and small industrial unit
- [ ] Office building
- [ ] Curtain-wall tower
- [ ] Shopping centre
- [ ] Supermarket with its car park
- [ ] Retail park shed
- [ ] Kiosk
- [ ] Restaurant, café, pub
- [ ] Hotel
- [ ] School
- [ ] University building
- [ ] Hospital
- [ ] Church, and a tower with a spire is a landmark at any distance
- [ ] Chapel
- [ ] Mosque, synagogue
- [ ] Town hall and civic building
- [ ] Museum, theatre, cinema, library
- [ ] Police station, fire station
- [ ] Prison
- [ ] Sports hall
- [ ] Stadium with a stand roof
- [ ] Swimming pool building
- [ ] Multi-storey car park
- [ ] Petrol / charging station with a canopy
- [ ] Car wash
- [ ] Bus station
- [ ] Railway station hall and platform canopy
- [ ] Airport terminal
- [ ] Aircraft hangar
- [ ] Power station block — Grohnde is a declared acceptance target and it is a building set of its own
- [ ] Cooling tower
- [ ] Substation building
- [ ] Waterworks and sewage plant buildings
- [ ] Waste transfer station
- [ ] Data centre
- [ ] Telecom exchange
- [ ] Cemetery chapel
- [ ] Allotment hut
- [ ] Garden shed
- [ ] Garage and carport
- [ ] Boathouse
- [ ] Lighthouse
- [ ] Windmill and watermill — heritage
- [ ] Castle, keep, town wall, gate tower — epoch 1, and Hameln has them
- [ ] Bunker
- [ ] Grain elevator
- [ ] Market hall
- [ ] Public toilet
- [ ] Container terminal and stacked containers
- [ ] Scaffolded building under construction, with a crane
- [ ] Ruin — decay dial: collapsed roof, standing gables, vegetation in the shell

### IV.7 Boundaries and small structures

- [ ] Masonry wall, with a coping
- [ ] Drystone wall
- [ ] Rendered garden wall
- [ ] Retaining wall
- [ ] Timber fence: close-board, picket, post-and-rail
- [ ] Wire fence, chain-link, welded mesh
- [ ] Palisade and security fence
- [ ] Electric fence for stock, on plastic posts
- [ ] Deer fence
- [ ] Crash barrier used as a boundary
- [ ] Noise barrier
- [ ] Gate: field gate, driveway gate, pedestrian gate, sliding gate
- [ ] Bollard: fixed, removable, illuminated
- [ ] Stile, kissing gate, cattle grid
- [ ] Hedge as a boundary — cross-references III.7
- [ ] Ditch and bank as a boundary
- [ ] Terrace steps, garden stair
- [ ] Pergola, arbour, gazebo
- [ ] Bin store and refuse bins
- [ ] Letterbox, house number plate, doorbell panel
- [ ] Washing line
- [ ] Playground equipment
- [ ] Bench, picnic table
- [ ] Monument, memorial, wayside cross, shrine
- [ ] Fountain and basin
- [ ] Flagpole
- [ ] Statue

### IV.8 Roads

- [x] Ways carried as centrelines with a declared half-width per kind (`world/StreetField`)
- [x] Seventeen street kinds classified: motorway, trunk, primary, secondary, tertiary, unclassified, residential, living street, pedestrian, service, track, path, footway, cycleway, steps, bridleway, busway
- [x] Rail kinds classified: rail, light rail, tram, narrow gauge, subway, monorail, funicular
- [x] Aeroway kinds classified: runway, taxiway, apron, helipad
- [x] Street polygons as areas rather than ribbons
- [x] Way surface as a class-grid colour under the terrain shader
- [x] Point query: what is made here, and how wide (`generators/Infrastructure`)
- [ ] A road drawn as its own geometry rather than as a colour on the terrain
- [ ] Carriageway with camber and superelevation on a curve
- [ ] Lane subdivision from the width, with the lane count stated
- [ ] Hard shoulder and verge
- [ ] Kerb with an upstand, dropped at a crossing, with a corner radius
- [ ] Gutter channel and drainage grate
- [ ] Manhole and inspection cover
- [ ] Junction geometry: the corner fillet, the flared mouth, the island
- [ ] Roundabout with its island and apron
- [ ] Motorway interchange ramps as geometry
- [ ] Level difference between carriageway, verge and field
- [ ] Cutting and embankment along a road
- [ ] Surface by class: asphalt, concrete, setts, gravel, unpaved, and the served `surface` attribute already carries it
- [ ] Tracktype as a surface gradient on a farm track
- [ ] Wheel-track polish bands and a darker centre strip
- [ ] Patches, joints, crack sealing
- [ ] Potholes and edge break-up — decay dial
- [ ] Road markings: centre line, lane line, edge line, stop line, give-way triangles, arrows, zebra, box junction, hatching, chevrons, cycle lane, bus lane, parking bay, painted speed limit
- [ ] Reflective studs
- [ ] Tactile paving at a crossing
- [ ] Wet road reflectance, and it doubles the apparent light at night
- [ ] The reference's road tool is a decal along a spline — REFUSED as a method: a decal needs an authored texture. Ours must be geometry plus a material row

### IV.9 Road furniture, signage and lighting

- [ ] Traffic sign: warning triangle, prohibition circle, mandatory blue, direction sign, gantry sign
- [ ] Sign face content — NO SUBSTITUTE for lettering and pictograms; the geometry and the colour are procedural, the glyphs are not
- [ ] Street name plate and house number
- [ ] Traffic light: mast, arm, pedestrian head, countdown, and its states
- [ ] Street lamp: column, bracket, lantern, and the modern LED versus the sodium heritage form
- [ ] Street lamp placement from the street centreline at a declared spacing — measured: the served vector data has no lamps, so placing them is the only route
- [ ] Lamp emission with a photometric cone, contributing to the light list
- [ ] Bollard, guard rail (steel W-beam, cable, concrete barrier)
- [ ] Crash cushion
- [ ] Delineator post
- [ ] Junction mirror
- [ ] Speed camera
- [ ] Bus stop pole, shelter, timetable case
- [ ] Bench, litter bin
- [ ] Cycle rack
- [ ] Charging post — post-scarcity default
- [ ] Parking meter and ticket machine
- [ ] Post box
- [ ] Advertising column and billboard frame
- [ ] Planter, tree pit with a grate
- [ ] Road works: cones, barriers, temporary lights, diversion signs, an open trench, a steel plate over it
- [ ] Height restriction bar
- [ ] Weather station and variable message sign

### IV.10 Rail

- [ ] Ballast bed with a shoulder
- [ ] Sleepers, concrete and timber
- [ ] Rails with a rail head shine
- [ ] Points and a crossing frog
- [ ] Buffer stop
- [ ] Platform with an edge line, a tactile strip, a canopy, seats and signage
- [ ] Overhead line: masts, cantilevers, catenary and contact wire, tensioning weights
- [ ] Third rail
- [ ] Signals: light signals, and semaphore for a heritage epoch
- [ ] Cable trough, kilometre post, lineside fencing
- [ ] Level crossing: barriers, lights, road surface panels
- [ ] Tram track set into a road surface
- [ ] Tram stop island
- [ ] Marshalling yard and siding
- [ ] Rail bridge and tunnel portal
- [ ] Funicular, narrow gauge, monorail beam
- [ ] Underground station entrance

### IV.11 Bridges and tunnels

- [ ] Beam and slab bridge
- [ ] Girder and box girder bridge
- [ ] Truss bridge
- [ ] Masonry arch bridge
- [ ] Concrete arch bridge
- [ ] Cable-stayed bridge
- [ ] Suspension bridge
- [ ] Bowstring arch
- [ ] Footbridge
- [ ] Pipe bridge and aqueduct
- [ ] Viaduct with repeated piers
- [ ] Abutment, bearing, expansion joint
- [ ] Parapet, railing, deck drainage
- [ ] Bridge lighting and under-deck shadow
- [ ] Culvert and headwall
- [ ] Tunnel portal
- [ ] Tunnel lining, lighting strip, ventilation fan, emergency bay, cross passage
- [ ] The served `bridge` and `tunnel` booleans consumed — they are in the streets and water_lines layers and nothing reads them

### IV.12 Water infrastructure

- [ ] Weir, and its foam line is visible from a distance
- [ ] Lock chamber and gates
- [ ] Sluice and penstock
- [ ] Dam and spillway
- [ ] Fish ladder
- [ ] Dyke and levee
- [ ] Revetment and riprap
- [ ] Groyne
- [ ] Quay wall, jetty, pontoon, slipway
- [ ] Mooring bollard, buoy, navigation marker
- [ ] Canal towpath
- [ ] Ford and stepping stones
- [ ] Storm drain outfall
- [ ] Water tower
- [ ] Pumping station
- [ ] Hydrant and standpipe
- [ ] Sewage works tanks
- [ ] Irrigation channel and ditch network

### IV.13 Power, energy and communications

- [ ] Lattice transmission tower, in its several types
- [ ] Conductor catenary sag, computed rather than drawn straight
- [ ] Insulator strings and earth wire
- [ ] Distribution pole, wood and concrete
- [ ] Pole-mounted transformer
- [ ] Ground transformer kiosk
- [ ] Substation: busbars, breakers, gantries, fence, gravel
- [ ] Cleared right-of-way through woodland under a line — a vegetation consequence of an infrastructure object
- [ ] Photovoltaic field with tracker rows and their shadow pattern
- [ ] Wind turbine: tower, nacelle, three blades, and the rotation is part of acceptance
- [ ] Aviation warning lights and the daytime red bands
- [ ] Battery storage containers
- [ ] Biogas plant with its digester domes
- [ ] District heating pipe bridge
- [ ] Industrial chimney with a plume
- [ ] Cooling tower with a plume
- [ ] Nuclear plant: reactor building, turbine hall, stack — the epoch 3 anchor at Grohnde
- [ ] Telecom mast with antenna panels and microwave dishes
- [ ] Guyed mast
- [ ] Street cabinet
- [ ] Satellite ground station

### IV.14 Aviation and ports

- [ ] Runway with threshold markings, centre line, touchdown zone
- [ ] Taxiway with its centre line and edge lights
- [ ] Apron and stands
- [ ] Approach lighting
- [ ] Windsock
- [ ] Control tower
- [ ] Helipad marking
- [ ] Quay crane
- [ ] Container stacks
- [ ] Ro-ro ramp
- [ ] Marina pontoons

---

## Band V — Vehicles

*GTA 5 names the construction: a vehicle is a hull on wheels with suspension, tyre grip and a torque
curve; a human is a capsule whose locomotion the animation leads. The field groups below follow RAGE's
own `handling.meta` division — mass and aero, drivetrain, brakes and steering, traction, suspension,
damage — because it is the published enumeration of what a driveable body needs. **Nothing in this band
exists.** It depends on I.12 in full.*

### V.1 Prerequisites

- [ ] Rigid-body dynamics (I.12) — every line below is blocked on it
- [ ] Declared body format carrying segments, joints, contacts, force sources and medium
- [ ] Vehicle as one declaration in that format, not a second format
- [ ] Vehicle prototype and instances, as vegetation already is
- [ ] Vehicle LOD ladder on the same one ladder
- [ ] Vehicle spawned by an actor spawner sharing the region key
- [ ] Vehicle occupancy claimed against the same sink

### V.2 Mass, hull and aerodynamics

- [ ] Mass, centre-of-mass offset, inertia multiplier
- [ ] Hull as a collision shape distinct from the drawn mesh
- [ ] Drag coefficient and frontal area
- [ ] Downforce
- [ ] Submersion depth at which the engine cuts
- [ ] Buoyancy volume and centre, so a car sinks and a boat does not
- [ ] Body panels as sub-bodies: doors, bonnet, boot, hatch, with hinges and limits
- [ ] Glass panes as breakable elements
- [ ] Number plate — NO SUBSTITUTE for its lettering; the plate is geometry, the glyphs are not
- [ ] Paint as a material row: base colour, clear coat, metallic flake, and it needs no texture
- [ ] Livery and decals — NO SUBSTITUTE, authored appearance
- [ ] Dirt accumulation as a function of use and weather
- [ ] Rust and wear — decay dial

### V.3 Wheels, suspension, tyres

- [ ] Wheel as a body with a hub, a rim and a tyre
- [ ] Wheel raycast or shape cast against the drawn terrain
- [ ] Suspension: spring force, compression damping, rebound damping, upper and lower travel limits, raise
- [ ] Anti-roll bar
- [ ] Roll centre heights, front and rear
- [ ] Suspension bias front to rear
- [ ] Tyre longitudinal and lateral force curves, maximum and minimum
- [ ] Traction spring delta and low-speed loss
- [ ] Camber stiffness
- [ ] Traction bias front to rear
- [ ] Surface grip multiplier per contact material — asphalt, gravel, mud, grass, wet, ice
- [ ] Tyre deformation and burst, with the rim then running on the road
- [ ] Wheel spin, lock-up and the marks they leave
- [ ] Steering geometry: lock angle, Ackermann, self-centring
- [ ] Ride height change under load

### V.4 Drivetrain, brakes, controls

- [ ] Drive bias: front, rear, all
- [ ] Gear count and ratios, final drive
- [ ] Drive force and drive inertia
- [ ] Clutch engage and shift rates
- [ ] Top speed limiter
- [ ] Reverse
- [ ] Electric drive with a single ratio and instant torque — the post-scarcity default, and it changes the sound and the acceleration curve
- [ ] Brake force, brake bias, handbrake
- [ ] Anti-lock behaviour
- [ ] Throttle, brake, steer as the only inputs a brain or a player reaches
- [ ] Cruise and speed limiter

### V.5 Damage

- [ ] Collision damage multiplier and body deformation
- [ ] Panel detachment
- [ ] Glass cracking and shattering
- [ ] Engine damage, smoke, fire
- [ ] Fuel or battery leak
- [ ] Light breakage
- [ ] Deformation reflected in the collision shape, not only in the mesh

### V.6 Road vehicle classes

*GTA 5's own 23, listed so the range is explicit; the classes are declarations over one construction.*

- [ ] Compact
- [ ] Sedan
- [ ] SUV
- [ ] Coupe
- [ ] Muscle — an era declaration rather than a construction
- [ ] Sports Classic — an era declaration
- [ ] Sport
- [ ] Super
- [ ] Off-Road
- [ ] Van
- [ ] Industrial — tipper, mixer, flatbed
- [ ] Utility — tractor, forklift, tow truck, crane
- [ ] Commercial — articulated tractor unit and semi-trailer
- [ ] Service — bus, coach, taxi, refuse truck, street sweeper
- [ ] Emergency — police, ambulance, fire appliance
- [ ] Military — setting-dependent; a post-scarcity world may have no place for it, and that is the owner's call rather than mine
- [ ] Open Wheel
- [ ] Motorcycle
- [ ] Cycle
- [ ] Boat
- [ ] Helicopter
- [ ] Plane
- [ ] Train

### V.7 Two-wheelers

- [ ] Rider lean as the steering input, not a yaw torque
- [ ] Counter-steer at speed
- [ ] Stand and parked pose
- [ ] Pedal drive with a cadence
- [ ] Gears on a bicycle
- [ ] Cargo bike and trailer
- [ ] E-bike and e-scooter — post-scarcity street furniture as much as vehicles

### V.8 Rail vehicles

- [ ] Constraint to the rail rather than free contact
- [ ] Bogies, and a long body articulating over them
- [ ] Coupling between units
- [ ] Pantograph contact with the catenary
- [ ] Doors, and a stop at a platform
- [ ] Freight wagon types: flat, hopper, tank, container
- [ ] Tram in a road surface, sharing it with traffic

### V.9 Watercraft

- [ ] Buoyancy from displaced volume against the core's water level
- [ ] Hydrodynamic drag and added mass
- [ ] Propeller or water-jet thrust
- [ ] Rudder
- [ ] Hull planing at speed
- [ ] Wake and bow wave, and they must move the water surface, not sit on it
- [ ] Sail: wind force on a sail plane, heel, tacking
- [ ] Rowing
- [ ] Mooring and anchoring
- [ ] Classes: dinghy, motorboat, cabin cruiser, yacht, canoe, barge, ferry, tug, workboat
- [ ] Inland barge on the Weser — the acceptance river carries them

### V.10 Aircraft

- [ ] Lift and drag as coefficients over angle of attack, not a table lookup
- [ ] Stall and its recovery
- [ ] Control surfaces: elevator, aileron, rudder, flaps, airbrake
- [ ] Propeller or turbofan thrust
- [ ] Landing gear with suspension and a ground handling model
- [ ] Ground effect
- [ ] Rotor thrust, cyclic and collective, and the torque a tail rotor answers
- [ ] Autorotation
- [ ] Classes: light aircraft, airliner, cargo, glider, helicopter, drone
- [ ] Drone as the everyday post-scarcity aircraft, and it is the one the camera can follow anywhere
- [ ] Wind field from the weather provider driving all of the above — the provider already answers wind at altitude

### V.11 Systems, equipment and the verbs

- [ ] Headlights: low, high, daytime running
- [ ] Tail, brake, reverse, indicator, fog, hazard
- [ ] Beacon and siren on an emergency vehicle
- [ ] Interior and instrument lighting
- [ ] Mirrors
- [ ] Wipers, and their effect on a wet windscreen
- [ ] Horn
- [ ] Doors, boot, bonnet opened
- [ ] Seats and passengers
- [ ] Cargo bed, tipper, crane, winch, plough, sweeper brush
- [ ] Trailer coupling and an articulated joint
- [ ] Roof rack, bike carrier
- [ ] Charging flap and a charging cable to a post
- [ ] Enter and exit
- [ ] Drive, brake, park
- [ ] Fly, land, taxi
- [ ] Sail, moor
- [ ] Tow and be towed
- [ ] Be repaired
- [ ] Be driven by a brain rather than by a player

### V.12 Traffic and the parked world

- [ ] Parked vehicles as dressing along a residential street — this carries more of the picture than driving does, and it costs almost nothing
- [ ] Parking bay occupancy from the street class and the time of day
- [ ] Traffic spawned on street centrelines at a density derived from the road class
- [ ] Lane following and junction rules
- [ ] Traffic light obedience
- [ ] Yield at a pedestrian crossing
- [ ] Headlights on at night, and it is the single most visible night-time element after street lighting
- [ ] Vehicles despawned outside the observer's reach without their *knowledge* becoming observer-dependent
- [ ] Agricultural machinery in a field in season — a tractor with a trailer is the Weser valley's traffic

---

## The count

| | |
|---|---|
| Lines in this file | **1626** |
| Feature lines | **1290** |
| `- [x]` built and checked | **183** |
| `- [ ]` not built | **1107** |
| Band I — engine | 194 |
| Band II — world | 155 |
| Band III — vegetation | 458 |
| Band IV — buildings and infrastructure | 342 |
| Band V — vehicles | 141 |
| `NO SUBSTITUTE` | 11 |
| `REFUSED` | 12 |
| `TILE` | 4 |
| `TOOL` | 8 |
| `UNSURE` | 3 |

**Read the 183 correctly.** 43 of them are declarations of one thing — sixteen tree species and twelve
land templates — and every one of the sixteen rides the single growth form the generator can shape. The
engine's own machinery accounts for most of the rest. Nothing in bands IV and V beyond the footprint
prism, the way widths and their point queries is ticked, and Band V is entirely unticked.
