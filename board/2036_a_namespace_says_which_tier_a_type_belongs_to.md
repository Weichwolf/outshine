Type: bug
State: open
Area: content, render
Tags: measured

# A namespace says which tier a type belongs to

**Benchmark** — Unreal: a module's types carry that module's prefix and its API macro, and
`FSlateBrush` never claims to be Engine's; the build refuses a type declared in the wrong module.
RAGE: the `rage::` namespace is the framework and a game type lives in the game layer, which is
what keeps `grcTexture` usable without `CPed`. **They agree**, so the matter is closed: a type's
namespace names the tier that owns it.

`src/content/shade/Image.h` declares `Raster`, `DecodeImage` and `EncodePng` in namespace
`outshine::Core` -- the ENGINE's namespace. It is the only header outside `src/engine/` that does.

MEASURED:

    namespace outshine::Core in a header outside src/engine/   1  (src/content/shade/Image.h)
    directories naming Raster / DecodeImage / EncodePng        6  (content/shade, engine, import,
                                                                  import/surface, render, render/plan)
    files                                                     12

The defect is not cosmetic and it was met while working board:1547. `Render::SurfaceRasters` holds
six of these and had to spell them `Core::Raster` -- a render-tier struct qualifying its members
with the engine's name, which reads as a dependency that does not exist. A reader following the
namespace looks for the type in `src/engine/` and finds nothing there.

It also hides the real question: **who owns an image in this tree?** `content` is where the shading
inputs live and where the file already sits, so the namespace to take is the folder's. That makes
`Raster` reachable from `render` and `import` without either of them naming the engine.

- [ ] `Image.h` and `Image.cpp` declare their contents in a namespace that names `content`, and no
      header outside `src/engine/` opens `outshine::Core`
      proof: harness/claims/EveryGuardSpellsItsFolder is the wrong instrument -- it checks the include
      guard, not the namespace, so this needs a claim of its own or an extension of that one
- [ ] `Render::SurfaceRasters` names its members without an engine-qualified type
- [ ] The claim that catches this stands, and its negative control -- one header outside
      `src/engine/` reopening `outshine::Core` -- goes RED
