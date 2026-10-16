# Shared finite eye transport with a bounded solar source

Refs #353/#320/#266, goal #190. Continues `cloud_air_depth.md` by composing the
actual declared CloudPrefix cells with the bounded solar-source curve before
integration. The new opt-in `SOURCE_LINEAR_EYE` consumes the same Weather density,
source, light and fixed cloud cells; `cloud_air_prefix.glsl`, Weather moments,
shared bindings and the production renderer are not edited.

In a fixed union cell, homogeneous air and constant cloud extinction add before
integrating. Air Direct is multiplied by the actual Weather solar transfer at
the source location; diffuse/Psi retains its prescribed meaning. Cloud emission
comes from the existing cloudSource callback, once per positive cloud cell.
With solar visibility C linear across the source cell, its restriction to each
union cell is also linear. For union width w and query distance d:

```text
sigma = sigma_air + sigma_cloud
E_a/b = Direct*C(a/b) + Psi + E_cloud
I(d) = integral[0,d] exp(-sigma*s) ds
K(d) = integral[0,d] s*exp(-sigma*s) ds
L_cell(d) = E_a*(I-K/w) + E_b*K/w
L_eye(q) = accumulated_L + T_eye(a)*L_cell(q-a)
T_eye(q) = exp(-sigma_air*air_length(q)-cloud_depth(q))
```

Source and extinction are added before the shared exponential. Computing a whole
air transfer and dimming it by cloud T, or placing whole air/cloud transfers in
sequence, does not implement overlapping media. Independent analytic tests make
both incorrect sequences fail. Native unshaded-air control retains the actual
cloud path but forces C=1; it must fail against the same independent RTE.

The union uses physical atmosphere entry/end, the original cloud-cell endpoints
and fixed physical source nodes. Queries do not repartition cells. Cloud depth
is accumulated once per original cloud cell, with an analytic partial depth;
source subdivisions do not multiply rounded T repeatedly. Actual end ownership
is retained when a cell is prepared. Recomputing an orbit endpoint independently
in meters/kilometres produced a terminal zero-width cell in the initial probe;
the canonical physical endpoint and explicit ownership fix this without an
epsilon. A no-progress state sets the private failure bit and fails analysis.

The declared daylight homogeneous control uses prescribed constant solar/Psi
LUTs. Source profiles compare expm1(i/64), expm1(i/96), expm1(i/128) kilometre grids,
with at most1024 intervals. The original256-cell Bernstein source remains a
comparison. No continuous all-ray bound or optimal grid is claimed. No-eye-cloud
and empty physical paths use the existing air-only control; clear paths avoid
solar-source work. The actual photon channels remain RGB L and RGB T.

In this joint mode only, the last ProbeSample uint packs committed union-cell
count in bits0..11, original cloud density evaluations in12..17 and original
cloud source evaluations in18..23; bit31 signals invalid/progress overflow.
The other uint counts actual solar-source positions. Existing default diagnostic
packing is unchanged. Medium80, ProbeRay32, ProbeSample32 and original six
bindings remain. No new host ABI or shader-visible runtime resource is added.

Weather explicitly confirmed the compatible reference-clock extension in
#353/6204796254: optional seconds=37.0 is appended to cloud_cells/reference_ray,
and passed to existing density/source/solar evaluation and eye cells. Prior
positional callers and the37s default remain. An external solar callback stays
bound to the same time. No second recipe or global clock is introduced.

The existing runner gains `--joint`, retaining its prior invocation/defaults:

```sh
python3 test/experiments/weather/cloud_air_depth_native.py --joint --output /tmp/outshine-linear-medium --api-root /path/to/clean-archive-producer --archive /path/to/liboutshine.a --archive-receipt /path/to/receipt.json
python3 test/experiments/weather/plot_linear_medium.py --captures /tmp/outshine-linear-medium --output /tmp/outshine-linear-medium/comparison.pdf
```

Thirty rays include outside-atmosphere entry, ground/grazing and direct-Sun paths,
full physical request10000km,4097 logarithmic queries plus random/candidate nodes.
Coverage0/0.65/1 and wind0/37s share the same declared time in both media.
Every dense covered query receives independent Gauss16/32 RTE with actual fixed
cloud density/source cells and true Weather Sun-T6 along the air source. Clear
compares to closed forms. RGB stays1% with1e-5 floor; T uses the existing clear
2e-6 /declared-cloud1e-4 limits from `cloud_air_reference.py`. Repeated/sparse
native records must remain byte-identical, including work counters. Empty orbit
paths must be exactly L0/T1 without source work. The old air-only/default paths
remain executable compatibility controls. CPU reference/compiler workers run
nice10; measured native children run at normal priority.

A separate1920x1080/step8 raster measures one complete transfer per coarse ray.
The PDF can display those actual samples over a fixed0.02 background with
exposure2. It is a coarse diagnostic, not a production Place or full image.
Dispatch timestamps exclude initial upload, readback and all other frame work;
no depth-volume writes or angular reconstruction are included. Original timestamp
endpoints, load observations, source/cell work and logical input/output payloads
remain distinct from physical residency, memory peaks and complete frame p99.
Density/source counts describe original camera cloud callbacks; internal solar
quadrature density evaluations are not separately instrumented.

Wronski's SIGGRAPH2014 PDF25/41, optically revisited for this step, separates
source products from prefix scattering integration. Its local50–128m fog radius
and texture sizes are not a bound for Earth-scale air. #353 records this and the
Lokovic/Veach source-reuse basis. The linear Beer moments are derived/tested in
#384; this change applies them to the union, with independent Gauss integrals
and composition counterexamples. Papers do not establish a target device budget.

Still open: resident depth/angle sharing, nonhomogeneous atmospheric coefficients,
full diffuse response under clouds, camera/Sun/time/history, actual opaque/Thin
raster consumption and the productive native Renderer/Places, Metal/A18, full
memory and frame budgets. This bounded direct-source composition does not close
#353/#320/#266 or the broader light goal.
