# Parent-dependent shoot growth

[#182](https://github.com/Weichwolf/outshine/issues/182) owns the mature spruce
shape defect. The existing complete stand has short upper primary branches but
keeps their descendants at the same absolute dimensions and sibling counts as
lower branches. Needle area alone did not repair its columnar silhouette.

`ParentGrowthGraph` is a small experimental adapter over `GeneratorGraph`.
It reuses placement, frames, stable paths, request limits, prepared profiles and
the installed physical `DistancePattern`. It does not replace the plant library
or change existing graph, v2, v3 or v4 recipes. Its explicit private descriptor is
`parent-generator-graph-1`; the original graph reader rejects that descriptor.

For an opted-in axis generator, the actual child length is
`ratio × actual_parent_length × local_scale(attachment)`.
The existing local profile and deterministic scale variation remain effective.
Descendants use that resulting physical length. Unmapped needle generators keep
their original physical size; inherited wood scale does not resize needles.
Distance patterns evaluate the actual parent length, so short parents have fewer
attachment sites. Conservative descendant-length and spatial bounds propagate
before requested instances are expanded.

The adapter also checks the attachment-radius fit before creating poses.
For fixed sites it evaluates the actual parameters. For physical patterns it
checks the union of both piecewise-linear profile knots and interval endpoints,
including maximum positive scale variation and conservative elliptical radii.
This is a radius-fit condition, not a watertight branch-junction construction.
Inherited experimental instance-array ownership is unchanged.

## Independent functional controls

Three actual parent lengths, 0.5/1/1.5 m, produce 0.25/0.5/0.75 m child axes at
ratio 0.5. A physical 0.2 m attachment spacing yields 3/5/7 children, while every
needle remains 0.02 m long. Different nominal parent prototypes use actual metres.
The measured spruce counterexample propagates through two orders: parents
0.23085/2.565 m yield secondary axes 0.07695/0.855 m and following axes
0.03591/0.399 m. Tests also cover interior radius/profile extrema, attachment fit,
budget refusal before pose work, an absent actual slot, conservative bounds,
owned ratios, exact JSON replay and warm requests without new poses.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/vegetation \
  .venv/bin/python -W error test/experiments/vegetation/parent_growth_checks.py
```

`parent_growth_experiment.py --output <build-directory>` prepares three private
2560×1440 scenes using the existing product exporter. The absolute-length control
has five children per parent; the next uses parent-relative lengths with the same
counts; the third also uses physical spacing. They contain 105/135/151 needles
and deliberately have unequal total leaf area. These are functional visual
controls, not a fair equal-quality cost comparison. Signed root presentation
offsets separate the three parents without changing plan identities or paths.

No control has yet been rendered or optically approved. A single convincing
mature spruce, requested physical area, bounded costs and matching near/far
products are still required before another complete eight-form bake.
The public showcase remains four current whole-tile pictures.

## Physical area before another bake

`parent_growth_area_probe.py` compares the three length/count policies against
the actual mature variant-1 recipes without creating axes or organs. The first
order remains unchanged. Later relative lengths use the actual parent length;
the physical-distance policy uses the nominal parent's original branch spacing
and the installed `DistancePattern`, with a continuous tapered length profile.
These proposed layouts do not yet define production IDs. The original alternative
must reproduce the installed needle counts and physical-area result exactly.
An independent three-parent case also checks the proposed relative lengths and
7/5/3 physical attachment counts. All ten small checks pass.

| Policy | Spruce area (m²) | Pine area (m²) |
|---|---:|---:|
| Original absolute lengths/counts | 200.00 | 202.46 |
| Relative lengths, fixed counts | 85.30 | 117.44 |
| Relative lengths, physical spacing | 34.88 | 59.89 |

Physical needle sizes, spacings and prototype areas remain unchanged. Thus the
parent-length correction alone loses substantial needle area; the alternatives
are not equal-quality candidates. Physical spacing also reduces complete axis
counts from 50,401 to 14,251 for spruce and from 19,705 to 8,959 for pine.
The next mature recipe must explicitly recover the requested area through a
plausible bounded branching architecture before an equal-quality cost decision.
Alpha or needle-size changes cannot conceal that missing area. Detailed length
quantiles and input hashes remain under
`build/experiments/vegetation/parent-growth-area/`.

## Primary source and scope

Weber and Penn, SIGGRAPH 1995, *Creation and Rendering of Realistic Trees*,
§4.3, PDF pages 3–4, motivates relative child lengths, attachment-dependent
lengths and parent-dependent counts. The local paper in
`doc/references/vegetation/siggraph/` was also inspected optically.
This adapter binds the existing prepared local profile to physical parent length;
it is neither the complete paper model nor a claim of SpeedTree equivalence.
Botanical spruce/pine references and whole-stand approval remain with #131/#168.
