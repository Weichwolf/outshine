# Prepared straight-flight station loft (#1003)

The frozen #1008/e3404bd2d envelope has a transverse gradient of0.165792 on
an actual retained facet. Its treads are horizontal, but the documented level
envelope cross-sections were not enforced: analytical XY proximity finds zero
vertices on all seven sections after binary-grid preparation. The recorded
negative facet in `fixtures/mvt_stair_nonlevel_envelope_face.json` preserves this
counterexample. The new audit rejects it using actual XYZ triangle normals.

`mvt_t_stair_loft.py` handles explicitly planned straight flights. It keeps every
original source coordinate, source domain and existing prepared vertex. Two
station cuts extend the flat landing zone by at most2^-10m per end. Choosing a
gap between existing stations avoids nearly coincident cuts. Canonical indexed
edges own their intersections; incident source faces reuse each new vertex.
Logical station values propagate during clipping, so no world-XY proximity
search reconstructs membership. Prepared source coverages must remain valid
and within the existing1e-7m² area contract before the plan is changed.

Every flight vertex receives an affine combination of its two shared landing
heights: constant before the first cut, linear between cuts, constant after the
second. This preserves a level transverse envelope on every actual flight face.
Curved axes and overlapping flights without a common height owner are refused;
they need a separate construction recipe. Work is bounded by16flights,
4096vertices and8192faces. Budget failure leaves the original plan unchanged.

The default sparse projection eliminates dependent flight-height variables.
An explicit-equality alternative expresses the identical height rule on the
identical refined geometry. Fresh simplex/interior-point workers, actual body
audits and equal finite minimax objectives compare their quality and cost.
Optima need not have identical landing heights or step counts. Prior error
remains a finite retained-vertex objective with the default2m cap; original
gradient, Float32, landing, support and riser bounds are unchanged. The added
whole-facet transverse gradient bound is1e-7, not a larger XY search tolerance.

Product and contact queries over known prepared owner geometry use strict
XY membership. Generic metric query defaults are unchanged. This avoids
re-admitting nearby nonincident faces around acute refined fans; complete
profile support and source coverage still have to pass their existing audits.
The product includes the flight frame, cut stations and shared landing indices.

```sh
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python -m unittest test_mvt_t_stair_loft test_mvt_t_profile_contacts test_mvt_t_profiles test_mvt_t_connections stair_junction_checks stair_metric_line_checks -v
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python test/experiments/transport/compare_mvt_t_profiles.py --output /tmp/outshine-stair-station-loft
```

Inspect the saved body/profile image. The report separates work, sparse bytes,
selected explicit copies, uninstrumented times and a traced-memory worker.
RSS includes imports/native libraries; live traced blocks are not allocation
counts. This is a local three-source Python proposal, not a whole-route,
native terrain/navigation, device or real outer-neighbour admission.
The separate seven-source neighbour input from Master bc03d54d2 is the next
region-construction input; none of its inner design heights becomes fixed here.
Sources and the inspected Galin/Chen figures remain in `mvt_t_profiles.md`.
