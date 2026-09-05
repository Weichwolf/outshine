Type: debt
State: open
Area: render, build
Tags: architecture, owner, audit

# The tree ships SPIR-V and reads no MSL; a shader is GLSL compiled by Khronos's tools

**Benchmark** -- Unreal: shaders are HLSL, cross-compiled per platform by the ShaderCompileWorker
(DXC, SPIRV-Cross for Metal); no `.metal` source stands in the engine. RAGE: HLSL through the
console vendors' compilers, the same shape. Filament: `matc` compiles one material source to
SPIR-V, MSL and GLSL at build; the runtime reads the package. **All agree**: one source
language, vendor forms are build products. SDL3's own answer is `SDL_shadercross` (HLSL or
SPIR-V in; SPIR-V, DXIL, MSL out) and `SDL_GPU` accepts SPIR-V on every backend it has, Metal's
included. Decided with the owner 2026-09-05: Khronos and Vulkan are the reference, the engine
reaches the GPU through SDL_GPU only, and what Apple does is not this tree's problem.

## Where it stands, measured 2026-09-05

```
  src/render/shaders/*.msl          32 files, 2 022 lines, Metal Shading Language
  SDL_GPU_SHADERFORMAT_MSL          8 sites; the device is created for MSL
                                    (src/render/SceneRenderer.cpp:245)
  ShaderText().Reads(...)           the stages concatenate .msl fragments by hand, a
                                    preprocessor of the tree's own
  CLAUDE.md                         until today asked for tile shading, mesh shaders and
                                    ray tracing to be MEASURED; SDL_GPU exposes none of the
                                    three, so the paragraph now stops where the API stops
```

## The solution

- every shader is GLSL (Vulkan dialect); `glslangValidator` compiles it to SPIR-V at build
  and the tree hands SDL_GPU SPIR-V (`SDL_GPU_SHADERFORMAT_SPIRV`); `#include` through
  glslang's includer replaces the by-hand concatenation, and the vertex arms generated from
  VertexArms.h are generated as GLSL
- the device asks for SPIR-V; MSL is derived by SDL (its Metal backend runs SPIRV-Cross) or by
  the build, and no `.msl` stands in the tree -- a claim holds `src` free of `msl` and
  `metal`
- pictures: the arithmetic is the same but the compiler path is not (glslang → SPIRV-Cross →
  Apple's compiler), so a digest may move by a rounding; every move is accepted with
  `pixels.py`'s count, its window and the look, or it goes back
- order: this opens every shader, and so does board:2149; the two are one round after the
  map of board:2101

## What will be true

- [ ] `find src -name '*.msl'` reads 0 and a claim keeps it there
- [ ] `SDL_GPU_SHADERFORMAT_MSL` appears nowhere; the device is created for SPIR-V
- [ ] the nine references bit-identical, or each move named pixel by pixel with its cause
- [ ] Negative control: a `.msl` file placed under `src/render/shaders` fails `make lint`
