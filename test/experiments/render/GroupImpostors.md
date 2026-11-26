# Directional depth cards for middle-distance groups

The native baker captures orthographic albedo, coverage, normal, material and depth.
The Python reference reconstructs 11 held-out angles from eight horizontal views,
four oblique views and one overhead view. Two selected cards resolve by depth.
Planar blocks fit covered samples only and subdivide when the sampled residual
exceeds half a capture texel or the plane slope exceeds two. Coverage remains in
the texture. The triangle count assumes two triangles per planar block.

```sh
cmake --build build --target outshine_experiment_group_impostors
probe=$(mktemp -d "${TMPDIR:-/tmp}/outshine-group-impostors.XXXXXX")
mkdir "$probe/block" "$probe/tree" "$probe/stand"
reference=test/experiments/render/group_impostor_views.py
python3 -B "$reference" directions > "$probe/views"
bake=build/outshine_experiment_group_impostors
spruce=data/world/species/spruce.json
"$bake" buildings "$prepared_surface_tile" "$probe/views" 128 "$probe/block" 64
"$bake" tree "$spruce" "$probe/views" 256 "$probe/tree"
"$bake" stand "$spruce" "$probe/views" 128 "$probe/stand"
python3 -B "$reference" evaluate "$probe/block" "$probe/block-results"
python3 -B "$reference" evaluate "$probe/stand" "$probe/stand-results"
python3 -B "$reference" hierarchical "$probe/tree" "$probe/stand" "$probe/composition"
```

At 64 texels per view, coverage compares fractional intersection and union. The
building overlays use Tokyo source tiles `19-465596-206493` and `17-116398-51622`.
The canopy control uses 25 native spruces on staggered roots about 5 m apart.

| Group | Flat ring median | Two patched views median / worst | Peak triangles / source |
|---|---:|---:|---:|
| Tokyo block | 45% | 93% / 80% | 1,050 / 10,008 |
| Tokyo district | 44% | 81% / 72% | 2,074 / 5,776 |
| Spruce stand | 59% | 65% / 61% | 3,870 / 235,452,600 |

Start the building runtime trial at block size. Keep the source representation
where the oblique angles exceed the visible error budget. The district at 128
texels can already cost more triangles than its source. The two-view spruce result
loses too much internal canopy coverage for the same selection policy.

The optional last argument also bakes each native packed card at the requested
side. The reference reads back the packed cards. At 64 texels their coverage and
triangle results match the building rows above to the reported precision. Across
the block's probes, the two selected files occupy 93,976 bytes at the median and
97,956 bytes at the peak. The full set of 13 views occupies 613,464 bytes. This
supports making each direction an independently resident product.

Compositing the single spruce captures at the 25 roots reaches 82% median and
64% worst coverage overlap against the direct stand captures. One native bake of
24 views took 0.98 s for the tree and 21.02 s for the stand. The Python composition
of all 24 stand views took 0.22 s. This measures a bake candidate, with native
capture including GPU submission and readback. Runtime GPU cost and continuity
through both world handoffs require separate measurements.

[Billboard Clouds](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
[Layered Depth Images](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
[Hierarchical Image Caching](../../../doc/references/geometry/siggraph/1996-hierarchical-image-caching.pdf).
