Type: task
Parent: 1137
Area: render
Tags: oracle, instrument

**What our side sampled: the uv at a pixel, and the tap that uv produces**

The oracle's uv is on disk (`oracle.uv.raw`). Ours exists only inside a fragment shader, so *the two sides
sampled different texels* is unspeakable and every disagreement about a textured pixel reads as a
disagreement about colour.

**OUR UV IS DERIVED ON THE HOST AND NOT PUBLISHED FROM THE DEVICE**, and that is the one place this
feature does not reach for an attachment. `RasteriseDeclaredNormals` in `test/render/Parity.cpp` already
walks the parts, projects each triangle through the case's own `clip`, keeps the nearest by this
projection's own convention and interpolates a **per-vertex attribute** barycentrically. A uv is that
routine with a different attribute and a different width. Writing a second rasteriser instead would be
two answers to one question (`ES.3`), and the existing one is validated: its depth convention was wrong
once, was found by the p50 it produced, and is now stated at the site.

**Then the tap, and this is why the uv alone does not decide anything.** With both uvs in hand, publish:

| | |
|---|---|
| `duv` | in **texels** of the image the slot actually binds, never in uv — 0.001 uv is 2 texels on a 2048 map and a quarter of one on a 256 |
| the texel each uv lands in | integer coordinates, both sides |
| the tap each uv produces | from the image the studio already decoded, so no second decoder and no second colour transfer |

**THE HOST DOES NOT REIMPLEMENT THE SAMPLER, AND THAT IS A REFUSAL RATHER THAN AN OMISSION.** Minification
filtering is the thing under investigation in `board:1130`, `board:1133` and `board:1135`; a host-side
filter would be a second implementation of the disputed code, and every disagreement it produced would be
confounded with the question. So the tap is published at **level 0 with the image's declared wrap and
interpolation**, and where the two sides differ by more than that can explain, the instrument says *the
uvs agree and the taps do not*, which is a finding pointing at the filter rather than a filter comparison
pretending to be one.

**The domain, because it is narrower than the headline.** A pixel whose winning triangle the host resects
differently from the device gets a uv from a different surface, and the instrument cannot tell that from a
uv error. It is detected rather than assumed: where `board:1138` says the two sides disagree about the
material or the object, the uv comparison is **refused at that pixel** and counted, not reported.

**Done when** a named pixel yields both uvs, their difference in texels, and both taps, on a case whose
uv derivative is ordinary and on one where it spikes — `materials/a-beautiful-game` (626, 347) has a
central difference of 0.043 to 0.127 per pixel and is the second.
