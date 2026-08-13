Type: bug
Area: clients
Tags: instrument

**Frames and units**

- **`TangentFrame::Geo` is not the inverse of `TangentFrame::Project`, and the bound written beside it is
  wrong by 1.9×.** `core/TangentFrame.h:37` states "over the 900 m a scatter reaches its error stays
  under a metre". `Project` is the exact ellipsoid projection (line 27); `Geo` is planar on
  `kMPerDeg = 111320`. Measured round trip `Project(Geo(e, n))` at longitude 9°: **1.771 m** at 900 m
  east and 0.735 m at 900 m north at 50 °N; 1.879 m / 0.410 m at 52.1 °N. It is a *scale* error, not
  noise, so it grows linearly — 5.9 m at 3 km east — and it is systematically eastward, so a whole stand
  is displaced the same way. The east term is wrong because the exact longitude scale is
  `N cos φ · π/180` = 71 700 m/deg at 50 °N against `kMPerDeg · cos φ` = 71 555, a ratio of 1.00203; the
  north term is wrong because the meridian scale is 111 229 m/deg there against 111 320. *Both consumers named when this was filed — `clients/StandField.cpp:32` and
  `clients/SceneRunner.cpp:287,318` — went with the clients on 2026-08-12. The header and its wrong
  bound are unchanged, and the next consumer inherits both* — so a declared metre of channel is 1.002086 m of
  world eastward and 0.999546 m northward at 52.106 °N. **Confirmed to 5·10⁻⁵ m** now that the eye is
  in the row, and the comparison must be made frame-by-frame or it proves nothing: the last row of
  a deleted run log (wasm `9b110bb85af592ce`, Chromium 151.0.7922.34)
  is at frame 899 of 900, where the channel commands 2250 · 899/900 = **2247.500 m** and `eyeEastM`
  reads **2252.189145** against 2252.18914 predicted from `N cos φ · π/180 / (kMPerDeg · cos φ)`.
  Northward the sign flips: `demo/walk-500`'s last row commands 504 · 10749/10800 = 501.620 m and
  `eyeTravelM` reads 501.392034 against 501.39200 predicted — so the run reaches **503.771 m of the
  declared 504**, and the remaining 2.4 m of the apparent shortfall is only the 1 Hz row landing 51
  frames before the end. `demo/ring` would be 18.8 m long over its 9 km. **`eyeTravelM` itself is not
  contaminated** — it is the exact measurement of a wrongly commanded motion, and it is what makes the
  error decidable. Right: either make
  `Geo` the exact inverse (one Newton step on the ellipsoid, or invert through ECEF), or scale it with
  the frame's own `M` and `N cos φ` computed once in the constructor — and in either case correct the
  stated bound. Decides it: `Project(Geo(e, n)) == (e, n)` to a declared tolerance, a pure unit test with
  no reference.
