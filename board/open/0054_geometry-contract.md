Type: feature
Area: scenario
Tags: instrument

**I.8 Geometry contract**

- [x] Core-defined vertex layout `pos3 @0 · uv2 @12 · nrm3 @20`, 32 B, `static_assert`ed
- [x] Declared second layout `pos3 · nrm3`, 24 B, for the water surface
- [x] Prototype plus instances, never geometry per instance
- [x] Positions as ECEF offsets from a declared anchor
- [ ] The anchor is **bounded by what is drawn from it**, so no shader computes a camera-relative position as the difference of two large floats: `|vertex − anchor|` within the block and `|anchor − eye|` within the view radius, for every field and not only the terrain. The vector fields anchor once at the standpoint and never again (`world/World.cpp` `Open`), which makes the bound the distance travelled — 1 px of near-field jitter at 65 km, 4.3 min of flight; the bug tasks in `board/` carries the derivation
- [x] Crack-freedom within a generator's own soup
- [ ] Winding declared once at registration instead of hard-coded at seven sites
- [ ] Mesh invariant check: unit normals, sign agreement with winding, angle agreement with the geometric normal
- [ ] Mesh invariant check: welding, with a split vertex legitimate only where a seam is declared
- [ ] Mesh invariant check: closure, as a declared property of the yield
- [ ] Mesh invariant check: degeneracy — zero area, NaN, index past the end, winding flip within a surface
