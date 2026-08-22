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

- [ ] ground streaming state lives in an object a Sim owns; two worlds coexist in one process
- [ ] the worker count arrives through the declaration, and the env excuse list shrinks by one
- [ ] no `FB_` name remains in the runtime contract
