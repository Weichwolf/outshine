# Conditional finite-LOD normal counterprobe

Owner: [#847](https://github.com/Weichwolf/outshine/issues/847). This CPU diagnostic
reads the *actual native card's* decoded RGBA8 normal image/mips and recorded
normal attachments. It changes no native resource, shader, error bound or frame.
A declared constant tangent-to-world basis and expected pixel-centre UVs are
hypotheses; actual fragment values have not been observed by this program.

The [Vulkan sampling specification](https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/chapters/textures.adoc)
(`textures-level-selection` / `textures-texel-mipmap-filtering`) defines linear
mipmap sampling by adjacent levels and their linear weights. The control models
ordinary non-anisotropic linear sampling followed by normal decoding and
normalization. It reuses `moment_codec.sample` for clamped bilinear weights.
Power-of-two images and integer base-pixel centres keep these weights exactly
binary-representable; UNORM8 division and the declared basis use Fractions.

For each level pair, the unnormalized world vector is `q(t)=a+t*d`, `d=b-a`.
For observed vector o, maximize `dot(q,o)/sqrt(dot(q,q)*dot(o,o))` on `[0,1]`.
There is at most one isolated interior stationary point. If the derivative
vanishes identically, the angular error is constant and the endpoints suffice:

`t = (A*ad-D*aa)/(D*ad-A*dd)`, where `A=a·o`, `D=d·o`,
`aa=a·a`, `ad=a·d`, `dd=d·d`.

Endpoints and that point suffice. A segment containing a zero vector is
unresolved. The unchanged native normal bound is `4*sqrt(3)/255`; its squared
value `48/65025` makes the comparison exact. For a nonnegative dot product,
`cos(angle) < 1-24/65025` is compared by squaring rational quantities. The
reported distance is a Float64 display value; exclusion uses the exact comparison.
This is an algebraic bound for this explicit model, not a global GPU guarantee.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error -m unittest \
  test_normal_mip_counterprobe test_moment_codec
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error test/experiments/render/normal_mip_counterprobe.py \
  --base /explicit/normal.base --lower-mips /explicit/normal.mips \
  --before /explicit/before.normal.f32 --after /explicit/after.normal.f32 \
  --size 128 --basis -1 0 0 0 -1 0 0 0 -1 \
  --plot --output /tmp/outshine-normal-finite-lod
```

The example basis is the declared axis control for the frozen view2 card, not a
universal renderer convention. Inputs/readbacks are size-checked and hashed;
outputs are private system-temp reports/plots. Cap1024×1024 with a complete chain
and4096 changed pixels; at most3 direction candidates per adjacent mip segment.
The exact predicate is a forensic oracle; Fraction allocation/bit costs and
native timings are unmeasured. It grants no runtime recommendation.

A negative result says that changing *only a finite LOD* cannot match the changed
normal within the existing bound *under the declared UV/basis/ideal-filter model*.
It excludes neither changed UVs/TBN, allowed sampling/compute precision, wrong
image/descriptor/lifetime, nor NaN/undefined implicit derivatives. In particular,
[nonuniform derivative control flow](https://docs.vulkan.org/glsl/latest/chapters/builtinfunctions.html)
remains outside this finite-model argument. The next useful native observation
is the actual sampled RGB and UV/TBN in the failing invocation; the counterprobe
does not establish a driver bug or close #847.
