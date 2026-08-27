Type: issue
State: open
Parent: 1953
Area: door

# A transform is spelled twice in the door

**Benchmark** — Unreal has ONE `FTransform` (translation, rotation, scale) and everything that
places anything takes it: an actor, a component, an instanced-static-mesh element, an asset
import. RAGE has `Mat34V`. **Taking Unreal** — a named transform type, because the loose form
cannot be validated, cannot be composed, and cannot say which convention it is in.

`include/Scenario.h` declares where things stand as LOOSE ARRAYS, four times over:

    Body      double AtM[3], FacingXyzw[4]
    Instance  double TranslationM[3], RotationXyzw[4], ScaleXyz[3]
    Slot      double AtM[3]
    Contact   double AtM[3]

and `include/Geometry.h` carries the same fact as `Place(part, const double modelM16[16])`. So a
placement is a triple-plus-quaternion in the declaration and a 4x4 in the value, and every path
between them converts by hand.

This is the type census (`EveryTypeNameIsDeclaredOnce`) seen from the other side: the census finds
one WORD meaning two things, and this is one THING spelled two ways, which no string walk can see.

- [ ] one transform type in the door, and both headers take it
- [ ] the conversion between a triple-plus-quaternion and a 4x4 happens in one place
- [ ] a scenario written in code and the same scenario in XML place a body identically, proven by
      a case that builds both and compares the matrices
