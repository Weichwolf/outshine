# Reusing exact native camera requests

Owner: outshine_research, #1861 / #1797; native consumer: #1853.

`export_native_world_field.py` normally derives its two perspective cameras.
On the measured Linux host this produces `yfov_rad=0.9272952180016122`, while
the existing A18 ss2 request stores `0.9272952180016123`. Although the reported
Float32 Lens operands match, the existing native World compositor correctly
rejects the different Float64 request. Its exact binding remains unchanged.

`--camera-input <World-packet-directory>` instead reuses the recorded cameras
and capture without angle/basis/position regeneration. Existing bounded header,
executed-request and control-binding checks run before rendering. Profile, two
poses, original controls and 128x128 capture must match. One-ULP changed requests
and profile/control mismatches are refused; no rescaling or identity tolerance.
The recorded input-manifest SHA binds the actual bytes read. This optional step
does not read/admit pixel arrays or original geometry: the existing producer
validates the whole geometry, and the World consumer still validates every point.
The default analytic-camera path is preserved.

For an external existing binary, `--native-driver-source <observed.cpp>` records
the observed source hash explicitly. This is separate from binary SHA and does
not certify compilation/CDB provenance; native-owner qualification stays required.
No native source, GPU ABI, BRDF, shader or device profile changes.

With `PYTHONPATH=lib/src:test/experiments/render`, project `.venv` and one BLAS/OMP
thread, reproduce using fresh explicit system-temp outputs:

```sh
python test/experiments/render/export_native_world_field.py \
  --binary <existing-NativeBuildingImpostors> --sample-axis 2 \
  --camera-input test/outshine/data/render/building-world-opponents/ss2 \
  --native-driver-source <observed-native-driver.cpp> --output <fresh-temp>/world
python test/experiments/render/compose_native_source_materials.py \
  --native <raster-envelope-native-output> --input <fresh-temp>/world \
  --output <fresh-temp>/composition
```

## Finite supplemental Iris result

Python native export `e62a7c1f6570d7815fe7b9c5a0dd2819329641b7`, camera-metadata
follow-up `d96775a86d630b58e2a014a1c11cbecb79850a27`. All serialized native
request values remain exact after the metadata-only preparation change.
Observed native source SHA:
`ad19f31d8576b74260521688743f98b38199059ca30f01f75f0c44c558e003f7`.
Existing copied executable SHA:
`b32442c8c71d6820e9731126f29c68588663975f29e259d1f38a2cfb9b0c95d2`.
The external 552 SPIR-V modules and executable remain unchanged before/after.
This is an existing-binary experiment, not a fresh source-to-binary qualification.

Intel Iris Xe TGL GT2, Vulkan, raw2560x1440 single samples, then2x2→1280x720:
1107/38476 original triangles,3165/78374 vertices,190524/4850656 input-array bytes.
Three original-vs-selected HDR/depth captures are bit-exact. All six native
source/terrain/opening compositions have zero RGB, coverage and sample-depth
error, with the unchanged0.25HDR/0.125coverage limits. Eight unique diagnostic
PNGs opened; four repeated-pose PNGs byte-identical. No image publication.
Actual validation layer1.4.363 inserted in all eight source/World native process
logs: no validation errors/warnings. The older system1.4.309 layer reports
unknown new queue structures; that negative run remains separately recorded.

The selector uses the existing A18 CPU-upload operand packet. Effective Iris
camera/Lens matches are checked; no new Iris uniform-upload capture or hardware
raster error bound is claimed. Mixing Iris source pixels with A18 World pixels
remains rejected. Runtime admission, A18/native-owner acceptance, avoided IO,
device/UMA peaks and frame/p99 performance are unproven.

The first optional preflight unnecessarily reloaded the whole World/source input:
seven-round median1.316245s,321101 allocations,91088238 requested bytes,
11086652B scoped peak. The existing camera/control metadata checks alone take
5.506ms,13322 allocations,2392969 requested bytes,1055948B scoped peak on the
same Linux i5-1135G7 host under concurrent work. Separate Memray1.20.0 traces;
these are offline preparation costs, not cache-hit/frame or general speedup data.

32 focused cases passed across camera inputs, native World validation and source
triangle materials; Ruff/format/diff checks passed. Source/pixel validation was
not weakened. Full numeric inputs, compact native points, reports, commands,
shader snapshot and negative controls remain in the explicit temporary result.
Redundant raw outputs were removed only after all96 full-buffer SHAs matched
lossless sparse/crop reconstruction;36 unused integrals were discarded.

Own #1320 acceptance: Python reference-input ownership stays with the exporter;
existing World metadata checks are reused and full consumers retain their own
validation. Exact requests and source bindings precede GPU work. No new cache,
queue, renderer or admission policy. Related: #1861; parent #1797 remains active.
