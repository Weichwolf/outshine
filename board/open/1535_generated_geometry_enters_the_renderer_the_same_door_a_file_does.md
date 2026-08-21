Type: task
Parent: 1498
Area: clients
Tags: scope

**Generated geometry enters the renderer by the same door a file does**

`Clients::Live::Declaration` carries `Stands` -- a PATH. `Live::Build` reads it into a
`Gltf::Document`, poses that into a `Gltf::Subject`, and then resolves the surface table **from the
document**. So geometry that was generated rather than loaded has no entrance at all, and the only way
in would be to write a GLB and read it back.

**The owner named that and refused it:** *do not serialise and deserialise glTF internally, use an
internal representation.* Round-tripping a file format for data already in memory is exactly the waste
the decomposition exists to avoid.

**And `CLAUDE.md` already says what the shape should be:** *a file is a generator kind, never a
scenario special case.* So the answer is not a second entrance -- it is ONE:

> `Live` takes a built `Subject` and a declared surface. Whoever has a file builds the subject from
> it. Whoever has a generator builds the subject from that. Neither is special.

## What must be true

- [x] **`Live` takes a `Subject`** -- `Declaration::Built` beside `Stands`, and declaring BOTH is a
      refusal, because which of the two the picture came from would be unanswerable. Moving the file
      reading out of Live entirely is still open
- [x] **The surface comes from the DECLARATION when there is no document** --
      `ResolveDeclaredSurface(geometry, row, table)`: one slot, one material row, no rasters.
      Asphalt is a base colour and a roughness. Reading it from `Scenario::Surfaces` rather than
      from the caller is still open
- [ ] **A generated part and a loaded part are indistinguishable downstream**, which is the test: the
      renderer must not be able to tell, and `board:1512`'s ladder must apply to both
- [x] **No GLB is written to stand a corridor up.** `Gltf::Emit` stays for what it is for -- handing a
      part to something outside this process

## Comments

**This is the last thing between the headless drive and the window.** `src/corridor/Ribbon` already
produces exactly what `Gltf::Piece` wants -- float positions, normals and indices -- and
`Subject::Assemble` already takes them. The geometry is ready; what is missing is that `Live` insists
on a filename.

`board:1534` is where the corridor's own remaining work lives (per-tile, streaming, lane markings);
this item is the door it has to walk through.

## What it measures

`tools/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp` lays the road with the same
`Journey::Lay` the headless driver calls, sweeps its first 400 m into 3200 triangles, assembles them
into a `Gltf::Subject` with no file in between, and stands it up at 1280x720.

| | |
|---|---|
| triangles drawn | 3200 in 1 part |
| physics steps a frame | 17 |
| frames drawn | 119 |
| how far the car got | 27.40 m |
| mean frame | **0.2258 ms** |
| worst frame | **11.272 ms** against a 16.667 ms budget |

**Two binaries, one translation unit.** `tools/driver` links no renderer; `tools/driver/window` links
the whole of it; `tools/driver/parts/Journey` is compiled into both and knows about neither.
