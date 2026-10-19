# Temporal motion export cost (#16)

The current shaders export current/previous homogeneous clips for motion vectors.
Both exports and the previous transform cost work before rasterization. Fewer
interface components do not by themselves prove fewer physical memory transfers.
The local CGF2020 TAA survey, section3.2/PDF page4, describes depth reconstruction
for camera-only motion; the page and equation were inspected visually.

Two private controls use the exact8f70952f7 native binary, complete fresh offline
worlds,1280x720/60, exactly60frames/360degrees and60GPU timestamps. A/B/B/A per
control/place, no source bytes/provider starts, no extra world frames. Each arm
repeats its own PNG exactly. Table entries average two complete GPU windows.

| Control | Place | Original mean ms | Control mean ms |
|---|---|---:|---:|
| Remove current clip only | Koerbersee | 13.1355 | 13.9604 |
| Remove current clip only | Tokyo | 14.1501 | 13.3838 |
| Force camera-only depth reconstruction | Koerbersee | 14.0069 | 13.6694 |
| Force camera-only depth reconstruction | Tokyo | 13.9328 | 13.6026 |

The first control derives current NDC from raster coordinates at the explicitly
fixed diagnostic resolution; it is not a general viewport implementation.
The second omits both clips and forces the existing static marker, extending the
existing sky reprojection to geometry depth. It deliberately does not establish
object eligibility, translated camera-origin correction or a dynamic-object
fallback. Its timings are not a quality-equivalent product comparison or an
upper bound on all possible implementations. Neither control is adopted.

Full pairs were opened. Current-only changes40,455/32,970pixels in Koerbersee/Tokyo
(max39/27LSB); camera-only changes349,541/180,374(max74/37LSB). No temporal
correctness, disocclusion or new error bound follows from these final pictures.
The narrow next candidate is the existing adaptive ground path: it already uses
the same current local point/instance matrix for both projections. A general
reprojection must include previous-minus-current render-origin shift, preserve
sky behavior and explicit object/deformation motion, and reserve its marker
without admitting genuine invalid vectors as static geometry.

All543recorded sources/products/pictures are restored exactly, including522shader
artifacts, binary/archive and12current PNGs. Private patches, controls, complete
commands, raw timestamps, numerical summaries and reviewed pairs remain under
/tmp/outshine-varying-cost-8f70952f7/. No GPU contract or reference pin changed.
