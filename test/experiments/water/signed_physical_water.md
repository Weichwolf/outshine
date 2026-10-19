# Signed local water heights with the physical prism envelope (#877 / #760)

The previous reference required positive ENU-Up bounds. A triangle with vertices
at ±1 km and declared ellipsoidal model height zero instead has ENU-Up targets
approximately -0.157…-0.078 m. Bounds [-1,1] were rejected before fitting. This
precluded sea-level/negative-datum controls even though their physical coordinates
and horizontal metric were regular. Height zero is a declared model coordinate,
not an observed water level or ASL/geoid conversion.

`water.frame.normal_caps` accepts signed ordered bounds above the ellipsoid's
normal singularity, including a certificate for the computed inverse centre.
The positive path retains its previous arithmetic and ordering. Source selection,
datum, constraints, whole-face grade tolerance 1e-9, finite solver checks 1e-7,
hard contacts and bounded absence retain their existing contracts.

## Signed support and regular normal field

Let E be the solid WGS84 ellipsoid, A the h=0 anchor, u_A its outward normal and
z(p)=u_A·(p-A). The tangent plane supports the convex E. Signed closest distance
h(p) to its boundary satisfies **h(p)>=z(p)**:

- Outside E, z<=0 is trivial; if z>0 every boundary point lies at least z away.
- Inside E, the ray p+t u_A exits E before reaching its support plane, at distance
  at most -z. Closest boundary distance is no larger; its negative is therefore
  at least z.

WGS84 has principal curvature radii N,M at least r=a(1-e²). Its multiple-nearest-
normal locus is the deep equatorial focal disk, with closest h<=-r. For h>-r
the normal field is regular, with derivative eigenvalues 1/(N+h), 1/(M+h), 0.
Thus a convex prism with z>=low>-r has the same Lipschitz bound
`||D Up|| <= 1/(r+low)` for negative, zero and positive low.

The argument uses signed **closest** height, not an arbitrary inverse normal
branch. GeographicLib's primary
[reverse contract](https://raw.githubusercontent.com/geographiclib/geographiclib/main/include/GeographicLib/Geocentric.hpp)
specifies the closest solution; its
[equatorial special case](https://raw.githubusercontent.com/geographiclib/geographiclib/main/src/Geocentric.cpp)
identifies the deep ambiguity. This support/curvature derivation supplies no
borrowed accuracy guarantee for PROJ or this Float64 implementation.

## Reconstructed inverse centre

An inverse-projected centre provides geographic coordinates, model height and
normal u'. Forward conversion gives c', which can differ from the requested prism
centre c. For nonpositive prisms, measure

```
epsilon = |c'-c| + 64*ulp(WGS84_A)
denominator = r + low - epsilon > 0
delta = (R + epsilon)/denominator + 1e-12
```

R encloses all six prism vertices about c, hence the whole prism. All segments
c'→p remain above low-epsilon; their lengths are at most R+epsilon. Integration
from the reconstructed centre bounds every prism normal relative to u'. Returned
inverse height must also exceed -r. Nonpositive denominators/nonfinite bounds and
delta>=0.01 are rejected. Positive prisms retain the original R/(r+low)+1e-12 path.

The metric/ECEF/trigonometric allowances remain declared Float64 numerical
allowances, not interval-certified arithmetic or universal PROJ error bounds.
An injected inaccurate inverse on a micrometre-sized negative prism makes the
naive radius-only cap fail; reconstructed-centre inflation contains its normals.

## Independent checks and scope

`signed_water_reference.py` solves the nearest point by a separate scalar Lagrange
multiplier: in units of a, semiaxis squares s=(1,1,1-e²), ECEF point x/a,
`sum(s_i*(x_i/a)^2/(s_i+t)^2)=1`. The closest point is
`q_i=x_i*s_i/(s_i+t)` and its normal is proportional to q_i/s_i. The diagnostic
uses [SciPy brentq](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.brentq.html)
on [-0.02,0.02]; it refuses points outside this explicitly near-surface bracket.
It never calls the PROJ inverse. Cross products of fitted physical triangles and
these independent normals check actual directional grades.

Four anchors include midlatitude, dateline/high latitude and southern hemisphere.
Whole prisms include [0,1], [-1,1], [-600,-400] and a -50 km numerical control.
The 12 driver cases use declared vertex model heights 0, -430 and -50000 m;
these are synthetic controls at geographic anchors, not observed sea/lake levels.
SOCP, derived Clarabel QP and OSQP keep the same hard vertex/interior contacts.
The local plot shows faceted height minus the vertex model coordinate: curved
constant-geodetic-height surfaces are not exactly represented by one triangle.
Its centimetre-scale chord error is reported, not admitted as a world threshold.
No native Place image is changed by this reference.

The old positive Ill input, complete topology/provenance, resulting vertices,
actual sparse P/A/q/b and synthetic pins are replayed against a frozen before
capture. The former tests that rejected [-1,600]/[0,600] are explicitly updated
because signed bounds are now valid; singular/nonfinite/reversed domains remain
negative tests. No other quality or budget requirement changes.

## Reproduce and costs

```sh
PYTHONPATH=lib/src:test/experiments/water .venv/bin/python -W error \
  -m unittest test_signed_water test_physical_river_fit
PYTHONPATH=lib/src:test/experiments/water .venv/bin/python -W error \
  test/experiments/water/compare_signed_water.py \
  --output "${TMPDIR:-/tmp}/outshine-research-water-signed"
```

Existing NumPy/SciPy, Clarabel/OSQP, pyproj and Matplotlib supply the diagnostic.
Only nonpositive caps add a forward conversion/roundtrip enclosure per face.
Reports count independent nearest queries/root residual evaluations, fits, retained
array payload, CPU/wall and traced/process peaks. No total allocation count, GPU
cost, native solver ranking or device admission is supplied.

SIGGRAPH-first context remains local *Simplification Envelopes* (1996), p.1/Fig.1
textually/optically reviewed: geometric error bounds must accompany approximation.
No simplification method is implemented. Native cooker/source/datum/region/terrain/
bridge contracts and current Place/device gates remain #760/#48 work.
