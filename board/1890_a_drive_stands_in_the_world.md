Type: bug
State: open
Parent: 1547
Area: clients, sim, render
Tags: measured, precision, drive

# A drive stands in the WORLD, and the studio is not a place

Three origins meet on a drive and they are not one: the drive stands on the CORRIDOR's own ENU
origin, the ground ring is laid about `Patchwork::OriginEcef`, and a scenario without a sphere
stands on the studio anchor. CLAUDE.md says *one world space*, and *precision has ONE boundary
and it is the camera*.

What has been settled, and it is most of the chain: `Live::Carry` takes a placement in WORLD
METRES and builds the model scale (`Rigged::MetresPerAssetUnit`, 0.01555 m per asset unit for
the F31) into the placement it writes, so nothing divides by a scale any more. The factor-64
mismatch between the camera and the body is gone.

## SETTLED 2026-08-25: the eye sits in the cabin, and the offset was 0.55 m

Both earlier readings of the still were wrong, mine and the review's, and they were wrong the
same way: an inference from a bounding box and a field of view, with the car's lower edge assumed
to be at the ground. It is not; the car leaves the frame. The arithmetic gave "6 m above and
10-13 m away" and the truth is 0.55 m.

What settled it was measuring instead of inferring. `Engine::Numbers()` now carries the
placement, refreshed in place each tick rather than appended, so the value channel answers where
the body, its mesh and the eye actually are:

  the body, up            524.444 m
  the mesh it carries, up 524.887 m       -- the corridor runs at 523.9 m
  the eye, up             525.115 m

`eye - mesh` was 1.00 m, exactly the declared seat. The placement was never wrong.

**The defect: a view offset is DECLARED from the road and was APPLIED from the centre of mass.**
The declaration measures its vehicle from the ground the wheels stand on -- contacts at 0.333,
centre of mass at 0.550, seat at 1.220, every one a height above the road. The body's origin IS
the centre of mass, which is why `Rigging` subtracts `CentreM` from every mount. The view offset
was not subtracted, so the camera rode 0.550 m too high -- above a roof that stands 1.45 m off
the road. The still was the car's roof from outside.

With the subtraction the same still shows the A-pillar, the windscreen surround, the roof lining
and the near door mirror: the cabin from within.

Proving test: `harness/outshine/physics/ScoreWhereASeatIs`, whose oracle is that a seat is inside
the car -- under the roof, inside the width, between the axles -- in the frame the body actually
uses. Negative control: the same seat taken RAW into that frame stands 0.32 m ABOVE the roof, and
the case checks that too, so it cannot pass on either reading.

## What was measured at c0de1b18, and superseded above

Three runs of the 302 m overridden drive, the same command, one number changed in the copied
scenario:

| `<view id="eyes" offsetY=…>` | the still |
|---|---|
| `1.220` (as shipped) | the car occupies rows 636-719 and columns 515-1147 -- the bottom 84 of 720 rows, seen from OUTSIDE and above |
| `3.220` | **nothing opaque at all**, 0 of 921 600 pixels |
| `51.220` | byte-identical to `3.220` |

So the declared eye IS taken and it DOES move in metres: two metres of it empties the frame.
What is wrong is that the SUBJECT is not under it. From the bounding box and the declared
65-degree field, the car's drawn geometry stands roughly 6 m below the eye and 10-13 m away
from it -- a residual placement offset of order ten metres between the body the camera rides
and the mesh that is drawn for it, not a scale error.

`<seat at="driver" x="-0.494" y="1.220" z="0.003">` and `<view id="eyes">` declare the same
point. A first-person view from that point must show the bonnet and the cabin; it shows the
outside of the car from six metres up.

## MEASURED 2026-08-25: the branch was opened and the ground landed on the roof

`overADrive` was set to `Drove && !Ground.Declared && !way.Fine.empty()` and the ground composed
without refusing. The still says what happened: the pale ground tile is drawn on the car's ROOF,
its rear window and its boot lid.

`Engine::State::Composes` ends with `Gltf::Subject world = Standing->Shown(); world.Append(
laidGround)`. Appending the ground to the VEHICLE's glTF makes it a part of that subject, so it
inherits the subject's placement AND its model scale -- 0.01555 m per asset unit for the F31.
An 8 km tile ring becomes 125 m of geometry sitting on the car.

So the fourth box below is not a tidy-up after the third; it is the thing that BLOCKS the third,
and the order is: the ground becomes a compositor draw item, and only then does `overADrive` go.

### And the frame conversion is wrong, arithmetically

The placement split is NOT the defect and that was worth ruling out: `Live::Carry` gives parts
below `Joined_` the vehicle's matrix and parts above it the second matrix, `Engine::State::Rides`
passes identity as the second, and the counts are right -- 258 parts for the vehicle, 259 with the
ground. The ground part DOES get an identity placement in world metres.

It lands 2712 km away regardless. Measured, with the branch open:

| | east | up | south |
|---|---|---|---|
| the ground's first vertex, in the corridor frame | 2 712 490 m | -0.0000082 m | 4 064 380 m |
| the body | 0.44 m | 524.5 m | -0.89 m |

The arithmetic names the fault exactly. The conversion is

    inFrame.east  = (where.LonDeg - way.FrameLon) * way.PerLonM
    inFrame.south = -(where.LatDeg - way.FrameLat) * way.PerLatM

Munich is lat 48.14, lon 11.58, and 48.14 - 11.58 = 36.56.

    36.56 * 74202 = 2.71e6   which is the measured east
    36.56 * 111195 = 4.06e6  which is the measured south

So `where.LonDeg` carried 48.14 and `where.LatDeg` carried 11.58: the Geo the ring converts to
holds latitude where the corridor reads longitude. `EcefToGeoWgs84` itself is sound --
`LonDeg = atan2(Y, X)` -- so what is swapped is upstream of it, in the axis order the tile
builder writes `OriginEcef` and its vertices in. Altitude comes back as 8 micrometres rather
than 524 m, which is the same fault seen on the third axis.

And a second defect stands beside it: `ground.Material = 0` reuses the vehicle's own first
material slot, so the ground's surface is painted onto every vehicle part that uses slot 0. In
the still the car's roof, rear window and boot lid wear the terrain.

The branch is shut again, and the refusal now states the real reason instead of denying what the
same run proved:

  a drive laid a corridor and a ground could be composed about it, but the ground is APPENDED to
  the vehicle's own glTF and would inherit its placement and its model scale -- measured, an 8 km
  tile ring lands on the car's roof. The ground is a compositor's draw item and until it is one
  this refuses

## What still blocks the ground

`const bool overADrive = false;` (src/clients/Engine.cpp:284) nails shut the only branch a
drive could compose a ground by, so `Engine::State::Composes` refuses with *the scenario
declares neither a sphere nor a drive that laid a corridor* on a run that has just laid 823
corridor stations. The refusal denies what the same run proved. The reason the branch is shut
lives only in commit 657b5903, and the source may carry no comment, so it is unreadable from
the tree.

## What will be true

- [ ] A driven body's placement and the mesh drawn for it are the SAME point in the one world
      space, in metres, as a 64-bit position. The corridor states where its origin is; the
      drive does not carry a private one.
- [ ] The CAMERA is in that space too. Proving case: the 302 m drive with `<view id="eyes">`
      shows the bonnet and the cabin, and `<view id="chase" distanceM="7.0">` shows the car
      from seven metres. Negative control: `offsetY` raised by two metres changes the picture
      and does not EMPTY it.
- [ ] No branch in the engine is nailed shut by a constant. `overADrive` goes, and a drive
      composes its ground.
- [ ] The ground is a COMPOSITOR's draw item, not a part appended to the vehicle's glTF.
      `Gltf::Subject world = Shown(); world.Append(ground)` is the shortcut this seam is made of.
