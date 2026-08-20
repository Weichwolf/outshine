Type: bug
Area: world
Tags: bug

**The streamer is reached by name and not by a global, and its names are outshine's**

Two defects in one file, found the first time a program outside `src/clients/Sim.cpp` tried to stream
tiles.

**`fb_tile_pool()` is a global**, and `OsmField::AddTile` reaches for it directly. A caller that
constructs its own `TilePool` is ignored -- the field silently uses whatever `fb_stream_open` last
installed, and if nothing did, it dereferences null. `CLAUDE.md` is explicit:

> *STATE BELONGS TO A FRAME. Two frames in flight is the normal case, so neither a global nor a
> singleton has a place to live.*

**And `fb_stream_open`, `fb_stream_close`, `fb_tile_pool`, `fb_stream_ground`,
`fb_stream_ground_post_m`, `fb_stream_ground_block`, `fb_load_image_file` and `FbGroundSurface` are
not outshine names.** They are a C prefix from another lifetime, in a repository whose every other
symbol is `outshine::Layer::Thing`. The owner named it: *fb_tile_pool ist kein outshine konformer
funktionsname.*

## What must be true

- [ ] **The pool is passed to whoever needs it**, so a program can stream two regions at once and a
      test can stream one without disturbing another
- [ ] **Every `fb_` name becomes an outshine name** in its layer's namespace, and `FbGroundSurface`
      and `FbGroundBlock` with them
- [ ] **The rename is the same round as `board:1526`**, because both are in `TerrainLoader` and two
      passes over one file is one pass too many

## Comments

The global is why the Munich to Hamburg planner segfaulted on its first run: it built its own
`TilePool`, `OsmField` asked the global one, and the global was null. The crash was the honest
outcome; a fallback would have been worse.
