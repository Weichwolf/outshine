/* THE BROWSER IS A CONSUMER OF THE ENGINE AND NOT A SECOND DRAWING PROGRAM (board:1447).
 *
 * **THE CLAIM IS GREPPABLE, SO IT IS A TEST AND NOT A HOPE.** *the renderer is the library* is easy to
 * write and easy to drift away from -- one pipeline created here to get a rectangle on screen, one
 * shader to tint it, and the sentence is false while every test still passes. This reads the browser's
 * own sources and refuses the verbs that would make it a renderer.
 *
 * **THE SPLIT IS PRESENTATION AGAINST DRAWING, AND IT IS NOT ARBITRARY.** A client owns its window: it
 * creates one, claims it for the device the library chose, acquires a swapchain image and submits the
 * buffer that presents it. None of that decides a pixel. What decides pixels is a pipeline, a shader,
 * a render pass and a draw -- and every one of those belongs to the library, or the library is not the
 * renderer.
 *
 * **THE INSTRUMENT IS THE FILESYSTEM AND NOT `git grep`.** `CLAUDE.md` names the hazard the other way
 * round -- a recursive grep silently skips what `.gitignore` re-includes -- and here the danger is the
 * same shape: a source not yet staged would read as compliant. This walks the directory, so a file
 * exists to this test the moment it exists on disk. */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Check.h"

using namespace outshine::Test;

namespace {

/* WHAT DECIDES A PIXEL, AND EVERY ONE OF THEM IS THE LIBRARY'S. */
struct Forbidden {
  const char *Spelling;
  const char *Why;
};
constexpr Forbidden kDrawing[] = {
    {"SDL_CreateGPUGraphicsPipeline", "a pipeline is how a picture is decided"},
    {"SDL_CreateGPUComputePipeline", "the same, one dispatch further"},
    {"SDL_BindGPUGraphicsPipeline", "binding one is choosing which picture"},
    {"SDL_CreateGPUShader", "a shader is a program that writes texels"},
    {"SDL_BeginGPURenderPass", "a pass is where drawing happens"},
    {"SDL_BeginGPUComputePass", "the same, without attachments"},
    {"SDL_DrawGPUPrimitives", "a draw IS the drawing"},
    {"SDL_DrawGPUIndexedPrimitives", "the same, indexed"},
    {"SDL_BindGPUVertexBuffers", "geometry bound here is geometry this program owns"},
    {"SDL_BindGPUFragmentSamplers", "a texture bound for a fragment is a fragment this program shades"},
    {"[[stage_in]]", "shader source in a client is a renderer in a client"},
    {"SDL_GPU_SHADERFORMAT_MSL", "the same, one declaration earlier"},
};

/* WHAT A CLIENT MAY DO WITH ITS OWN WINDOW, listed so the refusal above cannot be read as forbidding
 * it. None of these decides a pixel; every one of them is about WHERE the picture goes. */
constexpr const char *kPresentation[] = {
    "SDL_CreateWindow", "SDL_ClaimWindowForGPUDevice", "SDL_WaitAndAcquireGPUSwapchainTexture",
    "SDL_AcquireGPUCommandBuffer", "SDL_SubmitGPUCommandBuffer", "SDL_CreateGPUTexture",
};

std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path);
  std::stringstream held;
  held << file.rdbuf();
  return held.str();
}

/* A SPELLING INSIDE THIS FILE'S OWN TABLE IS NOT A USE. The list above names what it forbids, so a
 * naive search over `test/viewer/` finds every one of them here -- and a test that failed on its own
 * declaration would be unable to state what it checks. */
bool IsThisFile(const std::filesystem::path &path) {
  return path.filename() == "TheBrowserSpellsNoDrawingInstruction.cpp";
}

}  // namespace

int main(void) {
  std::error_code walking;
  std::vector<std::filesystem::path> sources;
  for (std::filesystem::recursive_directory_iterator it("test/viewer", walking), end; it != end;
       it.increment(walking)) {
    if (walking) { break; }
    if (!it->is_regular_file(walking)) { continue; }
    const std::string suffix = it->path().extension().string();
    if (suffix != ".cpp" && suffix != ".h") { continue; }
    if (IsThisFile(it->path())) { continue; }
    sources.push_back(it->path());
  }

  /* THE POPULATION IS NAMED WITH THE VERDICT. A walk that found nothing would pass every claim below
   * while proving nothing at all, which is the shape of every empty-set green light. */
  CHECK(!sources.empty(), "the browser has sources to read, so this verdict is about something");
  std::printf("NOTE sources of the browser read = %zu\n", sources.size());

  int drawing = 0;
  for (const std::filesystem::path &path : sources) {
    const std::string text = Read(path);
    for (const Forbidden &verb : kDrawing) {
      if (text.find(verb.Spelling) == std::string::npos) { continue; }
      ++drawing;
      const std::string claim = path.string() + " spells " + verb.Spelling + " -- " + verb.Why +
                                ", and the renderer is the library";
      Checked(false, "no drawing instruction in the browser", claim.c_str(), __FILE__, __LINE__);
    }
  }
  CHECK(drawing == 0,
        "the browser decides where the picture goes and never what is in it -- one renderer with two "
        "targets, and the client chooses which");

  /* THE PERMITTED HALF IS PRINTED AND NOT CHECKED. Requiring a client to CALL these would make the
   * headless arm -- which owns a texture and no window -- a failure, and the two arms are one program
   * on purpose. */
  std::string allowed;
  for (const char *spelling : kPresentation) {
    allowed += allowed.empty() ? "" : " ";
    allowed += spelling;
  }
  std::printf("NOTE presentation the client may own = %s\n", allowed.c_str());
  return Report();
}
