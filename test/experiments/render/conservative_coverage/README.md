# Whole-pixel opaque coverage experiment (#990)

This isolated native-RHI experiment tests a prerequisite for selecting asset pages
before payload IO. A depth sample at a pixel centre cannot establish that an opaque
triangle covers the whole pixel. Such samples can hide geometry through thin edges.
The experiment changes no product renderer, asset format or shared GPU bindings.

For each triangle, the vertex shader exports screen-linear barycentric coordinates.
An edge coordinate `b` is nonnegative inside the triangle. Across a square pixel its
minimum is `b - (abs(dFdx(b)) + abs(dFdy(b))) / 2`. The fragment shader retains a
sample only if this minimum is positive for all three edges. A deliberately unsafe
point-sample control makes the missing full-pixel guarantee observable.

The output is an R32Float colour attachment containing a reverse-Z depth bound:
the minimum of the three vertex depths, reduced by an experimental rounding guard.
For an unclipped valid projected triangle, depth is screen-linear and this minimum
is no closer than any point inside the triangle. A separate long-double CPU oracle
checks every pixel's four corners against the exact float32 input vertices and
checks the bound against the interpolated depth at those corners.

Near/far/eye-plane crossings and nonfinite inputs contribute no coverage. XY clipping
is exercised. The corpus includes subpixel shifts, varying homogeneous W, thin and
degenerate triangles, reversed winding, exact pixel edges and seeded random cases
with non-binary homogeneous weights. `--seed` selects a reproducible uint32 corpus.
At least 90% of fully covered corpus pixels must remain accepted, so reporting all
coverage as unknown cannot pass the calibration. This is a usefulness sanity check,
not a product quality threshold. Any false coverage, closer depth or invalid output
fails. The point-sample control must exhibit false coverage.

The pixel and depth guards are empirical. These checks are **not a general float32
rounding proof**, an original world-to-clip transformation error bound, a guarantee
over arbitrary camera movement, or a Metal/world integration test. Individual triangle
erosion also loses coverage at shared edges. Multi-triangle composition, hierarchical
depth reduction, actual opaque root acquisition, receiver/shadow/game/reflection union
and generation/fence ownership remain separate parts of #990. Alpha-tested surfaces
cannot become occluders through this untextured opaque-triangle calibration.

## Reproduction

Use a prepared native checkout/archive and its complete compilation database. The
runner reuses the calibration unit's full compiler flags and the checkout's native
link flags; it does not rebuild the engine or run broad gates. Shader compilation
requires `glslangValidator`; device execution uses the existing native backend and
requests validation and GPU timestamps.

```sh
python3 test/experiments/render/conservative_coverage/run.py \
  --native-checkout /path/to/prepared-checkout \
  --compile-database /path/to/compile_commands.json \
  --output /tmp/outshine-coverage-<commit>
```

Output must be a new empty directory under the system temporary directory. The
runner retains commands, terminal status, source/archive/database/binary hashes,
compiler and device logs, and numerical results. GPU totals measure the tiny draw
submissions, including attachment clears; readback, CPU oracle and waiting are
outside those GPU intervals. Missing timestamps are explicit. Validation-enabled
single-triangle timings do not predict scene FPS or integrated loading gains.

The runner may use a different prepared source revision only after checking the
relevant native-RHI/calibration sources and recording that distinction in the
delivery. It records both commits and content hashes; it does not certify that an
arbitrary archive matches its checkout. Current device results belong in #990.

## Primary reference

Durand et al., SIGGRAPH 2000, *Conservative Visibility Preprocessing using Extended
Projections*, section 3.2 describes shrinking occluders to retain only fully covered
pixels; section 3.1 distinguishes conservative occluder and occludee depth bounds.
The paper is stored at
`doc/references/geometry/siggraph/2000-conservative-visibility-extended-projections.pdf`.
Pages 3–4, including the diagrams, were inspected for this experiment.
[Author's publication page](https://people.csail.mit.edu/fredo/PUBLI/Sig2000/index.htm).
