# ScopeBBox floating-point diagnostic

[#178](https://github.com/Weichwolf/outshine/issues/178) reports valid rectangle
geometry together with Shapely/NumPy FP warnings on macOS/arm64. The same package
versions pass on Linux/x86-64. `bbox_fenv.py` distinguishes the Python binding
from the GEOS operation; it changes no modelling algorithm or warning policy.

```sh
.venv/bin/python -W error test/experiments/buildings/bbox_fenv.py
```

Outputs go to `build/experiments/buildings/bbox-fenv/`. The script compiles its
own small C11 helper using the system C compiler; build logs stay in system temp.
No GEOS headers, implementation or vendor library are copied. It binds the
public reentrant C API from the actual loaded Shapely extension. The control
uses independent GEOS context/reader/writer/geometry ownership and lossless WKB,
avoiding a borrowed Shapely pointer or an unrelated system GEOS installation.

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
