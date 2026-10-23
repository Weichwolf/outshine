# MVT stair endpoint inside a path ribbon

The fixed input preserves three OpenFreeMap source lines, their supplied tags,
derived native levels, recorded profile products and two actual rendered faces.
It is a frozen defect, not an accepted solution or a live engine test.

```sh
"$TRANSPORT_PY" test/experiments/transport/mvt_stair_t_junction.py --output "${TMPDIR:-/tmp}/outshine-stair-t-junction/observed.json"
```

Use the existing transport research environment (Python 3.13+, Shapely).
The stair endpoint misses a path centreline by about 0.282 m but lies inside its
0.75 m half-width. The native path has no junction there; the stair's upper end
has no prescribed grade. After its lower junction trims the stair, its remaining
length cannot reach the original upper height prior within its declared slope.
The fixed rendered faces overlap in XY with about 1.63 m vertical separation.
Provider tags and derived geometry/height information remain distinct.

The repair needs an admitted procedural connection and compatible profiles shared
by navigation, meshes and terrain contacts. Original source vertices need not
coincide in generalized MVT. Derived construction nodes must retain their origin;
geometric proximity alone does not establish source-node identity or compatible
levels. Existing exact-source frontier operations retain their narrower contract.

The graph-before-geometry architecture follows the existing
[transport reference review](../../../doc/references/transport/README.md).
The small projection and affine-face comparison do not implement either cited
paper's complete network algorithm. Native ownership and admission remain #826.
