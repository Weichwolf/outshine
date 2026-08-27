Type: issue
State: open
Parent: 1953
Area: door

# A transform is spelled twice in the door

**Benchmark** — Unreal has ONE `FTransform` (translation, rotation, scale) and everything that
places anything takes it: an actor, a component, an instanced-static-mesh element, an asset
import. RAGE has `Mat34V`. **Taking Unreal** — a named transform type, because the loose form
cannot be validated, cannot be composed, and cannot say which convention it is in.

`include/Scenario.h` declared where things stand as LOOSE ARRAYS. `Standing{AtM, FacingXyzw,
ScaleXyz}` now carries it and `Body`, `Placement` and `Instance` all take it -- `Body` was the
last holdout with its own `AtM[3]` beside `FacingXyzw[4]`.

`Slot` and `Contact` keep a bare `AtM[3]` and that is CORRECT rather than unfinished: a seat and
a suspension pickup are POINTS on a body, not placements, and Unreal spells that distinction too
-- `FVector` for a point, `FTransform` for a placement. Collapsing them would make the door say a
contact has an orientation, which it does not.

What is still two spellings is `include/Geometry.h`: `Place(part, const double modelM16[16])`
takes a 4x4 where the declaration takes a `Standing`. **This one is not obvious and the item owes
the reasoning.** glTF requires a node matrix to be decomposable to TRS, so no information is lost
in principle -- but the decomposition is numerical, and a mirrored part (negative scale) has no
unique answer. Unreal converts at the boundary and keeps `FTransform` inside; RAGE keeps
`Mat34V` and has no TRS type at all. So this is where the two benchmarks genuinely differ, and
the row has to be decided before the code moves.

This is the type census (`EveryTypeNameIsDeclaredOnce`) seen from the other side: the census finds
one WORD meaning two things, and this is one THING spelled two ways, which no string walk can see.

- [ ] one transform type in the door, and both headers take it. `Scenario.h` does; `Geometry.h`
      still takes a 4x4, and whether it should is the open question above
- [ ] the conversion between a triple-plus-quaternion and a 4x4 happens in one place. The
      READING of a placement now does -- `ReadStanding` replaced three separate spellings in
      `ScenarioRead.cpp`, one of which did not read `scale` at all, so the same attribute meant
      different things in different elements. The conversion itself is untouched
- [x] a scenario written in code and the same scenario in XML place a body identically. The
      comparison could not even be WRITTEN before this: the body's placement was its own pair of
      loose arrays and the instance's was a `Standing`, so there was nothing on the left of it.
      Body and instance both land on 0.000e+00 against the C++ value, exactly rather than
      closely, and an undeclared scale reads as unit rather than as zero. Negative control:
      halving `qw` in the reader lands the body at 2.500e-01 and turns the case red.
      proof: outshine/scenario/ScoreWhereADeclarationPutsABody
