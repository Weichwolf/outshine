Type: feature
State: open
Area: test, generators
Tags: corpora, measured, benchmark

# A synthetic corpus STATES the ground, so the oracle is arithmetic

**Benchmark** — **NEITHER UNREAL NOR RAGE FACES THIS**, and the item says so rather than inventing a
column: Unreal's automation compares a screenshot to a screenshot it took earlier, and RAGE's replay
compares a drive to a drive it recorded -- both grade against THEMSELVES, which is the exact thing a
synthetic corpus exists to avoid. The bodies that DID solve it are cited as evidence: **ASAM
OpenSCENARIO / OpenDRIVE conformance suites** ship synthetic roads whose answer is known by
construction, **`asam-ev/qc-opendrive`** ships 181 PAIRED valid/invalid cases, and **Khronos
glTF-Asset-Generator** generates its own assets and states `loadable: true/false` beside them. All
three agree: **generate the input, never the answer.**

## Why

With real OSM every finding costs forensics — is the defect ours, or a driveway with a 0 m turning
radius? On a terrain declared as `z = f(x, y)` the correct height is KNOWN at every point, so
"a road hovers" becomes a subtraction. Two foreign oracles, neither of them ours:

    the terrain function      mathematics states z(x, y)
    the design standards      RAS-Q / RAA state max gradient, min crossfall, batter

We generate the INPUT. If we also stated the answer it would be agreement with ourselves.

## How: N structures x M terrains

**M = 10 terrains**, each analytic, ascending in what it stresses:

    1  flat                        the null control -- a failure here is unconditional
    2  plane 2 %                   drainage-scale slope
    3  plane 10 %                  at the residential gradient limit
    4  plane 30 %                  beyond every class limit: the road MUST cut or switch back
    5  sine ridge, 200 m / 20 m    the crest vertical curve, and the chord that flew over it
    6  sine valley, 200 m / 20 m   the sag curve, and where water would pool
    7  sine grid, 100 m / 10 m     a 2D field, so no axis is privileged
    8  atanh escarpment, 40 m      a cliff: daylighting or nothing
    9  fBm noise, 5 octaves, 15 m  the realistic case
   10  fBm + escarpment            the adversarial combination

**N = 25 structures**, each declared as OSM-shaped input:

    ways        1 straight · 2 right-angle corner · 3 hairpin at 8 m · 4 doubling back at 0 m
                5 T junction · 6 crossroads · 7 shallow Y fork at 10 deg · 8 roundabout
                9 two ways crossing with NO node · 23 dead end · 24 with width · 25 without width
    vertical   10 bridge over a way · 11 bridge over water · 12 tunnel
               13 embankment (fill) · 14 cutting (cut)
    buildings  15 square · 16 on the fall line · 17 touching a way · 18 sharing a wall
               19 courtyard (a ring with a hole)
    surfaces   20 water polygon · 21 water meeting a way · 22 unsealed track (becomes a CLASS)

**250 cells, not 400.** The number follows from what DIFFERS: a tunnel on flat ground and a tunnel
on a 2 % plane are the same test twice. Cells that are meaningless are declared N/A and that
declaration is itself information -- "a bridge over water on a 30 % plane" says water does not
stand there.

## The oracle, per cell

    the body's underside sits within [z(x,y) - thickness, z(x,y)]     computed
    no two bodies overlap in plan AND height                          computed
    closed, manifold, consistently wound, no self-intersection        computed
    gradient <= the class's maxGradient                               RAS-Q / RAA
    crossfall within [min, e_max]                                     RAS-Q / RAA
    a cut or fill matches the daylight batter                         RAS-Q

## THE DECLARED RELIEF REACHES THE FIELD AND NOT THE RING -- what is RULED OUT

`<world><relief/></world>` is read, written and round-tripped, and `TerrainTiles::RawGrid`
synthesises the height field at the SOURCE, before the cache and before the fetch, so both the drawn
mesh and the height query read one ground. The flat null control renders correctly. A `sineRidge`
does not, and the loss is bounded to one hop with these ruled out by measurement:

    the shaped branch runs                  2 176 times          not skipped
    the parameters that arrive              amp 20, wave 400     not lost in the handover
    the field it fills                      -20.000 .. +20.000   the synthesis is exact
    the pool's declaration under its lock   applied per job      not a data race
    the field's side                        256 and 257 alike    not TerrainMesh's stride check
    PostingM's indexing                     as the fill writes   not a transposed read
    the ring that is drawn                  0.021 m              flat

So it is between `TerrainGrid::Holding` and the vertices `LayPatchwork` returns, and what stands
there is `TerrainMesh::Over` and `ChunkBuildEcef`. The latter DETECTS the column count by scanning
for where the east coordinate falls back, and refuses the whole tile when that detection fails --
`if (!(C >= 2 && R >= 2 && nVertices % C == 0)) return 0;` -- which is the shape of a defect that
would flatten exactly like this. It is the first thing to instrument next.

Writing down what is ruled out is the point: the next pass starts with six answers rather than
seven questions.

## THE OSM HALF: the seam, and the storage it has to fill

The terrain half is done and the structures follow the SAME pattern -- declare in the door,
synthesise at the source -- so the search is over and only the build remains. Written down because
finding these seams was the expensive part.

`OsmField` is the source. It has one entry today, `Accept(tx, ty, vectorTile)`, which takes raw MVT
bytes; a declared structure must NOT go through an encoder, so it fills the arrays directly:

    Points_    lat, lon pairs, appended in order
    Rings_     Ring{First, Count, Exterior}      -- a way is one ring, Exterior false
    Features_  Feature{FirstRing, RingCount, FirstTag, TagCount, Tile, Layer, Type, bbox}
    Tiles_     Tile{Z, X, Y, FirstFeature, FeatureCount}
    Tags_      indices into Keys_ / Values_, interned through Intern()
    Keys_ Strings_ Values_ with KeyIndex_ / StringIndex_

`Type` is what the consumers switch on -- `BuildingField` takes `f.Type == 3`, so a building is 3
and a way is not. `Layer` indexes `Layers_`, which the field is constructed with.

The door mirrors `<relief>`:

    <world>
      <relief kind="sineRidge" .../>
      <osm>
        <way kind="residential" widthM="6">lat,lon lat,lon ...</way>
        <area kind="building" heightM="8">lat,lon ...</area>
      </osm>
    </world>

and the reader, the writer and the round trip come with it, as the relief's did.

**And the camera is decided rather than inherited.** A cell is ONE structure on 200 m of ground,
seen obliquely from 80 m, filling the frame, flat-shaded, no atmosphere -- because a 30 cm gap is
what has to be visible, and in a twelve-kilometre vista it is not. The first grid renders were a
whole town seen from altitude, which is the wrong instrument for the question.

## What will be true

- [ ] The terrain is a declared function, not a fetched tile, so a cell runs offline and in
      milliseconds
- [ ] Each cell states its expected answer arithmetically; none of them states OUR output
- [ ] Negative control: a deliberately broken cell -- a road laid 1 m above its own terrain -- goes
      red in every geometric oracle above
- [ ] Real data resumes only when the grid is green, and the grid stays in the gate afterwards

## What it found on the first pass, 2026-08-31

The corpus renders. 25 structures x 10 terrains, each cell one declared relief plus a handful of
declared ways and areas, no network, ~4 s a cell. The FIRST look at a hard cell found the defect the
grid was built to find.

`cross` on `plane30` draws ONE arm of a crossroads. Both ways are laid -- `ways laid as ribbons 2`,
`ways it refused 0` -- so the second is not missing, it is BURIED:

    the deepest the ground stands over one     43.502 m
    how far on average                         22.525 m   over 72 vertices

The profile is designed with a bounded gradient, which is right, and the ground is then never cut to
meet it, which is the whole defect. The consensus answer is the one already written above: design
the alignment, CUT where the design surface is under the terrain and FILL where it is over, and let
the corridor surface replace the terrain inside its boundary. Today only the road moves.

**The number to beat: deepest 43.502 m, mean 22.525 m, on `cross-plane30`.** A repair that does not
drive both toward zero has not done the thing.

Two more, from the same first pass:

`hairpin` on `flat` -- ONE declared polyline, `(-80,0) (0,0) (-80,16)`, comes out as FOUR
disconnected fragments with visible gaps between them. Splitting a way at a corner tighter than its
class can drive is a defensible decision; leaving the pieces UNJOINED is not, because the network is
then not continuous, and continuity is the thing the whole derivation is for.

`roundabout` on `flat` -- the ring lays and the four approaches lay, and where an approach meets the
ring there is an overlap rather than a junction: one arm stops short of the ring with a gap you can
see at cell scale.

All three are the same missing idea from different sides: the derivation lays PIECES and never makes
them one BODY. Cut and fill is the ground yielding to the road; welding is the road being one thing
where two of its pieces meet.
