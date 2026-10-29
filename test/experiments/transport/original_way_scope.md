# Original node reach and finite ground scope — #1698

The actual v4 pair contains 41 missing Ways and 82 exact ordered original points.
State 2 identifies a published native group; State 1 positively identifies a valid
native node without such a group. State 0 is unresolved. State 1 is not group zero
and does not assert equality between native and raw point positions.

Engine's producer inspection is recorded in
[#1530/6491052777](https://github.com/Weichwolf/outshine/issues/1530#issuecomment-6491052777).
Both current StreetGraph producers configure a two-metre spherical snap with
radius 6378137 m. `Network::NodeNear` compares the original point directly to the
unchanged first node position. `SnapOnePoint` either retains that position or
copies the raw point; folding never successively moves the node. Splicing changes
edges, crossing detection records crossings and elevation changes heights.
This is a property of that inspected implementation, not a v4 schema promise.

## Geometric bound

For WGS84 geodetic latitude phi, let e² = f(2−f), a = 6378137 m and
f = 1/298.257223563. The meridional and prime-vertical metric radii are

```text
M(phi) = a(1−e²) / (1−e² sin²(phi))^(3/2)
N(phi) = a / sqrt(1−e² sin²(phi))
M(phi), N(phi) <= a / sqrt(1−e²)
```

The ellipsoid surface metric is `M² dphi² + N² cos²(phi) dlambda²`.
Map a shortest arc of a radius-R sphere to the ellipsoid with the same geodetic
angles. Its length is at most `a/(R sqrt(1−e²))` times the spherical arc length.
That path bounds the endpoint chord. An orthonormal ENU projection cannot increase
it. Thus a two-metre snap on the a-sphere has real-arithmetic zero-height ECEF/ENU
reach at most **2.006728179641953 m**. Two ellipsoidal metres would be unsafe near
the poles. This sharp real-arithmetic result has essentially no slack for the
floating-point spherical-target/ellipsoid round trip at a pole: strict numerical
checks exposed that limitation. `surface_reach_bound` deliberately uses the looser
valid radius upper bound `a/(1−e²)^(3/2)`, giving **2.020252517671284 m** here.
The extra geometric reserve is inexpensive and does not change the candidate
decision below. Its caller must bind actual spherical parameters. Invalid or
unrepresentable inputs fail rather than becoming zero. Neither the extra reserve
nor outward final rounding is a full interval-arithmetic certificate for native
floating-point computation.

Tests construct independent spherical arcs across the antimeridian, near both
poles, on the equator, in all bearings and at different distances/radii. The
actual 74 available raw-to-published-group WGS84 chords reach 1.992225752 m.
Eight State-1 native positions remain unavailable in the versioned fixture;
the replay records NaNs, not zero observations. Engine separately checked all
82 native points; its maximum spherical displacement is 1.988575681 m at State 1.
These distinct metrics are not interchangeable.

## Full-way and field limits

The point bound is not a bound on every inserted point of a woven Way, its entire
profile, C1 boundary effects, terrain deformation or functional influence.
`TieLooseEnds` can splice a segment through a loose node without moving that node.
No source, Unknown, local admission or World guard changes here.

The bounded actual census found seven whole small Ground trees, 34 original
members in total, rejected by feature-wide MissingTopology despite no original
missing-Way point sharing their published groups. Raw-polyline/profile-box
separation alone is insufficient. In particular a measured 1.96 m raw-box gap
does not survive the conservative node-position allowance. A consumer needs its
complete normalization/profile/field support contract before admission changes.
The existing finite-tree height policy does not certify all missing-Way effects.
Native adoption remains with #1419/#1530; source inputs and old recipes survive.

## Reproduce

Use the existing NumPy/Shapely environment. The plot additionally needs Matplotlib.
Only explicit system-temp outputs are accepted; no image is published.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 .venv/bin/python -B -m unittest test_original_node_reach
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B \
  test/experiments/transport/replay_original_node_reach.py \
  --input test/experiments/transport/fixtures/original-point-source-facts \
  --snap-m 2 --sphere-radius-m 6378137 \
  --output /tmp/outshine-original-node-reach
```
