Type: defect
State: active
Architecture: ready
Parent: 2191
Depends:
Priority: P0
Area: render, engine
Tags: publication, lighting, shadow, hockenheim

# Render-plan redeclaration preserves the shadowed world

## Reproduction

At Hockenheim lap station 1373.068 m, direct first declaration with default
outputs and direct first declaration retaining `sceneShadingNormal` and
`sceneSurfaceIdentity` both produce RGB (4,10,26) at pixel (640,650). A second
identical declaration also preserves that pixel. Changing only retained outputs
in a second `Engine::declare` instead produces RGB (91,88,83). The camera eye,
station, normal, surface identity and depth agree; the scene-linear RGB changes
from (169,375,831) to (3602,3444,3196). This is a lost shadow/light state on
plan replacement, not missing road geometry or a material substitution.

## Ownership decision

`src/render/SceneRenderer` owns frame resources, stage caches and bindings;
`src/engine/Declaring` owns the transactional world publication. A changed
render plan must rebuild its GPU frame resources while preserving the same
published world/light/shadow declarations and rebind every new attachment to
the candidate stages before publication. Do not clear the old frame until the
candidate is valid. No special case for Hockenheim or the pixel probe.

## Falsifiable acceptance

- A deterministic shadowed native scene renders identical linear and display
  colour before and after changing only retained diagnostic outputs, matching
  an independently fresh Engine with the new outputs. The newly requested
  attachments are readable and finite. Turning off the caster or moving the
  light is an effective negative control that changes the selected pixel.
- Pinned Hockenheim at 74.850 s retains the same shadowed road image and pixel
  under the plan transition, within declared float/encoding tolerance. Probe
  (640,650) stays on surface ID 1, an upward normal and matching depth.
- Rejected plan changes preserve the prior frame and successful retry works.
  Focused suites, `make format` and `LINT_JOBS=2 make lint` pass.
