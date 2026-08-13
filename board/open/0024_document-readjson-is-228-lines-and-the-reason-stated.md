Type: bug
Area: gltf
Tags: khronos, perf, instrument

**`Document::ReadJson` is 228 lines, and the reason stated for it is refuted 30 lines above it**

`src/gltf/Document.cpp:208-435`. Measured: **228 lines · 27 `if` · 18 `for` · 7 ternaries → ≥ 53 logical
paths · 23 `return Refuse` sites · eight top-level arms** (buffers, bufferViews, accessors, meshes,
cameras, nodes, the parent pass, scenes). `F.3`'s own enforcement note asks for a function that fits a
screen — *"try 60 lines"* — and *"more than 10 logical paths through"*: this is **3.8× the line budget
and 5.3× the path budget**. `F.2` as well, since eight arms is eight logical operations.

**The stated reason — one linear pass beats six functions sharing a refusal channel — is refuted by the
file itself.** `ResolveBuffers` (`:176-206`) *is* one of those six: a private member returning `bool`,
calling `Refuse`, invoked from `ReadJson:222`. It shares the channel through `Error_` with no ceremony,
it reads better than the arm it replaced, and it is the counter-example to its own argument sitting 30
lines above it.

**Ranked below the two entries above, because it admits no wrong value** — every arm's refusal is
correct today. It is a structure defect, and it is the one that decides how the next 400 lines land.

**Right:** six more members of `ResolveBuffers`' shape. **Timing:** not a round of its own — the
**opening edit of the round that widens the format**, because materials, textures, images, samplers,
skins, animations and extensions are seven more arms and roughly 400 more lines, so the split costs the
same before or after and is worth strictly more before. **What splitting does not buy, said plainly:**
the arms have a real order dependency — views need buffers, accessors need views, meshes need accessors,
nodes need meshes and cameras, scenes need nodes — which is today implicit in statement order and would
still be implicit in call order. The shape that would carry it is each arm taking what it depends on as
a parameter instead of reading the member; that is the version worth writing.
