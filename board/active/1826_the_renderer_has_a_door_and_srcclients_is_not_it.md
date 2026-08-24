Type: issue
Area: render, clients
Tags: layering, door, dead-code, measured

# The renderer has a door, and `src/clients/` is not it

The owner's reading, this round: *"was ist gltfstudio? das sieht nicht wie eine generische
engine komponente aus. allgemein ist src/clients/ ein Sauhaufen."* Measured, it is worse than
untidy -- it is where the renderer's own contract went to live.

## What GltfStudio is

`src/clients/GltfStudio.{h,cpp}` (74 + 453 lines) is the ONLY bridge from geometry to
`Renderer`. It holds every convention the renderer requires and publishes none of them:

| convention | where it lives today |
|---|---|
| the world is ECEF and a mesh carries an anchor near its own geometry | `kStudioAnchorEcefM`, `GltfStudio.h:20` |
| a camera needs basis AND `SetFovDeg` AND `SetNearM` before it sees anything | `SetProjection`, `GltfStudio.cpp:79` |
| vertices arrive interleaved in one buffer at layout-dependent offsets | `PackVertices`, `GltfStudio.cpp:226` |
| a draw list is one item per part, with `VertexRunsCarried` deciding the layout | `BuildDrawList`, `GltfStudio.cpp:179` |
| materials, lights and environment go in before placements | `Surface` then `Place`, `GltfStudio.cpp:326,345` |

**Correction to this item's own first reading (2026-08-24).** It said "five incomplete
declarations", and that is not what they were. Of the six attempts below, only TWO were
declarations the renderer could have refused:

| attempt | what it really was |
|---|---|
| no emitted-radiance run | a CONTRADICTION -- 3 vertices declared, no run handed over. Refusable, and now refused |
| a count of placements with no table | the same shape. Refusable, and now refused |
| no lights | a VALID declaration -- unlit is legal, and the depth should still have been written |
| a 0.02 m triangle at 40 m | valid -- subpixel geometry is geometry |
| a triangle edge-on to the eye | valid -- a camera may look along a surface |
| placements absolute rather than anchor-local | a CONVENTION nobody publishes, not a refusable error: both readings are well-formed and only one is meant |

So the item's shape holds and its wording did not: the door's defect is that a valid-looking
declaration and a contradictory one are indistinguishable to the caller, and the conventions
that decide between them live in a client helper. Two refusals landed; the convention is what
box two is for.

**Measured cost of that**: writing `test/render/outshine/frame/ADrawCostsWhatTheSweepSaysItCosts`
-- a case that wants to hand the renderer ONE TRIANGLE N times and time it -- took five failed
attempts, each of which the renderer accepted in full:

```
no lights set                -> 0 covered px, WhyNot() == ""
0.02 m triangle at 40 m      -> 0 covered px, WhyNot() == ""
world origin at ECEF (0,0,0) -> 0 covered px, WhyNot() == ""
no SetFovDeg / SetNearM      -> 0 covered px, WhyNot() == ""
triangle edge-on to the eye  -> 0 covered px, WhyNot() == ""
```

`Renderer::WhyNot()` answered the empty string every time. **A door that accepts five different
incomplete declarations and draws nothing, without a refusal, is not a door** -- and CLAUDE.md's
own rule is that a refusal carries its reason.

## And the folder is a bucket, with dead wood in it

```
$ grep -r "<header>" src/ tools/ apps/ test/ | grep -v "src/clients/<header>" | wc -l
CsvTelemetry.h     0 users
RunIdentity.h      0 users
Env.h              0 users as a header (the string "Env" matches 164 times, none of them this)
```

Three headers in the library that nothing includes. And `Sim.h` (235 lines, 25 includes,
563-line body) has exactly ONE user in the whole tree --
`test/render/outshine/world/AWorldStandsUpWhereItIsDeclared.cpp`. CLAUDE.md already paints it
red as a god facade; the measurement adds that it is a god facade nobody calls.

What the folder actually holds, by kind:

| kind | files |
|---|---|
| the public door | `Engine`, `Assembly` |
| process scaffolding that is not engine at all | `LogSinks`, `Env`, `RunIdentity`, `Sanitisers`, `CsvTelemetry`, `EyeTelemetry`, `StreamTelemetry` |
| the renderer's real contract, wearing a client's name | `GltfStudio`, `Surfaces`, `Image`, `Species` |
| god facades already painted red | `Sim`, `Live` |
| engine verbs filed under the wrong noun | `InputPump`, `SceneWeather`, `RegionForge` |

`clients` is not a layer of an engine. A client is `apps/` or `tools/`; everything under `src/`
IS the library.

## What will be true

- [x] The renderer REFUSES a CONTRADICTORY declaration by name instead of drawing nothing --
      a declaration that names geometry it does not hand over. Landed:
      `SubjectDraw.cpp:453` (eight shortfalls behind one silent `return true`, split into a
      legitimate empty-geometry `true` and four named refusals),
      `SubjectDraw.h:59` (a placement count with no table), and
      `Renderer.cpp:675` (a frame with no camera basis, which now sets `WhyNot()`).
      An EMPTY geometry stays legal and still returns true -- the distinction is contradiction,
      not emptiness. Proving arms in
      `test/render/outshine/frame/ADrawCostsWhatTheSweepSaysItCosts`.
- [ ] There is ONE door for handing geometry to the renderer that does not spell glTF, and
      `GltfStudio` is written against it rather than beside it. A case that wants to draw a
      triangle writes a triangle.
- [ ] `src/clients/` is dissolved: the door to the public interface, the scaffolding out of
      `src/` entirely, the renderer contract into `src/render/`, the engine verbs into the
      layer whose noun they carry.
- [ ] The three headers nothing includes are deleted -- "delete on the day you replace" applies
      the more when nothing replaced them.
- [ ] Proving test: a case that hands the renderer a triangle through the door and reads back a
      covered pixel count above zero, plus one that asserts a NAMED refusal for each incomplete
      declaration above. Negative control: the refusal removed -> the case goes green while
      drawing nothing, which is exactly today's behaviour.

## Comments

- 2026-08-24 -- filed from the owner's reading plus the five measured failures above. The
  failures are not a complaint about difficulty: each one is a declaration the renderer ACCEPTED
  and then silently did nothing with, which is the defect this item is about.

---

## Should `src/clients/` exist at all? Measured against the reference designs (owner, 2026-08-24)

> *"prüfe ob es src/clients/ überhaupt geben sollte. der client deklariert ein surface, übergibt
> es outshine lib und outshine rendert darein. der renderer ist dabei im prinzip austauschbar.
> halte dich an referenzdesigns."*

The reference designs agree on one shape, and it is the owner's sentence:

| engine | what the client hands in | what comes back |
|---|---|---|
| Filament | a NATIVE window handle to `Engine::createSwapChain(void*)` | `SwapChain*`, opaque; the backend enum is chosen at `Engine::create` and never leaks |
| bgfx | `PlatformData::nwh`, a native handle, before `bgfx::init` | nothing backend-shaped; `bgfx::frame()` is the whole loop |
| Ogre | `externalWindowHandle` in the params, or lets Ogre make the window | `RenderWindow*`; `RenderSystem` is swapped by name |
| Unreal | an `FViewport` the platform layer owns | RHI resources behind `FRHI*` interfaces |

**A native handle goes IN; nothing backend-shaped comes OUT.** That is what makes the renderer
swappable, and it is the test to apply here.

### Where this tree stands, measured

Good, and better than the folder's name suggests:

```
tools/viewer/...:553   SDL_CreateWindow(...)                        <- the CLIENT owns the window
src/clients/Live.cpp   0 occurrences of SDL_                        <- Live is not a platform layer
include/outshine/*.h   0 occurrences of SDL                         <- the public door is SDL-free
```

Broken, and it is one seam:

```
src/render/Renderer.cpp:162          SDL_CreateGPUDevice(...)       <- the LIBRARY owns the device
tools/viewer/...:610   SDL_ClaimWindowForGPUDevice(renderer.Device(), window)
tools/viewer/...:626   SDL_AcquireGPUCommandBuffer(renderer.Device())
tools/viewer/...:653   SDL_ReleaseWindowFromGPUDevice(renderer.Device(), window)
tools/viewer/Every...:159  SDL_CreateGPUTexture(renderer.Device(), &wanted)
```

`Renderer::Device()` hands an `SDL_GPUDevice *` OUT of the library, and the client then performs
swapchain and command-buffer work with it. So the client does not declare a surface and hand it
over -- it borrows the library's device and writes a piece of the renderer itself. **With that
seam in place the renderer is not swappable**, because every client is written against SDL_GPU
whether it wants to be or not.

### So: should `src/clients/` exist?

**No.** A client is `apps/` or `tools/`; everything under `src/` IS the library, and a library
does not contain its clients. The measurement says the same, file by file: the folder holds a
public door, process scaffolding that is not engine at all, the renderer's real contract wearing
a client's name, two god facades already painted red, and three headers nothing includes.

The order of work, largest lever first:

- [x] **The surface goes IN.** A client hands over a window or an extent and gets back pixels:

      ```cpp
      src/render/Renderer.h:40   [[nodiscard]] bool ShowOn(SDL_Window *window, std::string &error);
      src/render/Renderer.h:41   [[nodiscard]] bool ShowOffscreen(int widthPx, int heightPx, std::string &error);
      src/render/Renderer.h:42   [[nodiscard]] Shown PresentFrame();     // {Drew, WidthPx, HeightPx}
      src/render/Renderer.h:43   void StopShowing();
      ```

      `SDL_ClaimWindowForGPUDevice`, `SDL_WaitAndAcquireGPUSwapchainTexture` and
      `SDL_ReleaseWindowFromGPUDevice` have NO call site outside `src/render/` at all, and
      `renderer.Device()` has none in `tools/`, `apps/` or `test/` except the MSL-versus-C++
      twins under `test/render/outshine/shader/`, which exist to PROVE the device and are not
      clients of it. Held by
      `harness/claims/TheDeviceLeavesTheLibraryOnlyForItsOwnTwins` over 544 sources -- it counts
      CALLS, so a name in prose (the browser's own no-drawing-instruction list, a scenario
      case naming the command buffer's 128 bytes in a message) is not a finding.

      Negative control, run: the viewer's `ShowOn` swapped back for
      `SDL_ClaimWindowForGPUDevice(renderer.Device(), window)` -> `FOUND
      tools/viewer/TheBrowserDrawsItselfWithTheEngineItShows.cpp:610 calls
      SDL_ClaimWindowForGPUDevice`, red.
- [ ] **The renderer is swappable by construction**: what a client holds is an interface, and
      the SDL_GPU implementation is one of its implementations, chosen once.
- [ ] `src/clients/` is dissolved along the lines measured above.

---

## Sharpened by the hourly review, 2026-08-24: the refusals landed, and the gate does not see them

`src/render/stages/SubjectDraw.cpp:453-476` now splits `SetMesh`'s eight silent shortfalls into
one legitimate empty-geometry `true` and named refusals for a missing device, position run,
index run, draw list and emitted-radiance run. That is the first box, partly.

Two measurements against it:

- **`SubjectDraw` has no unit twin.** `test/unit/render/stages/` holds ten cases and every one
  of them is shading mathematics -- no case constructs a `SubjectMesh` and reads a refusal.
  `grep -rln SetMesh test/` returns nothing. The mirror IS the regression gate
  (`CLAUDE.md`, Tests), so behaviour this hour changed has no case in it.
- **The proof that exists is outside the gate.** `ADrawCostsWhatTheSweepSaysItCosts` lives under
  `test/render/outshine/frame/`, and this round's run printed:

  ```
  run.sh: the fast gate -- named-only suites excluded: harness/render render/outshine/drive
          render/outshine/frame render/outshine/scenario render/outshine/shader render/outshine/world
  ```

  So the only case that exercises the new refusals runs when somebody names it. A refusal
  removed by a later edit turns nothing red.

Two corrections to the measurements above:

- `Env.h` is not unreferenced: `test/harness/claims/NoEnvironmentVariableDecidesAPicture.cpp:22,28`
  names it as the declared host boundary that reads the environment. It is the one file in the
  "process scaffolding" row with a proof attached, and the box that deletes three headers
  covers `CsvTelemetry.h` and `RunIdentity.h` only.
- The refusal message this hour landed reports a count it has already cleared, and argues its
  own case in 118 characters of rodata -- filed separately as `board:1829`, because that is a
  defect in the refusal rather than in the door.

- [ ] Added box: every named refusal `SetMesh` returns has a case under
      `test/unit/render/stages/`, in the fast gate, that reads the reason back. Negative
      control: the refusal replaced by the old `return true` -> red.
