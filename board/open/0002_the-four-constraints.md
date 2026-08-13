Type: feature
Area: scenario
Tags: perf, instrument

**The four constraints**

**SDL3 · SDL_GPU · modern C++ · this device at 720p60.** The port met the first three for the first time
at `0161f88`. **The fourth is undemonstrated** — no camera path, no frame clock, no scene worth
measuring. `CLAUDE.md`'s *distribution over a moving camera, p50/p95/p99, never a mean* has **no
instrument, no subject and no case**, and until one exists no line anywhere may claim the target is met.

That is not three separate tasks. A declared camera path, a per-frame clock with its floor published,
and a scene are useless apart, and together they are the `scenario` suite's first member.
