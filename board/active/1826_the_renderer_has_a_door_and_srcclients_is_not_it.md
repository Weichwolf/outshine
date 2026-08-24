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

- [ ] The renderer REFUSES an incomplete declaration by name instead of drawing nothing: no
      camera projection, no lights where the surface needs them, a mesh whose anchor and
      placements disagree by more than the world can carry, a draw list whose layout no vertex
      run supplies. Each refusal names what is missing, and `WhyNot()` carries it.
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
