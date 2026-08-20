Type: issue
Area: core

**What a public include costs the layering proof, and which of the two gives way**

`board:1478` wants what SDL3 has: one header a consumer includes, optional bricks beside it, and one
`-I`. **Measured, that collides with the one structural claim this tree makes about itself.**

## The two numbers

**A consumer needs 8 headers and reaches 47.** `tools/viewer` is the sample: it names `Live.h`,
`Renderer.h`, `stages/OverlayDraw.h`, `Layout.h`, `Paint.h`, `Pointer.h`, `Script.h` and `io/Log.h` --
and those eight pull in **39 more** by value:

| | |
|---|---|
| `src/core` | 12 |
| `src/gltf` | 8 |
| `src/render/stages` | 6 |
| `src/render` | 4 |
| `src/clients` · `src/render/draw` · `src/render/plan` · `src/ui` | 3 · 2 · 2 · 2 |

**The layering proof is per-layer `-I` sets.** `CLAUDE.md`: *each directory compiles with its own
include set, so a name a layer must not reach has no spelling in it, and a breach is a compile error
rather than a report.* One `-Iinclude` that resolves everything gives every layer a spelling for
everything, and the proof becomes a convention.

## The options

| | what it costs | what it buys |
|---|---|---|
| **A. Move the 47** into `include/outshine/`, private headers stay in `src/` | a third of the library is "public"; the distinction is weak | one `-I` for a consumer, today, mechanically |
| **B. Reduce the exposure first** -- the eight stop naming internals by value, `pimpl` or opaque handles where they do | the largest of the three, and it touches the renderer's shape | a public surface that is 8 headers and stays 8 |
| **C. Two include roots**: the library's own layers keep their narrow sets, `include/` is built for consumers only and its headers are self-contained | the self-containment is B's work by another name | both claims, if B's work is done |

## The names are the argument against A, and the owner made it

**`Live.h`, `Paint.h`, `Layout.h`, `Pointer.h` are implementation names.** They say how this engine is
built, not what a consumer gets, and SDL -- the pattern this is being held to -- names its public
headers after the CAPABILITY: `SDL_gpu.h`, `SDL_render.h`, `SDL_audio.h`. **Moving eight internal
headers into `include/outshine/` produces eight internal names with a new prefix**, and the reader of
`#include <outshine/Live.h>` learns nothing they did not already have to know.

**So the surface is DESIGNED and not harvested.** Roughly:

| brick | what it is for | what is behind it today |
|---|---|---|
| `<outshine/Outshine.h>` | the engine: stand a scenario up and advance it | `Live.h` |
| `<outshine/Render.h>` | a device, a plan, a frame | `Renderer.h` · `stages/OverlayDraw.h` |
| `<outshine/Ui.h>` | a declared surface: lay it out, paint it, point at it | `Layout.h` · `Paint.h` · `Pointer.h` |
| `<outshine/Script.h>` | the bounded interpreter | `Script.h` |
| `<outshine/Log.h>` | what the library says and to whom | `io/Log.h` |

**And that shrinks the other number too.** The 39 transitively reached headers are reached because the
eight expose internals by value; a brick named for a capability is free to expose less, which is
exactly B's work with a reason to do it that is not merely tidiness.

**Recommendation: B, and A is no longer a useful first step.** Move first so a consumer has one `-I` and the shape is
visible; then shrink the surface header by header, with the count of transitively reached headers as
the metric that says whether it is working. **A is reversible and B is not, which is the order to do
them in.**

## What is NOT in question

**The viewer must link the archive.** `test/run.sh` compiles the library's own source directories into
the viewer binary, so `liboutshine.a` -- the thing the `Makefile` builds -- is linked by nothing, and
this tree has never proved the library links as a library. That is `board:1478`'s own line and it needs
no decision, only work.
