Type: bug
Parent: 1836
Area: render
Tags: frame-path, expected, allocation

# The surface door tells a lost frame from a broken device

`board:1836` gave `PresentFrame` four distinct reasons. The fourth is not a reason -- it is a
normal frame.

```cpp
src/render/Renderer.cpp:912
  if (!shown.Drew) {
    return std::unexpected(std::string("the swapchain answered no texture this frame: ") +
                           SDL_GetError());
  }
```

SDL's own header, fetched:

```
/opt/homebrew/include/SDL3/SDL_gpu.h:4299-4301
 * This function can fill the swapchain texture handle with NULL in certain
 * cases, for example if the window is minimized. This is not an error. You
 * should always make sure to check whether the pointer is NULL before ...
```

So a minimised window makes the door refuse, once per frame, and:

- the refusal text ends in `SDL_GetError()` on a path where SDL set no error -- the sentence
  carries whatever the last unrelated failure left behind, or nothing;
- building it is a `std::string` allocation **inside the per-frame present path**, which the
  house rule forbids without qualification: *no alloc/lock/disk on the frame path*. It costs
  nothing while the window is visible and it costs a heap round-trip per frame the moment it is
  not;
- the one caller cannot tell "there is no image this frame, come back" from "the device is
  gone" (`tools/viewer/TheBrowserDrawsItselfWithTheEngineItShows.cpp:632-635` counts both into
  one `unshown` and keeps the first sentence).

Beside it, the flag `std::expected` replaced is still declared:

```cpp
src/render/Renderer.h:42
  struct Shown {
    bool Drew = false;
```

`Drew` is `true` on every value the door now returns and is read by nothing outside
`Renderer.cpp` -- `grep -rn '\.Drew' src tools apps test` finds only the two lines that set and
test it inside the function. *Delete on the day you replace.*

And CLAUDE.md still describes the pre-`expected` door, one commit behind its own session:

```
CLAUDE.md:421   ... and what comes back is `{Drew, WidthPx, HeightPx}` ...
```

## What will be true

- [ ] "No image this frame" is a VALUE, not a refusal: the success type says whether an image
      was acquired (`std::expected<std::optional<Shown>, ...>`, or a `Shown` whose extent is
      zero, or a named enum), and `std::unexpected` is reserved for the three facts that are
      actually faults -- no surface declared, no device, no command buffer.
- [ ] Nothing on the present path allocates. The three fault sentences are `std::string_view`
      into static text, the way `Sim::AsideRatePerM` (`src/sim/CorridorLay.h:76`) already does
      it, or the error type is a value the caller resolves.
- [ ] `Shown::Drew` is gone.
- [ ] Proving test: extend `test/unit/render/ASurfaceIsDeclaredAndTheRefusalNamesWhatIsMissing`
      -- it already drives a renderer that never saw a device -- with a claim that the
      "no image" answer is not the same shape as the fault answers. Negative control: HEAD ->
      red, because all four come back as `unexpected`.
- [ ] CLAUDE.md's interface paragraph names what the door returns at HEAD.

## Comments

- 2026-08-25 -- filed by the hourly review. `board:1836` and `board:1837` are real repairs and
  both hold: the refusals are distinct, the case runs in the fast mirror without a GPU, and the
  command buffer is checked. This is the remainder -- a half-converted door keeps the flag it
  replaced and turns a legal frame into an allocation.
