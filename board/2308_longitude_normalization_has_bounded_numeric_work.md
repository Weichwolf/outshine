Type: defect
State: active
Architecture: ready
Parent:
Depends:
Priority: P1
Area: base, mathematics, geography
Tags: longitude, numeric-boundary, realtime, termination

# Longitude normalization has bounded numeric work

## Evidence

`src/base/geo/Geodesy.h::Wrap180` subtracts/adds 360 in while loops.
For large finite doubles (e.g. 1e300), subtracting 360 leaves the same
representable value, so the loop cannot make progress. Infinite input also
never terminates. This was found while implementing terrain-source coverage;
that planner uses bounded `std::remainder` and does not depend on the defect.
`GroundStream::Resident` wraps the lookup longitude but `SampleFrom` interpolates
the original clamped longitude. At 181 degrees and zoom 6, the correct wrapped
column fraction is 64/360; the original coordinate is clamped to a tile edge.
`At` also subtracts a wrapped integer column from an unwrapped fraction at
180 degrees, so its interpolation coordinate is a whole-world width.

## Contract and implementation

- Owners: `src/base/geo/Geodesy.h` and `world/ground/TerrainLoader.cpp`.
  Tests mirror those paths; all existing callers remain supported.
- Normalize every finite double to [-180,180] with constant-count operations.
  Preserve the existing signs at +/-180, including +/-540, and signed zero.
  Nonfinite input returns NaN; callers must validate their geographic inputs.
- Use finite classification plus `std::fmod(degrees, kDegPerTurn)` and at most
  one correction by a turn. Do not use magnitude-proportional iteration,
  silently clamp a longitude, or change a caller to hide the problem.
- Keep the helper allocation-free and nonthrowing. Audit callers only for
  endpoint/nonfinite behavior. `At` and `Resident` reject nonfinite geography
  before casts; lookup and interpolation use the same wrapped coordinate.
  Derive fractions relative to the unwrapped integer cell before wrapping its
  address, never by subtracting the wrapped column. Preserve sample provenance.

## Acceptance

- Ordinary positions and turns retain their results. Test both signs, signed
  zero, +/-180, +/-540, infinities, NaN and maximum finite magnitudes.
- For large exactly representable powers of two, compute the expected residue
  with independent integer modular exponentiation for 2^0 through 2^1023.
  The largest finite double has integer residue 128 modulo 360; verify both
  signs against that independent integer numerator, not the production helper.
- The old loop must fail the bounded test by timeout on an extreme finite
  input. Use the existing harness process timeout; never leave a runaway worker.
- Warm an independently specified column-gradient field; `At` and `Resident`
  at +/-180, 181 and -179 must match analytic fractions and valid normals.
  Restoring either old interpolation path must fail this control; comparing
  `At` only to `Resident` is insufficient because both could be equally wrong.
- `make format`, `make suite SUITE=outshine/src/base/geo/Geodesy` and
  `LINT_JOBS=2 make lint` pass.
