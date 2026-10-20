# Affine foundation extrema (#826)

`BuildingPreparation.cpp` fits one affine plane to the supplied corner heights.
Previously, both plinth top and sole independently sampled each part's outer
boundary every two metres. For an edge from p to q and t in [0,1],

`f((1-t)p+tq) = (1-t)f(p) + t f(q)`.

Both extrema therefore occur at endpoints. Evaluate each outer corner once,
then apply the existing high/low bounds, 0.50 m plinth and 0.30 m minimum sink.
Concavity changes neither statement. The previous method did not sample hole
boundaries; interior holes cannot enlarge the extrema of an affine function.
Non-ground parts retain their explicit height handling. The fitted plane,
source corners, recipes, geometry, asset formats and SFCGAL path stay unchanged.

For edge lengths L and n corners, the old number of plane evaluations is
`2 sum(1 + floor(L / 2 m))`; the new number is n. Work is bounded by input
corner count instead of physical perimeter. No sampled arrays are allocated
by either native implementation.

Reproduce with Python 3.12+; the fixture and its existing pins are only read:

```sh
python3.12 test/experiments/buildings/foundation_extrema.py \
  --output /tmp/outshine-foundation-extrema/report.json
```

The 48 original native footprint rings, three controlled affine planes and two
coordinate placements produce 288 cases. These planes are controls, not the
original fitted terrain. An independent 80-digit Decimal endpoint calculation
checks the corner extrema. Binary64 comparisons use a scale-dependent rounding
guard; exact real arithmetic does not prove identical floating-point samples.

On the initial A18-host run, both ranges are identical in all 288 cases. Maximum
corner evaluation error against Decimal is 2.605e-11 m. Evaluations fall from
90,780 to 10,572: `1 - 10572/90780 = 88.35%` fewer in this component. Python
timings are recorded separately and are not native or application speedups.
Native building/foundation/cache consumers and actual images remain separate
integration checks. No GPU/frame-budget claim follows from this CPU reduction.

The different #826 error remains open: a raw footprint's mean-height stamp can
disagree with the generated parts' actual foundations. This change does not
repair that ownership problem or claim to remove the Flensburg terraces.
