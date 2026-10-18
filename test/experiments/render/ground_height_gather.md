# Ground height stencil: rejected gather alternative

Owner: [#515](https://github.com/Weichwolf/outshine/issues/515).

The existing lattice reads the centre and four height neighbours separately;
coarse fragment normals read four neighbours. Two `textureGather` operations can
recover the same stencil from the existing R32Float atlas without another asset,
upload, format or resident allocation. Their eight texel values replace five or
four scalar fetches. Fewer source instructions do not imply less hardware work.

`ground_height_gather_checks.py` independently checks all 33×33 node positions
in all 256 atlas slots across two layers: Float32 normalized coordinates, gather
component order and halo bounds. All 557,568 node placements pass. The private
shader transformation retains the original gradient arithmetic and fine-normal
interpolation. Shadow artifacts remain unchanged.

Native `76f6d3dbf`, unchanged Körbersee, 1280×720/30, fresh offline processes,
exactly 30 frames/360° per capture:

| Capture | Original GPU mean, ms | Gather GPU mean, ms |
|---|---:|---:|
| First A/B pair | 23.227 | 17.244 |
| First reversed pair | 21.622 | 21.970 |
| Second A/B pair | 17.931 | 18.959 |
| Second reversed pair | 20.692 | 19.082 |

The first A/B/B/A averages are 22.424 versus 19.607 ms; the repetition gives
19.311 versus 19.021 ms, only 0.290 ms apart. GPU maxima remain approximately
36–37 ms. The changing means do not establish a stable gain. Reject the
alternative; product shaders stay unchanged. No whole-device budget admission.

All eight PNGs have identical full SHA-256:
`076343f482591dc796031d61805f847a2b728f0a32fc45b0a17ebf8d003c895e`.
Existing actual Metal tests pass before and after with `MTL_DEBUG_LAYER=1`:
`AdaptivePatchesKeepFineNormals` (96 checks) and
`SurfacePatchesPreserveRasterAndFractions` (423 checks). An initial invocation
without the required validation environment failed before device creation;
that invocation supplies no shader evidence.

Private evidence: `/tmp/outshine-ground-gather-489296f04-run/receipt.json`,
`repeat.json`, `checks.json` and their original logs. No concurrent full lint
ran during these captures; ordinary OS/background variation was not eliminated.

```sh
python3 test/experiments/render/ground_height_gather_checks.py
python3 test/experiments/render/native_ground_shader_compare.py \
  --alternative gather --native-root "$native_checkout" \
  --output /tmp/outshine-ground-gather-run
```

Use the existing Python 3.10+ NumPy environment. The same driver also reproduces
the [rejected distance alternative](ground_class_distance.md).
