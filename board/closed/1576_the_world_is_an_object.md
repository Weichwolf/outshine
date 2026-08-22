Type: bug
Area: world
Tags: bug

**The world is an object**

`TerrainLoader.cpp:34-77` holds the world as mutable globals -- `gPool`, `gGround[12]`,
`gSurface`, `gGroundClock`, `gGroundPending` -- behind `OpenGround`/`CloseGround`, and
`getenv("FB_TILEWORKERS")` sizes the worker pool from the machine. That contradicts two declared
laws at once (*neither a global nor a singleton has a place to live*; *the picture is a function
of the declaration, not of the machine* -- the excuse in
`NoEnvironmentVariableDecidesAPicture.cpp` already names board:1513's debt), hard-codes the
shape 0-or-exactly-1 where the rule is 0 or 1..N (two Sims cannot coexist), and leaks the dead
flightbox name into the runtime contract. CLAUDE.md's departure table certifies pilot, physics
and corridor free of mutable statics and stays silent about world -- this is why.

Related, one door up: `Live::TookPosing_` and siblings are mutable class statics (`Live.h:105`).

- [x] ground streaming state lives in `GroundStream`, an object -- `World::World` and
      `Journey::State` each own a `TilePool` and a stream over it, so two worlds coexist by
      construction; the free `OpenGround`/`CloseGround`/`GroundAt` family is DELETED, not
      wrapped, and every field (`OsmField`, `BuildingField`, `WaterField`, `ClassField`) takes
      the pool or stream it reads as a parameter
- [x] the worker count is a `GroundPoolConfig` parameter with the derived default
      (hardware_concurrency minus two, capped at the declared six); `getenv("FB_TILEWORKERS")`
      is deleted and the environment-excuse list in the claims test shrank by one
- [x] no `FB_` name remains: `FB_DAGLOG` (logging only) renamed to `OUTSHINE_DAGLOG`, grep
      reads zero


## CLOSED

The cut is total rather than a facade: thirteen call sites across Sim, Journey, the stills tool
and four field classes now receive the stream or the pool they read, and the globals file
(`gPool`, `gGround[12]`, `gSurface`, clocks, counters, the pending flag) became `GroundStream::
Held`, one per stream. The queries are const with the cache behind a unique_ptr -- logically
constant reads, the classic shape RAGE and Unreal both use for cached world queries. Proven the
whole way down: unit/world, unit/clients, render/outshine/world (a Sim stands a world up through
the object), harness/claims, and the full Munich-Hamburg stills drive PASS over the new
ownership. The windowed drive compiles and rode into its usual full-route timeout, which is its
pass condition at a 600 s ceiling.

Journey's own inline commentary -- 'it is a GLOBAL, which is what OsmField reaches for' -- came
out with the global it complained about, and its claim now reads: the streamer is an object this
journey owns.
