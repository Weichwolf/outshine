# Isolated normal-view filtering with independent coverage controls

Owner: vegetation, [#643](https://github.com/Weichwolf/outshine/issues/643).
Continues [prototype fields and the rejected atlas binding](prototype_normal_response.md).
The finite positive fields remain unchanged. This experiment fixes their Blender
sampling reference, without claiming a finished material, botanical crown or LOD.

## Source and contract

The local SIGGRAPH2003
[Billboard Clouds](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
PDF page5/sections3.2–4 and Figures9–10, was also opened optically. It distinguishes
precomposited alpha/normal data from plane selection and exposes texture/plane
costs. Its packing example does not prove our atlas minification correct.
The SIGGRAPH2007 normal-filtering source and our positive moment/bound derivation
remain owned by the previous report and
[library contract](../../../lib/normal_distribution.md).

Each view retains its own projection frame, coverage and weighted normal vectors.
Filter those fields inside the same view before evaluating the positive dot sum.
A filtered value must never silently acquire another view's frame. Extend/clamp
edge samples to that same image as well; missing or neighbouring tiles must not
change a known opaque primitive's coverage.

`normal_component_material.material` now accepts an explicit image factory and
1–64source views. Its default atlas remains only a rejected comparison. For the
corrected reference, `ViewImages` receives one demanded view, creates a32²Float32
non-colour image and uses Linear sampling with EXTEND. GPU half-precision source
storage is disabled; this is not a claim that every Blender shader operation is
Float32. Undemanded views are not scanned, copied or used to derive mean fields.

`local_uv` recovers the original chart's[0,1]coordinates without changing its
positive orientation. Whole windows are stably grouped by existing view indices;
all original positions, weights, source IDs and first-needle ranges survive once.
The Blender grouping represents texture-array layers for this reference; copying
hundreds of Blender materials into the native renderer is not proposed.
No shared/native binding, image format, recipe, source geometry or library changes.

## Actual alternatives and controls

Known opaque quad, unit light aligned with its source normal. The selected chart
must return conditional response1 and projected coverage1 at a1pixel footprint.
Neighbouring charts contain rotated encodings of that same source normal. These
are independent response and coverage assertions, not a fitted colour target.
Native64²EEVEE frames use32samples, llvmpipe software and one configured worker.
The magnified64pixel case additionally checks source/coordinate correctness.

| Binding /1pixel | Conditional response | Coverage sum | Decision |
|---|---:|---:|---|
| Prior linear atlas/view0|.475187|1.000218|Rejected: frame mixing|
| Tested UDIM/view0|1.000000|.880810|Rejected: coverage loss|
| Tested UDIM/view9|1.000000|.875479|Rejected: coverage loss|
| Tested UDIM/view10|1.000000|.755474|Rejected: coverage loss|
| Tested UDIM/view63|1.000000|.848628|Rejected: coverage loss|
| Independent clamped view0/9/10/63|1.000000|1.000218|Passes these controls|

The UDIM adapter round-trips all64signed Float32 tiles exactly and checks their
actual32²/128bit metadata. Those IO successes did not ensure correct edge
sampling. Its first report's `passes` was response-only; the separate coverage
audit explicitly rejects it. This is a result about this tested binding, not a
general claim that UDIM is unusable. Historical results are retained unaltered.

The corrected sampler passes all16actual frames across four view indices,
64/1pixel footprints and constant/checker fields. Checker response should be.5:
.499738 magnified and.499999672 at1pixel, with the same coverage as the constant
field. Thus avoiding cross-view mixing did not simply disable footprint filtering.
The separate8actual mean/four scalar,90°rotation and positive/negative light
mixture controls pass; maximum scalar discrepancy remains below.001, including
half-float output/shader precision. These do not establish arbitrary temporal,
anisotropic, moving/warped or continuous-source guarantees.

Frozen sampler/control implementation:cc7631862, with the reporting-only NumPy
bool conversion fixed in3459e9473. The earlier view run stopped after writing
its first frame because its coverage flag was not JSON serializable; no terminal
acceptance was claimed from that run. Successful complete controls use3459e9473.
The actual2560×1440 response/coverage comparison was opened; its6×6pixel crops
come directly from saved EXRs, with original paths/hashes and unclipped sums.

## Work, memory and whole-plant reference

One demanded view/five fields uses81,920B of controlled Float32RGBA pixels, versus
5,242,880B for the64-view UDIM set in the same single-view probe. Raw source is the
existing complete25,165,824B four-vector bank. NPZ arrays still decode all64views
of a chosen family before selecting a view; this private container is not a
spatially indexed production cache. No canonical needle expansion is repeated.

A single ViewImages staging buffer is16,384B per field; foreach_set copies that
payload once into Blender. Source field preparation is limited to the demanded
32²slice. Internal Blender/driver/GPU copies, total allocations and mip residency
are not measured by these controlled payload numbers.

The whole reference requests510of512available family/view groups. It preserves
49,186axes,79,085windows/158,170camera-window triangles and14,124,783represented
needles, plus the same four instanced wood prototypes/2,360,960wood triangles.
Mean uses1,020independent images/16,711,680B; four uses2,550/41,779,200B. This
avoids atlas gutters and unrequested view images, but increases material/groups
from8to510. That is an explicit Blender-reference cost, not a native draw budget.

Both actual2560×1440/32sample whole frames on c3297fb0a exit0 and were opened.
Source geometry/camera/light/identity stay fixed; mean/four raw-frame alpha is
exactly equal. Linear RGB RMSE .006284, max difference .152771, mean signed
change+.000445. PNG comparison via test/scripts/pixels.py changes163,751pixels
(4.4420%), channel MAE .185802/255. The narrow, regular/tiered crown remains;
this is a sampling correction, not a proved whole-tree physical error reduction
or a new botanical quality admission. No full explicit14million-needle reference
was constructed for those images.

Mean/four populate7.415/16.529s, render+export496.807/496.262s, process peak
2755.64/2961.60MiB RSS. Local concurrent llvmpipe runs also overlap the small
controls; CPU service/load differs from the earlier atlas run. These are offline
adapter measurements, not GPU timings, a speedup claim or an approved native
implementation. Controlled image payload excludes GPU/mip/allocator overhead.

The final metadata names the old atlas-only byte counter precisely; it is0for
view storage, while controlled_view_image_float_bytes records the positive
actual buffer total. After the rendered snapshot only empty-demand handling,
default storage and diagnostic metadata labels changed; the rendered nonempty
path/sampler bytes stay unchanged. Empty demand now returns before any material
or geometry work. Atlas/UDIM remain explicit negative alternatives, while the
portrait default selects view storage.

75source and75installed controls pass, with64actually imported reference-module
paths and source bytes checked inside the unchanged#667wheel. This includes
all original prototype/area/contact/leaf/near-crown controls plus signed tile IO,
all64UV mappings, demanded-only image reads and actual complete quad grouping.
No library or native code changed. No SpeedTree, species, forest, PBR,
transmission, shadow or hardware-device admission follows.

## Reproduction and remaining scope

All private data/logs/staging/.venv stay under/tmp, currently
`/tmp/outshine-vegetation-isolated-mips-proof-c246a03c4/`. No server or gallery.
Use one numerical/llvmpipe worker and explicit fresh/tmp outputs:

```sh
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/normal_component_mip_probe.py \
  --storage view --views 0 9 10 63 --pattern checker \
  --output /tmp/outshine-vegetation-clamped-checker-new
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/normal_component_portrait.py \
  --storage view --mode four --whole /tmp/existing-complete-plant \
  --catalog /tmp/existing-capacity-catalog --fields /tmp/existing-positive-vector-bank \
  --output /tmp/outshine-vegetation-clamped-whole-new
```

The preserved numerical moments/MIPs are portable to per-layer texture sampling;
this delivers no native array layout or cross-backend agreement. Joint light
visibility, R/T, roughness/specular, colour/normal correlation, wind, moving LOD,
whole-tree physical error and stand/world costs remain open under#643/#1.
