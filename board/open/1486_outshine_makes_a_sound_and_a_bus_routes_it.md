Type: task
Parent: 1480
Area: core
Tags: scope

**outshine makes a sound, and a bus routes it**

**The whole pillar is absent** -- there is no `src/audio`, no source, no bus, no listener, and a game
without sound is not a game. A scenario declares `Sounds` and `Buses` today and the engine reads them
and does nothing, which `Engine::Carried()` says out loud.

## What the frame rule means for audio, because it is not the frame's rule

**Audio's deadline is not 16.67 ms, it is the buffer's** -- a 512-sample buffer at 48 kHz is 10.67 ms
and a miss is a CLICK every listener hears, which is worse than a dropped frame. So the mix runs on the
host's audio callback, not on the frame, and **the frame's only job is to hand it positions**.

**Both references separate them the same way**, and the mechanism to take is the BUS GRAPH: sources
route into buses, buses into buses, one master -- so a scenario can duck music under dialogue without
the engine knowing what music or dialogue is.

## What must be true

- [ ] **A sound is played by id and the engine answers whether it started**, both directions like every
  capability here
- [ ] **A bus routes into a bus and there is one master**, and a cycle is a refusal naming it
- [ ] **A positional source attenuates by its declared falloff** against one listener, and the listener
  is a `View` -- so the camera that follows the player is the ear that hears
- [ ] **The mix takes nothing from the allocator**, on the audio thread, for the same reason the frame
  path does not -- and it is MEASURED with the same ledger
- [ ] **A missing sound is a named refusal and never a silence**, because a silence is what a working
  sound also sounds like when nothing is happening
- [ ] **The host surface is one interface** -- outshine declares what it needs of an audio device and
  calls nothing else, the way it does for the GPU
