# Finite depth for shadowed homogeneous air

Refs #353/#320, goal #190; follows `cloud_air_transfer_gpu.md`. This experiment
addresses depth, source-shape and integration errors before choosing a resident
volume or a production renderer resource. It retains the existing native
six-binding consumer and actual Weather Sun-T6 definition. Runtime passes,
Weather's eye-cloud prefix/moment work and shared GPU contracts are unchanged.

The previous fixed10km field does not specify V at other depths. For homogeneous
clear-air extinction sigma, direct source D and diffuse source P,

```text
I(q) = integral[0,q] exp(-sigma*s) ds
J(q) = integral[0,q] exp(-sigma*s) Csun(s) ds
L(q) = D*J(q) + P*I(q),  T(q) = exp(-sigma*q)
V(q) = J(q)/I(q),  limit V(0+) = Csun(0)
```

Length starts at actual atmosphere entry, not necessarily the eye. Zero eye
query remains exactly neutral. Setting V(0)=1 is an arbitrary quotient convention
and gives a false visible source when interpolated into nearby shadow. The
comparison retains that convention as a rejected control alongside corrected V,
linear J in distance/optical mass and Hermite J in optical mass. All recovered J
curves use the same raw native transfer; their interpolation error is distinct
from the source kernel's own error against independent RTE.

A new opt-in `SOURCE_LINEAR_DEPTH` kernel instead keeps the scalar solar source
linear between fixed physical nodes. Each source sample calls the existing
Weather optical-depth function; D is shadowed once and P is unchanged. If a cell
starts at a with width w and endpoint solar values A/B, its partial J is

```text
exp(-sigma*a) * [ A*(I(d)-K(d)/w) + B*K(d)/w ]
K(d) = integral[0,d] s*exp(-sigma*s) ds,  0 <= d <= w
```

The two weights are nonnegative and sum to I(d). Endpoint values in [0,1]
therefore give a bounded source and monotonically increasing exact J, without a
value clamp. K uses an eighth-order thin-depth expansion below sigma*d=0.25;
otherwise it uses `(I(d)-d*exp(-sigma*d))/sigma`. T and diffuse response are
analytic at each query rather than products of many rounded cell transfers.
FP32 roundoff is measured separately; exact-arithmetic bounds are not a claim
of bitwise monotonicity on every device.

Candidates use512/1024 normalized logarithmic cells, or a fixed physical grid
`s(i)=expm1(i/64)` / `expm1(i/128)` kilometres followed by the true physical end.
The fixed grid does not move its interior nodes when the physical end changes.
It uses at most1024 intervals in this Earth control; the longest possible air
chord above the6360km opaque sphere is below the final `expm1(8)` node.
All partitions precede queries, span the whole physical ray and stop at the
actual ground/atmosphere intersection, independently of sparse/opaque queries.

Private output keeps actual RGB radiance and RGB transmittance in ProbeSample32.
Its source counter counts actual solar-source positions. In linear-depth mode,
the final uint counts committed source cells, with no eye-cloud flags; the legacy
packed diagnostic format is unchanged when the opt-in is absent. Medium80 and
bindings0/0..3,1/0,2/0 remain. Clear/no-air controls avoid solar-source work.
No HDR/T channel is repurposed. This control assumes homogeneous air, prescribed
constant solar/Psi LUTs and the declared daylight direction; it is not a general
height-dependent/night source provider or joint cloud-eye integrator.

The30-ray cohort includes20/1772/2800/4000m eyes, outside-atmosphere entry, opposite
azimuths, grazing/ground clips and direct-Sun views. Physical request is10000km.
Each ray has4097 logarithmic queries,257 deterministic random queries and the
candidate depth nodes; all are rounded to actual FP32 input. Every covered dense
query receives independent Gauss16/32 RTE with25m reference segmentation. Equal
refinement values alone do not prove continuous convergence between these finite
queries. Clear compares to closed forms. RGB limit remains1% with1e-5 floor;
T remains2e-6 absolute, reference refinement1e-8 absolute.

Additional native sparse/repeated queries must reproduce every32byte record,
including work counters, exactly. Empty requests and outward orbit rays must
return L=0/T=1 without solar-source work. A hard analytic unit edge demonstrates
that the earlier signed angular cubic filter ranges below0 and above1; this
blocks a general physical-range claim and is not hidden by clamping. Angular
filter choice remains separate from this depth-source comparison.

A separate1920x1080/step8 coarse raster measures source work to the physical end.
It writes one final transfer per coarse ray. Its timestamps exclude volume stores,
reconstruction, all other rendering, initial uploads and readback; it is not a
full-frame or resident depth-field measurement. Exact dispatch endpoints and
logical source/cell counts are retained. Buffer/copy payload is derivable from
input32byte rays,4byte queries, output32byte records, two2x2 RGBA16F LUTs and the
80byte uniform; placement and physical residency are not equated. Global NVML
load samples are observations, not an isolated memory budget. CPU workers and
compilers run nice10; measured native processes run at normal priority.

```sh
python3 test/experiments/weather/cloud_air_depth_native.py --output /tmp/outshine-air-depth --api-root /path/to/clean-archive-producer --archive /path/to/liboutshine.a --archive-receipt /path/to/receipt.json
python3 test/experiments/weather/plot_cloud_air_depth.py --captures /tmp/outshine-air-depth --output /tmp/outshine-air-depth/comparison.pdf
```

Python3/NumPy, Matplotlib for plots, glslang and the project's native toolchain are
required. Archive/header provenance, all commands/terminal status and current
source/input/output/shader hashes are recorded. No new C++ code or host layout is
introduced. Shader translation/reflection does not replace Metal/A18 execution.

The source/integration separation follows the SIGGRAPH references in #353:
Wronski2014 PDF25/41 and Lokovic/Veach2000 PDF2/3, already inspected optically.
The cubic counterexample uses Mitchell/Netravali1988's Hermite basis from
`cloud_air_transfer_gpu.md`; its figures/equations were optically reviewed there.
The linear-source Beer moments above are derived explicitly and tested against
independent Gauss integrals. These references do not establish a world budget.

Next: joint source/extinction with actual eye-cloud cells, GPU-resident depth
sharing and its angular/precision/memory costs, moving camera/Sun/Weather,
nonhomogeneous atmosphere, renderer/Thin/history, Places and full target budgets.
