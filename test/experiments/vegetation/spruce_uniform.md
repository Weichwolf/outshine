# Bounded complete-interval branchlet placement

Owner: [#626](https://github.com/Weichwolf/outshine/issues/626), under #182.
The #619 radial hierarchy moved too much physical foliage into long lower
branches. Its upper-primary-owned area fell by half despite unchanged total
area. This study changes that allocation law, retaining its actual branch/needle
kernels, trunk, main branches, physical needles and portrait scene.

## Explicit placement contract

`DistancePattern(mode="uniform")` uses a constant positive physical `spacing`
as a minimum group distance and `limit_groups` as a complete-population cap.
For parent length L and normalized interval [a,b], the existing closed-endpoint
count is N=min(limit,floor((b-a)L/spacing)+1), with its existing32epsilon relative
ratio tolerance. A zero parent/empty interval/zero limit remains empty. One group
stays at b. For N>1, group k sits at b-k(b-a)/(N-1), k=0..N-1. Group/member order
and whorl divergence use the existing immutable table kernel. The minimum applies
between groups, not members of one whorl, up to that endpoint tolerance.

This spreads the admitted complete population over its whole interval instead
of clipping a physical sequence near its tip. Variable spacing and longitudinal
spread are explicitly unsupported in the new mode. Aggregate work is refused
before site allocation; unresolved Float64 positions fail instead of producing
coincident groups. Existing phyllotaxy/interval recipes and outputs are unchanged.
The new mode string explicitly distinguishes the new semantics and graph identity.

This is an own bounded allocation rule, not a recovered SpeedTree equation.
Local Weber/Penn SIGGRAPH1995§4.3, PDFp.3/Figure1 and p.4 were read and optically
inspected: child population and relative lengths depend on parent and attachment,
with authored maxima. That motivates bounded parent-dependent populations, not
these particular spacing/count values. Existing #38/Ghidra interval work establishes
backward physical sequences and positive steps; it does not establish this mode.
No vendor code/assets. No new curve, growth, pose cache or generic administration.

## Complete spruce feasibility and physical inventory

The #619 radial support structure stays:240main branches,0.15m support spacing,
parent length ratios0.5/0.7, compensated radii and the existing strict radius fit.
Only terminal sites change to uniform, minimum0.015m, maximum19groups. These are
multiannual branchlets, not claimed individual annual shoots or calibrated buds.

A small16/20/24population bracket, then the bounded17/18/19interval, checks the
unchanged60,000axis/80,000window and nominal250.072346m²±5%authoring envelope.
16/17/18give213.457/224.890/236.278m²and miss that same area admission;20exceeds
windows and24axes.19fits all three. This is feasibility under the old authoring
input, not observed botanical LAI or an image-optimal parameter fit.

| Quantity | Original control | #619 radial | Uniform19 |
|---|---:|---:|---:|
| Axes |44,697|43,545|49,186|
| Supporting secondaries |22,228|2,515|2,515|
| Terminals |22,228|40,789|46,430|
| Windows |75,612|76,443|79,085|
| Physical needles |13,822,818|14,167,937|14,124,783|
| Expected half-surface [m²] |242.346740|248.397494|247.640902|
| Owned by main attachments above20m [m²] |43.137600|27.085932|43.577947|
| Owned by main attachments above25m [m²] |13.978259|6.770531|13.663838|

Preflight prepares242poses and derives remaining lengths/counts from existing
pattern tables. The conservative terminal capacity100,320is not the actual46,430;
existing bounded single-path requests finish49,187stored poses including root,
within the unchanged100,000pose guard. Actual execution equals every pre-count.
No full foliage, needle poses or needle meshes. All240primary-owned expected
areas independently reconstructed from saved path/needle-ID spans are exact.
This area groups by attachment, not a horizontal slice through hanging leaves.

Product`741a4403c2f242363921a3ddb5475c55944eceb6faed3e1845cca2b376cdaf3d`;
graph`1ca6f5a239c0bff53455ffb5a54c6ca1b656ac7547ff5ab586a341e920db2514`.
Producer9fdc7b6c6. Complete cached axes/windows use the unchanged eight finite
capacity banks; demand replay does not rebuild growth or the full near plant.

## Directional result and image decision

The existing leaf-weighted channel7projection uses shared physical bounds and a
common window-centre convex envelope for each control/candidate pair.512²with
2×2subsamples, three horizontal azimuths. Main wood is excluded; canonical short
wood still occludes leaf coverage. Each complete2560×1440plot was actually opened
at physical aspect. The uniform shape has a fuller, more even upper crown and
smaller gaps, but the horizontal regularity remains conspicuous.

| Azimuth | Mean leaf control→uniform | Weak/empty fraction control→uniform |
|---|---|---|
|15°|0.407620→0.422755|0.228294→0.205721|
|75°|0.405713→0.422248|0.222985→0.204872|
|135°|0.410925→0.426103|0.206461→0.183817|

These are statistical proxy projections, not observed effective LAI, all-view
physical-needle fidelity, temporal coherence or native/LOD admission. Different
pair envelopes cannot rank absolute means against earlier studies.

The complete same-scene2560×1440/32sample light portrait and unchanged control
were actually opened at full resolution. Scene and software backend are equal.
Upper mass is restored and some gaps smaller, but the lit tree remains thin and
conspicuously tiered. It is rejected as a species default. Portrait SHA256
`2f21104830bf4fce141922c812ec157137e947905cba6ad56f4e596238b366d4`;
unchanged control`ae0e5cbe88483f700d9d700e82f86c2437a264e5fe7d4f7ccc1a80da493524dd`.
The generic allocation contract is correct; this is not whole-form admission.
No more narrow population/whorl/profile tuning follows this comparison. Next
isolate the cached needle-window material/light response against the actual
physical needle source under the same camera and lighting. Unlit coverage alone
does not establish lit optical fidelity. No material bug is asserted in advance.

## Verification, work and reproduction

Seven library controls cover378exact-rational scalar table cases, nondyadic
minimum spacing, the actual closed-endpoint boundary, saturation vs truncation,
zero/one group, whorls, immutable ownership, early budget refusal, reload/reorder,
unsupported settings and unresolved positions. Exact rational positions use a
four-epsilon absolute rounding allowance; counts and angles are exact. Three
experiment controls check independent inherited lengths, scalar cyclic organ-area
spans and a real tiny variable graph with fresh individual demand replay.

61affected source and61installed experiment controls pass; six old placement
library controls and seven new ones pass in each environment.180old scalar
recurrence cases pass in both (maximum error4.44e-16). Fresh wheel205Python
modules byte-exact;54actual reference imports in cached twig replay are inside
that installation, without source fallback. Three fresh source/installed demanded
twigs are exact; growth/whole foliage/needle mesh forbidden. Unchanged complete
control cache still passes its provenance reader. Ruff/format, functions≤120lines
(actual max87), diff and lint-docs pass. Native gates are outside this Python-only
change; no shared habitat/reservation/material API changed.

Linux x86_64,4logical CPUs,16GiB, Python3.13.5, NumPy2.5.4, bpy5.2.2. Build62.953s,
process peak275.945MiB; retained axes8,829,504B/windows11,783,665B. Coverage92.919s,
peak993.574MiB at nice10, overlapping the light process; unrelated native fuzzing
also ran during the render. Earlier studies had different host load/priority, so
no build/render speedup follows. Complete light328.853s/process332.513s,
peak2816.047MiB, llvmpipe(LLVM19.1.7,256bits);2,835,470camera/shadow triangles
and53,039,472geometry/instance payload bytes. No allocation/internal-copy total, native GPU,
frame or device budget proof is inferred. Original per-axis/site/window/fragment
limits remain. Catalog18,874,368decoded bytes is unchanged shared data.

From an ordinary`.venv`, set`TMPDIR=/tmp PYTHONDONTWRITEBYTECODE=1`and the usual
`PYTHONPATH=lib/src:test/experiments/vegetation`. Rebuild the existing self-contained
study source from the fixed input; run`spruce_uniform.py --source ... --output ...`
then`uniform_execute.py --graph .../uniform-19.json --source ... --output ...`.
Existing capacity-rebind, portrait, coverage and cached-demand-replay CLIs consume
that product. Installed checks omit`lib/src`and use the freshly installed wheel.
Original products, complete opened plot/portrait, actual source/installed replays
and terminal logs remain under
`/tmp/outshine-vegetation-uniform-proof-773bf6a0d/`. Shared unchanged source,
control and catalog retain their previous paths recorded in the product manifests;
no duplicate full output copy. No gallery, server or doc images.
