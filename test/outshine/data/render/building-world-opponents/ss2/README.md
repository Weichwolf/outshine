# Common quarter-pixel World references

Native Apple A18 Pro inputs for #1838 / #1797. Six fresh processes, eighteen
query renders; whole unchanged 1558-source/38476-triangle original group and
19 original operand hashes, matching the parent point fixture. Capture remains
128²; both perspective poses and the repeat use 2560×1440 with the same FoV,
eye, clips, world PBR viewer and light as the point fixture. Every raw texel is
one native centre sample, not a prefiltered output pixel.

The existing comparison with `--sample-axis 2` orders source and outer depth
at common quarter-pixel positions before resolving 2×2 samples to 1280×720.
See [reproduction](../../../../../experiments/render/native_world_field.md).
`manifest.json` binds actual requests, reports, binary/source hashes and each
full raw-buffer hash. `samples.npz` is lossless; load without pickle. All
omitted pixels reconstruct to exact zero. Both native original unions and
identical repeats are exact; runtime World revision/WorldFromField remain null.

The existing field model still fails the declared research limits of 0.25
linear-HDR RGB component and 0.125 visible-source coverage per output pixel.
The near pose reaches 4.73645 RGB / 1.0 coverage error; the revealing pose
5.02441 / 0.75. All four affected-group original fallbacks have zero colour,
coverage and sample-depth error. Filtering alone does not qualify this model.
Opaque union alpha does not establish source coverage. These inputs do not
qualify the live renderer, GPU arithmetic bounds, performance or 8-GB usage.
