Type: feature
State: open
Area: generators
Tags: scope

**The road is drawn geometry, the car drives on it, and the terrain yields to it**

**The owner's ruling, in five sentences:** *what the car drives on must agree with the geometry the OSM
generator draws. The geometry the generator draws IS the carriageway. It is probably still surfaces --
the geometry must be fully three-dimensional, because a road has thickness. Roads and buildings may
change the elevation data, raising and flattening it, the way SimCity does. The OSM-generated geometry
acts on the terrain.*

**This inverts the direction the corridor currently works in.** Today the reference line samples the
elevation source and takes whatever gradient it finds -- which on Munich to Hamburg gives -17.1 % at
km 313.9, a gradient no motorway has. A real road does not follow the ground: **it cuts and fills the
ground until it is drivable**, and what is left over is an embankment or a cutting.

## What must be true

- [ ] **The car drives on the DRAWN surface and not on an abstract line.** One geometry, two readers:
      the renderer draws it and the contacts stand on it. A carriageway the physics agrees with but
      nobody can see, or one that is drawn and cannot be driven, is two roads
- [ ] **The road declares its own vertical alignment** -- grades joined by vertical curves, fitted to
      the terrain within a declared tolerance and bounded by what the class and the vehicle allow --
      and the TERRAIN is deformed to meet it
- [ ] **Cut and fill are published per metre of road**, because that is the number that says whether
      the reconstruction is plausible: a road that fills 30 m is a viaduct nobody marked
      (`board:1518`), and one that fills 0.3 m is a road
- [ ] **The geometry is a SOLID and not a surface.** A carriageway has a thickness, a shoulder, a
      verge and a side slope; a bridge deck has a soffit something passes under (`board:1518`'s
      clearance rule needs it)
- [ ] **A building sits on ground it flattened**, by the same mechanism, so a house on a slope has a
      pad and not four corners at four heights
- [ ] **The deformation is a FIELD the ground generator reads**, never a mesh edit, or two generators
      disagree about where the ground is

## Comments

**This is why the drive keeps leaving the line.** The corridor is fitted in plan from OSM ways -- which
is right -- and then draped over raw elevation posts 96.53 m apart, which is not a road profile. The
vertical curvature that produces is what the speed profile must slow for and what launched the car at
km 117.1 before the crest term existed.

`board:1505` already carries cut and fill as a task. This item is the wider statement it belongs to:
the drawn geometry, the driven surface and the terrain are one thing seen three ways.

## And it is tile-based AND streamed

**The owner's ruling completes the pipeline:** *elevation data shapes terrain -> OSM data builds
infrastructure AND also shapes terrain. That has to work tile-based but streamed as well.*

So the deformation is not a mesh edit and cannot be: it is a **field a ground tile evaluates**, and a
tile must be able to evaluate it from what is loaded around it.

- [ ] **A tile's ground is a function of its own OSM features and its neighbours' reach**, and the
      reach is DECLARED -- an embankment's side slope has a length, and that length is how far into
      the next tile a road can push
- [ ] **The result does not depend on which tiles happen to be loaded.** That is `board:1518`'s
      tile-shift relation applied here, and it is the single most valuable check this whole pipeline
      has: shift the tile grid by half a tile and the ground inside must not move
- [ ] **A tile whose neighbour has not arrived says PENDING and not a guess**, the way
      `GroundSample` already answers Resolved, Pending or Hole

### The centreline is not a lane, and the car drives in a lane

**The owner's ruling:** *a rural road has two opposing lanes and a motorway has at least four -- so do
not drive exactly down the middle.*

The corridor's reference line is the centreline of the whole carriageway. Driving it means driving on
the centre line, which on a two-lane road is head-on into the oncoming lane, and it also OVERSTATES the
lateral budget by a factor of the lane count: on a 12 m motorway the budget is not
`6.0 - 0.906 = 5.09 m` but the lane's own `1.5 - 0.906 = 0.594 m`.

- [ ] **The lane count is derived from the declared width**, because every width in
      `src/assets/world/vegetation.json` came from a standard that names its cross-section -- RAA RQ 28
      is 2 x 3.75 m plus a 2.5 m hard shoulder per carriageway, RAL RQ 11.5 is 2 x 3.5 m, RASt's 5.5 m
      residential street is two 2.75 m lanes
- [ ] **The car's line is the centre of ITS lane**, offset from the reference line, and which side
      depends on the country's driving side -- a declaration, and `board:1524`'s Dover to Calais route
      exists to catch it
- [ ] **The tracking budget is the LANE half-width minus the car's half-width**, everywhere it is used:
      the speed the pursuit lag allows, the grip reserved for holding the line, and whether a wheel has
      left the road
- [ ] **A oneway carriageway has all its lanes in one direction** and a two-way one splits them, so
      the offset depends on what the way declares and not on the width alone

## Comments

**This makes the car SLOWER and that is correct.** With the whole carriageway as budget the pursuit law
allowed 179 km/h on a motorway; with one lane it allows about 60. A car that used the full width to
hold its line would be crossing the centre line to make its corners, which is what the number was
quietly permitting.
