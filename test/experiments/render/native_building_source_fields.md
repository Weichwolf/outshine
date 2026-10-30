# Native complete-source field jobs

`NativeBuildingSourceFields` uses the production `BuildingFieldRequest`: prepared
native assets → metadata job → selected field job → stored field → held CPU lease.
It adds no renderer, scheduler, source selector or meshing algorithm.

```sh
cmake --build "$native_build" --target test_experiments_render_NativeBuildingSourceFields --parallel
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings:test/experiments/transport \
python test/experiments/render/compare_native_building_fields.py \
  --binary "$native_build/test_experiments_render_NativeBuildingSourceFields" \
  --output "$fresh_system_temp_output"
```

The existing versioned original archive supplies the complete selected cells.
Import stages its full native tile through the existing full-tile codec and
generic AssetCache. The existing loader migrates that snapshot to index/cell
products before timing begins. Import is experiment preparation, never a World
loading algorithm. Normal requests must read only selected stored cell forms;
they must not rebuild them.

The fresh hit runs without SDL initialization or a native source store. Both
archive inputs and native source storage are moved aside during that process.
The stored source descriptor, field, intrinsic World frame and cache suffice;
encoded atlas bytes and frame must remain exact.
Another declared but unstored view must fail explicitly without publishing an
atlas; the cached view cannot stand in for it.

Direct invocation takes source descriptor JSON, request JSON and a new output
directory. Requests use the existing direction/pixel catalogue and explicit
source/face/triangle, producer-reservation, resident-reservation and field-byte
limits. A request with `source` and `cache` fields uses only stored field data.
`native-field-result.json` separates archive import, source reads/writes, request
wall time, reserved input and sampled host heap. Sampling occurs in the polling
driver, not the runtime hot path. It is not an exact peak or GPU/driver counter.

This proves the real native job/cache/CPU-lease connection. It does not compare
the resulting field pixels with independent original rendering, qualify pose or
background composition, activate World replacement, or prove 720p60/8GB. The
producer reservation remains an admission estimate until all working memory is
bounded. Outputs stay in system temp; no ordinary oracle or Place is changed.
