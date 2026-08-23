Type: bug
Area: render
Tags: shaders, embedded-source, c-ism

# The shader prelude and the velocity constant are files like every other shader

CLAUDE.md, mechanical bar: "**No embedded shaders or scripts**: shader and script sources
live as files in the tree, never as string literals inside C++ -- an embedded MSL/GLSL/script
blob is a defect." Two survive, and both were introduced by a closure:

```cpp
static const char *kMslPrelude = R"(
#include <metal_stdlib>
using namespace metal;
)";
```
— src/render/stages/ShaderPrelude.h:10-13

```cpp
static const char *kVelocityMsl = R"(
constant float kVelStatic = VELOCITY_STATIC;
)";
```
— src/render/stages/SceneTargets.h:26-28

They are the only two `R"(` blobs left in `src/` (`grep -rn 'R"(' src/ --include='*.h'
--include='*.cpp'` = 2 files). Every other stage already loads its MSL from
`src/render/shaders/*.msl` via `LoadShaderText` (SubjectDraw.cpp:210-214), so the machinery
exists and these two are the exception, not the pattern. board:1651 closed by CREATING
`kMslPrelude`; that closure paid one debt with another.

## The second defect in the same two lines

`static const char *` at namespace scope in a HEADER gives every translation unit that
includes it its own internal-linkage copy of the pointer AND, in a header included widely, an
unused-variable warning surface that only `-Wno-unused-*` hides. The house form for a
compile-time text constant is `inline constexpr std::string_view`. This is the same C-ism
board:1489 and board:1621 swept out of the doors; two survive here because nobody grepped the
render stages.

## What will be true

1. `src/render/shaders/prelude.msl` and `src/render/shaders/velocity.msl` exist, and
   `MslPrelude()` / the velocity define load them the way every other stage loads its source.
2. No `R"(` blob containing MSL remains under `src/`; a claim in `test/harness/claims/`
   asserts it, so the next blob cannot ship green.
3. Whatever text must stay in C++ (the injected `#define` lines, which are DERIVED numbers
   and belong in code) is `std::string_view`, not `static const char *`.
