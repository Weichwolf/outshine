Type: bug
State: open
Parent: 1953
Area: door

# A world stands without a graphics device

**Benchmark** — Unreal: `LoadMap` builds a `UWorld` with no renderer at all; that is what a
dedicated server and every commandlet are. RAGE: the map streams without the draw side. **Both
agree** — assembling a world is not drawing it, and the two have different lifetimes.

`Engine::Declare` stands the PICTURE, so a client that only wants the world is refused:

    UNPREPARED the renderer has no device to stand a canvas on

Found by `outshine/door/ScoreWhatAClientBuildsInTheWorld`, which reaches `Engine::Scene()` through
`include/` alone and needs no pixels at all. It stands UNPREPARED rather than green, and says why.

The cost is not only that case. It means a tool, a test, a headless tick and a dedicated run all
need a GPU, and it hides which parts of the engine actually depend on one.

- [ ] `Declare` and `Assemble` stand a world with no device, and say so by doing it
- [ ] the picture stands when a frame is asked for, not before
- [ ] proof: `ScoreWhatAClientBuildsInTheWorld` goes from UNPREPARED to green with no renderer
