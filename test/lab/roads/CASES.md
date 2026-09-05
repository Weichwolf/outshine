# The cases a road on this planet has to hold

**What this bed produces is a CONSTRUCTION ORDER.** OSM says where a road runs and what class it
is; the DEM says what the ground does. Out of the two comes what a civil engineer would draw: an
ALIGNMENT (the axis in plan, the gradient in profile, the cross-section with its crossfall), and
the EARTHWORKS that make the ground carry it (cut, fill, batter, wall, portal, abutment). The
engine then builds exactly that -- the surface a wheel touches and the ground it stands on. So
every number here is a road builder's number with its table beside it, not a modeller's taste:
RAL 2012 and RAS-Q for the alignment and the section, AASHTO's Green Book where it agrees, and
netconvert for what a junction IS. A road that is drawn without them looks like tape on a hill.


Every case is a NETWORK (nodes, ways with tags) laid over a TERRAIN (a height function and,
where it matters, a water level), and the lab's synthetic bed (`synthetic.py`) builds each one,
solves the map (profile, junction polygons) and the structure (lane and junction surfaces),
and checks the INVARIANTS below. A case that is not in this table does not exist; a case whose
check cannot go red proves nothing. Real places (`profile.py`, `band.py`) are the p99 sample
of these; the synthetic bed is where each is held in isolation.

## Invariants -- what "correct" means, per product

| id | invariant | holds on | negative control |
|---|---|---|---|
| I1 | C0: every way through a node reads ONE height there | the map | two heights per node (the corridor's way) reads a step |
| I2 | C1: a way's profile is continuous in grade between its nodes | the map | linear interpolation reads a grade jump at every node |
| I3 | the profile deviates from the DEM by less than the DEM's own error where the DEM is the authority (no bridge, no tunnel) | the map | a smoothing length of ten postings cuts a hilltop by more than the error |
| I4 | a bridge's deck is a smooth curve between its abutments, above the water level plus clearance, and the abutments stand ON the terrain | map + structure | a deck tied to the river bed dips below the water |
| I5 | a tunnel's portals stand ON the terrain and its bore stays under the terrain by at least the cover between them; the terrain above is untouched | map + structure | a tunnel drawn on the DEM surfaces mid-hill |
| I6 | no step along a junction's boundary: the junction surface's height at every point of a leg's edge equals the leg's surface there (< 1 mm) | structure | one plane per junction on a slope reads centimetres at the steeper leg |
| I7 | a surface is closed and welded: every edge of the road mesh has exactly two faces except at the free boundary, and no two vertices closer than the weld tolerance are distinct | structure | a corridor's cap not welded to the junction reads an open edge |
| I8 | tile seams: a way crossing a tile border reads identical vertices from both tiles | structure | a per-tile halo one node short reads a crack |
| I9 | the driving surface (analytic, from the map) and the drawn surface (structure) agree to < 1 cm at every station and lane offset | both | a mesh sampled from the DEM instead of the profile reads decimetres |
| I10 | determinism: the same case built twice gives identical bytes | both | a hash-map iteration order in the junction builder |
| I11 | the ground meets the road: the terrain is stamped to the road's edge (cut or fill), the batter reaches the ground, and nothing floats or sinks | structure + ground | a road at the DEM's own height on a cross-slope floats on the downhill side |

## The builder's numbers, and where each comes from

| what | value | origin |
|---|---|---|
| design speed by class | 130 / 110 / 100 / 80 / 70 / 50 / 30 km/h | [SET] RAL 2012 EKA/EKL, RASt 06 |
| longitudinal grade, max | 4 / 6 / 8 / 12 / 15 % by class | [SET] RAA 2008, RAL 2012, RASt 06 |
| crossfall, normal crown | 2.5 % | [SET] RAS-Q |
| superelevation, max | 6 % | [SET] RAL 2012 (7 % on a motorway) |
| side friction | 0.10 at 100 km/h | [SET] RAL 2012 |
| superelevation from radius | q = v^2 / (127 R) - f | derived, the point-mass balance |
| rotation rate of the section | 1:200 of the half width per m at v >= 70, 1:100 below | [SET] RAL's Anrampung |
| crest / sag radius, min | 0.75 v^2 / 0.30 v^2 (m, v in km/h) | derived from RAL's H_K, H_W |
| kerb upstand | 0.12 m | [SET] RAS-Q, DIN 483 |
| embankment batter | 1 : 1.5 | [SET] RAS-Q |
| cutting batter | 1 : 1 | [SET] RAS-Q |
| retaining wall above | 3 m of batter | [SET] the point where a slope becomes a wall |
| deck grade, max | 4 % | [SET] RAA/RAL for a structure |
| clearance over road or water | 4.5 m | [SET] RAS-Q / the class's Lichtraum |
| tunnel cover, least | 3 m | [SET] the rock a bored tunnel keeps above its crown |
| junctions joined within | 10 m along a way | [SET] netconvert --junctions.join |

## Terrain (T)

| id | terrain | why it is its own case |
|---|---|---|
| T1 | flat plain | the base case; every invariant trivially, so it catches builder bugs |
| T2 | constant slope ALONG the road (grade 5 %, 15 %) | the profile's grade; junction on a longitudinal grade |
| T3 | constant CROSS-slope (5 %, 15 %, 30 %) | the road's cross-section against the hill: cut uphill, fill downhill, the batter |
| T4 | crest (convex vertical curve, sag radius 300 m) | C1 over a hilltop; sight-line; the profile must not cut it |
| T5 | sag (concave, valley bottom) | fill; drainage; the profile must not fill it |
| T6 | terraces (steps of 3 m every 30 m) | sub-posting steps the DEM aliases; the smoothing length |
| T7 | cliff (a 20 m step over one posting) | the DEM's word against the road's: a road never climbs a cliff, a path may (steps) |
| T8 | river valley: V-profile with a water level plane | bridges (I4), banks, the road along the bank |
| T9 | coast: a plateau meeting sea level | roads along the shore; nothing below the sea |
| T10 | mountain: switchbacks on a 40 % slope | hairpins with tight radius, superelevation, retaining walls |
| T11 | noise: T1 plus white noise of 1 m per posting | the smoothing length against DEM noise |
| T12 | DEM hole: T3 with a missing posting (NaN) | a refused sample; the profile must not read zero |
| T13 | DEM with a baked bridge: T8 plus a ridge along the bridge line | a LiDAR DEM that holds the deck already: the profile must not stack a bridge on it |
| T14 | DEM coarser than the road: T4 with a 90 m posting and roads 10 m apart | aliasing; roads sharing postings |

## Infrastructure (R)

| id | network | invariants stressed |
|---|---|---|
| R1 | straight two-lane road, 500 m | I1-I3, I9 |
| R2 | gentle curve (radius 200 m), tight curve (30 m), hairpin (12 m) | superelevation, inner-edge welding, self-intersection of the offset |
| R3 | S-curve | the offset's sign change |
| R4 | T-junction, 90 deg; skewed 30 deg | I6, I7: the junction polygon from the legs' offsets (netconvert's shape) |
| R5 | X-junction, 90 deg; skewed | I6 with four legs |
| R6 | Y fork / merge (acute 20 deg) | the polygon's reflex corner, slivers |
| R7 | roundabout (radius 15 m, four legs) | a ring way with junctions on it |
| R8 | dual carriageway with a median, and a turn across it | two ways side by side; a junction that spans both |
| R9 | on-ramp / off-ramp (a leg leaving at 10 deg and diverging) | tapers, the merge nose |
| R10 | grade-separated interchange: a bridge (R1 on layer=1) over a road (R1 on layer=0), with ramps | I4 + I6 at two levels; the two roads never share a node |
| R11 | multi-level: bridge over bridge (layer 2 over 1 over 0) | stacked decks |
| R12 | cul-de-sac (a dead end with a turning circle) | a way end with no junction |
| R13 | parking area (a polygon), pedestrian area | a pad, not a ribbon |
| R14 | steps, a footpath, a track (unsealed) | no class grade; projected, not built |
| R15 | roads of different widths meeting (a 12 m primary and a 4 m service) | the polygon's asymmetry |
| R16 | one-way pair (two parallel one-ways 20 m apart) | independent profiles, shared terrain |
| R17 | a road crossing a tile border (two tiles) | I8 |
| R18 | bridge over a river (R1 over T8 water) | I4 |
| R19 | viaduct: 600 m bridge with piers every 60 m, over T5 | I4 with a long deck |
| R20 | tunnel through a hill: R1 with tunnel=yes through T4's crest | I5 |
| R21 | underpass under a railway (rail on layer 0, road tunnel=yes layer -1) | I5 with the ground kept above |
| R22 | embankment: road 5 m above the plain (T1) | I11 fill; the batter |
| R23 | cutting: road 5 m below (T4 crest cut through) | I11 cut; retaining walls where the batter would exceed the corridor |
| R24 | causeway across a lake (road at water level + 1 m over water) | I4 without a span: a fill through water |
| R25 | ford (a track through a river) | the surface goes UNDER the water; no bridge |
| R26 | dam crest road | a road on a structure that is itself the terrain's edge |
| R27 | elevated urban road (a bridge=yes way over a city street grid) | I4 over roads, not water |

## What a bridge IS, and what a tunnel IS -- a deterministic heuristic

A generator has no engineer to ask, so the TYPE follows from numbers OSM and the terrain
already carry. The rule below is deterministic (same input, same structure) and every threshold
is a span a real structure of that type covers; the references are the standard span tables
(Leonhardt, *Bruecken*; the Eurocode's span ranges; FHWA's bridge inventory by type). What the
type decides is what a viewer READS from a distance: the depth under the deck, the rhythm of
the piers, the presence of an arch or a cable.

| span between piers | structure | depth of the deck | what stands under it | seen at |
|---|---|---|---|---|
| < 8 m | culvert / slab | 0.3-0.6 m | the fill itself | a stream under a country road |
| 8-25 m | reinforced concrete slab or beam | span/20 | abutments only | every motorway overpass |
| 25-60 m | prestressed concrete beam, 4-6 girders | span/22 | piers, one row | Ruhrgebiet's Autobahn crossings |
| 60-150 m | steel or composite box girder, haunched | span/18 at the pier, span/45 mid | tall piers | Wuppertal's valley crossings, Bern's Lorraine |
| 150-500 m | arch (concrete or steel) or cable-stayed | deck slender, span/60 | one arch, or a pylon per 0.4 span | Kohlbrand, Zurich's Sihlhoch, SF's approaches |
| > 500 m | suspension | span/100 | two towers, a main cable | the Golden Gate |
| any, rail | the same, one class heavier: rail loads are 2-3x road | span/16 | more piers | every Berlin S-Bahn viaduct |

The material follows the span the same way: concrete below 60 m, composite to 150 m, steel
above. A `bridge:structure`, `bridge:material` or `man_made=bridge` tag in OSM overrides the
heuristic where it exists (about 3 % of bridges carry one, measured on taginfo), which is the
rule everywhere here: the data decides where it speaks, the heuristic where it is silent.

| tunnel | when | portal | what the ground does |
|---|---|---|---|
| cut and cover | cover < 6 m, or `tunnel=building_passage` | a retaining wall each side, a lid | the ground is cut open and closed again |
| bored | cover >= 6 m | a portal head wall in the slope | the ground is untouched above the crown |
| gallery / avalanche shed | on a mountain side, one wall open | columns on the valley side | half cut, half built |
| underpass | a road under a road, `layer=-1` | wing walls | a trough with a wall each side |

## Data pathologies (P)

| id | pathology | must hold |
|---|---|---|
| P1 | duplicate nodes (two nodes at one position, two ways) | one node after snapping; I1 |
| P2 | zero-length segment | no division, no NaN; the profile ignores it |
| P3 | a gap of 0.5 m between two ways that should join | snapped (kNodeSnapM = 2 m); a gap of 5 m stays a gap |
| P4 | two ways with identical geometry (a duplicate way) | one structure, not two z-fighting |
| P5 | bridge=yes with no ground continuation (the way starts mid-air) | the deck ties weakly to the DEM and is COUNTED; nothing at -19 m |
| P6 | a way with 400 nodes 10 cm apart | no 1e14 grade; stations from the node positions |
| P7 | layer=1 without bridge=yes | treated as ground (OSM's word) and counted |
| P8 | a road inside a building footprint | the building wins the pad; the road is cut |
| P9 | a road inside water without bridge or ford | counted; drawn as a causeway |
| P10 | DEM hole under a node | the node's height from its neighbours; I3 |

## Buildings, beside the roads (B) -- `research/world.md`, "Buildings from OSM"

| id | case | must hold |
|---|---|---|
| B1 | a house on T3 (cross-slope 15 %) | the pad is level, the skirt reaches the ground on every side, no floating corner |
| B2 | a house on stilts (building:levels + min_height / a pier) | the mass starts at min_height, the ground under it untouched |
| B3 | a house over water (a boathouse) | the pad at the deck height, the water under it |
| B4 | a house at a retaining wall (a terrace) | the wall is the pad's edge; the pad does not spill down the slope |
| B5 | a house whose footprint touches a road | the road's kerb and the house's wall share the edge; no gap, no overlap |
| B6 | building:levels, height, roof:shape (gabled, hipped, flat, pyramidal) | the roof is built from the tag; a missing tag falls to the generator's rule with its origin |

## What stands and what does not, measured 2026-09-05

All 33 cases green. The roundabout took three rules, each with its measurement: netconvert
JOINS junction nodes into clusters (`--junctions.join`, `NBNodeCluster`) and a ring tagged
`junction=roundabout` is one junction WHOLE, its two-legged nodes included -- leaving them out
made the ring its own leg four times and read 8.75 cm of step; a cluster's shape is the hull
of its members' node shapes, so their regions cannot overlap (ten edges carried three faces
before); and a roundabout's drivable surface is an ANNULUS, not the hull's disc, because the
island is not carriageway -- the disc's crown fell 0.375 m to a centre 15 m from the ring.

## What the bed does for each case

1. builds terrain and network from the case's parameters, deterministically
2. solves the MAP: node heights (the profile QP of `band.py`, bridges and tunnels as their
   own fidelity and clearance terms), junction polygons from the legs' offsets
3. builds the STRUCTURE: lane surfaces by sweeping the cross-section along the profile,
   junction surfaces from the polygon lifted onto the through road's surface, decks,
   portals, skirts; welds vertices; splits at tile borders
4. checks the invariants above with a tolerance that has an origin, prints them, and writes
   a picture (matplotlib: plan, profile, and a 3D view; Blender for the look when it matters)
5. a case's row in `README.md`'s inventory names what went to C++ and the number it has to reach
