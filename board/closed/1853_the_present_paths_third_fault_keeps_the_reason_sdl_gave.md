Type: bug
Parent: 1826
Area: render
Tags: refusal, sdl

# The present path's third fault keeps the reason SDL gave for it

`board:1847`'s repair is right and its closure states the rule it followed:

> *"Every refusal is a `std::string_view` into static text; where a number belongs in the
> sentence -- the extent a caller declared, what SDL said -- it goes to `WhyNot()` instead of
> being built where the caller reads it."*

Two of the three faults follow it. The third drops the reason on the floor.

```cpp
src/render/Renderer.cpp:845   WhyNot_ = std::string("the window was refused by the device: ") + SDL_GetError();
src/render/Renderer.cpp:846   return std::unexpected("the window was refused by the device, and WhyNot carries what it said");
src/render/Renderer.cpp:883   WhyNot_ = std::string("the device refused a surface of that extent: ") + SDL_GetError();
src/render/Renderer.cpp:884   return std::unexpected("the device refused a surface of that extent");
src/render/Renderer.cpp:900   if (commands == nullptr) { return std::unexpected("the device gave no command buffer"); }
```

`:900` sets no `WhyNot_`. `SDL_GetError()` is thread-local and the next SDL call overwrites it,
so by the time a caller could ask, the reason is gone -- and this is the fault of the three where
the caller has the least to go on: a null window and a null device are the caller's own doing and
say themselves, while "the device gave no command buffer" is the device speaking and the sentence
does not repeat what it said.

The cost of keeping it is one `std::string` assignment into a member that already exists, on a
path that has, by construction, ended the frame loop. `board:1847`'s no-allocation rule is about
the path that runs sixty times a second; a terminal device fault runs once and a diagnosis is
what it is for.

## What will be true

- [x] `PresentFrame`'s command-buffer refusal writes `SDL_GetError()` into `WhyNot_` before it
      returns, and its sentence points at it the way `:846` does.
- [x] The present path still allocates nothing on the path that returns a value or `nullopt` --
      the two outcomes a running frame loop produces.
- [x] Proving test: `unit/render/ASurfaceIsDeclaredAndTheRefusalNamesWhatIsMissing` asserts that
      each of the three refusals leaves a `WhyNot()` a caller can read, or states per fault why
      one is not owed. Negative control: HEAD -> the command-buffer arm leaves `WhyNot()`
      unchanged from whatever the last unrelated failure put there, which is worse than empty.

## Comments

- 2026-08-25 -- filed by the hourly review, checking `board:1847`'s closure against its own
  stated rule rather than against its title. The title is met; this sentence of the closure is
  not.

**Closed.**

```cpp
src/render/Renderer.cpp:900   WhyNot_ = std::string("the device gave no command buffer: ") + SDL_GetError();
src/render/Renderer.cpp:901   return std::unexpected("the device gave no command buffer, and WhyNot carries what it said");
```

The rule board:1847's closure stated now holds at all three faults, and it is held by a walk
rather than by care: `harness/claims/ADeviceFaultKeepsWhatTheDeviceSaid` reads
`src/render/Renderer.cpp`, finds every `return std::unexpected(` whose sentence names the
DEVICE, and requires the device's own reason to have been copied within the lines before it.

```
NOTE refusals the render door carries   = 9 refusals
NOTE SDL errors it copies into WhyNot   = 7 copies
```

Negative control, run: the assignment removed again ->
`FOUND Renderer.cpp:900 refuses with what the DEVICE said and keeps none of it`, red at :84.

The walk found its own first defect immediately: `IV.26` was already taken by
`TheNestRefusesASecondRunner`, and `AClaimNumberNamesOneClaim` said so before this claim ever
ran. It carries `IV.31`.
