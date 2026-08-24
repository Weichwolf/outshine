Type: bug
Parent: 1826
Area: render, test
Tags: mirror, regression-gate, measured

# The surface door is proven by a case

Four public methods landed on `Renderer` this session --

```cpp
src/render/Renderer.h:46   [[nodiscard]] bool ShowOn(SDL_Window *window, std::string &error);
src/render/Renderer.h:47   [[nodiscard]] bool ShowOffscreen(int widthPx, int heightPx, std::string &error);
src/render/Renderer.h:48   [[nodiscard]] Shown PresentFrame();
src/render/Renderer.h:49   void StopShowing();
```

-- carrying four refusal messages between them (`Renderer.cpp:837, 841, 847, 867, 872, 886`), and
**nothing under `test/` names any of them**:

```
$ grep -rn "ShowOn\|ShowOffscreen\|PresentFrame\|StopShowing" test/
(nothing)
$ grep -rn ... tools/ apps/
tools/viewer/TheBrowserDrawsItselfWithTheEngineItShows.cpp:610,626,645
tools/viewer/EveryRenderCaseTheBrowserShowsDrawsSomething.cpp:151,159,167
```

Both users are `tools/viewer`, which `test/run.sh` excludes from the fast gate
(`named-only suites excluded: ... tools apps`). So the door has no case in the regression
mirror at all, and a later edit that removes a refusal turns nothing red.

Two of the refusals need no device and no window:

| refusal | reached before the device is touched |
|---|---|
| `ShowOn(nullptr, why)` -- *"a renderer is shown on a window and this call names none"* | `Renderer.cpp:836-838` |
| `ShowOffscreen(0, 0, why)` / negative extent | `Renderer.cpp:866-869` |
| `ShowOn` / `ShowOffscreen` before `Init` -- *"the renderer has no device ..."* | `Renderer.cpp:840-843`, `:871-874` |
| `PresentFrame()` with nothing shown | `Renderer.cpp:895` |

Every one of them is a pure-logic path on a default-constructed `Renderer`. A unit twin under
`test/unit/render/` reads all four back without a GPU.

## What will be true

- [ ] `test/unit/render/` holds a case that drives all four methods on a renderer with no
      device and reads every refusal string back by content, not by emptiness.
- [ ] Negative control: each refusal replaced by a bare `return false` -> red, naming the
      method whose reason went silent.
- [ ] Where a path genuinely needs a device, it is proven in
      `test/render/outshine/frame/` and the fast-gate twin covers the refusals that do not --
      the same split `board:1826`'s added box asks for on `SetMesh`.

## Comments

- 2026-08-25 -- filed by the hourly review. `board:1826`'s sharpening already recorded the
  identical gap one method over: *"SubjectDraw has no unit twin ... behaviour this hour changed
  has no case in it."* The same hour that read that sentence added four more methods with the
  same gap.
