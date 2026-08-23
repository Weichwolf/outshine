#include <algorithm>
#include <cmath>
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

constexpr int kSurfaceW = 1280;
constexpr int kSurfaceH = 720;

size_t Varying(const std::vector<uint8_t> &rgba) {
  if (rgba.size() < 4) { return 0; }
  size_t varying = 0;
  for (size_t at = 4; at + 3 < rgba.size(); at += 4) {
    const bool same = rgba[at] == rgba[0] && rgba[at + 1] == rgba[1] && rgba[at + 2] == rgba[2];
    varying += same ? 0 : 1;
  }
  return varying;
}

}

int main(void) {

  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::vector<View::Listed> cases = View::Cases();
  CHECK(!cases.empty(), "the tree declares cases");

  outshine::Render::Renderer renderer;
  const std::vector<uint8_t> sheetBytes = View::Sheet();
  View::Showing showing;
  Ui::Markup chromeTree;
  Ui::Stylesheet chromeSheet;
  Ui::Layout chromePlaced;
  Ui::Painting chromePainted;
  const View::SheetFont face;

  int drew = 0, declined = 0, documents = 0, unprepared = 0, blank = 0, refused = 0;
  int chromed = 0, shown = 0, reduced = 0, scripts = 0;
  std::string suite;
  for (const View::Listed &one : cases) {
    if (one.Suite != suite) {
      suite = one.Suite;
      std::printf("SUITE %s\n", suite.c_str());
    }
    if (one.Script) {
      ++scripts;
      continue;
    }
    if (one.Document) {

      ++documents;
      bool found = false;
      const std::string entry = View::EntryOf(one.Prepared, found);
      if (!found) {
        ++unprepared;
        continue;
      }
      Ui::Markup tree;
      Ui::Stylesheet sheet;
      Ui::Layout placed;
      Ui::Painting painted;
      std::string trouble;
      const bool reads = tree.Read(entry, trouble);
      if (reads) {
        sheet.Read(Ui::UserAgentSheet());
        sheet.Read(View::LinkedSheets(one.Prepared));
        sheet.Read(tree.StyleText());
      }
      const bool laid = reads && placed.Build(tree, sheet, 1280, 720, face, trouble);
      const bool drawn = laid && painted.Build(placed, face, trouble);
      if (drawn && !painted.Quads().empty()) {
        ++shown;
      } else {

        ++reduced;
      }
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

    if (!held.Start(renderer, why, {outshine::Render::Stage::Overlay}, kSurfaceW, kSurfaceH) ||
        !held.PoseAt(0, why)) {
      ++refused;
      Checked(false, "the case the browser shows starts", (one.Name + ": " + why).c_str(), __FILE__,
              __LINE__);
      continue;
    }
    if (!renderer.SetOverlayAtlas(sheetBytes.data(), View::AtlasWidth(), View::AtlasHeight(), why)) {
      ++refused;
      Checked(false, "the browser's face is uploaded", (one.Name + ": " + why).c_str(), __FILE__,
              __LINE__);
      continue;
    }
    const View::Region where = View::StageRegion(kSurfaceW, kSurfaceH);
    renderer.SetPictureRegion(where.X, where.Y, where.Width, where.Height,
                              (double)held.WidthPx() / (double)held.HeightPx());
    showing.Note = "SHOWING " + one.Name;
    const std::string document = View::Declaration(cases, showing, kSurfaceW, kSurfaceH);
    chromeTree = Ui::Markup();
    chromeSheet = Ui::Stylesheet();
    if (chromeTree.Read(document, why)) {
      chromeSheet.Read(Ui::UserAgentSheet());
      chromeSheet.Read(chromeTree.StyleText());
      if (chromePlaced.Build(chromeTree, chromeSheet, kSurfaceW, kSurfaceH, face, why) &&
          chromePainted.Build(chromePlaced, face, why)) {
        const std::vector<outshine::Render::OverlayQuad> ready =
            View::AsOverlay(chromePainted.Quads());
        if (!renderer.SetOverlay(ready.data(), ready.size(), why)) {
          Checked(false, "the browser's chrome reaches the library",
                  (one.Name + ": " + why).c_str(), __FILE__, __LINE__);
        } else if (!ready.empty()) {
          ++chromed;
        }
      }
    }

    SDL_GPUTextureCreateInfo wanted{};
    wanted.type = SDL_GPU_TEXTURETYPE_2D;
    wanted.format = renderer.SurfaceFormat();
    wanted.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    wanted.width = (Uint32)kSurfaceW;
    wanted.height = (Uint32)kSurfaceH;
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

    const auto codeAt = [&rgba](int x, int y, int channel) {
      const size_t at = (((size_t)y * (size_t)kSurfaceW) + (size_t)x) * 4u + (size_t)channel;
      return at < rgba.size() ? (int)rgba[at] : -1;
    };
    {

      const int r = codeAt(6, 400, 0), g = codeAt(6, 400, 1), b = codeAt(6, 400, 2);
      Checked(std::abs(r - 0x0f) <= 2 && std::abs(g - 0x13) <= 2 && std::abs(b - 0x17) <= 2,
              "the browser's own columns are over the case it shows",
              (one.Name + ": the corpus column reads " + std::to_string(r) + " " +
               std::to_string(g) + " " + std::to_string(b) + ", declared 15 19 23")
                  .c_str(),
              __FILE__, __LINE__);
    }

    const int outsideR = codeAt((int)View::ColumnsWidth(kSurfaceW) - 4, kSurfaceH / 2, 0);
    const int outsideG = codeAt((int)View::ColumnsWidth(kSurfaceW) - 4, kSurfaceH / 2, 1);
    Checked(outsideR >= 0 && outsideR < 40 && outsideG < 40,
            "the picture stops at its pane's edge",
            (one.Name + ": the texel left of the region reads " + std::to_string(outsideR) + " " +
             std::to_string(outsideG))
                .c_str(),
            __FILE__, __LINE__);
    ++drew;
    std::printf("  %-42s %4d x %-4d in %.0f x %.0f px of the pane  %zu varying px\n",
                one.Name.c_str(), held.WidthPx(), held.HeightPx(), renderer.PictureW(),
                renderer.PictureH(), varying);
  }

  std::printf("NOTE cases the tree declares = %zu\n", cases.size());
  std::printf("NOTE render cases the browser drew = %d\n", drew);
  std::printf("NOTE render cases that declined by their own declaration = %d\n", declined);
  std::printf("NOTE document cases the browser shows = %d of %d, %d this engine declines\n", shown,
              documents, reduced);
  std::printf("NOTE script cases, whose subject is a program and not a page = %d\n", scripts);
  std::printf("NOTE render cases the browser drew its own chrome over = %d\n", chromed);
  std::printf("NOTE cases whose preparation has not run = %d\n", unprepared);
  std::printf("NOTE cases that refused or came back blank = %d\n", refused + blank);
  CHECK(drew > 0, "the browser drew at least one case, so the counts above are about work");
  return Report();
}
