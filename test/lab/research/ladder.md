# The ladder: what each generation of games solved, and where this lab stands on it

**The proposal was: work up graphically from the nineties to today, through the iterations the big
engines went through. It is right, and I would change one thing about it.**

Why it is right: each generation solved a NAMEABLE problem, and the solutions COMPOSE. You cannot
have screen-space reflections before a depth buffer, physically based materials before linear
lighting, or a city that reads before a facade grammar. A ladder also fixes the thing I have been
worst at all day -- saying precisely WHERE the work stands. "Not AAA yet" is a mood; "rung 5 of 8,
and the gate is a junction that carries its own surface" is a position.

**The correction: the ladder I climb is CONTENT, not shading.** The rendering ladder --
Quake's lightmaps, Doom 3's stencil shadows, Crysis's SSAO, UE4's PBR, UE5's Lumen and Nanite --
is the RENDERER's, and this tree's renderer is decided elsewhere: SDL_GPU, the frame budget, and
`research/world.md`'s own split of CPU / compute / fragment. Walking it here would be work on the
wrong axis, and it is already moot in the lab: Blender's Cycles is years past AAA and costs me
nothing to use. What my generators owe is the CONTENT each era learned to build, and that ladder
is just as real:

| rung | the era's own answer | the gate, measurable | where this lab stands |
|---|---|---|---|
| **R0 the block** | Quake, 1996: mass only, one material, no openings | closed, consistently wound, welded; a place builds with no red | **cleared** -- 81/81 road cases, 41/41 building cases |
| **R1 the silhouette** | Half-Life, 1998: what breaks the outline against the sky | no body is a plain prism; chimneys, dormers, parapets, ridges, gutters as GEOMETRY | **cleared** -- L1 adds 348 tris to a 526-tri mass |
| **R2 the opening** | Half-Life 2, 2004: a wall is a facade with holes, and the ground floor differs | every reachable wall carries real holes with a reveal >= 0.10 m; no glass inside solid geometry | **cleared** -- the wall is meshed with its holes from L2 |
| **R3 the material** | Crysis 2007 into PBR 2011: measured reflectance, one shading model | glTF `pbrMetallicRoughness`; every albedo inside its measured band; an 18 % card exposes to 118 | **cleared, and carried further** -- 25 stock materials, exposure measured, and each one now carries the surface's own DIMENSIONS (`grain_m`, `relief_m`, `mottle`, `unit_m`, `joint_m`, `bond`) in glTF `extras`. A laid material is LAID: a 240 x 71.5 brick in a stretcher bond with a 12.5 mm joint, built in METRES and projected triplanar, because noise at a brick's size reads as dirt. `material_card.py` stands the whole stock in one frame of raking light |
| **R4 the rhythm** | Assassin's Creed / CityEngine, 2007: a grammar, not a texture | a storey hierarchy with a measurably taller beletage; party walls; no two neighbours identical | **cleared for the block**, open for the house and the hall |
| **R5 the street** | GTA V, 2013: the street level is where the budget goes | a JUNCTION with its own surface, stop lines, crossings and drains; furniture per 100 m at the reference's order | **junction cleared** -- one kerb ring around the NETWORK with RASt 06 corner radii, dropped kerbs at every crossing, markings interrupted across a junction, give-way teeth on the minor leg only, terrain cut by the street (I17, I18, I19); open: lanes, parking, signals from OSM, gullies on a real profile |
| **R6 the wear** | The Witcher 3, 2015: nothing is clean, and the dirt has a REASON | a wetness/exposure field drives the material: streaks under sills, moss at a plinth, wear on a desire line | **not started** |
| **R7 the density** | Cyberpunk 2077, 2020: the street is FULL | interiors behind glass, signage, awnings, aerials, cables, balcony clutter, parked cars; instances per m2 at the reference's order | **not started** |
| **R8 the continuum** | UE5, 2021: LOD you cannot see | the same body at rung n and n+1 differs under a threshold in screen space at the switch distance; the world to the sight limit | **partial** -- ladders exist per generator, no morph, ground reaches 12 km of 240 |

## What each open rung actually needs

**R5, the junction. WHAT IT COST, measured 2026-09-06.** A network of ribbons is not a street
network, and every one of the five defects below was invisible to every check that existed and
visible in the first picture:

| what was wrong | how it read | what now says so |
|---|---|---|
| the footway was built PER WAY | at every junction the minor leg's 2.50 m footway lay across the whole 10 m major carriageway, and the major's kerb ran unbroken across the minor's mouth | **I17** `walk on carriageway m2`, control 37-121 m2 over six networks |
| no corner radius | two ribbons crossing, which is not a junction | the kerb line is the channel CLOSED with RASt 06's radius -- 6 m residential, 8 m where a lorry turns |
| the terrain ran STRAIGHT UNDER the road | a road has a crown, so of a 10 m carriageway a 3 m ribbon showed and the rest was grass | **I19** `ground over street m2`, and `ground.py` cuts the sheet with the street's own outline |
| the paint read the TERRAIN's height | a zebra on a crowned road stood 0.12 m proud and Cycles drew four rows of kerbstones | every marking reads `kerbline.Surface`, which interpolates the DRAWN mesh |
| the height off the road was the NEAREST triangle | at a corner fillet the nearest leg SWITCHES and the height jumps by the difference of their crowns: a crease and a bright wedge at every corner | eight triangles, inverse-square weighted; the footway's worst face went from 77 deg off level to 30 |

**LANES, closed the same day (board:2158).** `roads/lanes.py` derives the lane list from the
surveyor's tags where they exist and from RASt 06's Fahrstreifenbreite and a per-class cap where
they do not, and every marking is a lane boundary. I20 over all 50 synthetic networks: 226 lanes,
0 outside the 2.25-3.75 m band. Control, the one-line-down-the-middle construction: 96 of 206
outside, widest 8.75 m, red on 32 of the 50 networks.

What is still open on the rung: parked cars in the parking lanes (that is R7's), the signal and
the sign from OSM at a real junction, a gully on a profile that has a low point, and a turn lane
at a junction -- `turn:lanes` is in the data and nothing reads it yet.

**R6, the wear.** The rule is that dirt has a REASON: water runs off a sill and streaks the wall
under it; it stands at a plinth and grows moss; a foot wears a path where people actually walk.
So the generator owes a field -- how wet, how sheltered, how walked -- and the material reads it.
That is a per-vertex or per-UV quantity and it is cheap; what it is not is noise.

**R7, the density.** The reference's street is FULL, and the count is the gate. A shopfront has a
sign, an awning, a menu board and a bin; a balcony has a rail, a plant and a bicycle; a facade has
downpipes, aerials, satellite dishes, cables and meter boxes. Interior mapping behind the glass is
the one item that is a renderer's job and not a generator's, and `research/world.md` already places
it in the fragment stage.

**R8, the continuum.** The ladders exist -- the tree's is its recursion depth, the building's is
its rung, the road's is its station step -- but nothing measures the SWITCH. The gate is a picture
pair: the same body at rung n and n+1 from the distance the switch happens at, differing under a
threshold in screen space.

## How a rung is cleared

A rung is cleared when three things hold at once, and never on two of them:

1. **a check that can go RED**, with a live negative control
2. **a picture, looked at**, at the distance the rung is about
3. **a number with its origin** -- the reflectance, the standard, the count

That is the same bar the rest of this tree carries, applied to a thing that is usually judged by
feel. A rung with a green check and no picture is not cleared; a rung with a good picture and no
check is not cleared either, because the next change will break it and nothing will say so.
