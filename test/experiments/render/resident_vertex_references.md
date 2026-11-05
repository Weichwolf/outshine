# Resident vertex references

Native input for #1955; complements `metal_vertex_pulling.md`.
Read back actual source indices, cluster templates, instance roots, batch state and
accepted GPU selection after measurement. No cache-row sampling or assumed cluster size.
Terrain, prisms and transmission use separate products and are outside this export.

```sh
build/outshine benchmark --scenario scene.scenario --view start --frames 60 \
  --angular-velocity 0,-360,0 --offline --geometry-output /tmp/resident-geometry
python3 -B test/experiments/render/resident_vertex_references.py /tmp/resident-geometry
```

The output directory must be new. `geometry.json` is written last as the completion
marker; `benchmark.json` records the measurement before snapshot allocation/readback.
The Python comparison validates sizes, source ranges and selected index counts against
native draw arguments. Rejected roots cannot contribute stale `kept` values.

## Tokyo observation, 2026-11-05

- 8,355 resident mesh batches; 151 distinct material/layout/pipeline/motion/pass/
  instance-count/surface-kind tuples. These are state groups, not measured merged draws.
- 50,572 logical clusters/source ranges; 12,018 selected, 4,205,964 selected indices.
- Maximum 384 unique vertices per cluster; p50/p95/p99 = 211/354/368.
  A fixed 64-slot virtual vertex address is invalid. Nine local bits cover this input;
  other products still require admission checks.
- Original source indices: 71,398,896 bytes across unique ranges.
  Uniform 16-bit cluster-local indices plus 32-bit vertex references:
  35,699,448 + 42,207,636 = 77,907,084 bytes. Additional indirection loses memory here.
- Per-cluster minimum vertex index plus 8/16/32-bit deltas: 35,605,402 bytes before
  alignment/mode metadata. Vertex spans reach 412,329; uniform 16-bit deltas are invalid.

These are representation costs, not GPU speedups. Compare grouped vertex pulling with
source-index fetch against local unique-vertex references. Corner IDs lose within-cluster
post-transform reuse; the larger reference table may still win total GPU time.
Compare compression separately; retain materials, transforms, coverage and shadow semantics.
The synthetic Metal result does not establish scene performance or p99 compliance.
