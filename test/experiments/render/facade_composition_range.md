# Shared facade colour range

Owner: #306, material composition #6, goal #189. This corrects the composition
introduced by PR368. The original Metal `CheckMasonry` exposed negative blue;
it and atlas sample validation remain unchanged.

## Bound

For each source colour channel `b` in [0,1], the existing coating amplitude is
`c=min(0.16*b,2*min(b,1-b))`. With opaque wall weight `w`, coating contributes
within `[-0.5*c*w,+0.5*c*w]`, and the joint brick field is in `[-0.16*w,+0.84*w]`.
Previously both contributions independently consumed the entire source range.
The Metal case has blue `b=0.032`: coating can subtract0.00256 while the old
masonry amplitude can subtract another0.032, so the sum becomes negative.

Reserve both sides before selecting the masonry amplitude:
`low=max(b-0.5*c,0)`, `high=max(1-b-0.5*c,0)`.
Positive amplitude is at most `min(low/0.16,high/0.84)`; negative magnitude is
at most `min(low/0.84,high/0.16)`. Thus the combined wall stays in [0,w] in real
arithmetic. Existing pane/frame contributions are convex weights and fit the
remaining range. Source colour and coating amplitude remain unchanged.

Both available rooms have a relative reserve `1-2^-19` (sixteen binary32 machine
epsilons) before the divisions, to leave room for floating-point cancellation.
The native boundary stress below checks this implementation; it does not prove
every compiler/device's rounding. There is no final RGB clamp. Amplitude depends
only on the source colour, so it does not introduce a per-sample correlation
with coating. Brick dimensions, phase/filter, and zero-mean field are unchanged.

## Reproduction

```sh
PYTHONPATH=lib/src python3 test/experiments/render/facade_composition_range.py \
  --raster-binary /tmp/NativeFacadeProbe \
  --compute-binary /tmp/NativeMaterialProbe \
  --baseline-checkout /tmp/outshine-before-facade-range \
  --output /tmp/outshine-facade-range
```

Build the existing `NativeFacadeProbe.cpp` and `NativeGroundLitterProbe.cpp`
consumers under `test/harness/render/`. `GLSLANG` and `SPIRV_CROSS` select the
installed shader tools. Baseline must contain the pre-fix production helpers;
the experiment compiles both actual fragment shaders and both helper versions.

Linux/RTX3060,16October2026, baseline a56f3e3be:

- The effective painted triangle of `CheckMasonry`, at160×96, has7680 covered
  pixels. Old blue reaches−0.00254422; corrected minimum is0.0000158306.
  A full affine wall reproduces the same defect. All metal/roughness attachment
  bytes remain identical. These are actual production fragment executions;
  precomposing vertex paint preserves the material input, but does not replace
  the original129×129 atlas test or prove its Metal execution.
- 41536 native compute combinations cover649 source colours,16 opening/pane/
  band configurations, both extreme coating signs and brick-field endpoints.
  Previous RGB range is−0.00614399…1.07399702; corrected range is[0,1].
  Roughness, the coating-only base and field outputs remain byte-identical.
- Existing24 wall/gable raster pairs and eight exclusions pass. The independent
  screen-area reference retains its0.015 additional-RGB error limit. Fourteen
  unflagged cases remain byte-identical to the c8507e906 predecessor shader.
- The local PDF is optically opened and displays raw signed linear blue,
  with the same scale before/after. No image is published under `doc/`.

This adds bounded vector arithmetic to the existing amplitude calculation, no
noise/texture samples, bindings, passes, allocations or asset bytes. Whole-frame
time, Places, Metal/A18 and the original atlas rerun remain integration checks.
