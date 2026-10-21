# ScopeBBox floating-point diagnostic

[#178](https://github.com/Weichwolf/outshine/issues/178) reports valid rectangle
geometry together with Shapely/NumPy FP warnings on macOS/arm64. The same package
versions pass on Linux/x86-64. `bbox_fenv.py` distinguishes the Python binding
from the GEOS operation; it changes no modelling algorithm or warning policy.

```sh
.venv/bin/python -W error test/experiments/buildings/bbox_fenv.py
```

Outputs go to a fresh `outshine-bbox-fenv-*` directory in system temp; an explicit
`--output` must also stay in system temp. The script compiles its
own small C11 helper using the system C compiler; build logs stay in system temp.
No GEOS headers, implementation or vendor library are copied. It binds the
public reentrant C API from the actual loaded Shapely extension. The control
uses independent GEOS context/reader/writer/geometry ownership and lossless WKB,
avoiding a borrowed Shapely pointer or an unrelated system GEOS installation.

`--geos-library /absolute/path/to/libgeos_c.dylib` explicitly selects an existing
alternative C library for the **direct** call only; use a separate `--output`.
Its actual C API version and path are reported. Shapely still uses its original
bundled backend. No borrowed geometry or context crosses library versions: input
and output cross only as WKB bytes. A clean direct alternative is a candidate for
an isolated source build of Shapely, not proof that its unchanged wrapper passed.

Each square, rational rotated rectangle, line and point runs in a fresh process.
The strict Shapely operation runs with RuntimeWarnings promoted to errors.
Errors are reported explicitly. A C call records FP flags immediately after
`GEOSMinimumRotatedRectangle_r`, before Python arithmetic or geometry formatting.
Native `fenv.h` constants are queried rather than assuming x86 flag bits on ARM.
An independent flag loopback raises divide-by-zero/invalid inside a saved,
temporarily non-trapping control environment and restores the original FENV.
Only the control is isolated this way; actual Shapely/GEOS masks are unchanged.
The direct GEOS measurement clears prior flags to measure this operation alone;
it does not suppress the Python failure or alter the engine runtime.

Reports include platform/package versions, native masks, rounding mode, before/
after binding flags, direct GEOS flags, strict errors and exact normalized
geometry comparison. `inexact` is reported separately from invalid/division/
overflow/underflow; ordinary rounding is not itself a failed BBox contract.
`diagnostic-recorded` denotes observations, never a fixed platform or gate pass.

Run the existing isolated ScopeBBox tests and complete source/wheel suites
separately with `-W error`. A clean Linux diagnostic cannot admit macOS, and
correct-looking geometry cannot excuse a strict warning. The outcome should
identify either the faulty backend path or a verified supported combination.

The library path can be checked in the same fresh-process observer:

```sh
PYTHONPATH=lib/src .venv/bin/python -W error \
  test/experiments/buildings/bbox_fenv.py --rectangle-path library \
  --output /tmp/outshine-bbox-library-observation
```

`buildings._rectangle.minimum_rectangle` checks whether all four corners of the
input's axis-aligned bounds are actual input coordinates. All coordinates are in
these bounds, so their convex hull is exactly that rectangle. Every enclosing box
has area at least the hull's area, and that box attains the bound. Axis-degenerate
inputs use their point/line hull. There is no geometric tolerance, snapping or
warning filter; even a one-ULP non-axis edge retains GEOS. Other polygons keep the
original GEOS call on the original geometry, without an intermediate hull or
changed starting-corner order. Scope normalizes as before; footprint metrics
retain their original convention. Axis ties, geometry and payloads stay with
their existing consumers; both library consumers use the shared procedure.
Both consumers lend their already constructed/validated coordinate arrays;
the helper neither decodes those coordinates again nor retains them.

The offline original-footprint comparison is independently reproducible:

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/buildings \
  .venv/bin/python -W error \
  test/experiments/buildings/rectangle_bounds_comparison.py \
  --baseline ed48f0f7b --output /tmp/outshine-rectangle-original-comparison
```

It reads only existing pinned sources. Historical source acquisition fields in
the inventory are not timings for this run. Fallback rectangle bytes stay on the
original GEOS call; changed answers must independently equal the exact source
hull. Emitted bodies/payloads use the existing scope stationarity bound. The
original source vertices are retained; tiny caliper rounding differences in
returned box corners/dimensions are recorded, never hidden by a new tolerance.

The direct C comparison still calls raw GEOS, including its original platform
warning/flag counterexample. A clean library path is therefore distinguishable
from an upstream backend fix. Primary reports:
[Shapely #2215](https://github.com/shapely/shapely/issues/2215),
[GEOS #1235](https://github.com/libgeos/geos/issues/1235).
The affected macOS tests and source/wheel suites remain necessary; Linux alone
does not close #178.
