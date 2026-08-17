Type: bug
Area: render
Tags: instrument

**A shader typo survives a green library, a green unit tree and a green shader suite**

`board:1205` updated four of five `shadeRow` call sites in `src/render/stages/SubjectDraw.cpp` and left
the fifth one argument short. **Nothing caught it until a render case created a device.**

| what ran | verdict | why it could not see it |
|---|---|---|
| `make` | **green** | the shader is a C++ string literal; the host compiler never reads it |
| `outshine/unit` | **54 of 54** | no unit layer creates a device |
| `outshine/shader` | **12 of 12** | **no member of this layer compiled `SubjectDraw`'s source** — the only mention of that file in the whole layer was a prose comment |
| the full suite | **73 PASS, 134 FAIL** | 71 cases red on a picture they never got to draw |

**The cost was the whole round's verification**: a corpus preparation and a 208-test run, to find an error
a compiler states in milliseconds.

## What was wrong with the instrument, and it is not that a test was missing

`CLAUDE.md` says a shader case is *shader text against its C++ twin — no asset, no camera, no oracle, a
device*. **Every member of the layer honoured that and every one of them carried its OWN text.** A twin
proves an arithmetic agrees; **only the unit's own source proves the driver accepts it**, and that half
of the layer's brief had no member at all.

## The repair

`test/outshine/shader/EverySubjectShaderTheUnitEmitsCompiles.cpp` creates a device and calls
`SubjectDraw::Configure` over **all eight attachment sets** the compiled plan can hand it — velocity,
shading normal and surface identity are each spliced into the text, so a set is a shader variant. Still
no asset, no camera, no oracle.

**The layer's include set was incomplete rather than narrow**, which is why widening it is not a
concession: it already exposed `Gpu.h`, whose own `#include` of `RenderCatalogue.h` it could not satisfy.
It now compiles the renderer's four directories, which is one layer and not a reach into another.

**Done when** — met: the test exists, passes on three arms over eight sets, and **reintroducing the exact
defect takes it red**, which is how this entry knows it would have paid for itself.

## Comments

**The mutation is the whole value and it was run**: putting the fifth call site back one argument short
takes `outshine/shader` from 15 PASS to 12 PASS 6 FAIL — in **22 seconds**, against the corpus
preparation plus 208-test run that found it the first time.
