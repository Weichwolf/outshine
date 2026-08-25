Type: bug
State: open
Area: render
Tags: measured, telemetry, frame-path

# A frame-path count is published by the stage that made it, not by a process global

Landed 2026-08-25 in 7f395f2f, and it is the wrong storage for a right idea:

    src/render/stages/LightVisibilityStage.h:40   static size_t CastBatches_;
    src/render/stages/LightVisibilityStage.h:32   [[nodiscard]] static size_t CastBatches() { ... }
    src/render/stages/LightVisibilityStage.cpp:11 size_t LightVisibilityStage::CastBatches_ = 0;
    src/render/stages/LightVisibilityStage.cpp:218 CastBatches_ = 0;
    src/render/stages/LightVisibilityStage.cpp:220 ++CastBatches_;
    src/engine/Engine.cpp:985                     Render::LightVisibilityStage::CastBatches()

One mutable `size_t` at process scope, reset and incremented inside the per-frame encode and read
from a different translation unit. Two renderers in one process share it; a second window
overwrites it; a threaded encode races on it with no atomic; and a scenario suite that asserts on
it is asserting on whatever ran last. The engine's rule is values over globals and a temporally
DETERMINISTIC simulation, and a count that depends on which stage encoded most recently is
neither.

**The capability was already there and unreachable, which is the finding CLAUDE.md predicts.**
`src/base/io/Telemetry.h` declares `TelemetrySource` with `DeclareTelemetry(TelemetrySchema&)` and
`SampleTelemetry(TelemetryRow&)` -- an instance-based, declarative channel for exactly this. Two
implementations stand: `StreamTelemetry` and `EyeTelemetry`, and STATE.md lists BOTH as linked by
no suite. A new static was written beside a stranded interface that does the job.

## What will be true

- [ ] No `static` mutable lives in a render stage. `CastBatches` is a member of the stage
      instance and reaches the door through the same channel every other per-frame number does.
- [ ] The shadow pass declares its counts through `TelemetrySource`, which stops being stranded
      the day something reads it.
- [ ] Proving case: two stages encoded in one process report their OWN counts, and the case
      fails if one reports the other's. Negative control: the static restored, and both report
      the same number.
