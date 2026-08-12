# Outshine

> **A CryEngine-class game engine: an OSM-based global open world, LLM-driven intelligence and an RPG
> above it, every piece of content from a generator behind one interface, external data behind another,
> and declarative `scenarios/` that declare interactive or non-interactive worlds — with or without a
> world at all. Kingdom Come: Deliverance is the world and its vegetation, GTA 5 the built world and
> the verbs — walk, drive, fly.**

The world is **loaded, not modelled**: terrain, land cover, buildings, vegetation, weather and the night
sky are fetched from upstream and decoded here. **One physics system** carries walking, driving, flying
and swimming. An **epoch and decay dial** dresses the same geometry. The actors **think**. The setting is
post-scarcity — modern infrastructure, lush nature.

**The repository speaks one language: English.** Code, comments, documents, commit messages.

## The constraints, and there are no others

**SDL3** · **SDL_GPU** · **modern C++** · **this device at 720p60** — an Apple A18 Pro, 2 performance and
4 efficiency cores, 5 GPU cores, 8 GB, Metal 4. It is the development platform *and* the budget, so no
machine stands between the work and the target.

Everything else in this file is a stance or a setup. **The scope is [`doc/requirements.md`](doc/requirements.md)** and it is the
authority on what the engine must do.

## Stance

**The owner's comments outrank everything.** The bar is CryEngine's level out of upstream data alone, and
**the way is the goal** — a round that learned something is a good round.

**Nothing is a possession.** Formats, directories, algorithms, interfaces, build, tools — all material.

**Good C++ and proven engine design.** The [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines)
are binding and a deviation is a defect until its reason stands beside it. The established way is the
starting point; a deviation needs its reason too. Prefer the shape that makes a mistake **unspellable**
over the rule that merely forbids it — a rule a checker counts can be broken and then reported; a rule
the type system carries does not compile.

**The engine is a library and it is platform agnostic.** A kernel manages it, so this can: the library
declares what it needs from a host and calls nothing else. Everything that runs it is a test.

**Testability is a design property, not an afterthought.** If a thing cannot be tested, that is a fact
about its shape. **Very high coverage is part of the CryEngine-class claim**, not an extra: requirement
coverage is the target, line and branch coverage the instrument. Every commit is covered.

**Something missing is a task, not a limit.** "That number does not exist" ends with "so the tool gets
built". Distinguish **not measurable** from **not yet measured** — the second has a cost, not a boundary.

**Every number carries its origin** — derived, measured or `[SET]` — with its unit and frame of
reference. **No magic numbers.** Performance is a **distribution over a moving camera**, p50/p95/p99,
never a mean. **Appearance is judged by eye and in motion**; a number decides whether the frame floor
holds, never whether it looks right.

**The mathematics is deterministic.** If pace decides the result, the coupling is a bug.

## Setup

| | |
|---|---|
| `src/` | the library. **Pure C++ and nothing else** — no entry point, no build file, no asset |
| `test/` | **mirrors `src/` exactly**, plus the harness, the fixtures and the assets a test needs. The interactive client is a test; so is the frame oracle |
| `test/run.sh` | the harness. One process per test, a real verdict per test, non-zero on any failure or undeclared skip |
| `doc/requirements.md` | **the scope** — one line per feature with a box and a stable id; a ticked line **names the file that implements it and the test that holds it**. The architect extends it on its own evidence; only the owner shortens it |
| `doc/todo.md` | the current work item. Short |
| `doc/bugs.md` | what exists and is wrong — file and site, what decides it, what right looks like. A fixed bug is deleted |
| `.claude/agents/` | **`engine-developer`** builds and measures · **`engine-architect`** designs, judges and owns `doc/bugs.md` and `doc/requirements.md` |
| this file | the vision, the constraints, the stance, the setup. At most **100 lines** |

**Layering is the build, never a checker.** Each directory compiles with its own include set, so a name
it must not reach **has no spelling**. A breach is a compile error, and `test/` mirrors `src/` so every
test is a continuous proof that its layer's include set is exactly what it claims.

**A generator is a pure function `(Region, Ground) → Yield`**, `const noexcept`. **A provider is the same
idea for external data** — an upstream source declaring what it covers, behind one registry, so a new
one is a plugin rather than a patch. Both are declared, both are replaceable, neither knows the engine.

**Settings are two-tiered.** The library carries the defaults and the values a consumer must not change;
everything else is set by the test or the client. A value lives in exactly one tier.

**Only correct work is committed**, and `git log` is what was — no journal.

## References

**Stroustrup/Sutter, [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines) — BINDING.**

| Field | Titles |
|---|---|
| **Engine** | Gregory, *Game Engine Architecture* 3e · Lengyel, *Foundations of Game Engine Development* I–III |
| **Rendering** | Akenine-Möller, *Real-Time Rendering* 4e · Pharr, *Physically Based Rendering* 4e · Lagarde/de Rousiers, *Moving Frostbite to PBR* |
| **Procedural** | Ebert/Musgrave/Perlin/Worley, *Texturing & Modeling* — the canon for "appearance is a function" |
| **C++** | Meyers, *Effective Modern C++* · Pikus, *The Art of Writing Efficient Programs* |
| **Physics** | Ericson, *Real-Time Collision Detection* · Bridson, *Fluid Simulation* |
| **Implementations** | **CryEngine** — the level to match · **Kingdom Come: Deliverance** — the world and its vegetation on a known budget · **GTA 5** — the built world and the verbs · SpeedTree · Blender, as the external oracle a render can be checked against |

## Host

Apple A18 Pro, macOS 26.4.1, Apple clang. **macOS has no `timeout(1)`** — a harness brings its own.
