# Recessed facade surfaces

`outshine_ref.buildings.recessed_facades.RecessPlan` references an existing
`FacadeRepeat`. It adds inward depth, a front return and an immutable pair of
metric frame widths. It does not choose window positions, room uses or doors.
The recipe is procedural; provider geometry and inferred openings remain distinct.

`recess_surface(plan, bounds=None, max_cells=50000, method="batch")` emits
caller-owned NumPy arrays in local `(U, V, outward)` metres. The basis is right
handed. The facade front lies at outward zero; glass is at `-depth_m`.
The original opening rectangle has a wall return to `-front_step_m`, followed
by the frame annulus and its inner returns down to the glass.

The output contains positions, triangle indices, hard point normals, repeat UVs,
face roles and global `(column, row)` identities. Roles are 0 wall, 1 glass,
2 frame. Return faces have degenerate projected UVs; callers use a plain material
there rather than applying a 2D facade-repeat shader to them.
The perimeter is open and belongs to the existing building shell. This is a
render surface, without a new collision, room, door or native asset ABI.

Bounds are half-open global cell indices `(x0, y0, x1, y1)`. Count and uint32 index
capacity are checked before any cell grid or geometry expansion. Empty ranges
remain cheap even in a very large logical repeat. Partial ranges replay the same
global cells as the full product. Camera/detail selection happens in the caller.
The old facade experiment retains its original cell-relative frame widths;
the current city adapter uses 0.06 m frames and 0.18 m depth.

Scalar and batch paths share the same cell template and emit identical arrays.
The batch path performs one position-array addition instead of one per cell and
avoids the scalar path's list of intermediate position arrays. Other allocations,
UVs, normals and output arrays remain. The experiment reports those quantities
separately; neither construction path is a runtime/device performance claim.

Independent controls cover projected wall/frame/glass areas, removed cavity
volume, normal orientation, perpendicular and oblique ray intersections,
partial/full replay and capacity rejection before expansion. The city adapter
checks actual source-footprint containment and original rectangular facade area,
keeps far geometry by identity, and leaves constructive room/door plans alone.
Its existing painted fallback remains for nonrectangular or unsupported frames.
Projected recess demand is a geometry criterion, not a radiance, shadow or night
lighting error bound. Material/lighting continuity across detail transitions and
native admission remain unproved.

The source derives from the former `facade_geometry.py` prototype. References:
[CGA Shape, SIGGRAPH 2006](../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
the external local Candler `WindowOpening`/negative-extrusion rule and pinned
Ghidra extrusion dispatch evidence. Full executable SDK/material equivalence is
not asserted. Feature, actual consumers and visual/cost evidence belong to
[#318](https://github.com/Weichwolf/outshine/issues/318).
