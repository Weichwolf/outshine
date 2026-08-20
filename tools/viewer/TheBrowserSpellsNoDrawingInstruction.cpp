#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Check.h"

using namespace outshine::Test;

namespace {

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

bool IsThisFile(const std::filesystem::path &path) {
  return path.filename() == "TheBrowserSpellsNoDrawingInstruction.cpp";
}

}

int main(void) {
  std::error_code walking;
  std::vector<std::filesystem::path> sources;
  for (std::filesystem::recursive_directory_iterator it("tools/viewer", walking), end; it != end;
       it.increment(walking)) {
    if (walking) { break; }
    if (!it->is_regular_file(walking)) { continue; }
    const std::string suffix = it->path().extension().string();
    if (suffix != ".cpp" && suffix != ".h") { continue; }
    if (IsThisFile(it->path())) { continue; }
    sources.push_back(it->path());
  }

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

  std::string allowed;
  for (const char *spelling : kPresentation) {
    allowed += allowed.empty() ? "" : " ";
    allowed += spelling;
  }
  std::printf("NOTE presentation the client may own = %s\n", allowed.c_str());
  return Report();
}
