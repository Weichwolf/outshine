Type: defect
State: open
Architecture: ready
Parent:
Depends:
Priority: P1
Area: base, mathematics, geography
Tags: geodesic, longitude, termination, realtime

# Geodesic longitude difference has bounded numeric work

## Evidence

`src/base/geo/Geodesy.h::GeodesicOn` normalizes a longitude difference
with repeated additions/subtractions of 2*pi before its bounded solver.
Large finite differences cannot progress; opposite maximum doubles overflow
the subtraction to infinity. The solver's 200-iteration limit does not guard
these preceding loops. WI 2308 fixes `Wrap180`, not this independent path.

## Contract and ownership

- Owner: `src/base/geo/Geodesy.h`; tests under the mirrored Geodesy suite.
  Keep the existing geodesic result and convergence contract.
- Reduce each finite input longitude in degrees with the shared bounded
  `Wrap180` before subtracting. Normalize the resulting bounded difference
  before conversion to radians. Do not subtract unreduced extremes, multiply
  unreduced degrees, or duplicate a second normalization algorithm.
- Reject nonfinite geography and invalid ellipsoid parameters with a default
  nonconverged result before trigonometry. Latitude is within [-90,90]; the
  semi-major axis is finite and positive, flattening finite in [0,1).
- Preserve shortest-arc behavior and established endpoint/bearing conventions.
  Preserve the bounded iterative convergence policy; antipodal failure must
  remain an explicit nonconverged result, never an invented distance.
- Keep allocation-free numeric work. If ordering requires moving `Wrap180`,
  maintain one definition and all existing callers.

## Acceptance

- Ordinary WGS84 cases match independently pinned geodesic vectors; retain
  existing geographic conversion regressions.
- On a sphere, independently derive equatorial shortest-arc distance as
  radius times angle. Cover turns, both antimeridian directions, identical
  points, extreme powers of two and opposite maximum finite longitudes using
  integer modular residues rather than the production helper as the oracle.
- Nonfinite geography, invalid latitude and invalid ellipsoids terminate with
  nonconverged results. Nearly antipodal input terminates within the solver
  budget without weakening its accuracy or convergence contract.
- Restoring the original difference loops causes the existing harness to
  time out on an extreme finite case; retain the worker timeout guard.
- `make format`, `make suite SUITE=outshine/src/base/geo/Geodesy`,
  and `LINT_JOBS=2 make lint` pass.
