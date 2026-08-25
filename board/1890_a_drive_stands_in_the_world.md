Type: bug
State: open
Parent: 1547
Area: engine, sim, render
Tags: measured, precision, drive

# A drive stands in the WORLD, and the studio is not a place

CLAUDE.md says *one world space*. A drive stands on the CORRIDOR's ENU origin, the ground ring is
laid about `Patchwork::OriginEcef`, and a scenario without a sphere stands on the studio anchor.
The car has no ground, no horizon and no sky, and this is the item that ends that.

## What is TRUE at a32c4919, measured by running the binary

The overridden 302 m drive (`--from 48.13720,11.57560 --to 48.13600,11.58200`) leaves 9 stills
and prints:

    MEASURES the body, up        519.794 m      the mesh it carries, up 520.184 m
    MEASURES the eye,  up        520.463 m      -- eye - mesh = 0.279 m, the declared seat
    MEASURES batches the picture draws = 258 batches      (the car alone)
    CARRIES  the ground did not compose: ... the ground is APPENDED to the vehicle's own glTF
             and would inherit its placement and its model scale ... until it is one this refuses

The eye is in the cabin and the placement chain is right: the 0.55 m centre-of-mass subtraction,
the material shift on `Append`, the framing over the framed parts only, `Joined_` before `Build`,
and the tile round trip are all landed and each has a proving case. The refusal now states the
real reason instead of denying what the same run proved.

## What blocks it, and it is ONE line plus THREE that are not in the tree

    src/engine/Engine.cpp:283   const bool overADrive = false;

nails shut the only branch a drive could compose a ground by. Behind it, three named lines were
measured to get the ground into the draw list for the first time -- **517 batches drawn and 517
cast, 258 the car and 259 the ground**, where every earlier attempt drew 0 or refused:

1. `GltfStudio.cpp:372` `Place` passes `studio.EyeStandsInside` to `Aim`, whose fourth parameter
   defaults to false. A field carried and not read -- the class of `Lit.Declared` (board:1900),
   `Render.Declared` (board:1901) and `ShadowRadiusM` (board:1867)
2. `Live::Stand` sets `Stood_.EyeStandsInside` and `Stood_.Eye` from `HaveEye_`, because a studio
   rebuilt from scratch must know whether its eye was declared or derived
3. `Rides()` runs before `Composes()`, because a world is composed AROUND a placed body

**None of the three is in the tree, and the reason given -- that a case for them needs a drive and
a drive needs the network -- does not hold.** The door already publishes what such a case would
assert: `Engine::Numbers()` carries *batches the picture draws* and *the ground did not compose*,
`Engine::Capture(path)` writes the still, and the f31 corpus carries the network the drive needs.
`harness/outshine/door/ScoreWhatTheShadowCasts` is the shape, written this same session, through
`include/` alone. A change that moves 258 batches to 517 is provable through the door as it
stands; withholding it costs six nodes on the distance axis (board:1864).

The mixing that refuses one step further in is real and is the fourth box below: the vehicle's
geometry is in ASSET UNITS and the ground's in METRES, and `Subject::Append` puts them in one
vertex buffer while framing runs before any placement is known. That is what "a compositor's draw
item" means -- and the three lines above land WITHOUT waiting for it, because they are what puts
the ground in the draw list at all.

## What will be true

- [ ] The ground is a COMPOSITOR's draw item with its own scale, not a part appended to the
      vehicle's glTF. `Gltf::Subject world = Shown(); world.Append(ground)` is the shortcut this
      seam is made of.
- [ ] No branch in the engine is nailed shut by a constant. `overADrive` goes, and a drive
      composes its ground.
- [ ] The still shows ground under the car and a horizon behind it. Proving case: the 302 m
      drive through `include/` alone, asserting the composed batch count exceeds the vehicle's
      own and that the still carries opaque pixels below the horizon line. Negative control:
      `overADrive` restored to `false` and the same case is red.
