Type: feature
State: open
Area: engine, world, generators
Tags: measured, owner

# A road AT GRADE is a material on the ground, not a ribbon over it

**Benchmark** — Unreal: a Landscape Spline DEFORMS the landscape heightmap and PAINTS its material
layer; the road is the landscape. RAGE: roads are baked into the terrain the map ships, materials
and all. **Both agree, and neither ships a road as a separate mesh laid on top.** A bridge is the
exception in both: a structure, its own body, resting on the terrain only at its abutments.

## Why, and it is already measured

Two meshes that merely TOUCH always leave a sliver: a ground triangle that crosses a carriageway
edge is above the road on one side and below it on the other, and no amount of pressing fixes a
seam the mesh has no vertex on. Measured on `cross-plane30` before the repair below:

    ground: the carriageway's footprint corners        104
    ground: of those, a ground vertex shares the spot    0

Sewing those corners INTO the ground mesh took the burial from 1.305 m to 0.540 m and, more to the
point, made the picture right rather than nearly right. The invariant is now an oracle and every
one of the 246 cells holds it.

**The consequence is the item.** If the ground already carries every vertex of the footprint, the
ribbon on top is a second body describing a surface the ground now describes itself.

## How

- **Resting infrastructure first, and it becomes the ground.** The ground is tessellated to the
  PROJECTION of the road band and those triangles take the road's material. Same for a building
  footprint: the terrain under it takes the building's material. No separate ribbon is drawn.
- **Floating infrastructure second, against the already-welded body.** A bridge keeps its own
  geometry. It welds at its upper outer vertices to the road ground, and the terrain UNDER it is
  tessellated and welded on its own.
- **The order is not decoration**: floating geometry can only share vertices with a body that
  already exists, so resting must be united first.

## What will be true

- [ ] Every vertex of a RESTING footprint is a vertex the ground mesh carries. Holds today:
      `ground: footprint corners NO ground vertex shares` reads 0 across all 246 cells.
- [ ] A resting way draws NO ribbon of its own -- the ground carries its material instead, and
      `streets: ways laid as ribbons` counts only what floats.
- [ ] Every END vertex of a floating span is a vertex the welded terrain/OSM body carries.
- [ ] Negative control that goes RED: move one footprint corner by 1 cm without re-sewing and
      require the shared-corner oracle to fail.
