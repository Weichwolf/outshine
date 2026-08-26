Type: bug
State: open
Area: scenario, render
Tags: declaration, shadow

# A subject declares whether it casts a shadow, and one that casts none is expressible

Shadows are not optional in the declaration language. `Live::Build` derives
`ShadowRadiusStoodM_` from the subject's own extent whenever the scenario declares none, and
`DeclarePlan` puts `Stage::LightVisibility` in the plan whenever that radius is positive. There
is no way to say "this subject draws and casts nothing":

- `Lit.ShadowRadiusM` only turns shadows ON -- a zero means DERIVE, not OFF
- a subject with no extent would give a zero radius, and the engine refuses it first, correctly:
  *"the subject has no extent over its own grid, so no camera can be derived from it"*

Both benchmarks carry the flag per primitive. Unreal's `bCastDynamicShadow` /
`CastShadow` on the component and the material's shadow settings; RAGE's drawables carry
per-model flags the shadow drawlist filters on. It is not an optimisation -- a glass pane, a
light shaft, a decal and a skybox all draw and cast nothing, and a renderer that cannot say so
draws their silhouettes into the depth the light sees.

## What this blocks

board:1922's proving arm. The claim there is that a frame no light stage ran on reads no atlas --
`SubjectDraw::Shadowed_` used to be set true by the stage and set false by nothing, and
`Renderer::RenderFrame` now clears it. **That repair is unproven** and stays unproven until a
scenario can stand a subject that draws without casting: today "no light stage" and "no subject"
are the same arm, and an empty stage proves nothing about a flag.

## What will be true

- [ ] A declared asset carries whether it casts, defaulting to yes, and the plan drops
      `LightVisibility` when nothing standing casts.
- [ ] Proving case: two scenarios differing only in that flag, both drawing the same batch count,
      and only one of them writing an atlas -- and the non-casting one reports `frames the
      subject drew shadowed` unchanged from a previous shadowed frame. Negative control:
      `CastsNoShadow()` removed from `RenderFrame`, and the count grows.
