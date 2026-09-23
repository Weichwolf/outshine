Type: defect
State: active
Architecture: ready
Parent: 2234
Depends:
Priority: P0
Area: engine, rendering, streaming
Tags: materials, structures, ground, candidate

# Structure materials exist before ground geometry

## Defect and evidence

A cold Basel Badischer client run (47.568 N, 7.607 E, 4 km sight) fails after
the first structure bake: `a piece refers to a native surface 1 but the subject
has 0 surface slots`. `Models` appends wall/roof surfaces to a partless ground
`Geometry`, then calls `SetGroundGeometry` to make them available before the
terrain mesh exists. That build clears a subject without parts. The first wall
piece then refers to a nonexistent native surface. Any populated region can
hit this; Basel is only the reproducer.

## Ownership and contract

Streamed structure pieces own independent, registered wall and roof materials;
their availability must not depend on the generated ground subject. Register
the two materials in the private world candidate before the first bake. Copy
their source with world candidates and preserve their slot indices across
publication and rebuilds. A failed candidate must not alter published slots.
The ground subject owns only surfaces actually used by its geometry. `TilePieces`
must carry explicit `PieceSurface` references so native API fixtures and
registered generated pieces cannot be confused. Do not create dummy triangles
or surface slots and do not special-case a place.

The structure palette is currently fixed. Reuse the published registration on
later ground revisions rather than appending two new slots every rebuild. A
future variable structure palette needs an explicit versioned replacement
contract; do not silently mutate published slots.

## Falsifiable acceptance

- A cold empty-world candidate accepts wall/roof pieces before any ground mesh,
  then publishes ground geometry with those pieces intact. Repeat after a
  second candidate/revision: registration count and material references remain
  stable. An invalid registered roof fails atomically and leaves prior pieces.
- The public-client Basel scenario reaches a PNG; open and inspect it. Wien and
  Malcesine render unchanged unless a real material defect is demonstrated.
- Run `make format`, focused structure/candidate tests and `LINT_JOBS=2 make
  lint` against the committed code. Record the checked commit and actual
  result; a black GPU fixture alone is no visual proof.
