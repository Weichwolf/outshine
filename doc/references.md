# References

The bar in [`vision.md`](vision.md) is not reachable by invention. Nearly everything Outshine needs is
written down, measured and shipped somewhere already; the engineering task is integration, and an
invention owes a reason that stands next to it ([`CLAUDE.md`](../CLAUDE.md), *Haltung*).

This file says **why each title is here and where it bites** — nothing else. It is not a reading list
and it is not a bibliography of everything good; a title earns its row by answering a question this
tree actually asks.

## Spec

A round that reaches for a technique names its source in one line at the decision point, the way
[`conventions.md`](conventions.md) already requires for numbers. When an implementation departs from
the canonical form, the departure carries its measurement — otherwise it is not a decision, it is
drift.

## State

### Engine

| Title | Where it bites |
|---|---|
| Jason Gregory, **Game Engine Architecture**, 3rd ed. | The reference for the class of engine this is, written from Naughty Dog — the studio behind the Days Gone / Horizon-class targets in `vision.md`. Subsystem boundaries, the tick, resource lifetime, tools-vs-runtime. The `Outshine` entry point and the `world/` ↔ `render/` seam answer to this book. |
| Eric Lengyel, **Foundations of Game Engine Development**, I Mathematics · II Rendering · III Animation | Volume I is the transform and geometric-algebra ground under ECEF, reversed-Z and the camera basis; II is the shading and visibility layer; III is what the body format will need when actors move. Short, dense, no framework attached. |

### Rendering

| Title | Where it bites |
|---|---|
| Akenine-Möller, Haines, Hoffman u.a., **Real-Time Rendering**, 4th ed. | The survey that settles arguments: shadow techniques, LOD and its transitions, antialiasing, the whole BRDF chapter. When a round says "AAA does it this way", this is where that claim is checked. |
| Matt Pharr, Wenzel Jakob, Greg Humphreys, **Physically Based Rendering**, 4th ed. (free at pbr-book.org) | Not because Outshine ray-traces, but because it is the unambiguous statement of what radiance, irradiance and reflectance *mean*. Every "the number is right but the picture is wrong" round in this tree turned on that distinction. |
| Sébastien Lagarde, Charles de Rousiers, **Moving Frostbite to Physically Based Rendering** (SIGGRAPH course notes) | The single best account of the units chain end to end: photometric light values, exposure as EV, and the tonemap. The defect where a white point sat at scene radiance 182 because the range came from the sRGB container is exactly the failure these notes exist to prevent. |
| Eric Bruneton, Fabrice Neyret, **Precomputed Atmospheric Scattering** (EGSR 2008), and Bruneton's 2017 revision | Already the shape of the sky and aerial perspective here. Kept in the list because the failure mode is instructive: the tables were correct and an authored halo drawn over them made them look wrong. |

### Procedural

| Title | Where it bites |
|---|---|
| Ebert, Musgrave, Peachey, Perlin, Worley, **Texturing & Modeling: A Procedural Approach**, 3rd ed. | The canon for principle 2. Noise bases, spectral synthesis, antialiasing a procedural function, and Musgrave's fractal-terrain chapters — including the honest limits, which matter more here than the recipes: it says plainly how much amplitude a self-affine surface carries over a given wavelength, and that number is what separates legitimate surface detail from painted detail. |

### C++

| Title | Where it bites |
|---|---|
| Bjarne Stroustrup, Herb Sutter, **C++ Core Guidelines** (isocpp.github.io/CppCoreGuidelines) | **BINDING, by the owner's decision** — not a canon entry but a rule, ranked with the hard rules in [`CLAUDE.md`](../CLAUDE.md). It settles ownership, lifetime, interface and style; a departure is a defect until its reason stands beside it, and where it collides with a house opinion it wins. Everything `CLAUDE.md` states about C++ is a **house deviation from it**, not a substitute. |
| Scott Meyers, **Effective Modern C++** | Move semantics, `auto`, lambdas, smart-pointer ownership — the idiom layer this tree is written in. |
| Fedor Pikus, **The Art of Writing Efficient Programs** | Where the performance work actually lives: memory order, cache behaviour, allocation, and — its most useful chapter here — how to measure so the measurement is not the artefact. This tree's rule that a benchmark pins its binary is the same instinct. |

### Physics

| Title | Where it bites |
|---|---|
| Christer Ericson, **Real-Time Collision Detection** | Broad phase, spatial partitioning, robust primitive tests. The contact half of [`body-format.md`](body-format.md) answers to it. |
| Robert Bridson, **Fluid Simulation for Computer Graphics**, 2nd ed. | For water beyond a reflecting plane. Its early chapters also state the surface-wave model (Gerstner and successors) that a lake needs before any of the rest applies. |

## Gaps

- **No canonical text covers OSM-as-world.** The nearest things are implementations, not books:
  OSM2World, F4map, and Microsoft Flight Simulator's published talks. They are read as references and
  named where used, but nothing here is settled the way the rows above are settled.
- **Vegetation has no book, only a product.** SpeedTree's SDK documentation and its published talks
  are the reference for the LOD and instancing structure; growth models come from forestry literature,
  cited at the number rather than here.
- **No title above has been read cover to cover in this project.** They are cited where a technique is
  taken, and a citation means the technique was checked against them — not that the book was studied.
  Saying otherwise would be the kind of claim this tree does not make.
