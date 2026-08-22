Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**III.3 Representation and level of detail**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Procedural growth: trunk, taper, minimum radius, twig radius, branch chance, branch angle and its variance, order length and radius, wander, leader bias, branch up-bias, whorl count and spacing, terminal fork, shade prune *(was 0493)*
- [ ] Trunk sides as a declared polygon count *(was 0494)*
- [ ] Bark colour, darkening, frequency, ridge and style *(was 0495)*
- [ ] Leaf kinds: broad, needle, palmate, pinnate, palmate compound *(was 0496)*
- [ ] Leaf blade: segments, length, width, widest point, base fill, base skew, tip, lobes, lobe depth, serration, fold, curve, leaflets, palmate lobes and spread *(was 0497)*
- [ ] Needles: width, length, forward rake, droop *(was 0498)*
- [ ] Leaf cards per point and a card budget per prototype *(was 0499)*
- [ ] Leaf angle distribution as a declared population *(was 0500)*
- [ ] Four mesh LOD levels plus an impostor rung, one ladder, model-space error as a fraction of height *(was 0501)*
- [ ] Instanced sheets standing for sixteen quad elements each *(was 0502)*
- [ ] Octahedral impostor atlas baked at runtime from our own grown prototype *(was 0503)*
- [ ] Every declared species measured by a bench (`make treebench`, the deleted tree bench enumerates the directory rather than listing names, so a form nobody grew cannot look green) *(was 0504)*
- [ ] A grower that takes a *(was 0505)*
- [ ] Impostor cells that are never sampled without a bake *(was 0506)*
- [ ] A far rank that is one plane per stand, merged per fixed spatial cell into a single draw, corners expanded in the vertex shader so each element faces the camera individually *(was 0507)*
- [ ] Crowns that are not bow-ties: the cross must never survive to the range where its own geometry is legible *(was 0508)*
- [ ] Stands that do not vanish seen from directly above *(was 0509)*
- [ ] Crown self-shadowing, so a crown reads as one mass with a lit top and a shadowed underside *(was 0510)*
- [ ] Leaf albedo at the top comparison rung *(was 0511)*
- [ ] Two-sided transmission through a leaf, driven by the material declaration rather than a per-leaf shader *(was 0512)*
- [ ] Bark normal detail at the near rung, as a function *(was 0513)*
- [ ] Root flare, so a trunk meets the ground instead of intersecting it *(was 0514)*
- [ ] Buttress roots on a mature beech *(was 0515)*
- [ ] Lean and sweep, so a stand is not a set of verticals *(was 0516)*
- [ ] Damage forms: broken leader, forked stem, lightning scar, browsing line *(was 0517)*
- [ ] Epiphytes on a host: ivy on the trunk, moss on the north side, lichen on the bark *(was 0518)*
- [ ] Merged-mesh treatment for a dense low stratum, batched into fixed cells that LOD by removing items *(was 0519)*
- [ ] Flower and fruit as declared elements *(was 0520)*
