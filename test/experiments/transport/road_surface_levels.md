# Lazy road surface LOD products

Owner: #1955 / #915. Analytic Upper-surface reference; no native port approval.
Extends the qualified position model in `road_surface_lod.py` without changing its
existing defaults, approximation formula, source recipes or stored geometry words.

`products(curve, errors_m, max_stations=..., max_queries=...)` yields strictly
coarse-to-fine products. Input curves remain immutable during iteration.

- Requesting only the first product performs no finer subdivision or allocation.
- Each next level refines the accepted frontier. Dyadic partitions remain nested;
  unchanged intervals reuse their measured bound and endpoint data.
- The generated buffers and position bounds match independent `prepare` calls
  at every requested error. No fine mesh is an input or simplification source.
- Query admission precedes pose evaluation. The caller bounds cumulative queries
  and stations; finer failure leaves already yielded coarse products valid.
- Geometry buffers are independent per level and can become independently readable
  asset payloads. This Python iterator does not implement the asset DB or residency.
- Storing every level duplicates buffer data. Generate/cache demanded levels within
  the shared DB budget; the comparison's retained validation buffers are not a runtime
  residency policy or a measured generator peak.

```sh
PYTHONPATH=lib/src python3 -B -m unittest discover \
  -s test/experiments/transport -p 'test_road_surface*.py'
PYTHONPATH=lib/src python3 -B test/experiments/transport/compare_road_surface_levels.py \
  --output /tmp/outshine-road-levels --plot-ordinal 59592
```

Use a Python environment with the existing reference dependencies; plotting additionally
uses matplotlib. Output stays outside the checkout. The driver verifies the fixed input
archive and checks that producer files do not change during the comparison. Every retained
original recipe is compared, rather than only successful candidates.

Errors 16/4/1/0.25 m are comparison inputs, not runtime distance bands or a new allowed
screen error. Camera projection and product bounds must choose actual runtime demand.
Upper boundaries do not contain final native port stitching. Normal interpolation,
material/UV behavior, junctions, bridge lower/wall/cap geometry and whole-world placement
remain separate requirements. Existing references for those contracts remain authoritative.
No native speed, full road coverage or 720p60 claim follows from this experiment.
