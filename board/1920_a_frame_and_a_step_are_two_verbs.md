Type: bug
State: open
Area: engine, clients
Tags: measured, door, frame-path

# A step and a frame are two verbs, and a headless run pays for one

**Benchmark** — Unreal: `Tick` and the render thread are separate, and a headless commandlet ticks without presenting. RAGE: fixed simulation step, interpolation to the display. **Both agree** — stepping and drawing are two verbs and a headless run pays for one.

**Repaired at HEAD; this item holds what is LEFT.** `Live::Advance` called
`Renderer_->RenderFrame()`, so `Engine::Advance()` advanced a simulation AND drew a picture. A
client that wanted the physics paid for the graphics at every step:

| | before | after |
|---|---|---|
| 2.896 km, 15 467 steps, 6 pictures | ~10 min | **4.1 s** |
| 300 steps, every one drawn, 4 pictures | — | **5.5 s** |

Physics runs a long route fast, graphics runs a short one slowly. Both under five seconds, which
is what a test round can afford.

`Engine::RenderTo(Extent)` is the verb CLAUDE.md's TARGET door already named and the tree did not
have. It refuses a frame that is not the canvas the engine stands on, and it publishes what the
DRAW produced -- batches drawn, batches cast -- because those are properties of a picture and not
of a step. Two door cases had to draw before they could count, which is the point.

## What is left

- [ ] The windowed client presents through the same verb: `apps/viewer` still leans on Advance
      having drawn, and a client that presents without asking is the same conflation facing the
      other way.
- [ ] A frame drawn from a step that did not happen is refused, or the pair is stated: today
      `RenderTo` will happily draw the same state twice and call it two frames.
- [ ] Proving case: a scenario stepped N times with no draw reports N steps and zero frames, and
      the same scenario drawn once reports one. Negative control: the draw folded back into the
      step, and the two counts cannot differ.
