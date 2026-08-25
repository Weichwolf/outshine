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

## What is measured at c0de1b18, and it is NOT "the view is not taken"

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
