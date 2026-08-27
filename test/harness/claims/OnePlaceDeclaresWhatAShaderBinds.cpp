#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

// A PIPELINE'S BINDING COUNTS ARE THE SHADER'S, AND A COPY OF THEM IS A COPY THAT WILL DRIFT.
//
// `SDL_GPUShaderCreateInfo` carries four numbers -- samplers, uniform buffers, storage buffers and
// the stage -- and SDL binds exactly what they say. A shader that declares a resource the struct
// does not count is handed nothing at that slot, and Metal reads it as zero rather than refusing.
// So the failure is SILENT: the pipeline builds, the draw runs, the picture is wrong somewhere
// only a case that looks at that resource can see.
//
// Unreal derives the bindings from the shader's own SHADER_PARAMETER_STRUCT and RAGE keeps them
// with the loaded grcProgram; in neither can the pipeline's count disagree with the shader's,
// because there is only one statement of it. `DrawShape` is this tree's version, and `ShaderFrom`
// in `src/render/stages/KernelShape.h` is the one place that turns a shape into a shader.
//
// MEASURED, and the reason this claim exists: the struct was filled by hand at 13 sites across 7
// files. `LightVisibilityStage::ConfigureDepthOnly` copied the samplers and the uniform buffers
// from `SubjectDraw::DepthOnlyShape` and never copied the storage buffers. When the depth-only
// shader grew a placement buffer at `buffer(1)` the pipeline still declared zero, the shadow
// atlas came back holding one value, and `khronos/glTF` stayed 444/444 because it never reads the
// atlas. Two door cases caught it. A field dropped in a copy is invisible until a shader needs it.
//
// So the number this walks is not a style preference. One declaration is what makes the defect
// unwritable.

namespace {

constexpr size_t kOneDeclaration = 1;

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<std::string> sites = Lines(
      Ask("grep -rn 'SDL_GPUShaderCreateInfo [A-Za-z_]*{' src/ 2>/dev/null | sort"));

  std::printf("SITES DECLARING A SHADER'S BINDING COUNTS  %zu\n", sites.size());
  for (const std::string &one : sites) { std::printf("  %s\n", one.c_str()); }

  const std::vector<std::string> calls =
      Lines(Ask("grep -rln 'SDL_CreateGPUShader' src/ 2>/dev/null | sort"));
  std::printf("FILES CALLING SDL_CreateGPUShader          %zu\n", calls.size());
  for (const std::string &one : calls) { std::printf("  %s\n", one.c_str()); }

  CHECK(sites.size() == kOneDeclaration,
        "**ONE PLACE DECLARES WHAT A SHADER BINDS**: `ShaderFrom` reads every field of a "
        "`DrawShape`, so a stage that grows a storage buffer cannot forget to declare it. A "
        "second hand-filled `SDL_GPUShaderCreateInfo` is a place where one field can go missing, "
        "and a missing field binds nothing and says nothing");

  CHECK(calls.size() == kOneDeclaration,
        "and one place calls `SDL_CreateGPUShader`, because a second caller is where the second "
        "declaration comes back -- the count above only holds while there is nowhere else to "
        "fill the struct");

  Covers("the render tier's shader construction: the binding counts a pipeline declares are "
         "stated once, from the shape, and no stage restates them");
  return Report();
}
