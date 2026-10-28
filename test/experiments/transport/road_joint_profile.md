# Coupled road height and crossfall fallback

[#1539](https://github.com/Weichwolf/outshine/issues/1539), under
[#20](https://github.com/Weichwolf/outshine/issues/20), extends the bounded
[crossfall recipe](road_bank_profile.md). The axis and outer height/grade/q/q'
profiles stay fixed. Inner height and crossfall controls may change within
explicit metre/dimensionless allowances. No native/default/asset-format change.

The optional [shared scalar recipe](road_shared_profile.md) avoids microscopic
scalar seams; this original local-control entry point and its defaults remain.

## Preserve good products, then do only needed work

`plan_joint` first retains a qualified existing bank-only result. It next checks
whether the unchanged cubic-crossfall prior was merely rejected by conservative
Bernstein coefficient signs. Only the remaining local cases enter a coupled
HiGHS program. A single cubic's four controls are fixed by its two end profiles;
there is no fictitious interior freedom there. More knots/changed end bindings
would be a different family, not a successful result of this one.

At each road edge `u=±width`, set `D=1-u*k>0` and `N=h'-u*q'`. To bound the actual
grade, require `G*D+N>=0` and `G*D-N>=0` throughout each source span. These are
quadratic Bernstein polynomials. For controls `[a,b,c]`, the minimum is at an end
or, if `a-2b+c>0`, at `t=(a-b)/(a-2b+c)` clipped to [0,1]. Checking that minimum
avoids rejecting a valid polynomial merely because its middle control is negative.
If the numerical minimum is slightly negative, divide its magnitude by the
positive minimum of D before applying the existing 1e-9 grade residual. No raw
coefficient tolerance is allowed to amplify at a nearly folded inner edge.
This is an algebraic whole-interval check with a numerical floating-point model,
not a new IEEE interval-arithmetic or universal physical-optimality proof.

The coupled LP uses displacements from prior height/crossfall controls. Its rows
retain the conservative whole-span Bernstein grade conditions. Fixed outer
controls, scalar C0/C1 constraints and crossfall bounds remain linear. The
objective minimizes the maximum normalized height/crossfall control adjustment
within supplied allowances. Restore outer controls exactly and independently
recheck the result. Changed height controls require fresh actual-unit C0/C1
checks; an unchanged input height inherits its original continuity contract.
Alignment continuity, full physical/native-body C1 and ports still need their own
qualification. Every accepted new recipe receives the same whole-width grade
condition; no old grade limit, test tolerance or pin is relaxed.

An endpoint Lipschitz envelope gives a necessary height-change lower bound at
the original seams. Required corrections above H are rejected before a joint LP.
Actual 4937/10793 need at least 0.55866 m from those witnesses alone, so H=0.5 m cannot
qualify them. The 5 mm fixed-edge-displacement counter also remains Pending for
every H. Enlarging an unrelated rendering tolerance cannot fix either condition.

## Same actual inputs and bounded result

The unchanged 1462-record archive is
`df68c2ff306fe5115db4daf7b1efd366caa96338bf673634bd9c5bba9f9d2bf8`.
Use explicit test parameters G=0.35/Q=0.12/Dq=0.025/H=0.5 m. They are recipe choices,
not reconstructed Source classes; the binding boundary remains
[#1533/6451069262](https://github.com/Weichwolf/outshine/issues/1533#issuecomment-6451069262).
The code commit/source hashes plus complete parameters identify the procedure.

All 1131 old ready bank-only results remain, unchanged. Another 41 priors are
qualified without changing their cubic controls. Actual 7977 additionally changes
its inner height/crossfall: observed maximum edge grade 0.58116→0.35000, maximum
height-control change 0.44642 m, observed whole-surface vertical change about 0.341 m.
Its original axis and outer sections remain. The saved old/new unexaggerated
geometry and grade plot are the functional witness; 666 is an unchanged-prior
control. The old angular model and its cubic-crossfall prior remain distinct as
documented for #1533; no added planner adjustment is made to the 41 priors.

The initial 0.1 m pilot could not repair the true interior-grade case; seam bounds
already require more than 0.191 m for 7977. A larger exploratory result met the
selected 0.5 m cap, which is used consistently for the final methods comparison.
Conservative/fixed-family/numerical residual cases remain explicit. A valid
partial set is delivered, never a fabricated global feasible network.

Compare staged and always-coupled paths with identical G/Q/Dq/H, source intervals
and outer contacts. Their numerical ready sets/fidelity may differ; no shared
global optimum is claimed. The staged path can perform at most one existing
bank solve and one coupled solve, each with the supplied 1 s/20000-iteration cap.
Input is capped at 512 source spans. Setup/return is outside the HiGHS timer;
this is no hard frame deadline. Timing excludes source decode and diagnostics;
separate Memray traces cover warm planning. Requested bytes are not copied bytes.

`BankedSurface` accepts optional height controls and recomputes its analytic
surface envelope. Existing defaults are preserved. Only the two requested
diagnostic witnesses build 1 cm display meshes; all ready staged results receive
grade/contact sampling. Finite display faces, terrain/clearances, real bound
Source/ports, whole networks, vehicles, native/World/device budgets remain open.

## Reproduce

Use existing `.venv` dependencies, BLAS/OMP=1 and
`PYTHONPATH=lib/src:test/experiments/transport`; choose fresh system-temp output:

```sh
.venv/bin/python -B -W error -m unittest test_road_joint_profile test_road_bank_profile test_road_surface_frames test_road_surface_lod test_road_surface_uniform test_road_surface_quadrature -q
.venv/bin/python -B -W error test/experiments/transport/compare_road_joint.py --output /tmp/outshine-joint-profile --grade .35 --crossfall .12 --change .025 --height-change .5 --rounds 3 --trace
.venv/bin/python -B -W error test/experiments/transport/plot_road_bank.py /tmp/outshine-joint-profile/profiles.npz --output /tmp/outshine-joint-profile/profiles.png --grade .35 --witnesses 7977 666 --after-label 'Coupled or unchanged certified profile' --title 'Fixed axis and outer sections, bounded interior height'
```

Open the local figure; no image is published. SIGGRAPH 2008 graph/geometry
separation and Galin 2010 road cross sections remain the starting references in
`road_bank_profile.md`; Galin p8/Figs 17–18 was inspected again. The coupled
program, quadratic certificate and seam lower bound above are our derivations.
