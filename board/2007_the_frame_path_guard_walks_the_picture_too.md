Type: bug
State: active
Area: render, test
Tags: measured, frame-path, benchmark

# the frame-path guard walks the PICTURE too, and the picture blocks on a readback every frame

**Benchmark** — Unreal: `stat` counters never stall a frame. GPU timings come from double-buffered
queries read at least one frame BEHIND, and `FRHIGPUBufferReadback` is explicitly documented as a
thing you poll, never wait on. RAGE: the timing buffer is double-buffered for the same reason.
**Both agree, and neither reads a render target back to publish a number.**

board:1937 closed with `harness/claims/NoFramePathCallReachesABlock` as its guard -- a walk over
the LINKER's own relocation graph, 10797 edges, 43 reachable. It is a good instrument and its
verdict is honest about what it walked. What it walked is the problem:

    constexpr Seed kSeeds[] = {
        {"Sim9DriveTick", "the physics step: ..."},
        {"GroundSupport2AtEdd", "the one virtual the step crosses ..."},
    };

**Two seeds, and both are the SIMULATION.** `Engine::RenderTo`, `Live::Draw`,
`Renderer::RenderFrame` and `Engine::State::Drew` are outside the walk entirely, so the claim
named `NoFramePathCallReachesABlock` has never once looked at the picture. CLAUDE.md names this
exactly -- *a measure that cannot see* -- and the guard question is to ask what the measure cannot
see before trusting its number.

**MEASURED, through the door, and it is not small.** The engine now publishes `heap taken under
<tag>` per written tag (board:1574), and `apps/bench --heap` differences it across frames:

    scene            untagged/frame   render-frame   subjects
    DamagedHelmet      43 640 833 B          416 B       32 B
    Sponza             45 214 081 B          416 B      544 B
    VirtualCity        50 700 646 B         2 000 B    4 128 B
    BrainStem          66 279 113 B         2 000 B    1 040 B

Stable at 43.64 MB/frame for DamagedHelmet over 2, 8 and 40 frames, so it is per-frame and not a
one-off. Tagging `Engine::State::Drew` attributes essentially all of it:

    HEAP  untagged             1 680 B per frame
    HEAP  frame-measures  43 639 232 B per frame

`Drew()` calls `ReadSceneVelocity` on EVERY frame -- the other three readbacks are guarded by
`Ticking.Steps < 2` and this one is not. A readback is a GPU->CPU sync: the CPU waits for the
device to finish, which is the one thing a frame must never do. DamagedHelmet's step is 0.006 ms
and its wall is ~20 ms a frame, and the difference is this.

**And it is spent on two numbers**: `pixels the velocity target says moved` and `the furthest any
of them moved`. Both are real and one door case depends on them; neither is worth a stall and 43 MB
on every frame of every run.

- [ ] the guard seeds the PICTURE path as well as the simulation, and every seed still matches
- [ ] a readback the picture does not need is not taken -- the client ASKS, the way it already
      asks for pixels through `WantsPixels`
- [ ] `apps/bench --heap` reads under a megabyte a frame for a still Khronos scene

**The measurement that would show I am wrong:** if seeding the picture path leaves the guard green,
the readback is not reachable the way I think and this item is withdrawn. Negative control for the
seeds: a seed matching no symbol must FAIL the claim by name, which it already does.
