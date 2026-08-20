/* THE SUBJECT UNIT'S OWN SHADER TEXT, HANDED TO A REAL DEVICE AND NOTHING ELSE (board:1207).
 *
 * WHY THIS EXISTS, AND IT IS A DEFECT'S OWN EPITAPH. A `shadeRow` call site was left one argument
 * short while four others were updated. The library BUILT -- the text is a C++ string literal, so the
 * host compiler never reads it. The unit tree passed 54 of 54. The shader suite passed, because not
 * one of its members compiled THIS unit's source. The error surfaced only when a render case created a
 * device, and it took a full corpus preparation and a 208-test run to see: 71 cases red on a picture
 * they never got to draw.
 *
 * SO THE CLAIM IS DELIBERATELY THE SMALLEST ONE THAT WOULD HAVE CAUGHT IT: every shader this unit
 * emits is accepted by the driver. No asset, no camera, no oracle, no corpus -- which is exactly what
 * `CLAUDE.md` says a shader case is, and the one shape of failure the other four members cannot see.
 *
 * IT IS THE ATTACHMENT SETS THAT MAKE IT MORE THAN ONE COMPILE. `Configure` splices the plan's colour
 * targets into the text -- velocity, shading normal and surface identity each add an output and an
 * arm -- so a set the plan can produce is a shader variant nobody else compiles. The sets below are
 * walked rather than assumed: the empty one, each single target, and all three together. */
#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "Gpu.h"
#include "SubjectDraw.h"

using outshine::Render::AttachmentSet;
using outshine::Render::Gpu;
using outshine::Render::Resource;
using outshine::Render::SubjectDraw;

int main() {
  using namespace outshine::Test;

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    CHECK(false, "the video subsystem starts, which a shader case needs a device for");
    return Report();
  }
  SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
  if (device == nullptr) {
    CHECK(false, "a Metal device is created");
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return Report();
  }

  /* THE THREE OPTIONAL TARGETS AND EVERY COMBINATION THIS TEST WALKS, named rather than counted so a
   * target added to the plan and not to this list is visible as a name that is not here. */
  const Resource optional[3] = {Resource::SceneVelocity, Resource::SceneShadingNormal,
                                Resource::SceneSurfaceIdentity};
  int compiled = 0;
  for (unsigned mask = 0; mask < 8u; ++mask) {
    Gpu gpu;
    gpu.Device = device;
    gpu.FiltersFloat32 = true;
    /* `Add` answers whether the set had room, and the answer is CHECKED rather than dropped: a set
     * that silently lost a target would make this test compile a shader the plan cannot produce and
     * report a pass about a variant nobody renders. */
    bool held = gpu.SceneColours.Add(Resource::SceneHdr);
    std::string named = "SceneHdr";
    for (int at = 0; at < 3; ++at) {
      if ((mask & (1u << (unsigned)at)) == 0u) { continue; }
      held = gpu.SceneColours.Add(optional[at]) && held;
      named += at == 0 ? " + velocity" : (at == 1 ? " + shadingNormal" : " + surfaceIdentity");
    }
    CHECK(held, "the attachment set holds every target this combination names");

    SubjectDraw unit;
    std::string error;
    const bool ok = unit.Configure(gpu, error);
    CHECK(ok, "every shader the subject unit emits for this attachment set is accepted by the driver");
    if (!ok) {
      std::printf("       %s: %s\n", named.c_str(), error.c_str());
    } else {
      ++compiled;
    }
  }

  Note("attachment sets compiled", (double)compiled, "of 8");
  Covers("I.26 the subject unit's shader text compiles on a real device, over every attachment set "
         "the compiled plan can hand it");
  SDL_DestroyGPUDevice(device);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return Report();
}
