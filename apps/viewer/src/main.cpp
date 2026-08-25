#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "Face.h"
#include "Log.h"
#include "GlyphSheet.h"
#include "Layout.h"
#include "Paint.h"
#include "Pointer.h"
#include "RenderCase.h"
#include "Live.h"
#include "Renderer.h"
#include "Script.h"

namespace Ui = outshine::Ui;
namespace View = outshine::Viewer;

namespace {

constexpr int kWidth = 1600;
constexpr int kHeight = 900;

constexpr double kBrowserFill = 0.92;


struct Browser final : outshine::Script::Host {
  std::vector<View::Listed> Cases;
  View::Showing Showing;
  View::SheetFont Face;
  int WidthPx = kWidth;
  int HeightPx = kHeight;

  [[nodiscard]] outshine::Clients::Declaration Declare(void) const {
    outshine::Clients::Declaration out;
    out.SurfaceWidthPx = WidthPx;
    out.SurfaceHeightPx = HeightPx;
    out.Surfaces = Surfaces();
    out.AtlasRgba = Sheet_.data();
    out.AtlasWidthPx = View::AtlasWidth();
    out.AtlasHeightPx = View::AtlasHeight();
    const View::Listed *one = Selected();
    if (one != nullptr && one->Ready && !one->Document && !one->Script) {
      out.Stands = View::EntryPath(one->Prepared);
      out.Fill = kBrowserFill;

      out.KeyLux = 3.0;
      out.KeyElevationDeg = 35.0;
      out.KeyBearingDeg = -35.0;
      out.Environment[0] = out.Environment[1] = out.Environment[2] = 0.35;
      const View::Region where = View::StageRegion(WidthPx, HeightPx);
      out.PictureLeftFrac = where.X;
      out.PictureTopFrac = where.Y;
      out.PictureWidthFrac = where.Width;
      out.PictureHeightFrac = where.Height;
    }
    return out;
  }

  [[nodiscard]] std::vector<outshine::Clients::Shows> Surfaces(void) const {
    std::vector<outshine::Clients::Shows> out;
    outshine::Clients::Shows chrome;
    chrome.Markup = View::Declaration(Cases, Showing, WidthPx, HeightPx);
    out.push_back(std::move(chrome));

    const View::Listed *one = Selected();
    if (one == nullptr || (!one->Document && !one->Script)) { return out; }
    const double left = View::ColumnsWidth(WidthPx) / (double)WidthPx;
    const int w = PaneWidth(), h = HeightPx;
    if (w <= 0 || h <= 0) { return out; }

    bool found = false;
    const std::string entry = View::EntryOf(one->Prepared, found);
    outshine::Clients::Shows pane;
    pane.LeftFrac = left;
    pane.WidthFrac = 1.0 - left;
    if (one->Script) {

      outshine::Script::Program program;
      std::string why;
      const bool reads = found && program.Read(entry, why);
      const std::string token =
          program.Stopped().empty() ? std::string() : "token:" + program.Stopped();
      pane.Markup = View::Console(one->Name, found ? entry : "the case is not prepared",
                                  reads ? "READS -- " + std::to_string(program.NodeCount()) + " NODES"
                                        : "REFUSED: " + why,
                                  token.empty() ? nullptr : outshine::Script::WhyOutside(token), w, h);
    } else if (found) {
      pane.Markup = entry;
      pane.Style = View::LinkedSheets(one->Prepared);
    } else {
      return out;
    }
    out.push_back(std::move(pane));
    return out;
  }

  [[nodiscard]] const View::Listed *Selected(void) const {
    const std::vector<int> shown = View::Filtered(Cases, Showing);
    if (Showing.Selected < 0 || Showing.Selected >= (int)shown.size()) { return nullptr; }
    return &Cases[(size_t)shown[(size_t)Showing.Selected]];
  }

  [[nodiscard]] std::string Body(void) const {
    const View::Listed *one = Selected();
    return one != nullptr && one->Ready && !one->Document && !one->Script ? one->Prepared
                                                                         : std::string();
  }

  std::vector<uint8_t> Sheet_ = View::Sheet();

  [[nodiscard]] int PaneWidth(void) const { return WidthPx - (int)View::ColumnsWidth(WidthPx); }

  void Touched(const Ui::Touched &found) {
    if (found.Action.empty()) { return; }
    outshine::Script::Program handler;
    std::string why;
    if (!handler.Read(found.Action, why) || !handler.Run(*this, why)) {
      Showing.Note = "THE DECLARED ACTION WAS REFUSED";
    }
  }

  [[nodiscard]] outshine::Script::Value Global(std::string_view name) override {
    if (name == "select") { return outshine::Script::Value::OfRef(kSelect); }
    if (name == "suite") { return outshine::Script::Value::OfRef(kSuite); }
    if (name == "scroll") { return outshine::Script::Value::OfRef(kScroll); }
    return {};
  }
  [[nodiscard]] bool Call(const outshine::Script::Value &callee,
                          const outshine::Script::Value *args, size_t count,
                          outshine::Script::Value &out) override {
    if (callee.What != outshine::Script::Kind::Ref || count != 1) { return false; }
    out = outshine::Script::Value();
    switch (callee.Ref) {
      case kSelect:
        Showing.Selected = (int)args[0].Number;
        return true;
      case kSuite: {
        const std::string wanted = args[0].AsText();
        Showing.Suite = Showing.Suite == wanted ? std::string() : wanted;
        Showing.Selected = -1;
        Showing.ScrolledRows = 0;
        return true;
      }
      case kScroll:
        Scrolled((int)args[0].Number);
        return true;
      default: break;
    }
    return false;
  }

  void Scrolled(int rows) {
    const int shown = (int)View::Filtered(Cases, Showing).size();
    const int most = std::max(0, shown - View::RowsThatFit(HeightPx));
    Showing.ScrolledRows = std::clamp(Showing.ScrolledRows + rows, 0, most);
  }

  static constexpr int kSelect = 1;
  static constexpr int kSuite = 2;
  static constexpr int kScroll = 3;

  void Recount(void) {
    int ready = 0, documents = 0;
    const std::vector<int> shown = View::Filtered(Cases, Showing);
    for (const int at : shown) {
      ready += Cases[(size_t)at].Ready ? 1 : 0;
      documents += Cases[(size_t)at].Document ? 1 : 0;
    }
    Showing.Note = std::to_string(shown.size()) + " CASES / " + std::to_string(ready) +
                   " PREPARED / " + std::to_string(documents) + " DOCUMENTS";
  }
};

class Spoken final : public outshine::LogSink {
public:
  void Write(double, outshine::LogLevel level, const char *, const char *tag,
             const char *event,
             std::span<const outshine::LogField> fields) override {
    if (level < outshine::LogLevel::Warn) { return; }
    std::fprintf(stderr, "%s %s", tag, event);
    for (const outshine::LogField &field : fields) {
      std::fprintf(stderr, " %s=%s", field.Key, field.Value.c_str());
    }
    std::fprintf(stderr, "\n");
  }
};

struct Distribution {
  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0, MaxMs = 0.0;
};

[[nodiscard]] Distribution Over(std::vector<double> &samples) {
  Distribution out;
  if (samples.empty()) { return out; }
  std::sort(samples.begin(), samples.end());
  const auto at = [&samples](double fraction) {
    return samples[(size_t)(fraction * (double)(samples.size() - 1) + 0.5)];
  };
  out.P50Ms = at(0.50);
  out.P95Ms = at(0.95);
  out.P99Ms = at(0.99);
  out.MaxMs = samples.back();
  return out;
}



int Windowed(Browser &browser, int frames) {
  Spoken spoken;
  outshine::Log::SetSink(&spoken);
  outshine::Render::Renderer renderer;

  std::unique_ptr<outshine::Clients::Live> live;
  std::string error;
  int broken = 0;
  int imageless = 0;
  std::string brokenWhy;

  SDL_Window *window = SDL_CreateWindow("outshine cases", browser.WidthPx, browser.HeightPx,
                                        SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    std::printf("the window was refused: %s\n", SDL_GetError());
    return 1;
  }

  bool running = true;
  bool restand = true, redeclare = false;
  std::string body;
  int drawn = 0;
  std::vector<double> advanceMs;
  while (running) {
    if (frames >= 0 && drawn >= frames) { break; }
    ++drawn;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) { running = false; }
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) { running = false; }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        browser.Touched(live ? live->Under(event.button.x, event.button.y) : Ui::Touched{});
        redeclare = true;
      }
      if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        browser.Scrolled(-(int)event.wheel.y);
        redeclare = true;
      }
      if (event.type == SDL_EVENT_WINDOW_RESIZED) {
        browser.WidthPx = event.window.data1;
        browser.HeightPx = event.window.data2;
        restand = true;
      }
    }
    if (redeclare && browser.Body() != body) { restand = true; }

    if (restand) {
      browser.Recount();
      outshine::Clients::Declaration declaration = browser.Declare();
      declaration.Presents = true;

      if (!outshine::Clients::Live::Open(renderer, std::move(declaration), &browser.Face, live,
                                         error)) {
        const std::string refusedBy = error;

        std::printf("DECLINED %s: %s\n", browser.Body().c_str(), refusedBy.c_str());
        outshine::Clients::Declaration bare = browser.Declare();
        bare.Presents = true;
        bare.Stands.clear();
        if (!outshine::Clients::Live::Open(renderer, std::move(bare), &browser.Face, live, error)) {
          std::printf("the browser could not stand up its own interface: %s\n", error.c_str());
          return 1;
        }
        browser.Showing.Note = "DECLINED " + refusedBy;
        if (!live->Redeclare(browser.Surfaces(), error)) {
          browser.Showing.Note = "THE DECLARATION WAS REFUSED";
        }
      }
      if (live) {
        auto standing = renderer.ShowOn(window);
        if (!standing) {
          std::printf("the window was refused: %.*s\n", (int)standing.error().size(),
                      standing.error().data());
          return 1;
        }
      }
      body = browser.Body();
      restand = false;
      redeclare = false;
    } else if (redeclare) {
      browser.Recount();
      if (live && !live->Redeclare(browser.Surfaces(), error)) {
        browser.Showing.Note = "THE DECLARATION WAS REFUSED";
      }
      redeclare = false;
    }
    if (!live) { break; }

    auto presented = renderer.PresentFrame();
    if (!presented) {
      ++broken;
      if (brokenWhy.empty()) { brokenWhy = std::string(presented.error()); }
    } else if (!presented->has_value()) {
      ++imageless;
    } else {
      const outshine::Render::Renderer::Shown shown = **presented;
      if (shown.WidthPx != browser.WidthPx || shown.HeightPx != browser.HeightPx) {
        browser.WidthPx = shown.WidthPx;
        browser.HeightPx = shown.HeightPx;
        restand = true;
      }
      const auto began = std::chrono::steady_clock::now();
      if (!live->Advance(error)) { browser.Showing.Note = "REFUSED " + error; }
      advanceMs.push_back(
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
    }
  }
  renderer.WaitForGpu();
  if (imageless > 0) {
    std::printf("%d frame(s) had no image to draw into -- a minimised window is not an error\n",
                imageless);
  }
  if (broken > 0) {
    std::printf("%d frame(s) the door refused: %s\n", broken, brokenWhy.c_str());
  }
  if (frames >= 0) {
    const Distribution advancing = Over(advanceMs);
    std::printf("advance p50 %.3f p95 %.3f p99 %.3f max %.3f ms over %zu frames\n",
                advancing.P50Ms, advancing.P95Ms, advancing.P99Ms, advancing.MaxMs, advanceMs.size());
  }
  renderer.StopShowing();
  SDL_DestroyWindow(window);
  std::printf("the browser drew %d frame(s) into a window it owns\n", drawn);
  return 0;
}


}

int main(int argc, char **argv) {

  int frames = -1;
  std::string opening;
  bool windowed = false;
  for (int at = 1; at < argc; ++at) {
    if (std::strcmp(argv[at], "--window") == 0) { windowed = true; }
    if (std::strcmp(argv[at], "--show") == 0 && at + 1 < argc) { opening = argv[at + 1]; }
    if (std::strcmp(argv[at], "--frames") == 0 && at + 1 < argc) {
      frames = std::atoi(argv[at + 1]);
      windowed = true;
    }
  }
  if (windowed) {
    Browser browser;
    browser.Cases = View::Cases();
    browser.Recount();
    return Windowed(browser, frames);
  }

  std::printf("outshine-viewer -- shows a scenario\n\n"
              "  --window        open a window\n"
              "  --show PATH     the scenario to show\n"
              "  --frames N      stop after N frames\n\n"
              "board:1880: it becomes a GENERIC scenario viewer -- it loads a scenario the way\n"
              "any client does and contributes only its own face. Today it still browses the\n"
              "tree's own declared cases.\n");
  return 0;
}
