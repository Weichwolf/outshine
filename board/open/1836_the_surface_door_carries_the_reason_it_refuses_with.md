Type: bug
Parent: 1826
Area: render
Tags: refusal, door, expected, boundary

# The surface door carries the reason it refuses with

`board:1826`'s first box exists because *"a door that accepts five different incomplete
declarations and draws nothing, without a refusal, is not a door"*. The surface door that
landed under that item's second heading has the same hole in its third method.

```cpp
src/render/Renderer.cpp:893   Renderer::Shown Renderer::PresentFrame() {
src/render/Renderer.cpp:894     Shown shown;
src/render/Renderer.cpp:895     if (Showing_ == nullptr || Device_.Get() == nullptr) { return shown; }
src/render/Renderer.cpp:896     SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device_.Get());
...
src/render/Renderer.cpp:906     SDL_SubmitGPUCommandBuffer(commands);
src/render/Renderer.cpp:907     return shown;
```

`Shown{false, 0, 0}` comes back for four different facts -- nothing is being shown, the device
is gone, the command buffer was refused, the swapchain gave no texture -- and `WhyNot_` is not
touched on any of them. The only caller behaves accordingly:

```cpp
tools/viewer/TheBrowserDrawsItselfWithTheEngineItShows.cpp:626   const Shown shown = renderer.PresentFrame();
tools/viewer/TheBrowserDrawsItselfWithTheEngineItShows.cpp:627   if (shown.Drew) { ... Advance ... }
```

No else. A frame that never reached the surface is not advanced, not counted, not printed --
the browser simply spins. That is the same silence `board:1826` measured five times over, in
the door written to answer it.

Two more on the same twelve lines:

- `SDL_AcquireGPUCommandBuffer` (`:896`) can return `nullptr`; the result is handed to
  `SDL_WaitAndAcquireGPUSwapchainTexture` (`:899`) and to `SDL_SubmitGPUCommandBuffer` (`:906`)
  unchecked. CLAUDE.md's own rule is defensive at system boundaries, and this IS the boundary.
- `ShowOn` and `ShowOffscreen` are `bool` plus `std::string &error`, while this tree already
  spells the C++23 form on every other door of the same shape:
  `RenderPlan::Compile` (`src/render/plan/RenderPlan.h:53`), `TableBook::Stand`
  (`src/scenario/Tables.h:28`), `ViewBook::Stand` (`src/scenario/Views.h:17`),
  `TriggerField::Stand` (`src/scenario/Triggers.h:24`) all return
  `std::expected<T, std::string>`. CLAUDE.md names `std::expected` where a refusal carries its
  reason; a door added on 2026-08-25 in the older idiom is a C++17-ism beside four C++23 twins.

## What will be true

- [ ] `PresentFrame` answers a reason. `Shown` carries why it did not draw, or the method
      returns `std::expected<Shown, std::string>`; four distinct facts get four distinct
      sentences, and "nothing is being shown" is not spelled the same way as "the swapchain
      refused".
- [ ] The command buffer is checked before it is used, and a refused command buffer is one of
      those sentences.
- [ ] `ShowOn` and `ShowOffscreen` return `std::expected`, matching the four doors above.
- [ ] The browser prints or counts a frame that did not reach the surface, so a swapchain that
      stops answering is a number rather than a stall.
- [ ] Proving test: see `board:1837` -- the same case reads these reasons back.

## Comments

- 2026-08-25 -- filed by the hourly review. Split from `board:1826` on that item's own
  precedent: *"filed separately as board:1829, because that is a defect in the refusal rather
  than in the door."* The door is right; its refusals are not.
