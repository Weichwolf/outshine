#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <string>

#include <Outshine.h>

#include "Face.h"

namespace {




struct Asked {
  std::string Scenario;
  std::string Assets;
  std::string Shipped = "src/assets";
  std::string Cases = "test";
  std::string Case;
  std::string Into;
  std::string Prepared;
  int WidthPx = 1280;
  int HeightPx = 720;
  long Frames = 0;
  bool Windowed = true;
};

void Usage() {
  std::printf(
      "outshine-viewer -- shows a scenario\n"
      "\n"
      "  --show PATH      the scenario to show\n"
      "  --assets DIR     where its asset URIs resolve\n"
      "  --shipped DIR    where outshine's own data is (default src/assets)\n"
      "  --size WxH       the surface to open (default 1280x720)\n"
      "  --cases DIR      where the case manifests are (default test)\n"
      "  --prepared DIR   where their prepared subjects are\n"
      "  --case NAME      open with this case selected\n"
      "  --frames N       stop after N frames\n"
      "  --into DIR       keep a still of each frame here\n"
      "  --headless       stand it up without opening a window\n");
}

}

class Browser final : public outshine::Host {
public:
  Browser(const std::vector<outshine::Viewer::Listed> &cases, outshine::Viewer::Showing at,
          bool moved)
      : Cases_(cases), At_(std::move(at)), Moved_(moved) {}

  [[nodiscard]] outshine::Surface Face(int widthPx, int heightPx) {
    outshine::Surface face;
    face.Document = outshine::Viewer::Declaration(Cases_, At_, widthPx, heightPx);
    face.Style = outshine::Viewer::Style();
    face.Z = 100;
    return face;
  }

  void Steers(outshine::Engine *engine) { Engine_ = engine; }

  [[nodiscard]] bool Calls(std::string_view name, std::span<const outshine::Argument> args) override {
    if (name == "next-view" && Engine_ != nullptr) {
      const std::vector<outshine::View> &named = Engine_->Declared().Views;
      if (named.size() < 2) { return false; }
      Viewing_ = (Viewing_ + 1) % named.size();
      return Engine_->Takes(named[Viewing_].Id);
    }
    if (name == "select" && args.size() == 1 &&
        args[0].Is == outshine::Argument::Kind::Number) {
      At_.Selected = (int)args[0].Number;
      const std::vector<int> shown = outshine::Viewer::Filtered(Cases_, At_);
      if (At_.Selected >= 0 && At_.Selected < (int)shown.size()) {
        At_.Note = Cases_[(size_t)shown[(size_t)At_.Selected]].Name;
      }
      Moved_ = true;
      return true;
    }
    if (name == "suite" && args.size() == 1 && args[0].Is == outshine::Argument::Kind::Text) {
      At_.Suite = std::string(args[0].Text);
      At_.Selected = -1;
      At_.ScrolledRows = 0;
      Moved_ = true;
      return true;
    }
    return false;
  }

  [[nodiscard]] bool Moved() const { return Moved_; }

  void Settled() { Moved_ = false; }

  [[nodiscard]] const outshine::Viewer::Listed *Picked() const {
    const std::vector<int> shown = outshine::Viewer::Filtered(Cases_, At_);
    if (At_.Selected < 0 || At_.Selected >= (int)shown.size()) { return nullptr; }
    return &Cases_[(size_t)shown[(size_t)At_.Selected]];
  }

  void Noted(std::string said) { At_.Note = std::move(said); }

private:
  const std::vector<outshine::Viewer::Listed> &Cases_;
  outshine::Viewer::Showing At_;
  bool Moved_ = false;
  outshine::Engine *Engine_ = nullptr;
  size_t Viewing_ = 0;
};

int main(int argc, char **argv) {
  Asked asked;
  for (int at = 1; at < argc; ++at) {
    const char *said = argv[at];
    const bool wants = at + 1 < argc;
    if (std::strcmp(said, "--help") == 0) {
      Usage();
      return 0;
    }
    if (std::strcmp(said, "--headless") == 0) {
      asked.Windowed = false;
    } else if (std::strcmp(said, "--show") == 0 && wants) {
      asked.Scenario = argv[++at];
    } else if (std::strcmp(said, "--assets") == 0 && wants) {
      asked.Assets = argv[++at];
    } else if (std::strcmp(said, "--shipped") == 0 && wants) {
      asked.Shipped = argv[++at];
    } else if (std::strcmp(said, "--cases") == 0 && wants) {
      asked.Cases = argv[++at];
    } else if (std::strcmp(said, "--prepared") == 0 && wants) {
      asked.Prepared = argv[++at];
    } else if (std::strcmp(said, "--case") == 0 && wants) {
      asked.Case = argv[++at];
    } else if (std::strcmp(said, "--into") == 0 && wants) {
      asked.Into = argv[++at];
    } else if (std::strcmp(said, "--frames") == 0 && wants) {
      asked.Frames = std::atol(argv[++at]);
    } else if (std::strcmp(said, "--size") == 0 && wants) {
      const char *size = argv[++at];
      const char *by = std::strchr(size, 'x');
      if (by == nullptr) {
        Usage();
        return 2;
      }
      asked.WidthPx = std::atoi(size);
      asked.HeightPx = std::atoi(by + 1);
    } else {
      std::printf("outshine-viewer: '%s' is not an option it knows\n", said);
      Usage();
      return 2;
    }
  }
  if (asked.Scenario.empty()) {
    Usage();
    return 2;
  }
  if (asked.Prepared.empty()) {
    const char *tmp = std::getenv("TMPDIR");
    std::string root = tmp == nullptr || tmp[0] == '\0' ? "/tmp" : tmp;
    while (root.size() > 1 && root.back() == '/') { root.pop_back(); }
    asked.Prepared = root + "/outshine-prepared";
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("REFUSED SDL did not start: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = nullptr;
  if (asked.Windowed) {
    window = SDL_CreateWindow("outshine", asked.WidthPx, asked.HeightPx, 0);
    if (window == nullptr) {
      std::printf("REFUSED the window did not open: %s\n", SDL_GetError());
      SDL_Quit();
      return 1;
    }
  }
  outshine::Engine engine;
  engine.Under(outshine::Roots{asked.Assets, asked.Shipped, "/tmp/outshine-viewer-cache", false});
  const bool standing = window != nullptr
                            ? engine.DrawsInto(window)
                            : engine.DrawsInto(outshine::Extent{asked.WidthPx, asked.HeightPx});
  if (!standing) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    if (window != nullptr) { SDL_DestroyWindow(window); }
    SDL_Quit();
    return 1;
  }

  if (!engine.Read(asked.Scenario)) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    SDL_Quit();
    return 1;
  }
  outshine::Scenario showing = engine.Declared();
  const std::vector<outshine::Viewer::Listed> cases =
      outshine::Viewer::Cases(asked.Cases, asked.Prepared);
  Browser browsing{cases, {}, false};
  if (!asked.Case.empty()) {
    const std::vector<int> shown = outshine::Viewer::Filtered(cases, outshine::Viewer::Showing{});
    for (size_t at = 0; at < shown.size(); ++at) {
      if (cases[(size_t)shown[at]].Name != asked.Case) { continue; }
      const outshine::Argument picked{outshine::Argument::Kind::Number, (double)at, {}};
      (void)browsing.Calls("select", std::span<const outshine::Argument>(&picked, 1));
      break;
    }
  }
  std::string shownCase;
  std::printf("BROWSING %zu case(s) under %s\n", cases.size(), asked.Cases.c_str());
  engine.Offers(&browsing);
  browsing.Steers(&engine);

  {
    outshine::Scenario stands = showing;
    stands.Surfaces.push_back(browsing.Face(asked.WidthPx, asked.HeightPx));
    if (!engine.Declare(stands)) {
      std::printf("REFUSED %s\n", engine.Error().c_str());
      SDL_Quit();
      return 1;
    }
  }
  if (!engine.Assemble()) { std::printf("REFUSED %s\n", engine.Error().c_str()); }

  std::printf("SHOWING %s at %dx%d%s\n", asked.Scenario.c_str(), asked.WidthPx, asked.HeightPx,
              asked.Windowed ? "" : ", headless");

  long frames = 0;
  bool closing = false;
  Uint64 wasNs = SDL_GetTicksNS();
  while (!closing) {
    const Uint64 nowNs = SDL_GetTicksNS();
    const bool advanced = engine.Advance((double)(nowNs - wasNs) * 1.0e-9);
    wasNs = nowNs;
    if (!advanced) {
      browsing.Noted(engine.Error());
      shownCase.clear();
      showing = outshine::Scenario{};
      showing.Render.Declared = true;
      showing.Render.Frame = outshine::Extent{asked.WidthPx, asked.HeightPx};
      showing.Render.Fill = 0.15;
      outshine::Scenario alone = showing;
      alone.Surfaces.push_back(browsing.Face(asked.WidthPx, asked.HeightPx));
      if (!engine.Declare(alone) || !engine.Advance()) {
        std::printf("STOPPED the browser itself did not stand: %s\n", engine.Error().c_str());
        break;
      }
    }
    ++frames;
    for (SDL_Event event; SDL_PollEvent(&event);) {
      closing = closing || event.type == SDL_EVENT_QUIT;
      (void)engine.Handles(event);
    }
    if (browsing.Moved()) {
      const outshine::Viewer::Listed *const picked = browsing.Picked();
      outshine::Scenario stands = showing;
      bool stood = false;
      if (picked != nullptr && picked->Prepared != shownCase) {
        stood = true;
        const outshine::Viewer::Stands held = outshine::Viewer::StandOf(*picked);
        if (!held.Why.empty()) {
          browsing.Noted(held.Why);
        } else {
          shownCase = picked->Prepared;
          engine.Under(outshine::Roots{held.Under, asked.Shipped, "/tmp/outshine-viewer-cache", false});
          stands = outshine::Scenario{};
          stands.Render.Declared = true;
          stands.Render.Frame = outshine::Extent{asked.WidthPx, asked.HeightPx};
          stands.Render.Fill = 0.15;
          stands.Lit.Declared = true;
          stands.Lit.Key.Lux = 40000.0;
          stands.Lit.Key.ElevationDeg = 42.0;
          stands.Lit.Key.BearingDeg = 150.0;
          stands.Lit.Environment[0] = 0.20;
          stands.Lit.Environment[1] = 0.22;
          stands.Lit.Environment[2] = 0.26;
          stands.Render.Picture =
              outshine::Viewer::StageRegion(asked.WidthPx, asked.HeightPx);
          if (!held.Uri.empty()) {
            outshine::Asset shown;
            shown.Uri = held.Uri;
            shown.Kind = "gltf";
            stands.Assets.push_back(shown);
          } else {
            outshine::Surface page;
            page.Document = held.Document;
            page.Style = held.Style;
            page.Programme = held.Programme;
            page.Where = stands.Render.Picture;
            stands.Surfaces.push_back(page);
          }
          browsing.Noted(picked->Name);
          showing = stands;
        }
      }
      if (!stood) {
        std::vector<outshine::Surface> over = showing.Surfaces;
        over.push_back(browsing.Face(asked.WidthPx, asked.HeightPx));
        if (!engine.Shows(over)) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
        browsing.Settled();
        continue;
      }
      stands = showing;
      stands.Surfaces.push_back(browsing.Face(asked.WidthPx, asked.HeightPx));
      if (!engine.Declare(stands)) {
        browsing.Noted(engine.Error());
        shownCase.clear();
        showing = outshine::Scenario{};
        showing.Render.Declared = true;
        showing.Render.Frame = outshine::Extent{asked.WidthPx, asked.HeightPx};
        showing.Render.Fill = 0.15;
        outshine::Scenario alone = showing;
        alone.Surfaces.push_back(browsing.Face(asked.WidthPx, asked.HeightPx));
        if (!engine.Declare(alone)) {
          std::printf("STOPPED the browser itself did not stand: %s\n", engine.Error().c_str());
          break;
        }
      } else if (!engine.Assemble()) {
        browsing.Noted(engine.Error());
      }
      browsing.Settled();
    }
    if (!asked.Into.empty()) {
      char named[512];
      std::snprintf(named, sizeof named, "%s/frame%03ld.png", asked.Into.c_str(), frames);
      if (!engine.Capture(named)) {
        std::printf("REFUSED %s\n", engine.Error().c_str());
        break;
      }
    }
    if (asked.Frames > 0 && frames >= asked.Frames) { break; }
  }

  std::printf("SHOWED %ld frame(s)\n", frames);
  if (window != nullptr) { SDL_DestroyWindow(window); }
  SDL_Quit();
  return 0;
}
