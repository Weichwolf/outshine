Type: bug
State: open
Area: world, render
Tags: measured, picture

# The ground wears the floor UNDER the vegetation, and nothing stands on it

**Benchmark** — Unreal: a landscape layer paints the SURFACE and `FFoliageType` instances are
scattered on top of it; the layer weight also drives where the foliage goes, so ground and cover
cannot disagree. RAGE: terrain materials plus a separate grass/prop scatter driven by the same
material index. **Both agree, and the matter is closed**: the ground material is what is under the
plants, and the plants are separate instanced geometry. Neither paints a forest green onto the
terrain.

## This tree does the first half and not the second

`ground-materials.json` says it in its own subject line: *"A ground material is an independent
layer that a vegetation template REFERENCES; foliage and clutter sit on top of it and never
replace it."* The file is right and its twenty classes are right. **Nineteen of the twenty are
brown, grey or blue.** The only green one is `moss` at [0.126, 0.180, 0.093]:

| class | linear albedo |
|---|---|
| forest_floor | [0.224, 0.135, 0.049] |
| leaf_litter | [0.264, 0.190, 0.113] |
| needle_litter | [0.209, 0.126, 0.073] |
| grass_thatch | [0.280, 0.189, 0.078] |
| earth_dry | [0.235, 0.159, 0.090] |

There is no `grass`, no `meadow`, no `crop` and no canopy, and there should not be: a beech wood's
floor IS brown, and `grass_thatch` is DEAD grass, which is brown too. **The brown is correct.**

## Measured

At the rebuild that produced Heidelberg's picture, logged past the ledger:

    named=253515  unmapped=0  version=3  verts=653400

38.8 % of the ring's vertices wear a REAL land class -- not one falls to the unmapped row. The
classification works. The Koenigstuhl comes out (143, 123, 108) because it is being drawn as what
it is underneath: `forest_floor`. What a viewer sees of a beech wood from outside is the CANOPY,
and no canopy is drawn.

`src/generators/Forest.cpp` exists and is a complete generator. `git grep Forest` outside its own
files finds exactly one caller -- `Shipped.cpp`, the registry that lists it. **No place scenario
puts a tree anywhere**, and no measure in `--audit --measures` mentions a tree, foliage, a canopy
or vegetation of any kind.

## What will be true

- [ ] A land class that means vegetation SCATTERS the generator its `vegetation.json` template
      already names, and the instances are drawn. The ground keeps its floor material underneath,
      unchanged -- this item adds a layer, it does not repaint one.
- [ ] Measurement that shows this is wrong: the Koenigstuhl's pixels at Heidelberg's declared
      hour. They read (143, 123, 108) today, R > G, and must read G > R once a canopy stands over
      them. An aerial photograph of that hill in June is unambiguously green.
- [ ] Negative control: a place whose classes are all mineral -- rock, scree, paving -- gains no
      instances and its pixels do not move.
- [ ] The frame budget is stated with the answer, because this is the first item that adds
      geometry to every vegetated square metre of the world. `Forest` is a RECURSIVE generator and
      CLAUDE.md lists high geometry with recursive generators FIRST among the five things the
      budget is laid out for; that is what it is for, and what it costs is the item's to say.
