Type: feature
Area: core
Tags: scope

**outshine has one public include, and the rest are bricks you ask for**

The owner's ruling, and the pattern is SDL3's: `#include <outshine/Outshine.h>` brings the engine, and
everything optional is a second include a consumer names because it wants that brick. **A generator and
a data provider are the clearest cases** -- a consumer that draws no forest should not compile one.

Today there is no public surface at all. A consumer reaches into `src/‹module›/‹Header›.h` with an
`-I` per module, so **the include set is the build's and the library has no shape of its own**:

| | headers at the module root |
|---|---|
| `src/core/` | 39 |
| `src/generators/` | 23 |
| `src/world/` | 18 |
| `src/clients/` · `src/data/` | 15 each |
| `src/gltf/` | 11 |
| `src/scenario/` · `src/ui/` · `src/render/` | 7 · 5 · 5 |

**138 headers, none of them marked public or private.** That is what makes every consumer's include
line a guess about which one is the door.

## What must be true

- [ ] **One header names the engine** and a consumer that includes it can stand a scenario up
- [ ] **A brick is a second include and the consumer names it.** Generators and providers first,
  because they are the ones a consumer genuinely chooses between
- [ ] **What is public is spelled by WHERE IT SITS**, not by a comment -- a private header a consumer
  can reach is a private header that will be reached
- [ ] **The layering proof survives.** `test/unit/‹module›` compiles with its layer's include set and
  that is what makes a breach a compile error; a public tree must not become a way around it
- [ ] **`test/run.sh`'s per-layer `-I` sets and the `Makefile`'s compile groups say the same thing they
  say now**, or the two disagree about what a layer may reach

## The question this has to answer first

**Whether the public tree is a copy, a move, or a facade.** `libsoftgl` moves: `include/GL/` is the
surface and `src/` is everything else. A copy would drift; a facade adds a level of indirection to
every call. *The measurement that decides it is how many of the 138 a consumer actually needs* --
`tools/viewer` is a real consumer and its include list is the sample.
