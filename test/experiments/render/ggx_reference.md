# Direct GGX numerical reference (#829)

`ggx_reference.py` supplies an independent Decimal oracle for the shared isotropic
GGX functions. It retains the original NDF algebra and evaluates visibility through
the unoptimised correlated Smith lambda expression. This follows the local SIGGRAPH
2014 Frostbite course notes, §3.1.5 / Listing 2 (printed p.12, PDF p.13), inspected
including its equations and code. The reference uses 300-digit intermediates and
a 100-digit π constant; exact binary inputs enter through `Decimal.from_float`.
It handles exact r⁴ even for the smallest positive Float32 r without repeating a
Float32/Float64 subtraction failure. Closed-form tests cover normal incidence and
unit roughness, zero Fresnel, range classification and source underflow.

`probe_ggx_reference.py` calls unchanged product functions through the existing
`NativeGroundLitterProbe` four-buffer consumer. No C++ implementation, product
shader correction, new binding, texture sampler or native image format is supplied.
The private wrapper records D, V, lobe, actual GPU a², final scalar specular,
unchanged energy scale, Fresnel and GPU r². White directional incident radiance is
implicit; final specular includes energy compensation and nl. The energy function
and original generated tables are copied unchanged; a separate Float64 table
interpolation control checks the captured energy scale. This is not an independent
validation of those energy tables' physical accuracy.

The 730 inputs include r=0…1, tiny positive r, nh=1 and the adjacent Float32 value,
normal/grazing scalar factors, F0=0/0.04/1, actual normal/mirror-symmetric poses,
explicit a² near Float32 normal/subnormal boundaries, and three nh overshoots.
Most scalar tuples are primitive-function tests, not geometrically realisable
poses. Overshoots are explicitly grouped against the physical nh=1 target;
they do not silently redefine the mathematical function for nh>1.

Two references separate causes: captured GPU a² versus exact represented-input r⁴
(or the explicit input a²). Zero a² follows the existing lobe guard, not a delta
reflector model. Both arrays remain available. Normal, subnormal, zero and out-of-
Float32-range expectations are counted separately, retaining every nonfinite GPU
output. The 64-eps relative diagnostic applies to normal/zero results only; it is
an arithmetic check, not a visual tolerance or permission to flush denormals.
Subnormal zero/nonfinite counts and true range overflows remain explicit.
A successful process means capture and energy controls completed; the report's
two numerical flags may remain false. Neither flag grants full material admission.

## Measured unchanged baseline

On Iris Xe, consumer source/archive cea66e2a9 and product shader 0eb2e739d:

| Normal incidence, F0=0.04 | Native specular | Decimal with captured a² |
|---|---:|---:|
| r=0.01 | +Inf | 318309.9093 |
| r=0.02 | 15928.2236 | 19894.3693 |
| r=0.12 | 15.3503780 | 15.3510559 |

The r=0.02 error is about −19.94%; avoiding NaN alone is insufficient.
There are 58 nonfinite specular responses. For captured-a² normal/zero expectations,
154 specular responses exceed the numerical diagnostic or are nonfinite; 3 further
specular expectations exceed Float32 range. Visibility has no failing normal/zero
cases (maximum relative error 6.36e−8). The source path turns positive r⁴ into zero
in 110 cases; exact-r⁴ comparisons expose this separately. These are matrix counts,
not frequencies in ordinary rendered scenes.

A real mirror-symmetric pose with r≈1e−8 and nl=nv=vh≈1e−6 has lobe≈7.96e42,
but final specular≈7.96e36 fits Float32. Explicit a²≈1e−40/F0=0.04 at normal
incidence similarly separates an oversized D from a representable final answer;
the existing device execution flushes this denormal operand. Zero Fresnel must
also remain zero despite an oversized intermediate. These controls distinguish
intermediate range, source/denormal loss and cancellation. They do not establish
a new policy for mathematically unrepresentable final radiance.

The consumer requests five buffers totalling 140672 bytes and copies 70336 bytes
for 730 rows; input and output are each 23360 bytes. GPU duration is unmeasured.
This is a numerical shader-function capture, not a frame-budget, raster, Metal or
full world gate. The separate mixed optical atlas defect remains #824.

## Reproduction

Supply an existing consumer executable and the original generated energy table:

```sh
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/render/probe_ggx_reference.py \
  --source-commit <shader-commit> --tables /absolute/path/brdfTables.glsl \
  --binary /absolute/path/native-ground-litter-probe \
  --output /tmp/outshine-research-ggx-reference-<commit>/capture
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/render/plot_ggx_reference.py \
  /tmp/outshine-research-ggx-reference-<commit>/capture \
  /tmp/outshine-research-ggx-reference-<commit>/normal-incidence.png
OPENBLAS_NUM_THREADS=1 .venv/bin/python -m unittest discover \
  -s test/experiments/render -p test_ggx_reference.py
```

Dependencies: NumPy, Matplotlib, glslangValidator and the native consumer/runtime.
Capture reports pin shader/table/binary and artifact hashes, commands and terminal
statuses. Compare identical `fields.npy` and unchanged energy outputs before using
a fixed shader to assess improvement. Keep diagnostic images local in system temp.
