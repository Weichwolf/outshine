Type: issue
Area: core, ground, render
Tags: layering, frame, precision, world

# A frame is declared, and ECEF is one instance of it

Owner's reading, 2026-08-24: *"ist ecef für einen generischen engine korrekt? funktioniert das
auch für mond, mars, raumstation, raumschiff?"*

Three answers, and only the first is yes.

## As a KIND of frame, it is right

A body-centred body-fixed cartesian frame is correct for any rotating body -- moon, mars, a
minor planet. The mathematics does not change; `(a, e^2)` and the rotation rate do. The generic
name is BCBF and ECEF is its Earth instance.

## As a NAME and as a CONSTANT, it is wrong, and it is in the core

```cpp
src/core/Geodesy.h:10   const double a = 6378137.0, e2 = 6.69437999014e-3;
```

The WGS-84 equatorial radius and first eccentricity, hard-wired inside `GeoToEcef`, taking no
sphere as a parameter. `src/data/Wgs84.h` is a file named after an EARTH datum. `Ecef` appears
89 times across 20 files in `src/`, including `src/core/`, `src/render/` and `src/ground/`.

CLAUDE.md's own table says `any of src/` may not spell *"Earth · Moon · a planet's name or
numbers -- worlds are declared spheres"*, and its own rule says every number carries its origin.
`6378137.0` sitting unlabelled in a core header breaks both, and it breaks the architecture it
sits under: a scenario declares a SYSTEM of spheres, and `GeoToEcef` can only ever answer for
one of them.

## For a station or a ship, it is wrong in PRINCIPLE

A body in free fall has no body-fixed frame to be centred in. Every reference solves this with a
TREE OF FRAMES rather than one world space:

| engine | what it does |
|---|---|
| Star Citizen | Object Containers, nested, each with its own 64-bit origin |
| Kerbal Space Program | floating origin, rails for the unmeasured, physics near the vessel |
| Unreal 5 | Large World Coordinates (double) plus World Partition cells |
| Outerra / Unigine | double precision plus a camera-near origin per frame |

CLAUDE.md's **"One world space"** is exactly the assumption that breaks the day a scenario
declares a system with a ship in it -- and CLAUDE.md ALSO declares that system
(*"travel between them is an actor with thrust and a possession relink"*). The two sentences
cannot both hold.

## The seed is already here

`SubjectPose::Anchor` (`src/render/stages/SubjectTypes.h:94`) IS a local frame: the stage adds
`Anchor - Eye` and hands the shader a float relative to the eye
(`src/render/stages/SubjectDraw.cpp:841`). That is the right mechanism. What it lacks is that
the anchor is a VECTOR IN EARTH COORDINATES rather than a HANDLE TO A DECLARED FRAME, and a
mesh carries exactly one.

## What will be true

- [ ] A frame is a declared thing with a handle: a body's centre and spin, or a vehicle's own
      origin, or the camera. Geometry is expressed IN a frame; the engine composes between them.
      Nothing in `src/` spells `Ecef` as if it were the only one.
- [ ] `GeoToEcef` becomes a query on a declared sphere -- `(a, e^2)` arrive from the sphere the
      scenario declared, and a sphere that declares none is refused at assembly rather than
      silently given Earth's.
- [ ] `src/data/Wgs84.h` is a shipped ASSET's numbers, not a core header: it moves to
      `src/assets/` beside the world templates, where a datum belongs (CLAUDE.md: "Earth ships
      as a TEMPLATE").
- [ ] `CLAUDE.md`'s "One world space" is restated as what it must be: one world space PER FRAME,
      composed. A diagram that keeps the old sentence beside the system-of-spheres paragraph is
      itself a finding.
- [ ] Proving test: a scenario that declares two spheres of different radii and places a body on
      each, with both drawn in one frame; and one that places a body in a frame with no sphere
      at all -- a ship -- and draws it. Negative control: the sphere's radius replaced by Earth's
      -> the second body lands in the wrong place and the case names the distance.

## Comments

- 2026-08-24 -- filed from the owner's question. The measurement that makes it urgent rather
  than theoretical: `6378137.0` is in `src/core/`, not in an asset, and no call site can pass a
  different one.

---

## The precision boundary is right in the code and absent from the map (owner, 2026-08-24)

> *"ist claude.md korrekt? 64bit ecs und 32bit kamera zentrischer renderer."*

**The renderer does exactly that**, and it is the reference pattern (Unreal 5 LWC, Outerra,
Star Citizen):

```cpp
src/render/stages/SubjectDraw.cpp:841   carried[12+axis] += Anchor[axis] - ctx.Eye[axis];  // double
src/render/stages/SubjectDraw.cpp:846   sum += ctx.Mvp16[...] * carried[...];              // double
src/render/stages/SubjectDraw.cpp:854   uniform[i] = (float)placed[i];                     // float here
```

Double all the way to the camera-relative product, `float` only at the uniform push.

**CLAUDE.md did not say so** and now does -- but the owner's correction is the sharper reading:
*"was der code macht ist die schärfste Schreibung"*. Prose in the map is the weakest form of
this statement. The code states it exactly once, at one call site, and prevents nothing at the
next one; a line in CLAUDE.md prevents even less. **The statement belongs in the TYPE**, which
is this tree's own rule -- `static_assert` and the type system over checkers -- and a world
position that CAN be 32-bit is the defect, not a world position that HAPPENS to be 64-bit here.

**The scene half is NOT guaranteed.** `include/outshine/Column.h` and `Store.h` carry no
precision statement at all -- `Column<T>` is generic, so the precision of a position is a
property of whatever component a client registers. Nothing refuses a `float` world position.

- [ ] A world position is 64-bit BY CONSTRUCTION -- a declared type the catalogue owns, which a
      client cannot register a 32-bit stand-in for, and which the render path takes as its input
      rather than a bare `const double *`. The boundary where it becomes `float` is ONE function
      taking the eye, so there is no second place to get it right or wrong.
- [ ] Proving test: a case that walks every registered component naming a position and asserts
      its scalar is 64-bit, and a case that measures the error of a point 20 000 km from the
      anchor through the render path -- with a control that swaps the anchor subtraction to
      `float` and names the metres it costs.
