# outshine

**A high-performance OPEN-WORLD game engine at RAGE/Unreal level, in C++23, whose world is the real
EARTH: a digital elevation model under OpenStreetMap's vectors, streamed around a moving camera.**
The development platform **is** the target: Apple A18 Pro — 2P+4E cores, 5 GPU cores, 8 GB —
holding **720p60**, measured as p50/p95/p99 over a moving camera and never as a mean. **Every
camera looks real** -- a metre from a wall, ten kilometres from a city, at any hour of any day
anywhere on Earth; the webcams of foto-webcam.eu are how that is measured against a photograph.

An engine is an **interactive physics simulation with a focus on graphics**, and each word is a
bound: physically as accurate as NECESSARY, graphically as good as the FRAME BUDGET allows,
temporally DETERMINISTIC. The middle bound is the one "as good as possible" cannot carry — a
target without a ceiling cannot be missed.

**This page is the AIM and never the state.** Where the tree actually stands, what is open and what
was decided is `board/` and `git log` — read those first, and `make help` for what can be run. A
sentence here that describes today would be a lie within a week, so there is none.

**WHAT THE ENGINE IS LAID OUT FOR.** Textures must WORK — the vendor corpora are the oracle and
their textured path has to be right. But the frame budget is laid out for five things, and they are
the answer to "what actually looks stunning": **high geometry with RECURSIVE generators · many
lights and their shadows · a realistic atmosphere · parameterised materials · reflections and
mirroring**. A texture is somebody else's photograph of a surface; light and composition are the
engine's own and are what a viewer reads. The budget is finite, so every millisecond has a place it
did NOT go, and that list is where it goes.

**THE PICTURE LEAVES THROUGH A CAMERA, NEVER THROUGH A BUFFER.** The default world is GRADED --
a sensor and an aperture rather than a pinhole, an exposure with a shoulder rather than a clamp, a
180-degree shutter, a committed palette, grain over the graded image. That is the DEFAULT and not a
setting, because a raw linear frame is what makes a picture read as a game, and this engine's claim
is that its picture can be argued with. A LOOK is then a DECLARATION evaluated against the hour and
the weather the providers already answer -- RAGE's timecycle carrying Unreal's physical camera --
and the same rule reaches the generators: **mass simple, skin deep.** A baroque church is a box with
a dome and its whole effect is a layered surface in raking light; a hull covered in greebles is the
same grammar. So relief depth per facade area and shadow-casting edges per storey are NUMBERS a
generator owes, and a generator that draws the median of its epoch is what makes a street read as
generated. `board:2155` holds the carriers and the measurements.

**AAA IS NOT AN ADJECTIVE, IT IS A DISTANCE LADDER, and the generators are held to it.** The
measure is Witcher 3, Cyberpunk 2077, Fallout 4 and GTA V, and what those share is not polish: at
EVERY distance a viewer reads something true. At 200 m it is the SILHOUETTE -- nothing is a plain
prism, and a row shares one eaves line where it shares a party wall. At 100 m it is the RHYTHM --
a bay grid the epoch owns, a piano nobile unlike the floors above it, and relief no shallower than
half a brick, because less casts no shadow at that distance. At 30 m it is the GROUND FLOOR and
the STREET -- shopfronts, doors, steps, a kerb with a FACE, a gutter, a footway, markings, lamps,
trees, walls; in the references that is where a large share of the visible triangles sits. At 5 m
it is the MATERIAL, dirty where water runs. **Correct means three things at once**: architecturally
(a roof has the pitch its COVERING needs, not a table's), functionally (a carriageway is continuous
and rounded and drivable, a tunnel keeps its cover, a stair has its riser) and geometrically
(closed, wound, welded, nothing floating or buried). **And every rung ADDS** -- L0 is the mass and
each rung above it adds geometry rather than replacing it, the one exception being a HOLE, because
a wall's openings are not something you add.

**THE GENERATOR INVENTS ALMOST EVERYTHING, and the number says so.** Measured on taginfo over 707
million `building=*`: `building:levels` on 6.0 %, `height` on at most 3.8 %, `roof:shape` on 1.4 %.
So **94 % of the planet's footprints carry no height and 98.6 % no roof shape** -- OSM gives an
outline and a word, and the height, the roof, the epoch, the material, the rhythm, the openings and
everything at the street are the generator's own. That is why a generator's truth is a
DISTRIBUTION and never a value, and why a rule without an origin is worth nothing here: there is no
tag to fall back on.

The world is EARTH and the engine is online by definition: a picture can be made of any place on
it, and what comes out has to be comparable with reality. Elevation always, vector data where a
scenario asks for it, and the sun, the moon, the stars and the weather standing where the place and
the hour put them rather than where a number says. A hand-set sun is a sun that disagrees with its
own shadows the moment the clock moves. **Reachable providers are PRESUMED** -- elevation, vectors,
weather and whatever a scenario declares next are servers this engine may count on, so no design
here pays for their absence, and an offline world is a cache and never a second architecture.
**How far the tree is from any of this is `board/`'s to say, never this page's.**

**THE WORLD IS A DEM UNDER A VECTOR SET, AND EVERYTHING ELSE FOLLOWS FROM THAT.** The height of any
point on the planet comes from a DIGITAL ELEVATION MODEL, and the outlines, ways, waterways,
landuse and names come from OPENSTREETMAP; the generators expand those two into everything a viewer
actually sees. Neither is held -- both are FETCHED, so an open world here is a STREAMING problem
before it is anything else: the planet arrives around a moving camera in LOD rings, what leaves is
released, there is no loading screen and no bounded map, and a fetch stands on the IO pool and
never on the frame path. The DEM is the ground every generator stands on -- a road follows its
grade, a building sits IN it rather than on it, and a horizon is where the terrain puts it.

**WHO AUTHORS: NOBODY, AND THAT IS THE DIFFERENCE.** RAGE and Unreal are engines UNDER an
authoring tool -- a studio places every prop, and the map is content. outshine has no tool for a
person and is not going to get one: the world is DATA (the Earth, fetched), the assets are glTF
an AI built, the game is a SCENARIO an AI declared over a real place, and the engine does the
rest. A Fallout is Boston plus a layer that ruins it; a Cyberpunk is a city plus a layer that
lights it. So everything a level designer does by hand in the references is here either a
GENERATOR or a DECLARATION -- placing, dressing, lighting, populating -- and the door's round trip
(read, write, diff) plus the instrument's picture are the author's only hands and eyes. A verb
that cannot be declared, or a result that cannot be looked at through the door, is a verb the
only author this engine has cannot use. **A WORLDWIDE SANDBOX, data-driven and never authored.**

**WHAT IT IS FOR: ONE GAME, AND I AM THE ONE WHO BUILDS IT.** The engine is MINE -- the author it
has no tool for is an AI, and when the engine is done I build the games with it, for one person,
by declaring them. The game is a single-player open-world RPG over the real Earth, at the level
of GTA, Cyberpunk and Fallout in what a player does and sees, with this difference: the map is
the planet, and every character in it is a MIND -- an LLM with a prompt, a memory and a place
to stand -- and every quest is a scenario's text that those minds carry out. So the engine holds
no dialogue tree and no quest script. It holds the VERBS a mind needs -- PERCEIVE (what stands
here, who, when, what weather, by the names the Earth's data gives them) · ACT (walk, say, give,
take, trigger) · REMEMBER -- and the RULES a declaration sets, against which every act an LLM
proposes is checked before it happens, because a mind may want what the world does not hold.
The mind is a PROVIDER on the IO pool and never on the frame path: a character acts on its last
snapshot while its answer is on the wire, and every answer is logged as an EVENT so a run plays
back frame for frame without the model. **A game is a scenario, a scenario is text, and a
scenario that cannot be written is a game I cannot make** -- inventory, ownership, state, time,
save and load are DECLARATIONS with a round trip, or they do not exist. The proof the engine owes
before a game is built on it: a minimal RPG (one place, three minds, one quest) declared, played,
written back and replayed bit for bit; and hours at the frame budget with a walking camera and
a heap that does not grow.

## Why

Because a frame that holds 16.7 ms while a world lives inside it is one of the few things in
software that is unmistakably *done well* — and you can see it. RAGE and Unreal got there. Not by
being clever in one place, but by being right in a thousand, and by refusing to leave a decision
half-made because it was expensive to finish.

I want that. Not a port of it, not a tribute to it: the same standard, reached here, on hardware
that fits in a pocket. A road you can drive down with trees that grew where they stand, a shadow
that falls where the sun says it should, an engine note that comes from the machine making it
rather than from a file somebody recorded. And underneath it, code a stranger can read and say
"yes, that is how it should be done".

**AND THE QUESTION IS ALWAYS HOW IT LOOKS.** Every rendering gets looked at, by me, and the
judgement is AESTHETIC before it is anything else: is the light right, does the place have weight,
would a stranger stop at it. Not whether a number moved, not whether a rule was kept -- **how it
looks is what counts**, and everything below serves that one judgement. It is the reason to build
this at all: a picture worth arguing with is the only thing this engine is finally for.

Every rule below exists to protect that. None of them is bureaucracy — each one is a day already
paid for, written down so it is not paid twice. When a rule stops serving the engine, it goes.

## The picture it owes

**EVERY CAMERA LOOKS REAL, AND A CAMERA IS ONLY A PARAMETER.** Position, bearing, focal length,
hour -- no value of any of them is a special case and none of them selects a mode. A metre in front
of a wall there is a WALL: brick and joint, or render with its grain, a window reveal with real
depth, dirt where the water ran down it. Thirty metres back there is the street. Ten kilometres out
there is the city, and forty kilometres out the ridge behind it. **The engine owes the same answer
at every one of those**, and "convincing from far away" is not the goal but one distance of it.

**THE WEBCAMS ARE HOW IT IS MEASURED, NOT WHAT IT IS.** foto-webcam.eu is the instrument, because
it is the one place that hands over a REAL PHOTOGRAPH from a KNOWN POINT: several hundred cameras
with published position, height and bearing, each writing 6000 x 4000 every ten minutes and keeping
years of them. So the goal above gets a procedure it can be held to:

> Stand the engine's camera where that camera stands. Set the clock to the frame's timestamp. Let
> the providers answer the place, the hour and the weather of that hour. Render at 720p60 -- and
> lay the two pictures side by side.

**The claim is never the same PIXELS, it is the same PLACE.** A stranger holding both accepts that
these are two pictures of one city, on one kind of day, at one hour -- and accepts it again with
the camera moved anywhere else in that city, down to a metre off a wall. Every camera in the
corpus, every hour of the day, every month of the year, in whatever the sky was doing: clear,
overcast, rain, snow, fog, an inversion with the valley filled and the summits out, and night.

**THE ENGINE RECOGNISES NOTHING, AND THAT IS THE POINT.** One Vienna frame holds a twin-towered
brick church, a box-girder bridge on river piers, a lighthouse on an island tip, a ferris wheel, a
concrete telecom tower with stacked platforms, and ten kilometres of tiled Gründerzeit roofs.
outshine knows none of them by name. What it is handed is what OSM says -- `building=church`,
`bridge=yes`, `man_made=lighthouse`, `attraction=big_wheel`, `tower:type=communication`,
`building=apartments` over a footprint and nothing more -- and a GENERIC generator per type turns
each into something plausible for that region, that epoch and that setting. The church generator
standing in Vienna is the one standing in Salzburg and in Lisbon, with no case for either. A
recognisable copy of a named landmark would be authoring, and outshine has no author: **a twin is
right when it is TRUE TO TYPE, and true to type is a distribution the place, the epoch and the
climate decide.**

**WHAT IS GENERATED AND WHAT IS IMPORTED.** The WORLD is generated; the things standing in it are
imported. Ground, water, sky and everything built or grown -- terrain, river surface, atmosphere and
cloud, buildings and their roofs, roads, bridges, walls, kerbs and the street's furniture, and all
flora -- is a generator's output from a DEM, OSM, the hour and the weather. Vehicles, vessels,
aircraft, people and any prop a scenario names arrive as glTF. A generator always owns the PLACING;
whether the mesh under one of its placements is grown or imported is that generator's own choice,
and for anything with an engine in it the answer is import.

**AT 720p A PIXEL IS A MILLIRADIAN, AND THAT CONVERTS DISTANCE INTO DETAIL.** Derived: a 74-degree
frame across 1280 px is 1.29 rad / 1280 = **1.01 mrad per pixel**, so one pixel covers d x 1e-3
metres at distance d -- a millimetre at a metre, forty metres at forty kilometres. The table is
what a camera's position DEMANDS, rung by rung, and every rung is on screen for somebody:

| distance | one pixel is | what fills the frame | what is owed there |
|---|---|---|---|
| 1 m | 1 mm | one wall | the material itself: a brick is 200-odd pixels and its mortar joint ten, render has grain, a reveal has depth, water has run down it |
| 10 m | 1 cm | a facade and its door | openings with real reveals, sills, downpipes, the ground floor's fittings, wear at hand height |
| 30 m | 3 cm | the street | shopfronts, steps, a kerb with a face, gutter, markings, lamps, trees |
| 200 m | 20 cm | a row of houses | relief no shallower than half a brick, because that is what still casts a SHADOW |
| 1 km | 1 m | a district | the bay rhythm, the piano nobile, roof detail |
| 3 km | 3 m | a quarter | mass, roof form, a row's shared eaves |
| 10 km | 10 m | the city | roof colour, roofline statistics, the grain of a district |
| 40 km | 40 m | the horizon | the DEM's silhouette and the haze in front of it |

**No rung is optional and none of them is a different engine.** The camera decides which rungs are
on screen, the LOD decides what each one costs, and the budget goes to what the camera is actually
looking at: a Vienna frame spends nearly all of it between one and ten kilometres, and one step
forward to a wall spends the same budget on that wall. The AAA distance ladder above and this table
are one rule read from its two ends -- the ladder says what a viewer reads, the milliradian says
what it costs to give it to them.

**THE MATRIX IS THE WORK.** One camera at one hour is a single picture; the corpus is a camera
times a time of day times a season times a weather, and each axis reaches a different subsystem.
The hour moves the sun, the shadows, the exposure and the lit windows. The season moves the
phenology, the snow line, the sun's arc and the water's colour. The weather moves the cloud deck,
the visual range, the wetness of every surface and the fog in the valley. **None of those is a
setting.** Each is a provider's answer, and a look is a declaration evaluated against it.

**WHAT IS MEASURED AND WHAT IS LOOKED AT.** The geometry is TRUTH-grade against the photograph: a
terrain silhouette from a known point is decided by the DEM alone, so a ridge that lands on the
photograph's ridge proves the camera model, the projection, the curvature term and the refraction
term in one line -- four subsystems, one oracle, and a corpus that is free and years deep. The
numbers under it are derived and checkable: the horizon drops d^2 / 2R, which is 7.8 m at 10 km,
31.4 m at 20 km and 125.6 m at 40 km, and refraction lifts it back by about an eighth (k = 0.13
gives 6.8 m, 27.3 m, 109.2 m). Sun azimuth and elevation are read off the shadows in the
photograph. The sky's luminance gradient and the falloff of contrast with distance compare as
curves. **Everything else -- the roofscape, the flora, the water, whether the place READS -- is
looked at, by me, in the PNG**, which is the rule this page already carries.

**AND IT HOLDS SIXTY.** The frame above spans hundreds of square kilometres and arrives on under a
million pixels, all of it streamed around the camera while it moves -- and the next frame may be a
metre off a wall in the same city. That is the ceiling every answer in this document is measured
against, and the reason every rung of the ladder is an LOD rather than a wish.

## What done means

**outshine is to be a REFERENCE DESIGN — something a technical university could teach from.** That
is a harder bar than "correct" and it changes what finishing means: a structural answer has to be
LEGIBLE, defensible from first principles, and carry its reason where the decision is made. Code
that works and cannot be explained is unfinished; a number without its derivation is unfinished;
an abstraction that needs the author present to be understood is unfinished. The reader I am
writing for is a competent stranger, not myself in a week.

Not "it works". The bar is an answer RAGE or Unreal would recognise as their own, or a better one
I can defend with a measurement of THIS tree.

## How I decide

For every structural question, one of RAGE or Unreal already has the right answer. My job is to
hold the better of the two, never to invent a third. Where they agree the matter is closed. Where
they differ, the decision is written down with its reason — the reason is the part I owe. Where
neither faces the question, the item says exactly that and says why the choice is mine.

**A measurement of THIS tree outranks both.**

**THE GPU IS SDL_GPU, THE SHADER IS SPIR-V, AND KHRONOS IS THE REFERENCE.** The engine reaches
the GPU through SDL3's `SDL_GPU` and nothing else, so that it ports; what Apple does with Metal
is SDL's problem and never this tree's. A shader is written once in GLSL, compiled to SPIR-V by
Khronos's own tools, and the tree hands SDL_GPU that SPIR-V -- a Metal or DXIL derivation is a
build product the tree never reads, and a source file in a vendor's shading language is a
finding. **THE ENGINE DOES NOT KNOW THE HARDWARE.** No path in this tree is chosen by a GPU's name, a
vendor's extension or a feature the API does not carry: programmable tile shading, mesh
shaders, hardware tessellation stages and ray-tracing units are a brochure until SDL_GPU
exposes them, however loudly the hardware advertises them. **And that is no ceiling on
geometry**: SDL_GPU has compute pipelines, storage buffers and indirect draws, and a compute
pass that writes vertices and an indirect draw that reads them IS tessellation on this API
-- Ghost of Tsushima's grass is exactly that -- while the fragment stage tessellates in its
own way (parallax, interior mapping, decals). What moves to the GPU is decided by that
route, per technique. What SDL_GPU does expose (a render pass's load and store actions, a
compute pass) is chosen by a measurement on THIS tree against the reference's path, and the
number decides, never the spec sheet. The machine named at the top of this page is the
POPULATION every number here is measured on, and the reason it is named: it is roughly a PS4's
class, so what a PS4 title reached is what this engine can reach here, and that bounds the look.

**THE DOOR SPEAKS FILAMENT AND CESIUM; THE MOTOR HOLDS RAGE AND UNREAL.** `include/` uses the names
a reader already owns — **Filament** for the renderer (`Engine` · `Scene` · `View` · `Camera` · `Renderer` · `Material` ·
`MaterialInstance` · `TransformManager`) and **Cesium** for the Earth (`Georeference` ·
`GlobeAnchor` · `LongitudeLatitudeHeight` · `sampleHeight`). Both are Apache 2.0 and READABLE,
which is the admissibility this page asks of any source. Filament because it is a RENDERER rather
than an engine and its authors ship it on phones, which is this target's constraint; Cesium because
georeferenced 3D is what it is FOR.

**Not Godot**, and the reason is a rule rather than a preference: its vocabulary is a NODE TREE —
`Node3D`, `add_child`, `owner` — and a scenario over a store has no such hierarchy. A name is a
promise, and borrowing those names would promise a shape this engine does not have.

Behind the door the answer is still the better of RAGE and Unreal. **Outward the names a client
expects; inward the engineering those two paid for.** Where a door name and a motor name disagree,
the door keeps the client's word and the motor keeps its own — and the translation happens once, at
the boundary, rather than in the reader's head.

**Two bodies decide, and a third may be CITED.** The table has exactly two columns because a
column has to be READABLE: Unreal is source, RAGE is reconstruction. Ubisoft's three engines —
Anvil, Snowdrop, Dunia — are strong where outshine is weak (crowds, vegetation, procedural
authorship) and none of them can be read, so a column for them would be a vote cast from a slide
deck. **id Tech 4 is GPL and therefore readable**, so it may stand beside a decision as evidence
the way a paper does — inside the item, never as a third column. **Jolt Physics is MIT and
readable** and is the one body that promises determinism across platforms, so for a physics
question it is cited before Chaos. **Cesium** is cited for anything that streams a planet. A source that cannot be read
supports a TECHNIQUE and never decides a STRUCTURE.

**CARLA IS CITED FOR THE ROAD AND FOR THE DRIVER, AND SUMO'S netconvert STANDS BEHIND IT.** CARLA
is MIT and READABLE, and its Digital Twin Tool is the one shipped pipeline that turns an OSM
extract into a drivable town with no hand in between -- the outshine premise, built by somebody
else. Read to the bottom it is three bodies: SUMO's `netconvert` (EPL-2.0, readable) imports the
OSM ways through a type table (`osmNetconvert.typ.xml`: lanes, speed, priority and permissions
per `highway=*`), joins the nodes, and computes every junction's polygon from the extended lane
boundaries of its edges (`NBNodeShapeComputer`); its OpenDRIVE writer turns edges into reference
lines with per-lane widths, an elevation profile and one internal road per connection through a
junction; and CARLA's `MeshFactory` samples each lane at `vertex_distance` across
`vertex_width_resolution`, marks it, walls it and adds sidewalks. For anything a road IS -- its
lanes, its junction, its marks, its network (board:2101, board:2133) -- that is the readable
baseline, and the driver that CARLA's Traffic Manager puts on it is board:2134's. **CARLA IS
CITED FOR WHAT A ROAD IS AND HOW A CAR DRIVES IT, AND FOR NOTHING A VIEWER SEES**: its look is
a simulator's, and the picture stays measured against RAGE and Unreal. **A REFERENCE IS A
BASELINE**: it is adopted where it is right, corrected where it is measured wrong (a
Laplacian relaxation of 100 rounds where a grade profile belongs; a flat map where a DEM stands;
a bake where a frame has to stream), and never a ceiling.

## Before I write

Four questions, answered IN WRITING before a type, a function or a file exists. They land in the
commit, which is what makes every answer visible.

1. **What does Unreal do here, and what does RAGE do?** Naming both is what shows the problem is
   understood
2. **Does this already exist here, unreachable?** `grep` first. A capability is finished on the day
   a declaration reaches it, so the richest work available is usually reaching the one that already
   stands
3. **WHAT PROVES IT, AND WHAT SHOWS IT?** Two oracles, and each answers what only it can. **What
   mathematics can decide is DECIDED mathematically** — a `static_assert` over a layout, a size, a
   trait, an enum's exhaustiveness; a closed form checked against the derivation it came from; an
   invariant a case states whose negative control goes red. That is most of an engine, and it is
   held to a proof rather than to a plot of numbers. **What the RENDERER produces is a PICTURE, and
   a picture is read with EYES.** `make shots` writes every frame as a PNG under its own DIGEST; I
   OPEN that PNG and LOOK at it, myself, every time, and the numbers about it are read afterwards.
   The expectation is written down before the looking, so the image answers a question rather than
   confirming one — a count needs a hypothesis to mean anything and an image needs none. `more than
   one colour` is true of a sky gradient over an empty world, and the picture is what says whether
   a world stands under it. The judgement is **how it LOOKS**, and it is made at the pixel and along
   the SEAMS -- a roof against its wall, a ribbon against its junction -- because a seam is where
   two generators had to agree and where a picture stops convincing. A surprise there earns a
   coordinate and a measurement, never a name like `artefact` that ends the looking
4. **What measurement will show I was wrong?** Name the case, the audit flag or the number, and
   what it reads if the change is bad. That number is what turns a change into a claim

**A MOVED DIGEST IS CARRIED BY THREE THINGS**: the count against the kept reference, WHERE the
pixels moved and why, and the picture looked at. The coordinates NAME a cause where the count
cannot -- 618 pixels at CentralPark read as "small" and were a tower behind a wrong occlusion
window; 59% of OldTown read as "a lot changed" and the IMAGE said the city was gone, grass to the
horizon, in one second. The PNG is opened first and the number read beside it, and
`build/shots/reference/` is this tree's regression test.

**The third question outranks anything already written down.** A cause recorded in an item or a
commit is a HYPOTHESIS until it is measured again — including one written on this page. The habit
is worth more than any conclusion it tests: state the measurement before the work, and the answer
arrives on the day rather than a month later.

## The craft

These are C++ truths rather than decisions about outshine, and they do not move.
- **C++23**, `-Wall -Werror -Wpedantic`, one `-std`; a warning is an error
- **What the compiler can decide is a `static_assert`, never a case** — layout, size, trait,
  catalogue completeness, an enum's exhaustiveness. Stricter than a suite, and unskippable
- **`constexpr` WHEREVER IT IS POSSIBLE, and the same for the values it feeds.** A number the
  compiler can compute belongs in the binary rather than in the frame, and a function that is
  `constexpr` is a function a `static_assert` can INTERROGATE — so this rule and the one above it
  are one rule: constexpr is what makes the assert possible, and the assert is what makes the
  constexpr worth writing. `consteval` where the answer must not reach run time at all. A table
  built at startup is a table that could have been built at compile time, and a derivation left to
  run time is a derivation nothing checks. **clang-tidy does the rest** — every rule that a checker
  already enforces stays out of this page
- **The type system over checkers**: `std::span` / `std::string_view` at boundaries, `std::mdspan`
  for field and instance views, `std::expected` where a refusal carries its reason
- **GEOMETRY HAS ONE CONVENTION AND IT IS OURS.** A normal is unit length, a front face is
  counter-clockwise seen from outside, the render frame is right-handed with +y up and the
  geodetic frame is East-North-Up; the door refuses what breaks it. A file format is somebody
  else's convention: glTF is ONE format this tree ships an importer for, and apart from that
  importer nothing in the engine, the door or a generator knows or allows for it -- an
  importer converts INTO the tree's convention the way a generator produces in it
- **AN ORIENTATION IS CHECKED AGAINST AN OUTSIDE.** A triangle wound consistently with its own
  normal agrees with ITSELF, which is the grade the table below gives a SNAPSHOT -- so the reference
  is external and stated: a carriageway against +up, a body against the ray from its centroid, an
  import against the convention above
- **`alignas` BELONGS AT THE DEVICE BOUNDARY, and equality is `operator==`, never `memcmp`.** A
  record a driver reads keeps the alignment that driver's rows demand and a `static_assert` on its
  size, because the layout is the boundary's word and not ours -- the same rule as SDL3's. But that
  alignment PADS, and `memcmp` compares the bytes nobody wrote: padding, and `-0.0` against `0.0`.
  A defaulted `operator==` compares the members and cannot see either -- measured here, twelve
  indeterminate bytes decided whether a sky table was recomputed
- **Private is the DEFAULT** and a wider door justifies itself; a public data member is an
  invariant nobody can hold. Composition usually; inheritance where a stable interface carries
  shared machinery
- **THE COST OF A LINE IS NOT IN THE LINE.** An array index and a hash lookup read the same and
  differ by two orders of magnitude, because one of them MISSES — and a miss is ~100 ns, ~300
  cycles, long enough to have scanned a kilobyte of contiguous memory instead. So the LAYOUT sets
  the speed and the algorithm only sets the shape: contiguous, one-width, pointer-free, the fields
  read together stored together. A `vector<vector<T>>` or a map on a hot path is a pointer chase
  per element wearing a container's name — measured here, a junction solver walked an
  `unordered_map` 24 times and became one flat node-sorted vector with the same arithmetic to the
  bit. Batch over per-item, fast path on the hot path, and nothing on the frame path allocates,
  locks, touches disk or blocks unbounded. **Ask what the MACHINE does, not what the line says**:
  which loads, how far apart, how many times
- **ON THE FRAME PATH AN ENTITY COSTS O(1) AND THE FRAME COSTS O(N); a preload may pay O(N log N)
  and never O(N²).** But the CLASS is the weaker half of that rule and the constant is the number
  of cache lines touched -- on one problem here a hash map (O(N)) took 565 ms, a comparison sort
  (O(N log N)) 425 ms and a counting sort (O(N)) 92 ms: the class predicted the wrong order and the
  memory accesses the right one. Where a bound genuinely
  exceeds O(N) the answer is a STRUCTURE, not a faster loop: many lights times many objects is
  clustered, not iterated
- **`make lint` DELETES forbidden comments.** `src/` keeps no comments, including the
  client. Only Doxygen survives in `include/`. All comments are allowed in `test/`.
  Scanner regression tests run before mutation; literals and token boundaries must survive.
- **ONE SOURCE PER RULE, AND EVERYTHING ELSE DERIVES FROM IT VISIBLY.** This is SQL's normal form
  carried into code: a functional dependency lives in one place, or the copies drift. But the test
  is **do these change together**, never do these look alike. `[12 + axis]` stood eight times and
  was FOUR meanings that never change together, so eight sites was right; the sun's direction stood
  twice, once negated, looking nothing alike, and those two cannot change apart without lighting a
  scene from one side and shadowing it from the other. Duplication is cheaper than the wrong
  abstraction, which grows a flag, then a second flag, then a `bool isTheOtherCase`. **For NUMBERS
  the rule is stricter and has no exception**: `kSPerHour = kSPerMin * kMinPerHour` carries its
  derivation where `3600` leaves it unstated, and `VisualRangeM(haze)` is the same idea as a
  function -- the rule is the source and the number falls out of it
- **A NAME IS THE ONE THE READER EXPECTS, and the reader is the engineer arriving from Unreal,
  Filament or a textbook -- never this tree's own metaphor.** A verb is `Set`, `Get`, `Place`,
  `Remove`, `Build`, `Update`, `Attach`; a class is the noun of what it holds. A name read at its
  own call site is a grep saved at every other one, so the name the reader already owns is the one
  to write -- and the sweep that brings `Hands`, `Wears`, `Framed`, `Forgets`, `Restand` and `Grounds` over to those
  names is board:2139
- **A name is a promise.** A word that means something else in Unreal or RAGE spends a reader's
  knowledge against them. The engine's vocabulary is LAW — body, joint, degree of freedom, drive,
  constraint, force, contact, integration — and a car, a seat or a door is a SUBJECT a scenario
  assembles. Generators are the exception: a tree grower's whole job is to make one concrete thing
- **SDL3 IS THE PLATFORM, AND WHAT EXISTS IS NOT WRITTEN AGAIN.** SDL3 is a hard dependency
  rather than a choice: the window, the GPU, input, audio, threads, files, time, the clipboard
  -- where SDL3 or one of its satellites supplies the structure or the function, it is the one
  used, and what SDL already decides stays decided there, true to a driver nobody here wrote. A
  thin RAII handle over an SDL type is ownership and belongs; everything above it hands the
  decision back to SDL and keeps one mechanism. The rule reaches past SDL with
  one bound: a capability that is PLUMBING (a format, a codec, a compressor, a font rasteriser,
  a shader compiler) is taken from a library whose source can be READ, and is never written
  here; what the engine IS (the frame, the lattice, the meshers, the atmosphere, the scheduler)
  is written here, because that is the reference design and nothing else supplies it
- **TELEMETRY IS A TOOL FOR PEOPLE, AND I AM NOT ONE.** A person plants a counter ON SPEC because
  a rebuild costs minutes and the moment may not come again; I rebuild this tree in twenty seconds
  and `make shots` hands me the same place back. So a number worth keeping is one a CLIENT reads —
  frame time, memory, triangles, whether the preload finished — and everything else is built the
  day it is needed and removed the same day. A count that states a CORRECTNESS claim is not
  telemetry at all: `houses buried in the ground` belongs in a case with an oracle that goes RED,
  never in a line somebody might read. Asking whether a counter can GO is what removes it; tidying
  it is what keeps it
- **Every number carries its origin** (derived · measured · `[SET]`) with unit and population;
  calibration measures, never decides
- **A diagnostic is a declared LABEL**, never a free literal: `namespace Says` at the top of the
  file, `std::format` at the site. The compiler checks the placeholders and a file's ways of
  refusing read as a list
- **A failure is loud.** Accepting a declaration and doing nothing with it is worse than refusing
  it. Delete on the day you replace; artefacts to the system temp dir or `build/`, never the tree
  -- `compile_commands.json` is the one exception, because clangd looks for it at the root, and it
  is gitignored

## The invariants

Four architectural commitments. Everything else is a decision an item can revisit; these are not.

- **Precision has ONE boundary and it is the camera.** Scene keeps 64-bit positions; the renderer
  is camera-relative in 32-bit. A `float` holding a world position is a defect; a `double`
  reaching a shader is a different one
- **ONE WORLD; everything else is a VIEW.** One space is a convention, one HOLDER is the thing —
  the second holder is what makes two subsystems disagree about the same place. The frame picks
  ONE pre-view translation and every view, light and instance transform builds against that one
- **DECLARED, NOT CODED.** Scenarios declare, the engine behaves. Content = data, engine = verbs.
  A section NOT declared decides nothing — the engine's own default stands in its place, never the
  zeroes of a struct nobody filled in. A scenario is a STREAM: `Declare` seeds, then parts enter
  and leave, and the work a declaration causes is proportional to what CHANGED
- **DETERMINISM IS COMPULSORY OUTSIDE THE SHADERS.** The same declaration renders the same bytes,
  twice, on this machine — so anything assembled from work that ran on more than one thread is
  combined in a DECLARED order and never in completion order. Both references depend on it: RAGE's
  replay plays a drive back frame for frame, and Unreal's automation compares screenshots bit for
  bit and calls a wandering one a streaming bug. `make shots` writes every picture under its own
  digest, which is how a lost determinism is noticed the same day.
- **PROVIDERS ANSWER · GENERATORS EXPAND · THE ENGINE HOLDS THE VERBS · THE RENDERER GIVES THE
  LOOK.** Four roles, and the line between them is TESTABLE rather than a matter of taste. A
  PROVIDER answers a question that has one right answer — how high is it here, what does OSM say
  stands there, where is the sun at 17:40 — and it answers the same way whether it reads SRTM,
  MOLA or noise, because the role is the ANSWER and never the source. A GENERATOR EXPANDS: a
  loader is linear, N bytes in and N bytes out, while eight outline points become four hundred
  triangles and a seed becomes twenty thousand. So the question that sorts them is "can this be
  checked against a truth OUTSIDE this tree" — the height of a point can (SRTM), a house outline
  can (OSM), that house's untagged HEIGHT cannot, and neither can a cloud or the shape of a tree.
  The ENGINE owns the verbs — fetch, hold, ask, place, draw, simulate, hold the frame — and the
  RENDERER owns the LOOK, which is why the atmosphere belongs to the engine and only a cloud's
  FORM to a generator: scattering is physics, form is invention. A generator therefore hands over
  GEOMETRY and a material, never a light. **The planet is a PARAMETER and the Earth is the
  YARDSTICK**: a Mars needs no other engine, only other providers and another catalogue — and it
  is the Earth, alone, that a photograph can argue with
- **FOUR THINGS RUN INDEPENDENTLY — SIM · VIDEO · AUDIO · IO — and what passes between them is a
  SNAPSHOT.** The simulation owns the world and hands the renderer a delta; the renderer draws a
  frame behind and never reaches back; the mixer reads where sources stood when it mixed. **IO is
  the fourth and it is not a task**: a fetch BLOCKS, and a blocking task on a compute worker is a
  worker doing nothing while holding a slot. The two pools are sized by DIFFERENT quantities —
  compute by cores, IO by how many requests may be outstanding — so they cannot be merged, and a
  compute worker is NOTIFIED rather than polling. Unreal separates it (`FIoDispatcher`) and so does
  RAGE (streaming threads beside `sysTaskManager`). **Headless is the fast path, not a degraded
  one.** A subsystem that reads another's live state instead of its snapshot is the defect, because
  it puts a wait where a handoff belongs

## How the tree is arranged

Principles and not a map, because a map goes stale the day a directory moves.

- **A header is PUBLIC only if a client cannot use the engine without it.** The public headers are
  the door and nothing else stands in it
- **A directory IS a dependency tier** and each carries a `reaches` file naming what it may see.
  The include path is DERIVED from that one declaration, so a cross-tier include fails at the
  `#include` with a file and a line instead of being reported afterwards — what Unreal spends
  `Build.cs` on
- **The generators are a library with their own door**: a client registers its own beside them, and
  that tier links with none of the engine behind it
- **THERE IS ONE CLIENT**, the engine through its own door and the camera that measures it: a
  product is a SCENARIO plus one command, so a second program would be a second door
- **The vendor's word and ours stand apart, and the directory says which.** `board/` is one flat
  directory of work items, and `make` is the only way in

## What proves what

**Only a VENDOR CORPUS proves anything** — where a standards body, or a computation carried further
than ours, states the answer. Everything we wrote ourselves is a REGRESSION NET of unknown grade: it
holds the tree to what the tree already did, which is agreement with ourselves. **A refactor is
where that distinction earns its keep** — green says the previous behaviour was preserved exactly,
whatever that behaviour was, so a red there is INFORMATION and is worth more than the green beside
it. The case keeps its wording and the code answers it.

**Two exceptions, both narrow.** A check that pins a SPELLING rather than a property is
mis-specified and the CHECK changes. And a declared CEILING is a baseline: it may only FALL, and
lowering it after a repair is the discipline. Everything else red stays red.

| grade | it holds | it proves |
|---|---|---|
| **SPEC** | a standards body states the answer | conformance |
| **TRUTH** | a measurement or computation carried further than ours | correctness |
| **SNAPSHOT** | another implementation, frozen | agreement, never correctness |
| **INPUT** | nothing is supplied | that we survive it |

**A BENCHMARK IS A TOOL, NOT A GATE.** A PROOF states an invariant and its negative control goes
RED. A RATE has no negative control — it is faster or slower, never wrong — so it can never earn a
tick. It BOUNDS a decision, "the number the change has to beat", and is quoted in the item that
spends it. Every instrument states what it does NOT cover where it prints, so a subject's rate is
read as a subject's and a world's as a world's.

**THE CLIENT IS THE INSTRUMENT AND THE CASES SCORE IT.** `make shots` stands real places on Earth
and writes each picture under its own DIGEST beside what it cost; the cases run that same command
and apply their oracles to its rows. A number from here always has a picture beside it, the picture
is OPENED and looked at rather than summarised, and the digest says when one moved. The instrument
reaches the door and nothing else, so it is held to what a stranger gets.

Every case is a scenario with an invariant oracle whose truth does not depend on our design. **A
tick is earned when its proof stands AND its negative control goes red** — the red control is what
gives the tick its meaning, and it is the half worth the most here.

**`make` IS THE ONLY DOOR** and nothing is started by reaching past it:

| | |
|---|---|
| `make` | strip the comments, build the library and the generators, and the client beside them |
| `make lint` | format · static analysis · the repository's own rules · the door's documentation |
| `make shots` | the places, through the client, each picture under its digest |
| `make test` · `make suite` | the fast gate · one suite by name |
| `make db` · `make doc` | the compile database · the generated documentation |

**Every baseline may only SHRINK.** A recorded count that a commit may lower and never raise holds
new code to zero and lets old code be repaired at the pace it is touched — which is what keeps a
strict analysis switched ON over a grown tree. `make help` is the list.

## How I work

**Order: repair the VISION first if it is short of the benchmark · rebuild onto it · then close
the feature gaps.** A refactor toward a short target arrives somewhere that still has to be left.

**A BATCH IS AS BIG AS THE MEASUREMENT CAN STILL ATTRIBUTE**, and the bound is exactly that: when
a digest moves, the batch has to be small enough to name which change moved it. The digest is the
SAFETY, never the metronome — repairs in different files batch freely, two changes to one behaviour
do not.

**A FUNCTION A LATER GOAL WILL OPEN IS OPENED ONCE.** The order above is a priority, not a wall:
a function a later goal will reopen is cut on that goal's terms, once — the same rule as
refactoring onto the finished vision, applied to my own plan instead of to the tree.

**THE GATES RUN ALONE, AND THE NUMBER COMES FROM THE RUN.** `make`, `make test`, `make shots` and
`make lint` share one nest and one tree, so a gate owns both while it runs: the edit and a second
build wait for its trailer, and a report is the run's word only once that trailer has printed.

**THE COMPILER IS THE CHEAPEST ORACLE IN THE TREE, and a suite is the most expensive.**
`c++ -std=c++23 -fsyntax-only -Iinclude <file>` answers "did I catch every call site" in a second;
a suite answers it in three minutes and one site at a time. Let the compiler judge SHAPE and keep
the suite for BEHAVIOUR.

**A SWEEP OVER A WORD IS A SWEEP OVER FOUR MEANINGS.** `[12 + axis]` appeared eight times in five
files; six read a translation column and two transformed a point. `grep -c` counts the sites and
the reading names the meanings, so every site is read before the regex is written -- and a rename
the compiler can refuse beats both: change the declaration first, then let the errors name the
callers.

**Every item carries the benchmark and the choice** — what Unreal does, what RAGE does, which is
taken and why. An item that cannot say it is not understood yet, and writing that line is most of
the thinking. **Titles say what WILL BE TRUE**: one in the present tense is a complaint, one in the
future is a target somebody can aim at.

**THREE CONVENTIONS ARE WRITTEN DOWN BECAUSE THEY HOLD FOR GOOD.** Everything else about the board
is legible from the board itself and is not this page's business.

- **An item's number is issued ONCE and never again**, and the next one comes from the HISTORY,
  which remembers every id ever filed — the directory remembers only what is still open, so the
  history is what keeps an id unique for good
- **Closing an item is DELETING the file.** What it said is in the commit and `git log` is the
  logbook, so the directory holds exactly what is OPEN and is read at a glance — which is what
  makes it worth reading
- **`active` is said in the item's own commit BEFORE the work** -- the board's only owner mark.
  Several may stand on one chain, each naming what it waits on

Grep the history before filing: a removal was a decision, and the history is where that decision
still stands.

**A finding met while working something else becomes an item in the same round**, even if it closes
in that round: an item is how it becomes something more than one person knows.
