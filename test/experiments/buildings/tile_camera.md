# Complete-tile camera fit

The source bounds must fit before expensive geometry is generated. A fixed
world AABB centre wastes image space when perspective projects its two sides
asymmetrically. `tile_camera.camera` shifts both eye and target in the image
plane while retaining the original view direction, vertical FOV and profile.

Let each bound point have coordinates `(x_i, y_i, z_i)` in the fixed camera
basis around an initial origin. Moving the eye backwards by `D` gives depth
`z_i + D`. Let `u` and `v` shift the origin in the image plane, and let
`kx = 0.46 * width / focal_px`, `ky = 0.43 * height / focal_px`.
Full containment requires:

```text
abs(x_i - u) <= kx * (z_i + D)
abs(y_i - v) <= ky * (z_i + D)
z_i + D >= 0.1 m
D >= 0.1 m
```

For either image axis `p`, define `a = max_i(p_i - k*z_i)` and
`b = min_i(p_i + k*z_i)`. The allowed shift interval is
`[a - k*D, b + k*D]`. It exists exactly when `D >= (a-b)/(2*k)`;
its midpoint `(a+b)/2` is independent of `D`. The maximum of both axis bounds
and the near-depth bounds is the smallest permitted `D` for this fixed basis,
point set and margins. The camera retains the existing additional 10 m safety
distance. Orientation, lens and scene geometry are not optimized.

`verify_tile_camera.py` checks this bound against an independently formulated
SciPy/HiGHS linear programme, with all shift/depth variables and original point
inequalities. Asymmetric source boxes, translations, planar/linear/point inputs
and invalid bounds are covered. The actual renderer must still pass its ray
projection and full emitted-mesh framing checks; mathematical point containment
does not replace a new render or admit a changed LOD product.

All delivered building bounds and source heights remain. No building is removed
to improve framing. The tighter projection changes detail demand, so public
images require fresh whole-tile generation, source accounting and optical review.
The public page remains exactly four current complete images with manual refresh.
Contract and current evidence: [#180](https://github.com/Weichwolf/outshine/issues/180),
under [#33](https://github.com/Weichwolf/outshine/issues/33).
