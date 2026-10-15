# Portable whole-face gap reference

This is the unchanged 25-pair corpus from [PR #150](https://github.com/Weichwolf/outshine/pull/150),
delivered for the native consumer in [#48](https://github.com/Weichwolf/outshine/issues/48).
It contains complete own bridge/continuation/tunnel triangle height surfaces, not
decompiled code or an SDK runtime dependency. Normal verification reads these pins;
it neither rebuilds recipes nor updates expected results.

`pairs.npz` SHA256:
`e4b8279fccabcb5f10999417f2ae6e6a87c282c19a4322ab4681f2a835d101de`.
Load with `numpy.load(path, allow_pickle=False)`. Each `metadata.json` case names
its lower/upper `vertices` (Nx3 float64) and `faces` (Mx3 int64) arrays, dtype,
shape and raw C-order SHA256. Preserve the face-row order as original face IDs.
No coordinates, faces or row IDs were remapped for this delivery.

`metadata.json` binds the common metric frame, surface kinds, original source IDs
and fixture hashes. Source IDs describe whole-surface contributions, not exclusive
whole-face OSM ownership. The local XY projection and unproven vertical-datum
boundary are explicit; `layer` tags are not heights. Terrain is selected explicitly.
`pins.json` retains the existing Python minima and deterministic witness points.
For native comparisons, a different witness on a minimum plateau may be correct:
check positive overlap and attained signed gap, not just equality of tie-breaking.
The native consumer owns its stated numerical tolerance and frame/source binding.

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 PYTHONPATH=lib/src \
  .venv/bin/python -W error test/experiments/transport/verify_height_gap_reference.py
```

The corpus checks individual affine-surface operations. It does not certify a
complete road generator, traffic routes, terrain construction or device costs.
