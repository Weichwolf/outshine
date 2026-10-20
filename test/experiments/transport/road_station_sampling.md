# Road profile event sampling

Owner: [#915](https://github.com/Weichwolf/outshine/issues/915), under
[#844](https://github.com/Weichwolf/outshine/issues/844).

The native fixed-pose inventory identifies the largest selected Tokyo package as
`streets`: 830,208 selected triangles from 2,928,930 stored road triangles and
3,042,210 vertices. It is not the terrain package. Generic cluster cooking creates
root groups, without coarser replacement surfaces. Finer current-depth culling
alone did not improve whole-frame cost (#894).

## Candidate and limited proof

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

The first native candidate produces 2,513,462 road triangles and 2,755,970 vertices:
14.18% and 9.41% fewer respectively. This fixed-pose image changes 903 pixels by at
most 53/255. Both original and candidate were opened; image/curve and full-frame
admission remain open. All eleven Ribbon/ProfiledRoadMesher tests pass without
changing existing expectations. Counts alone do not justify adoption or a cache
recipe version change.

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
