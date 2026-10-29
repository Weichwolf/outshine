# Existing point/prefix spatial reuse controls

Owner: [#1759](https://github.com/Weichwolf/outshine/issues/1759), Parent #1570.
This is an ongoing Python diagnosis. Native capture density belongs to #1756;
the direct source-relight raster belongs to Master in #1760. No new representation,
shader, renderer, jobgraph, World admission or native API is introduced here.

## Verified finite controls

The frozen A18/Metal inputs are under `test/outshine/data/render/building-depth-samples/`.
The reader verifies NPZ and original full-buffer SHA256, restoring clear borders
incrementally without allocating full1280×720 buffers. It preserves the actual
native geometry/capture/driver/binary/light binding across the three input sets.
The declared source contains1558 originals/38476 triangles; source headers and
native readbacks are authoritative here, not a new Python primitive replay.

`MaterialFieldSamples::Prepare` defines the orthographic capture: eye=C+3H*d,
near=H, far=5H. Its two actual Float32 images determine the source raster128².
The recorded camera pitch reconstructs the original grid; the stored96×128 crop
is padded with its verified transparent-black clear value. All2687 point hits
match the original native capture mask. No fractional mask becomes an opaque point.

The existing `summed_field`/`reconstruct` path integrates this canonical native
**sample radiance**, held without query relighting. Float64 is an independent
arithmetic control, not an emulation of native Half/Hi-Lo shaders. The centred
crop keeps the native focal length and every full-frame pixel ray exactly;
projected occupied candidate support must fit. This is no lower-resolution profile.

| Native query / finite reference | Held-prefix RGB-L1 | Coverage-L1 | Direct block axis |
|---|---:|---:|---:|
|128px, yaw0 /4² |95.6788% |42.4742% |1 |
|64px, yaw0 /4² |48.2634% |20.8852% |2 |
|32px, yaw0 /4² |22.0210% |8.8650% |4 |
|64px, yaw4° /4² |60.4814% |30.1987% |not aligned |
|32px, yaw0 /64² |24.4157% |9.8998% |4 |

For aligned footprints, direct means of the **same** stored source cells match
the existing prefix within3.64e-12 maximum RGBA. At32px/4², prefix/native-sample
coverage differs by at most1.25e-13, and RGB-L1 by0.0776%. This narrow total-image
agreement is not an isolated BRDF bound: projection/sampling may also contribute.
The native32px/64² sample opponent remains23.1892%RGB/8.8111%coverage from #1752.
Its original4²→64² reference changes by23.0398%RGB/8.8137%coverage. None of these
finite sample counts certifies a converged continuous image.

The existing outward-rounded `displacement_bound` gives the declared whole-source
flat-projection L1 bound0.221357px at32px, despite the large finite image error.
It bounds geometry only, excluding capture discretization, visibility, materials,
BRDF changes and filtering. It must not admit a new colour/coverage/LOD domain.

## Remaining causal checks

Master's512 native result (#1756) improves direct sample error to about4.06%RGB,
while native flat-prefix error remains about14.30%; these are separate native
measurements, not the held128-source Python path above. Density, registration,
PBR-view reuse and filtering still require separate matched inputs.

`MaterialFieldRelight::FrameOf` uses -Forward as the orthographic PBR viewing
direction, and stored samples require the original capture camera. Capture
projection and shading direction are coupled by the current native contract.
This packet neither changes it nor attributes all residual error to depth.

For quantization-independent original visibility, the legacy/source-world Python
reconstruction matches source counts and UV/colour/index bytes, but six position/
normal hashes differ from the actual native opponent. Matching world bounds do
not make it the identical input. Exact original/frame and512 raw/query-relight
inputs are requested in [the technical question](https://github.com/Weichwolf/outshine/issues/1759#issuecomment-6508522199).
Only dependent original-ray/PBR checks wait; current arithmetic controls continue.

SIGGRAPH first: Shade et al.,
[Layered Depth Images,1998](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
pages3–5, equations3–9/Figure3, inspected visually. Geometric warp, resampling and
disocclusion are separate problems; no new LDI family is implemented.

## Reproduce and costs

Use the existing `.venv`, NumPy and optional Matplotlib, with
`PYTHONPATH=lib/src:test/experiments/render`, BLAS/OMP threads1:

```sh
python -B -m unittest test_building_sample_reuse test_field_view_sampling test_field_view_integrals test_field_composition test_field_view_projection
python -B -m compare_building_sample_reuse --output /tmp/outshine-research-reuse/run
python -B -m plot_building_sample_reuse --study /tmp/outshine-research-reuse/run --output /tmp/outshine-research-reuse/error.png
```

Sources must be frozen and outputs fresh/outside the checkout.43 checks pass;
the actual native-channel/error figure was generated and opened privately.
The source of that figure is4c6dac144; current88d18e455 adds geometric bounds/cost
metadata without changing its pixel arrays. No README/doc image publication.

One Linux/i5-1135G7/Python3.13.5/NumPy2.5.4 run at nice10 under load13.21/13.58/14.31:
full-buffer verification/decode0.878sCPU/1.304swall; prefix plus five comparisons
0.121sCPU/0.222swall. Decoded arrays total7667712B;575078400B
are incrementally hashed full-frame bytes. One16384-texel source,532512B Float64
prefix,94852 prefix evaluations/379408 logical fetches; process RSS peak54280192B.
Held input/prefix bytes are not added to RSS. Allocation events/total copies are
unmeasured; no alternative speedup, IO/runtime policy, GPU/device/p99 or UMA claim.
