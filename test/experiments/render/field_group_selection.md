# Field grouping before payload IO

Issue: [#1955](https://github.com/Weichwolf/outshine/issues/1955).

`compare_field_group_selection.py` reads native source metadata, source traits and
the spatial hierarchy in one read-only SQLite transaction. Basis, source forms,
meshes and raster fields remain unopened. Select an explicit nearest-tile inventory;
this is a demand experiment, not a reconstruction of World residency.

```sh
python3 test/experiments/render/compare_field_group_selection.py \
  --database build/assets/assets.sqlite --eye LAT LON ASL_M \
  --fov 55 --viewport 1280 720 --error-px 1 --tiles 49 \
  --output "$TMPDIR/outshine-field-groups.json"
python3 -m unittest discover -s test/experiments/render -p test_field_group_selection.py
```

Requires Python >=3.10 and NumPy. No network requests or cache writes.

1. Transform each leaf's ECEF box into the producer's local frame. Union child
   boxes bottom-up, matching the complete source-view bounding operation.
2. Estimate the existing capture request and its complete rotated capture domain.
   Evaluate the fixed eye under all visible rotations using the panoramic
   [reference](field_panoramic_geometry.md). Float candidates grant no native authority.
3. Refine a rejected parent; retain geometry/unknown leaves. Verify exact source
   partitioning. Compare eligible subgroups with whole-cell admission.
4. Count capture texels, groups and residual sources before native generation.
   More eligible sources alone are not a win: raster residency, draw bindings,
   real source support, occlusion, shadows and native costs still decide admission.

Tests exercise approaching a town, foreground geometry sharing a cell with far
buildings, an unavailable source retaining its original, and camera-domain bounds.
Frames/numerics secure these game behaviors. Model timing includes Python metadata
preparation and diagnostic recomputation; it is neither native load time nor GPU time.

Native counterparts: `SourceProductSelection`, `BuildingFieldStreaming`,
`BuildingSourceView`, `ImpostorBaker::ProjectionFor`, `ImpostorPoseBounds`.
