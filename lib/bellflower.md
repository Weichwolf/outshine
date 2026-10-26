# Regular pendulous corolla surface (#316)

`from outshine_ref.vegetation import Bellflower`

An own thin lobed bell surface with a closed upper attachment disk and open
lower mouth. It creates no species, material, terrain or stand. The real
flowering consumer and limits are in [the experiment](../test/experiments/vegetation/understory.md).

```python
flower = Bellflower(length=0.0075, radius=0.0035, lobes=6)
positions, normals, dt, dtheta = flower.surface(t, theta)
mesh = flower.mesh(segments=6, sides=24)
```

Local Z up, top-cap centre is the organ anchor(0,0,0), mouth points towards−Z.
Dimensions are metres. `t` is longitudinal0..1 and theta is periodic in radians;
the numerical interface bounds angles to±16π. Dimension bounds1µm..10m and
3..12 integer lobes keep arithmetic bounded. Defaults are authored study values,
not a measured species. Mesh subdivisions:3..128 longitudinal intervals,
3*lobes..256 angular samples. Invalid dimensions/budgets raise `ValueError`.

With q=(1+cos(lobes*theta))/2, the own side is

`r=R*(0.16+0.84*sin(0.85*pi*t)+0.15*t^12*q)`

`z=L*(-t+0.06*t^12*(1-q))`, `p=(r*cos(theta),r*sin(theta),z)`.

Analytical t/theta derivatives determine the normal, not averaged face vectors.
The surface is regular: r≥0.16R and z_t≤−0.28L, hence the radial component of
p_t×p_theta is−r*z_t≥0.0448RL>0. Distinct polar angles cannot intersect; at each
angle z decreases strictly. The side is therefore injective modulo the angular
seam. Its interior has z<0, so it also cannot cross the top disk at z=0. This is
a bound for this parametric surface, not for arbitrary flower profiles or a
whole plant's inter-organ collision.

The mesh has welded angular positions and an explicit cap with hard cap normals.
Returned owned arrays: Float64 `vertices[N,3]`, Int32 `faces[F,3]`, Float64
`normals[3F,3]` and `corner_uv[3F,2]`. Side UVs are(t,theta/2π), unwrapped at each
seam triangle; cap UVs use the radial disk. The explicit corner domain avoids
interpolating the angular seam through the entire texture. It is an open thin
corolla; the consumer supplies a two-sided material. There is no hidden solid
sphere/envelope filling its mouth.

Every returned triangle is checked against all three analytical corner normals.
If subdivision fails to resolve the oriented surface, meshing refuses. For
example L=0.00035m/R=0.0035m at3×18 is rejected, rather than silently returning
opposed facets. The regularity bound does not by itself accept a coarse mesh,
coverage preservation, normal filtering or a render LOD.

Three independent controls cover finite-difference jets, winding/unit normals,
UV seam/domain, the analytic positive Jacobian property, invalid work/input
bounds and the unresolved-facet counterexample. Source and fresh installed-wheel
checks accompany the actual near/stand consumer. The local SIGGRAPH1988
architecture reference motivates the plant's organ hierarchy; the radial
formula above is our own authored shape, with no vendor mesh or implementation.
