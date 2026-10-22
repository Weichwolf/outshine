# Native shared profile fit

`src/generators/road/profile/HeightProgram.h` accepts a bounded canonical CSC
program for one minimax adjustment variable. Fixed whole-profile contacts,
directional facet limits, shared landings and projected loft variables are
prepared upstream. The solver uses the installed HiGHS C API, serial dual
simplex and no presolve, matching the qualified contact-bound Python recipe.
Cancellation, time/iteration bounds and explicit infeasibility remain visible.
Every accepted solution is checked against the original matrix and variable
bounds, including coefficients the library may discard. A library warning is
reported. This verifies primal feasibility at the existing 1e-9 program tolerance;
it is not an exact-arithmetic optimality certificate.

`NativeProfileFitProbe.cpp` exposes only this native boundary for the existing
Python experiment. Compile both `HeightProgram*.cpp` units and the probe using
the complete `HeightProgramFit.cpp` compilation-database configuration, replacing
only source/object/dependency-output paths. Link the resulting objects against
`pkg-config --libs highs`. `make db` prepares the current compilation database;
`make suite SUITE=outshine/src/generators/road/RoadProfileHeightFit` runs the
isolated native boundary tests without building the renderer.

```sh
PYTHONPATH=lib/src:test/experiments/transport \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" \
  test/experiments/transport/native_profile_fit.py \
  --binary "$NATIVE_PROFILE_PROBE" --repetitions 3 \
  --output "${TMPDIR:-/tmp}/outshine-native-profile-fit"
```

The driver prepares the unchanged four native cut profiles, calls the native
fit instead of SciPy, then builds and audits the actual indexed body with the
existing Python consumers. It separately checks the unchanged whole-norm and
two-metre-prior negative controls. Contact residuals from the rendered-contact
helper are explicitly labelled before Float32 packing; body/facet Float32 checks
remain separate. The output contains the current body and compact receipts only.

The measured 111-variable/3062-row/5981-nonzero case motivated the dependency:
the tested unmodified-row OSQP configurations reached their two-second limit
without satisfying the original conditions. Native HiGHS reproduces the
reference heights and passes the same body checks. Other OSQP formulations are
not excluded. Exact commands, device costs and measured outcomes belong to
[#1003](https://github.com/Weichwolf/outshine/issues/1003).

This step does not port source topology, planar arrangement, body meshing or
navigation/terrain production to C++. Native outside meshes and real navigation
stations still need one shared prepared plan under
[#826](https://github.com/Weichwolf/outshine/issues/826). No scene repair, warm-load
speedup or frame-budget admission follows from these local fit timings.
