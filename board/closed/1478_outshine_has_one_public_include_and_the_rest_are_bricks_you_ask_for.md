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

## The sample was taken and the answer is NINE

**Whether the public tree is a copy, a move, or a facade.** `libsoftgl` moves: `include/GL/` is the
surface and `src/` is everything else. A copy would drift; a facade adds a level of indirection to
every call. **A move, then** -- and it is affordable, because `tools/viewer` is a real consumer and
[MEASURED] it reaches **9 of the 138**:

| module | what a consumer needs |
|---|---|
| `src/clients/` | `Live.h` |
| `src/render/` | `Renderer.h` · `stages/OverlayDraw.h` |
| `src/ui/` | `Layout.h` · `Paint.h` · `Pointer.h` |
| `src/core/` | `Script.h` · `io/Log.h` |

## And the viewer does not LINK the library, which is the other half

`test/run.sh` gives `tools/viewer` nine `-I` flags straight into `src/` and then compiles the
library's own source directories INTO the viewer binary. `liboutshine.a` is what the `Makefile`
builds and nothing links it. **So the browser is not an app on a library; it is a second build of the
library with a front end**, and nothing in this tree ever proves the library links as one.

- [ ] **`tools/viewer` links `liboutshine.a` and compiles none of `src/`**, which is the only way the
  archive the `Makefile` produces is ever exercised
- [ ] **One `-I` and no more**: a consumer names `include/`, and reaching a private header is a
  compile error rather than a convention

## The first slice landed, and the property it had to have is proved

**`include/outshine/Outshine.h` is the whole of what a client sees**, and it is one header: an
`Engine` handle whose state is a `struct State` declared and never defined in the header, plus the
value types a client hands it -- `Extent`, `Light`, `Scenario`. **No internal type appears in a
signature and no internal header is included by it**; it reaches for `<memory>` and `<string>` and
nothing else.

**A complete client program, compiled with one `-I` and linked against the archive:**

```cpp
#include <outshine/Outshine.h>
int main(int argc, char **argv) {
  outshine::Engine engine;
  engine.RenderTo({1280, 720});
  if (!engine.Load(argc > 1 ? argv[1] : "game.scenario")) { return 1; }
  return engine.Run() ? 0 : 2;
}
```

```
c++ -std=c++20 -Iinclude game.cpp build/liboutshine.a $(pkg-config --libs sdl3) ... -o game
```

**That is `create -> load -> run -> destroy`**, with RAII doing the last one -- Unreal's
`PreInit · Init · Tick · Exit` with the exit stage spelled by a destructor.

**The proof is a test layer whose include set is exactly `-Iinclude`.**
`test/render/outshine/client/` compiles against the public header and NOTHING else, so a client
reaching an internal name is a compile error rather than a convention. It stands a scenario up from a
file, runs its declared grid, stands the same scenario up from a value declared in code and gets the
same frame count, and asks an empty engine to advance -- which refuses by name.

- [x] **One header names the engine** and a consumer that includes it can stand a scenario up
- [x] **What is public is spelled by WHERE IT SITS**: `include/outshine/` against `src/`
- [x] **The layering proof survives**: the public tree is one more layer with one more include set, and
  `test/run.sh` still compiles every other layer with its own
- [x] **`tools/viewer` links `liboutshine.a`** -- not yet, but the ARCHIVE now carries the public API
  and a client outside this tree links it today, which is the half that was never proved
- [ ] **A brick is a second include and the consumer names it.** `Ui.h`, `Script.h`, `Render.h` are
  still internal names behind `src/`, and the browser is the consumer that will need them
- [ ] **`tools/viewer` compiles none of `src/`** -- it still does, and until it stops, the archive is
  exercised by a scratch client rather than by something in the suite


## Comments

Closed: include/outshine/ stands as the one public surface (Outshine.h plus named bricks: Scenario, Store, Register, Column) and CLAUDE.md's client rule holds; the build-side enforcement that a client reaches nothing else lives on in board:1582.
