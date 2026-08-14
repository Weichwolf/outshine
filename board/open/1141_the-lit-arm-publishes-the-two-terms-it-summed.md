Type: task
Parent: 1137
Area: render
Tags: instrument

**The lit arm publishes the two terms it summed, and the two scalars that reached it through a sampler**

`shadeRow` sums `(diffuse + specular) * nl * attenuation * tint` over the lights and adds the emitted
radiance. Only the sum leaves the device, so *our lobe is missing*, *our lobe is misplaced* and *the light
is not reaching this pixel* are three sentences with one observation behind them.

**THE TERMS ARE PUBLISHED, NOT THE INPUTS, AND THAT IS THE DESIGN DECISION.** A probe of the BRDF's
*inputs* — albedo, metalness, roughness, the light list — hands the host a re-evaluation to perform, and
the re-evaluation is where the error enters: the view vector, the resected point, the attenuation, the
occlusion ray and the model itself all have to be reproduced before a number appears, and each is a place
the probe can be wrong in a way that looks like a defect. The **terms** need no reproduction. They are
what `BrdfTerms` already separates, and they are directly the three questions: a zero specular sum with a
non-zero diffuse sum is a missing lobe; a non-zero specular sum in the wrong place is a misplaced one;
both zero at a lit-facing fragment is the light not arriving.

**TWO ATTACHMENTS, EIGHT CHANNELS, NO NEW FORMAT.**

| resource | `Rgba16Float` | what the alpha carries |
|---|---|---|
| `SceneDirectDiffuse` | the summed diffuse contribution, rgb | **metalness**, as `shadeRow` received it |
| `SceneDirectSpecular` | the summed specular contribution, rgb | **roughness**, as `shadeRow` received it — *after* `roughenedBy`, because the value the lobe used is the claim and the value before it is a different quantity |

The two scalars ride in the alphas because they are the two inputs the host **cannot** derive: they arrive
through the texture sampler that `board:1130`, `board:1133` and `board:1135` are an investigation of, and
a host-side resample of the same texture would be a second implementation of the code under test. Albedo
and the emitted radiance need no channel: `emitted = SceneHdr - diffuse - specular` on any lit,
non-blended fragment, exactly, by the arithmetic the shader performs.

**THE COUNTS, BECAUSE THE ARRAY IS NEARLY FULL.** `Stage::Subjects` contributes `SceneHdr`,
`SceneVelocity`, `SceneDepth`, `SceneShadingNormal` today. With these two it contributes **6**, and
`Resource Contributes[kMaxEdges]` at `kMaxEdges = 8` needs a `kNoEdge` terminator, so **7 of 8 are spent
and a third probe attachment does not fit without raising the constant**. Colour attachments reach **5 of
`kMaxColourAttachments` = 8** (depth is not one), which is inside the floor WebGPU guarantees. A plan that
does not name them in `Outputs` pays **nothing** — the prune of `board:1121` is the precedent and
`SceneShadingNormal` is the working example.

**THE COLOUR INDICES ARE SPLICED FROM THE COMPILED PLAN AND NEVER FIXED**, exactly as
`SUBJECT_NORMAL_COLOUR_INDEX` is, because a hardcoded index is the pipeline/pass disagreement `board:1121`
closed. **The pair is one capability and not two**: a plan naming one target and not the other would
splice a shader writing an attachment the pass has not got, so the compile refuses a partial pair and says
which. *That rule is written down rather than carried by a type, and the shape that would carry it is a
co-attachment field on the resource row — named here so the next round can decide it rather than
rediscover the refusal.*

**Done when** a plan that asks for the two targets gets them, a plan that does not is byte-identical to
today, and the shader permutation the pair adds is one flag rather than two.
