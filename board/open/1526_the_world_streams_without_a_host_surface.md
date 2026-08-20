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
