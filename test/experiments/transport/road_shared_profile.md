# Shared scalar cubics over original road axes

[#1542](https://github.com/Weichwolf/outshine/issues/1542), parent
[#20](https://github.com/Weichwolf/outshine/issues/20). This optional Python recipe
extends [bounded joint fitting](road_joint_profile.md); original entry points,
source intervals, plan XY and valid old products retain their behavior.

## Actual problem and representation

The preceding 1462-record run left 83 coupled candidates numerically unqualified:
54 predominantly height-rate seam residuals, 26 crossfall-rate seam residuals and
3 grade residuals. Some original intervals are only 4.7e-12 m long. Subtracting two
nearly equal local controls and dividing by that interval amplifies rounding.
The old residual rejection is correct; weakening it would hide a real output defect.

Instead, fit one scalar cubic over the whole original station range for height,
and one for crossfall q. Their four controls follow directly from the two outer
values and rates: `[f0, f0+L*f0'/3, f1-L*f1'/3, f1]`. There is no free optimization
variable or new solver call. Directly evaluate that common function and its
analytic derivatives; no scalar splice remains at an internal axis seam.
This is an explicit alternative recipe, not the original piecewise polynomial.
It may move inner heights within H and crossfall within Dq.

`ProfileRecipe` distinguishes `source-cubics` (Nx4) from `shared-cubic` (4).
Its factory selects the existing banked adapter or `SharedCubicSurface`.
Shared controls must never be serialized/interpreted as local Nx4 controls.
`Curve.height_at` is the shared value/derivative query, also used by the analytic
frame consumer. Existing defaults perform the same arithmetic as before.
The shared surface keeps the original axis/curvature and four global height
controls; its second-derivative envelope comes from direct common derivatives.
Neither frames nor the display mesher reconstruct rates from tiny restricted
height differences. The planner consumes an original source `Curve`, not an
already fitted surface.

## Admission and fallback

Keep the existing bank-only result first, then the unchanged quadratic-prior
certificate and necessary/fixed-family guards. Check the shared candidate before
the remaining joint program. Failure falls through to the unchanged bounded
program; no partially qualified candidate becomes Ready.

For each original interval, restrict the common *derivative polynomial* directly
to quadratic Bernstein controls. Do not differentiate restricted cubic controls.
The same signed `G*(1-u*k) ± (h'-u*q')` whole-interval certificate applies at both
road edges, including physical residual normalization near a folded inner edge.
Bounds on restricted height differences, crossfall differences and crossfall
controls provide the same H/Dq/Q control-hull conditions as the local family.
Outer values and actual scalar rates from the previous cubic-prior representation
are independently checked. The angular-bank source and cubic prior remain distinct
as documented for #1533; capture ordinals are not native Source identities.

The algebraic certificate uses the documented floating-point model, not universal
IEEE interval arithmetic. Scalar C1 follows from the common function; source-axis
position/heading/curvature continuity and complete native-body C1 are separate.
Measured one-sided normal jumps are diagnostics, not a universal source-axis proof.
The H allowance bounds center height; whole-surface displacement also includes q.

## Scope of the actual comparison

Use the unchanged 1462-record/13944-span archive with SHA256
`df68c2ff306fe5115db4daf7b1efd366caa96338bf673634bd9c5bba9f9d2bf8`.
G=.35, Q=.12, Dq=.025 and H=.5 m are explicit versioned experiment choices,
not reconstructed Source classes. Both paths share those values, source axes,
outer profiles, prior controls and residual thresholds. Existing regular HiGHS
termination, source-span and solver-work guards remain. No global optimum claimed.

The common-cubic stage adds 77 qualified recipes; all 1173 previously qualified
products keep their basis and coefficient bytes in this corpus. Real 937 and 6576
are local geometry/grade witnesses. Actual 24961 remains unqualified: its unique
common function exceeds G. The impossible 5-mm fixed-edge counter remains Pending.
The six remaining numerical cases are not promoted by looser tolerances.

Timing covers planning only; diagnostics and source decode are outside it.
Separate warm Memray traces measure allocation requests/requested bytes/high water;
these are not copied bytes or runtime UMA residency. RSS includes prepared input
and retained controls/diagnostics. Native Source/ports, terrain/clearance, networks,
vehicle behavior, World/native integration and device/frame budgets remain open.

## Reproduce

Use existing `.venv` dependencies, BLAS/OMP=1 and
`PYTHONPATH=lib/src:test/experiments/transport`. Choose a fresh system-temp output:

```sh
.venv/bin/python -B -W error -m unittest test_road_shared_profile test_road_joint_profile test_road_bank_profile test_road_surface_frames test_road_surface_lod test_road_surface_uniform test_road_surface_quadrature -q
.venv/bin/python -B -W error test/experiments/transport/compare_road_shared.py --output /tmp/outshine-shared-profile --grade .35 --crossfall .12 --change .025 --height-change .5 --rounds 3 --trace
.venv/bin/python -B -W error test/experiments/transport/plot_road_bank.py /tmp/outshine-shared-profile/profiles.npz --output /tmp/outshine-shared-profile/profiles.png --grade .35 --witnesses 937 6576 --after-label 'Checked shared scalar cubics' --title 'Original axis and outer profiles, directly evaluated scalar cubics'
```

Open the local unexaggerated geometry/grade figure; no image publication.
SIGGRAPH2008 graph/geometry separation and Galin2010 road cross sections remain
the inspected primary starting references in `road_bank_profile.md`. The shared
scalar representation, derivative restriction and admission above are our derivations.
Exact-head results, costs, original-buffer/frame parity and limits belong in the PR.
