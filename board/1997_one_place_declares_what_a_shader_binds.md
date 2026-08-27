Type: bug
State: active
Area: render
Tags: gpu-driven, duplication

# a shader's binding counts are declared in ONE place, and a guard counts the places

**Benchmark** — Unreal: `FShader` derives its parameter bindings from the shader's own
`SHADER_PARAMETER_STRUCT`, so a resource the shader declares and the pipeline does not is not
expressible. RAGE: `grcProgram` carries the bound register counts with the program it loaded.
**Both make the pipeline's declaration follow the shader's rather than repeat it.** Taking that,
because the failure this item was found by is exactly what repeating buys.

`SDL_GPUShaderCreateInfo` is filled by hand at **13 sites across 7 files** under `src/render/`.
Each names `num_samplers`, `num_uniform_buffers` and sometimes `num_storage_buffers`. `DrawShape`
exists to hold those four numbers, and `SubjectDraw.cpp`'s `MakeShader` reads all of them -- but
it hardcodes `SubjectDraw::ShaderShape`, so it serves one pipeline and every other site is a
hand copy.

**What the copy cost, measured**: `LightVisibilityStage::ConfigureDepthOnly` reads
`SubjectDraw::DepthOnlyShape` for samplers and uniform buffers and never sets
`num_storage_buffers`. When the depth-only shader grew a placement buffer at `buffer(1)`, the
pipeline still declared zero, Metal bound nothing, and the shadow atlas came back holding one
value. `khronos/glTF` stayed 444/444 -- it does not read the atlas -- and
`outshine/door/ScoreWhatTheShadowCasts` and `ScoreWhatMovingTheEyeDoesToAShadow` went RED. A
field dropped in a copy is invisible until the shader needs it.

**The repair**: one `ShaderFrom(device, source, entry, stage, shape)` beside `DrawShape` in
`KernelShape.h`, reading every field of the shape, and every site calls it with its own shape.
A site that forgets a field is then not a thing that can be written.

**The measurement that shows I am wrong**: a claim counts `SDL_GPUShaderCreateInfo` declarations
under `src/` and refuses when the number moves off 1. Negative control: a second hand-filled
`wanted` anywhere under `src/render/` turns that claim RED.
