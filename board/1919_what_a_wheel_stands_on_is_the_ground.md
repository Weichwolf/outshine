Type: bug
State: active
Parent: 1897
Area: sim, world, actor
Tags: measured, physics, architecture

# What a wheel stands on is answered by the ground, not by a corridor

A car does not follow a rail. It reacts to what is under it and around it, and it will share the
road with other traffic. The corridor is a HINT a mind reads on the way to a destination —
CLAUDE.md's own actor chain says so: a controller ASKS pathfinding for a corridor, PERCEIVES, and
acts on actuators. Nothing in that chain says the corridor decides physics.

At b0b59b3a it does:

    src/sim/DriveTick.cpp:147   const Standing on = StandAt(corridor, at.AlongM + armAlongM,
                                                            at.OffsetM + armAcrossM, 0.0);
    src/sim/DriveTick.cpp:149   under[which].Found = fabs(at.OffsetM + armAcrossM) <= edgeM;
    src/sim/DriveTick.cpp:150   under[which].HeightM = on.HeightM;
    src/sim/DriveTick.cpp:151-3 under[which].NormalM[...] = on.NormalM[...];

**Every wheel's height, its normal and whether it is on made ground at all come from the
corridor's own ribbon.** Not from the ground the world composed, not from a surface with a
friction, not from anything a second vehicle could also stand on.

## What that forbids, and it is the product

- **Other traffic is unspellable.** A second car has no corridor of mine, so my wheels can never
  know it is there. Neither can a kerb, a pothole, a painted line or a patch of ice.
- **Leaving the road is a boolean about a ribbon**, not a change of surface. Grip does not fall
  because the ground under the tyre changed; it falls because an offset exceeded a number.
- **The engine knows a car.** `edgeM`, `OffsetM`, `AlongM` are carriageway nouns deciding
  physics, which is board:1897's count of 46 growing rather than shrinking.

## What already exists to answer it properly

`Ground().At(lat, lon)` returns a sample with a height; `src/world/generators/ContactMaterial.h`
carries a surface with its own friction; `VegetationTemplates::Row::Ground` carries a colour and
a roughness per class. The pieces are in the tree and the contact does not ask them.

## What will be true

- [ ] A contact asks the GROUND where it is and what it is: height, normal and friction from the
      surface under that point, whatever put it there.
- [ ] The corridor is a hint the mind reads. Nothing physical is decided by it, and a body that
      leaves it is off the road because the ground changed, not because an offset exceeded a
      bound.
- [ ] Proving case: a wheel driven off the made surface loses grip by the FRICTION the ground
      under it declares, and the same wheel over a second body's occupied space finds it.
      Negative control: the corridor ribbon restored as the source, and neither is visible.
