# Observed neighbouring sources in one profile region (#1003)

Master bc03d54d2 captures seven original source parts. The three-source stair
crop is not a closed repair region: both ends of431 and the first end of432
meet other original sources. This experiment jointly replans240,431,432,402,
535and537. Selection belongs to the calling test scenario; the region procedure
contains no fixed native IDs or coordinates.239 stays outside this selected
region, with its complete observed input retained and an explicit reason.

`mvt_t_profile_region.py` indexes original coordinates and observed native nodes.
Coincident coordinates alone do not establish an observed contact. Compatible
nodes, derived levels and ground access are required. Native node integers stay
uint64-valued Python integers, not floating-point/provider node identities.
Budget limits precede construction; source selection preserves all supplied
original axes, profile samples, tags and capture provenance. A pairwise scan
independently checks the small fixture's observed contact inventory.

Five observed internal contacts plus the bounded upper stair extension create
six common patches. The existing profile solver, station loft, actual treads,
body closure and independent whole-surface/Float32/source-domain audits consume
this common plan. Navigation and terrain reservations derive from the same
arrays. The two ends of431 and the first end of432 now participate in this
joint region, rather than receiving fixed heights from their former inner caps.

There are four remaining frontiers:535/first meets the observed outside239;
432/last,402/last and537/last have no matching neighbour in this capture and
remain unknown. They are never labelled free ends. Only these four get external
profile proposals; internal contacts remain shared interface products. Neither
the unresolved frontiers nor the selected region have native admission.

## A real outside point is already restrictive

`compare_mvt_t_region_boundaries.py` conditionally keeps actual recorded outside
axis points unchanged, using the explicit contact binder from #1008. These are
necessary point constraints, **not** complete transverse boundary profiles.
The actual239/last point is535/first at height19.043438709663015m. Holding it
fixed makes both simplex and interior-point solvers refuse the unchanged2m
finite-prior recipe. No old inner height or automatically enlarged guard is used.

`mvt_t_profile_route_bound.py` independently explains this refusal. Along an
explicit, completely supported path through two continuous owner surfaces,
the height change cannot exceed the sum of declared gradient times path length.
For a retained target vertex with priorP and fixed outside heightH, the necessary
finite-prior error is at least `abs(P-H) - sum(gradient * length)`.

One witness uses lengths16.4959316331315m and13.42385390181961m through the
shared432/535 node. Its target prior is27.576009960761617m and both declared
gradients are0.20000000298023224. The bound is2.54861405494047m, above2m even
without using the extra flat-patch restrictions.153target vertices are checked;
48straight target routes are supported. This is a numerical necessary bound for
the explicit fixed point and retained finite geometry, not an exact interval
certificate, global network impossibility or minimum over different recipes.
Expanding the joint region into239 or changing the construction/prior contract
is a separate decision; this delivery does neither automatically.

## Reproduce

The existing research environment supplies NumPy, SciPy/HiGHS, Shapely and
Matplotlib. Use the seven-source fixture and explicit selection in both commands:

```sh
export PYTHONPATH=lib/src:test/experiments/transport
.venv/bin/python -m unittest test_mvt_t_profile_region -v
.venv/bin/python test/experiments/transport/compare_mvt_t_profiles.py --input test/experiments/transport/fixtures/mvt_stair_boundary_neighbours.json --region-sources native-part:240 native-part:431 native-part:432 native-part:402 native-part:535 native-part:537 --output /tmp/outshine-stair-region
.venv/bin/python test/experiments/transport/compare_mvt_t_region_boundaries.py --input test/experiments/transport/fixtures/mvt_stair_boundary_neighbours.json --region-sources native-part:240 native-part:431 native-part:432 native-part:402 native-part:535 native-part:537 --output /tmp/outshine-stair-region/boundary-probe.json
```

The first comparison uses fresh simplex/IPM/projected/explicit-equality and
separate memory workers at the same quality. Open the saved body/profile image;
the overview marks unresolved boundaries and the unchanged outside source.
The boundary probe is a separate conditional experiment. Work, sparse storage,
selected copies, process RSS and traced live blocks retain their stated scopes;
they do not establish native/device performance or an integrated world gain.
Sources and inspected Galin/Chen figures remain in `mvt_t_profiles.md`.
