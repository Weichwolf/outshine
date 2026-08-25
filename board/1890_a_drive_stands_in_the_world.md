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

### FOUND, and it is one line

Two inferences from those numbers were wrong before the third measurement settled it -- the axis
order of `OriginEcef` was suspected twice and is correct both times. Measured directly:

    the ring's origin ecef      (4.17144e6, 4.65332e6, 1.271e6)
    Munich's true ecef          (4.178e6,   0.856e6,   4.720e6)

That origin is the ECEF of latitude 11.5, longitude 48.1 -- the pair the other way round. The
ring was built somewhere in the Indian Ocean off Somalia, the terrain source answered because a
terrain source answers everywhere, the tiles meshed, and the ground stood 2712 km away at an
altitude of zero.

    GroundPatchwork.cpp:60   Ground::ToTileFracClamped(Ground::Geo{over.LatDeg, over.LonDeg}, ...)
    TileGeodesy.h:11         struct Geo { double LonDeg, LatDeg, AltM; };

A positional initialiser that reads correctly and means the opposite. It is a designated
initialiser now, so the next reader cannot make the same mistake, and `World.cpp:459` was checked
and is right.

Proving test: `harness/outshine/geo/ScoreTheTileRoundTrip` -- a coordinate turned into a tile and
back at three zooms in all four quadrants, worst movement 2.8e-14 degrees. Negative control in
the same case: the identical round trip with the pair swapped lands 357 degrees away, so a
conversion that reads them in the wrong order cannot pass.

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

      ### Walked to the bottom 2026-08-25, and the bottom is ONE sentence

      The branch was opened again and the ground got four steps further before the seam itself
      refused. Each step was a real defect, each is fixed, and none of them was the seam:

      1. `Append` copied the guest's material indices unchanged, so the ground's `Material = 0`
         collided with the vehicle's own first slot and the terrain was painted onto the roof.
         It shifts them clear now. Proving test:
         `harness/outshine/content/ScoreWhatAnAppendKeeps`.
      2. `Bound()` after the append covered the 8 km ring, so the camera framed the ring and the
         derived shadow radius became kilometres. `Subject::BoundsOf(parts, ...)` gives the
         bounds of the first N parts and both readers use it.
      3. `ClearsNearPlane` walked every vertex in the picture. A studio holds one subject and
         nothing may sit inside its near plane; a WORLD has geometry everywhere, which is this
         item's own title. It walks the framed parts now.
      4. `Restand` set `Joined_` AFTER `Build`, so every one of those fixes read zero while the
         camera was being derived. It is set before.

      What refuses now is the sentence at the bottom: **the vehicle's geometry is in ASSET UNITS
      and the ground's is in METRES, and `Append` puts them in one vertex buffer.** The two
      scales are separated per part by the placement matrix -- `perUnit` for the car, identity
      for the ground -- and the framing runs BEFORE any placement is known, so it measures a car
      64 times too big beside a ground at true size. The near plane lands at 637 m and the
      ground's nearest vertex at 624 m, and the refusal is correct.

      No placement can fix that, because the mixing happens in the buffer. The ground has to be
      its own subject with its own scale, which is what "a compositor's draw item" means and why
      this box is the one the other three wait on.

      ### The obvious fix was tried and WITHDRAWN, and the reason is the order of the stand

      CLAUDE.md already says where the factor belongs -- "glTF is metres by specification;
      `assetWheelbase / wheelbaseM` corrects a non-conformant asset, **applied once at the
      stand**" -- and today it is applied in the placement matrix on every frame instead.
      Moving it onto the vertices was attempted: `Subject::ScaleTo(factor)` scaling `Positions_`
      once, `Live::Carry` losing its `perUnit`, `Live::ScaledBy` scaling immediately.

      It does not converge in four edits and the reason is the ORDER, not the arithmetic:

      - `Live` is opened during `Declare`, when `MetresPerUnit` is still 1, so the first stand
        reads and poses unscaled geometry
      - `ScaledBy` is called later, from `Engine::Routes` after `AssembleDrive`, by which time
        the picture already stands
      - `Rigged::ModelShiftM` is computed in metres and applied through a matrix that ALSO
        scaled, so it has been arriving scaled twice -- roughly 0.0145 m where 0.93 m was meant.
        Unscaling the matrix makes that shift take effect for the first time, and everything
        downstream of it moves.

      So the change is right and it is not four edits. It is: the scale is known at ASSEMBLY,
      before the stand opens, and `ModelShiftM` is stated once in the space it is applied in.
      Withdrawn rather than half-landed, and the four fixes above stand on their own.

      **Tried a second time with the order fixed, and withdrawn again.** The factor IS derivable
      at `Declare` -- `wheelbaseM / assetWheelbase` is in the scenario, not only in the rig -- so
      the door can carry it before the stand opens, and it does not need `ScaledBy` pushing it in
      after assembly. That part is sound and measured: the door declared 0.0155498 m per asset
      unit at `Declare` time.

      ### MEASURED from inside Live::Build, and the premise of both attempts was WRONG

      Printed at each branch of the first stand:

          DIAG Build:          Built=0x0  Stands='.../scene.gltf'  mpu=1.00000000
          DIAG after Pose(0):  span 132.728 x 94.267
          DIAG at Carry:       parts=258 joined=258 span 132.728 x 94.267 x 297.584

      297.584 asset units x 0.0155498 = **4.63 m**, beside 2.06 m of width and 1.47 m of height.
      That is a 3-series to the centimetre. The geometry stands in asset units, the matrix
      scales it, and the answer is RIGHT. The "1102 units" that sent both attempts chasing a
      scale bug was a measurement of a different state, and only two of the three axes were
      printed the first time.

      So the scale is not the seam and moving it onto the vertices is a refactor, not a fix. It
      may still be worth doing -- CLAUDE.md asks for it and it removes a per-frame multiply --
      but nothing about the ground waits on it.

      ### What the ground actually waits on

      `Live::Stand` resets every placement to identity:

          Stood_ = Studio{};
          Stood_.PartPlacement.assign(Geometry_.Parts().size(), identity);

      and `Composes` calls `Restand` DURING assembly, before any `Carry` has run. So at the
      moment the camera is derived, the car's 297 asset units and the ground's 8000 metres are
      compared RAW, with no placement separating them -- and every refusal measured today
      followed from that one fact.

      The fix is that a restand keeps the placements the parts already had, rather than throwing
      them away and rebuilding a picture that has never been placed. That is one seam, it is in
      `Live::Stand`, and it is where the next attempt starts.

      ### Attempts five and six, both measured, both out of the tree

      **Five: keep the placements through a restand.** Carrying `Stood_.PartPlacement` forward in
      `Live::Stand` instead of assigning identity moved the near plane from 637 m to 214 m, so it
      IS part of the answer. Placing the body before composing -- calling `Rides` ahead of
      `Composes` -- put it back to 624 m, which was a guess and behaved like one. Neither is in
      the tree: with the ground branch shut the first changes no observable behaviour, and an
      unprovable change does not land.

      **Six: compose on the first `Advance` rather than in `Assemble`.** This clears the
      assembly-order refusal entirely -- placements exist, the client's eye is set, and `Composes`
      reaches `Restand`. What refuses one step further in is precise:

          Restand -> Build -> Look derives a NEW camera, near plane 637.888958 m

      the same number the derived framing produced before, so `Live::Look` took the derived
      branch and `HaveEye_` was FALSE -- immediately after `Rides` set the eye. `HaveEye_` is
      written in exactly one place (`Live.cpp:270`) and read in exactly one (`:324`) and is never
      reset. Either `Rides` returned before its eye block, or the restand is looking at a
      different `Live`. That is a printf away and it is where the seventh attempt starts.

      Six is also NOT in the tree, and the reason is not that it failed: moving the composition
      makes its refusal SILENT, because the driver prints its carried lines when assembly ends
      and the refusal now happens after. A failure that stops being loud is worse than one that
      stands. Moving the composition means moving where its refusal is read, and that is part of
      the change rather than a detail after it.

      ### Seven: the ground reaches the draw list, and three fixes are named

      `HaveEye_` was a red herring and the printf said so: `Look` runs with `HaveEye_=1` and
      `parts=517`. The refusal never came from `Look` at all -- `Look` is called only from
      `Live::Advance`, and this refusal happens during assembly.

      It comes from `Submit` -> `Place`, and there:

          GltfStudio.cpp:372   if (!Aim(renderer, subject, eye, error)) { return false; }

      `Aim`'s fourth parameter is `standsInside` and it defaults to false. `Studio` CARRIES
      `EyeStandsInside` and `Place` does not pass it. A field carried and not read, which is the
      same class as `Lit.Declared` (board:1900), `Render.Declared` (board:1901) and
      `ShadowRadiusM` (board:1867).

      Three fixes, each right on its own and all three needed together:

      1. `Place` passes `studio.EyeStandsInside` to `Aim`
      2. `Live::Stand` sets `Stood_.EyeStandsInside` and `Stood_.Eye` from `HaveEye_`, because a
         studio rebuilt from scratch must know whether its eye was declared or derived
      3. `Rides()` runs before `Composes()`, because a world is composed AROUND a placed body

      With all three the ground reaches the draw list for the first time: **517 batches drawn and
      517 cast**, 258 of them the car and 259 the ground, where every previous attempt drew 0 or
      refused. The still gains 4700 opaque pixels and shows no ground plane, so the geometry is
      submitted and lands somewhere outside the frame -- that is the eighth question.

      None of the three is in the tree. `Live::Eye` is not on the door, so a case that declares an
      eye and checks the picture is not refused for geometry near it needs a drive, and a drive
      needs the network. Unprovable changes do not land, twice tonight already. What lands is the
      measurement: the ground reaches the draw list, and the fix that got it there is three named
      lines.

      ### And one suspicion removed rather than confirmed

      `Place` ignoring `studio.EyeStandsInside` looked like it would refuse a glTF that declares
      its camera INSIDE its own geometry -- an interior shot, a cockpit view, both of which the
      format permits without restriction. A case was written for it: the same triangle seen from
      a camera 4.00 m away and from one 0.01 m away.

      Both STAND, and they still stand with the near-plane check's bounding-box early-out
      hollowed out, so the walk cannot be reached on that path at all. The case was deleted
      rather than landed: it asserts something true that no seeded defect makes red, and a case
      that cannot fail is the one this project already caught itself writing once today.

      So `Place` not passing `EyeStandsInside` is still a field carried and not read, and it is
      still worth fixing -- but it is NOT what refuses the ground, and this item no longer
      suspects it.
