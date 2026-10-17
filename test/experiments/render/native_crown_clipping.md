# Bounded crown clipping

Owner: [#1](https://github.com/Weichwolf/outshine/issues/1).

The native grower sampled a possible lateral branch at eight equal distances.
When the first probe left the crown, it returned zero even if the branch origin
had enough valid space. This removed upper branch levels from narrow crowns.
It is independent of Metal: the integrated SDL Place has the same defect, and
the fine physical spruce also loses its upper branches before impostor capture.

Keep the bounded coarse search, then bisect its first inside/outside bracket
twelve times. Return the inside endpoint; an outside origin in the first
interval remains rejected. The distance bracket becomes at most
`requested length / (8 * 4096)`, before floating-point rounding. This refines the
existing local boundary search; it is not a proof about arbitrary nonconvex
crowns, growth topology, botanical accuracy or global optimality.

A private capture of actual birch/spruce queries on `1125221bd` records 1,780
rejections across growth and diameter-calibration passes. Python re-evaluation
finds 548 that have sufficient positive room: 455 ovoid and 93 conical queries.
They are not 548 unique final branches. A 48-step reference differs from the
12-step result by at most 0.00008273 growth-coordinate units on these inputs.
The controlled native regression grows one horizontal branch at each of ten
trunk levels. Free, conical and columnar controls retain the same attachments;
the bounded crowns still shorten their branches. The unchanged implementation
fails three checks; the correction passes all eleven. All six affected growth,
prototype and prepared-card cases pass with Metal validation; focused tidy and
format pass. Prepared tree provenance advances to version 3, so old generated
atlases cannot masquerade as corrected products.

Fine physical and flat-card images were opened for birch/spruce at 35 and 140 m,
with identical 640×720 cameras and lighting. Upper spruce branch tiers return.
The resulting old native spruce recipe is still too sparse and regular. The
flat 256-texel whole-tree cards remain inappropriate for large nearby trees.
This change does not supply a near LOD or replace the ongoing Python research.

Twelve complete public-client captures at 1280×720/30 then reproduce their PNGs
exactly in fresh subsequent processes. All images were opened. Warm loading
ranges from 482 to 1723 ms; all captures contain exactly 30/30 GPU samples.
Koerbersee remains over budget at GPU maximum 36.790 ms; Tokyo and Central Park
exceed the one-second turn limit. No GPU speedup or native-port acceptance is
claimed. The first new-product preparation hit the ten-second capture timeout;
an explicit preparation run with a longer timeout completed the remaining work.
The later warm measurements keep the original ten-second limit. First asset
generation and prepared loading must remain separate measurements.

The backend-independent correction is also integrated separately into master.
Twelve affected SDL-path tests and the three changed implementation units pass
focused tidy. Twelve fresh integrated SDL Places load in 736–2223 ms, with
CPU-p99 at most 4.432 ms. Their overview and the three gallery images were
opened. Direct GPU timestamps remain unavailable in that backend; these are not
GPU-budget passes. The complete native port remains a separate draft.
