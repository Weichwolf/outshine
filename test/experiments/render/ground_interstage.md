# Ground interstage controls

Owner: [#16](https://github.com/Weichwolf/outshine/issues/16).
Two unadmitted shader experiments against native shader basis `4d9d047ae`.
The unchanged benchmark executable is `e2ae80985`; its separate CPU-frustum
candidate #901 is not part of these patches. Original shaders already include
the accepted object-only constant-coordinate change #908.

## Variants and reproduction

Apply exactly one patch to a clean native checkout, then build its shaders:

```sh
git -C "$NATIVE" apply /path/to/ground_interstage_constants.patch
make -C "$NATIVE" shaders
```

`ground_interstage_constants.patch` keeps ground's unused zero UV1 and white
colour local to each stage. It removes six Float32 interface components.
`ground_interstage_coordinates.patch` additionally derives ground UV in the
fragment shader from `groundWorldM.xz`, removing two more components. The vertex
already writes `groundWorldM=(frame.x,frame.y,-frame.z)` and `uv=(frame.x,-frame.z)`.
It changes neither geometry, height sampling, LOD nor bindings. Normals remain:
`groundFineNormal()` consumes the interpolated vertex normal at LOD0.

Both variants compile all 46 affected ground artifacts. The other 476 artifacts
remain byte-identical. Original, patch and non-patch ground variants are retained;
no new permutation. This is interface reduction, not measured memory traffic.

Use the existing generic benchmark with identical scenario/declaration, offline
prepared data, 1280×720/60, exactly 60 frames and a complete 360-degree turn:

```sh
cd "$NATIVE"
MTL_DEBUG_LAYER=1 build/outshine benchmark --scenario "$SCENARIO" \
  --scenario-overrides '{"render":{"fps":60}}' --frames 60 \
  --angular-velocity 0,-360,0 --offline --preload-seconds 120 \
  --output "$OUTPUT/benchmark.json" --image "$OUTPUT/image.png"
```

Keep output in system temp. Validate separately; normal cost runs use
`MTL_DEBUG_LAYER=0`. Compare original/candidate/candidate/original. No extra
world frames, omitted first samples, reduced content or relaxed image pins.
Encoder inspection uses the shared `metal_encoder_profile.py analyse` against
each actual benchmark JSON, matching individual submission IDs and elapsed times.

## A18 Pro observations, 2026-10-20

Forty-four complete windows, 2,640 frames, Koerbersee/Tokyo/Central Park. All
have exactly 60 GPU samples, zero unrefined frames, and byte-identical final
images against the paired original. API-validated pairs cover Koerbersee/Tokyo;
candidate images there were opened. This is not a worldwide image admission.

With reconstructed UV, profiled Koerbersee terrain vertex intervals are
6.796/6.855 ms original and 6.151/5.835 ms candidate. These intervals overlap
other GPU work; they are not exclusive frame costs. Unprofiled whole-frame
means, averaged across the two runs per variant:

| Variant | Scene | Original ms | Candidate ms | Original p99 ms | Candidate p99 ms |
|---|---|---:|---:|---|---|
| Reconstructed UV | Koerbersee | 14.403 | 14.411 | 22.579 / 22.248 | 26.150 / 25.172 |
| Reconstructed UV | Tokyo | 13.646 | 13.665 | 18.197 / 19.082 | 25.011 / 27.284 |
| Constants only | Koerbersee | 14.713 | 14.951 | 27.102 / 26.488 | 22.358 / 22.707 |
| Constants only | Tokyo | 14.134 | 13.470 | 22.960 / 34.356 | 20.811 / 21.140 |

The reconstructed-UV matrix has larger initial-frame peaks. However, unchanged
originals in the later constants matrix also develop larger initial peaks.
This counterexample prevents attributing all first-use latency to UV reconstruction.
Constants alone do not consistently improve the terrain vertex interval either:
Koerbersee originals 6.355/6.344 ms, candidates 6.773/6.337 ms.

Decision: adopt neither variant. Smaller reflected interfaces and shorter vertex
intervals do not establish a complete-frame saving. Do not spend another packing
iteration on this branch; actual geometry work and first-frame readiness remain
the larger tasks. No Vulkan result, p99 admission or change to product shaders.
Canonical commands, raw samples, hashes and next integration decisions belong
to #16; private outputs remain under `outshine-ground-{io,constants}-16-*`.
