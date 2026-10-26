# Rejected road profile station-gap experiment

Owner: [#915](https://github.com/Weichwolf/outshine/issues/915), under
[#844](https://github.com/Weichwolf/outshine/issues/844).

The native fixed-pose inventory identifies the largest selected Tokyo package as
`streets`: 830,208 selected triangles from 2,928,930 stored road triangles and
3,042,210 vertices. It is not the terrain package. Generic cluster cooking creates
root groups, without coarser replacement surfaces. Finer current-depth culling
alone did not improve whole-frame cost (#894).

## Candidate and limited counting proof

Road sweep generation currently combines required elevation events with a second
uniform sampling grid. The candidate starts with all required events and endpoints,
then fills only gaps longer than the existing maximum step. Reference lines,
elevation/bank profiles and junction ports are unchanged. Geometry is generated
from the resulting stations directly, without generating and reducing a fine mesh.

In exact arithmetic, let distinct anchors be `a[0] ... a[n]` and maximum step `h`.
Each gap requires at least `ceil((a[i+1] - a[i]) / h)` segments. Filling each gap
independently attains their sum; station count is one plus that sum. This proves
minimal count for these one-dimensional constraints, not optimal mesh quality,
curve approximation, native float arithmetic or rendering cost.

Tests compare 1,000 dyadic cases against an independent rational lower bound,
retain every required event, and check ordered bounded gaps. Input/output budgets,
malformed captures and nonadvancing float steps have separate checks. Unconstrained
uniform positions remain unchanged. A maximum step alone is not a surface-error
certificate: curved, elevated and banked sections and actual images still need
native assessment before adopting the candidate.

## Actual requests and first native control

An instrumented offline cache miss records 62,681 actual sampling requests on native
C++ `e2ae8098599d`, with shader package `e34537d3b`. Its original generated PNG is
exact to the cached original. A private recipe key preserves existing cache entries.
The capture is 4,511,784 bytes; diagnostic outputs remain in system temp.

The Python comparison counts 381,569 original stations versus 319,130 candidate
stations: `(381569 - 319130) / 381569 = 16.36%` fewer. No record grows; largest
candidate has 198 stations. Maximum observed Float64 gap excess is 4.93e-14 m;
this is measured roundoff, not a universal numeric proof. Validation takes 2.27 s
on the measured host and includes both models and input checks, not only sampling.

The native candidate produces 2,513,462 road triangles and 2,755,970 vertices:
`(2928930 - 2513462) / 2928930 = 14.18%` fewer triangles and 9.41% fewer vertices.
The fixed-pose image changes 903 pixels by at most 53/255. Eleven existing/new
Ribbon/ProfiledRoadMesher tests pass without changing prior expectations.

Full 60-frame controls on twelve places complete, but counts do not establish a
useful frame gain. In sixteen normal-priority A/B/B/A windows across four places,
mean GPU changes range from -2.00% to +10.58%; the latter slowdown does not recur
in a separate four-window control. The p99 budget remains unmet. These noisy,
nonisolated windows establish neither a general gain nor a general regression.

Direct point-to-opposing-triangle checks on actual pre-port upper surfaces find
an original vertex 0.3509328475 m from the candidate surface. A larger 1.205478 m
same-parameter envelope is **not** a Hausdorff lower bound. Neither measurement
proves which mesh is closer to the analytical road, nor certifies final contacts.
Preserving station spacing and events alone does not preserve the swept surface.

**Do not port this candidate.** Product Ribbon source and recipe remain unchanged.
The complete attempted native diff, including its additional test, is retained in
[road_station_gap_trial.patch.xz](road_station_gap_trial.patch.xz) as a rejected experiment;
it is not an automatically applied build input.

## Corrected architectural target

The asset DB retains the complete, adequately resolved functional road geometry,
profiles and contacts. Vehicle motion/collision must not inherit a display LOD.
Sparse rendering requires independently loadable render products selected before
fine payload reads, decode and upload, rather than thinning the functional basis.
Near-to-far demand uses coarse all-direction visibility; unknown visibility keeps
an available coarse parent. AABB intersection is not opaque coverage.

The existing SQLite R*Tree, metadata-only selection, parent and level fields are
reusable; they do not make the current flattened road package selectively readable.
Fast SSDs and shared CPU/GPU memory on both targets require measured IO, decode,
copies, residency and bandwidth costs, not inherited HDD-era loading assumptions.
The current strategy and cross-agent decision live only in #915, #10 and #12.

[Native curve inputs](road_curve_requests.md) support the next geometry/LOD study
without reducing the functional baseline or rebuilding the whole engine.

## Reproduce the request comparison

```sh
python3 -B -m unittest discover -s test/experiments/transport \
  -p test_road_station_sampling.py
python3 -B test/experiments/transport/road_station_sampling.py \
  --input "$CAPTURE" --output "${TMPDIR:-/tmp}/outshine-road-stations.json"
```

A capture consists of little-endian records: `<QQddd` header with marker
`0x53414d504c453031`, required-event count, start/end/maximum-step metres, then that
many Float64 events. Record the accepted requests at `SamplingStations`, before
mesh generation; this diagnostic is not a product logging/API requirement.

SIGGRAPH 1996 continuous height-field LOD, local
[paper](../../../doc/references/transport/siggraph/1996-continuous-height-field-lod.pdf),
page 4 §4.1/Figure 3, was inspected visually: spatial approximation must assess its
geometric delta and projection. It supplies background for the remaining surface
assessment. The interval-count argument above is elementary and does not implement
that paper's terrain algorithm.
