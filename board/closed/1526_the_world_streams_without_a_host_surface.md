Type: bug
Area: world
Tags: instrument

**The world streams without a host surface**

`src/world/TerrainLoader.cpp` includes `SDL3/SDL_surface.h`, so **the whole world layer needs SDL to
compile** -- and a headless run that only wants to know where the roads are cannot be built without
linking a window system.

`board:1504` predicted this in advance and named it as the reason the headless drive matters:

> *That is what makes it fast, and it is also the first real test of `CLAUDE.md`'s claim that the
> engine is a library: a world that cannot be simulated without a GPU is a world welded to one.*

It is welded. Found the first time anything tried to compile `src/world` without a device, which is
the instrument working exactly as the board said it would.

## What must be true

- [ ] **Decoding an elevation tile is not a rendering operation** and does not reach for a surface
      type. What TerrainLoader wants from SDL is an image decode; what it should take is bytes
- [ ] **`src/world` compiles with no windowing library named anywhere in it**, and a claim proves it
- [ ] **The split is by what the code DOES and not by a build flag.** A world that compiles both ways
      through an ifdef is two worlds, and one of them is never tested

## Comments

**The routing half is already clean**: `src/world/Wayfinding.cpp` compiles against `-Isrc/world` and
the standard library alone, and `src/world/RoadHarvest.cpp` needs core, data and the tile pool but no
device. The dependency is in the terrain path only, which is why this is a bug and not an
architecture problem -- `board:1525` is the general rule it violates.

## Closed -- the engine reads its own elevation tiles

**Two things were in the way and only one of them was real.** `fb_load_image_file` in `TerrainLoader`
called `IMG_Load` and was called by NOBODY -- its heap tag still said "moon rgba" and the caller was
long gone. That is a dead path, and `CLAUDE.md` is unambiguous about those. Deleted.

**The real one was the elevation decode.** `TerrainGrid::FromTerrariumPng` used SDL_image to turn a
Terrarium tile into RGB. `src/core/io/Png.{h,cpp}` now does it: signature, IHDR, IDAT and IEND,
inflated against zlib -- which the tree already links -- and unfiltered row by row through None, Sub,
Up, Average and Paeth. About 150 lines, ours, no new dependency.

Every shape it does not take is refused by name and by number: a bit depth it does not read, a colour
type it does not read, interlacing, a truncation that says how many bytes were claimed and how many
were left, and a row filter PNG does not have. Proven by
`test/unit/core/io/APngReaderTakesWhatAnElevationTileIs.cpp` -- **0 bytes wrong** over 64x48 tiles
under all three filters an encoder actually chooses, and RGBA read at four channels.

**`src/world` and `tools/driver` now compile and link with no SDL flag anywhere.** The goal's
*headless with no renderer linked at all* is literally true for the first time.

## The framing was wrong, and the owner corrected it

**SDL_image is a DECODER, not a renderer.** The goal asks that outshine be able to RUN without
initialising a renderer -- or against an offscreen surface -- and the headless drive already did
that: it never created a device. Linking a library that turns bytes into pixels was never the thing
the goal forbade. This item was filed on a confusion between *links SDL* and *needs a renderer*, and
`board:1504`'s sentence about a world welded to a GPU does not apply to an image decode.

**So the PNG reader has to stand on a measurement instead**, and it does. Over **130 real Terrarium
tiles from the Munich to Hamburg cache, 2600 decodes each way**:

| | per tile |
|---|---|
| `src/core/io/Png` | **0.8694 ms** |
| SDL_image | 1.1846 ms |
| | **1.363x** |

And the output is not merely the same size: comparing every byte of all 130 tiles against
`IMG_Load_IO` + `SDL_ConvertSurface` gives **0 tiles differing and 0 bytes wrong**. The whole drive
agrees too -- 774.851 km against 774.847, the difference being the run's own step boundary.

**It is kept because it is a third faster on the real population and produces the same bytes**, in a
path that runs thousands of times per route, not because SDL was forbidden. The dead
`fb_load_image_file` deletion stands on its own: nothing called it.
