Type: bug
Area: world
Tags: instrument

**Light and shadow**

- **A telemetry column with one reachable value, behind a branch no compiler can take.**
  `render/Renderer.cpp:111-115` writes `bool rg11 = false;` and then `if (rg11) feats.push_back(...)`,
  and line 177 logs `"hdr"` as `HdrFormat == RG11B10Ufloat ? "rg11b10ufloat" : "rgba16float"`. The
  format is set unconditionally to `RGBA16Float` at line 112 and never assigned again, so the feature
  request is dead and the log column is a constant string dressed as a measurement. The comment above
  it gives the real reason — rg11b10 has no alpha and the HDR target's alpha carries the occlusion
  fraction — which is a decision, not a run-time condition. Right: the format is a stated property of
  the plan (`board/` § I.27) with the alpha requirement as its reason, and a row that
  reports a format reports one the plan could actually have chosen.
- **Two adjacent terrain tiles compute two different normals at the posting they share.**
  `world/ChunkMesh.h:100-108` clamps the central difference at the grid border (`i0 = i > 0 ? i - 1 : i`),
  so the east edge of tile (x,y) is a one-sided difference toward the tile's interior and the west edge
  of tile (x+1,y) is the opposite one-sided difference toward *its* interior. The **positions** agree
  exactly — `osmmesh_tile_frac_to_geo(z,x,y,1,·)` and `(z,x+1,y,0,·)` are the same point, which is why
  there is no crack — but the shading normals do not, and `TerrainDraw`'s fragment builds its whole
  relief frame off the interpolated vertex normal (`nn = normalize(nrmIn)`, `groundMat`). The result is a
  lighting discontinuity along every tile boundary, a rectilinear grid at ~1.5 km spacing on z14. Right:
  sample one ghost posting beyond each edge — `osmmesh_tile_frac_to_geo` is defined outside `[0,1]` and
  needs no neighbouring tile, so this costs `2(gr + gc)` extra ellipsoid conversions and no streaming
  dependency — and give every drawn posting a centred difference. Decides it, and it is **decidable**
  with no reference: the normal at `fx = 1.0` of one tile against the normal at `fx = 0.0` of its
  neighbour must be the same vector.
