/* THE BROWSER SHOWS EVERY CASE THE TREE DECLARES, AND *SHOWS* MEANS TEXELS (board:1447).
 *
 * **A CASE THAT CONFIGURES IS NOT A CASE THAT APPEARS.** `EveryCaseTheTreeDeclaresConfigures` says a
 * manifest reads and a studio is built; this says the library then DREW something. The two are a long
 * way apart: a subject that reads, a plan that compiles and a device that comes up still leave a black
 * frame if nothing reached a texel, and a black frame is exactly what a browser must never show
 * quietly.
 *
 * **THE CLIENT OWNS THE TARGET AND THE LIBRARY IS HANDED ONE**, which is the same path the windowed
 * arm takes with a swapchain image in its place -- so this is a test OF the browser and not a
 * rehearsal beside it.
 *
 * **A CASE THAT DECLINES IS ANNOUNCED AND COUNTED, never hidden.** `limits-probe` says the engine
 * refuses its subject on purpose, and a browser that silently skipped it would answer a different
 * question than *what does this tree declare*. */
#include <algorithm>
#include <cstdio>

#include <SDL3/SDL_gpu.h>
#include <string>
#include <vector>

#include "Check.h"
#include "Chrome.h"
#include "GlyphSheet.h"
#include "Layout.h"
#include "Paint.h"
#include "RenderCase.h"
#include "Renderer.h"

using namespace outshine::Test;
namespace Ui = outshine::Ui;
namespace View = outshine::Viewer;

namespace {

/* HOW MANY PIXELS DIFFER FROM THE FIRST ONE. A frame of one colour is a frame nothing reached, whatever
 * that colour is -- so this asks about VARIATION and never about black, which would be a claim about a
 * clear colour rather than about drawing. */
size_t Varying(const std::vector<uint8_t> &rgba) {
  if (rgba.size() < 4) { return 0; }
  size_t varying = 0;
  for (size_t at = 4; at + 3 < rgba.size(); at += 4) {
    const bool same = rgba[at] == rgba[0] && rgba[at + 1] == rgba[1] && rgba[at + 2] == rgba[2];
    varying += same ? 0 : 1;
  }
  return varying;
}

}  // namespace

int main(void) {
  /* UNBUFFERED, because a run that dies on a device loses a buffered tail -- and the tail is the case
   * it died on, which is the one thing a reader needs. */
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::vector<View::Listed> cases = View::Cases();
  CHECK(!cases.empty(), "the tree declares cases");

  outshine::Render::Renderer renderer;
  const std::vector<uint8_t> sheet = View::Sheet();
  View::Showing showing;
  Ui::Markup chromeTree;
  Ui::Stylesheet chromeSheet;
  Ui::Layout chromePlaced;
  Ui::Painting chromePainted;
  const View::SheetFont face;

  int drew = 0, declined = 0, documents = 0, unprepared = 0, blank = 0, refused = 0;
  std::string suite;
  for (const View::Listed &one : cases) {
    if (one.Suite != suite) {
      suite = one.Suite;
      std::printf("SUITE %s\n", suite.c_str());
    }
    if (one.Document) {
      ++documents;
      continue;
    }
    if (!one.Ready) {
      ++unprepared;
      std::printf("  UNPREPARED %s\n", one.Name.c_str());
      continue;
    }

    ConfiguredCase held;
    std::string why;
    if (!held.Read(one.Prepared, why)) {
      if (held.Declines()) {
        ++declined;
        std::printf("  DECLINED %s -- %s\n", one.Name.c_str(), why.c_str());
        continue;
      }
      ++refused;
      Checked(false, "the case the browser shows reads", (one.Name + ": " + why).c_str(), __FILE__,
              __LINE__);
      continue;
    }

    /* THE BROWSER ADDS ITS OWN STAGE TO THE CASE'S PLAN -- one renderer, one plan, one more
     * contribution. The chrome is laid out for the case's own frame, because that is the surface it
     * will be drawn over. */
    if (!held.Start(renderer, why, {outshine::Render::Stage::Overlay}) || !held.PoseAt(0, why)) {
      ++refused;
      Checked(false, "the case the browser shows starts", (one.Name + ": " + why).c_str(), __FILE__,
              __LINE__);
      continue;
    }
    if (!renderer.SetOverlayAtlas(sheet.data(), View::AtlasWidth(), View::AtlasHeight(), why)) {
      ++refused;
      Checked(false, "the browser's face is uploaded", (one.Name + ": " + why).c_str(), __FILE__,
              __LINE__);
      continue;
    }
    showing.Note = "SHOWING " + one.Name;
    const std::string document =
        View::Declaration(cases, showing, held.WidthPx(), held.HeightPx());
    chromeTree = Ui::Markup();
    chromeSheet = Ui::Stylesheet();
    if (chromeTree.Read(document, why)) {
      chromeSheet.Read(Ui::UserAgentSheet());
      chromeSheet.Read(chromeTree.StyleText());
      if (chromePlaced.Build(chromeTree, chromeSheet, held.WidthPx(), held.HeightPx(), face, why) &&
          chromePainted.Build(chromePlaced, face, why)) {
        const std::vector<outshine::Render::OverlayQuad> ready =
            View::AsOverlay(chromePainted.Quads());
        if (!renderer.SetOverlay(ready.data(), ready.size(), why)) {
          Checked(false, "the browser's chrome reaches the library",
                  (one.Name + ": " + why).c_str(), __FILE__, __LINE__);
        }
      }
    }

    /* **THE CLIENT OWNS THE TARGET AND HANDS THE LIBRARY A POINTER TO IT.** A case's plan asks for
     * `Resource::Surface` because a host presents; this host is headless, so its surface is a texture
     * of its own -- which is the same declaration a windowed host makes with a swapchain image, and
     * the library cannot tell the two apart. Without one the present pass attaches nothing, and a pass
     * with no attachment is where this test found its own missing half. */
    SDL_GPUTextureCreateInfo wanted{};
    wanted.type = SDL_GPU_TEXTURETYPE_2D;
    wanted.format = renderer.SurfaceFormat();
    wanted.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    wanted.width = (Uint32)held.WidthPx();
    wanted.height = (Uint32)held.HeightPx();
    wanted.layer_count_or_depth = 1;
    wanted.num_levels = 1;
    SDL_GPUTexture *surface = SDL_CreateGPUTexture(renderer.Device(), &wanted);
    if (surface == nullptr) {
      ++refused;
      Checked(false, "the client's own surface is made", one.Name.c_str(), __FILE__, __LINE__);
      continue;
    }
    renderer.PresentInto(surface);

    if (!held.Draw(renderer, why)) {
      SDL_ReleaseGPUTexture(renderer.Device(), surface);
      ++refused;
      Checked(false, "the case the browser shows draws", (one.Name + ": " + why).c_str(), __FILE__,
              __LINE__);
      continue;
    }
    std::vector<uint8_t> rgba;
    const outshine::Render::ReadState read = renderer.ReadPixels(rgba);
    renderer.PresentInto(nullptr);
    SDL_ReleaseGPUTexture(renderer.Device(), surface);
    if (read != outshine::Render::ReadState::Ready) {
      ++refused;
      Checked(false, "the frame comes back off the device", one.Name.c_str(), __FILE__, __LINE__);
      continue;
    }
    const size_t varying = Varying(rgba);
    if (varying == 0) {
      ++blank;
      Checked(false, "the case the browser shows reaches a texel",
              (one.Name + ": every pixel of its frame is the same colour, which is what a browser "
                          "showing nothing looks like")
                  .c_str(),
              __FILE__, __LINE__);
      continue;
    }
    ++drew;
    std::printf("  %-42s %4d x %-4d  %zu varying px\n", one.Name.c_str(), held.WidthPx(),
                held.HeightPx(), varying);
  }

  std::printf("NOTE cases the tree declares = %zu\n", cases.size());
  std::printf("NOTE render cases the browser drew = %d\n", drew);
  std::printf("NOTE render cases that declined by their own declaration = %d\n", declined);
  std::printf("NOTE document cases, drawn by the UI engine and not this path = %d\n", documents);
  std::printf("NOTE cases whose preparation has not run = %d\n", unprepared);
  std::printf("NOTE cases that refused or came back blank = %d\n", refused + blank);
  CHECK(drew > 0, "the browser drew at least one case, so the counts above are about work");
  return Report();
}
