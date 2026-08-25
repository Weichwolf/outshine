#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "Check.h"
#include "Chrome.h"
#include "Log.h"
#include "GlyphSheet.h"
#include "Layout.h"
#include "Paint.h"
#include "Pointer.h"
#include "RenderCase.h"
#include "Live.h"
#include "Renderer.h"
#include "Script.h"

using namespace outshine::Test;
namespace Ui = outshine::Ui;
namespace View = outshine::Viewer;

namespace {

constexpr int kWidth = 1600;
constexpr int kHeight = 900;

constexpr double kBrowserFill = 0.92;

constexpr double kFrameBudgetMs = 16.67;
constexpr double kChromeShare = 0.10;

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

int Covering(const Ui::Painting &painting, double x, double y) {
  int found = -1;
  for (int at = 0; at < (int)painting.Quads().size(); ++at) {
    const Ui::Quad &quad = painting.Quads()[(size_t)at];
    if (x >= quad.X && x < quad.X + quad.Width && y >= quad.Y && y < quad.Y + quad.Height) {
      found = at;
    }
  }
  return found;
}

int Windowed(Browser &browser, int frames);

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

  if (!opening.empty()) {
    for (int at = 0; at < (int)browser.Cases.size(); ++at) {
      if (browser.Cases[(size_t)at].Name.find(opening) == std::string::npos) { continue; }
      browser.Showing.Suite = browser.Cases[(size_t)at].Suite;
      const std::vector<int> shown = View::Filtered(browser.Cases, browser.Showing);
      for (int row = 0; row < (int)shown.size(); ++row) {
        if (shown[(size_t)row] == at) { browser.Showing.Selected = row; }
      }
      break;
    }
  }
    return Windowed(browser, frames);
  }

  Ui::Markup tree;
  Ui::Stylesheet sheet;
  Ui::Layout placed;
  Ui::Painting painted;
  const auto lay = [&](const outshine::Clients::Shows &shows, const View::SheetFont &font,
                       int widthPx, int heightPx, std::string &why) {
    tree = Ui::Markup();
    sheet = Ui::Stylesheet();
    return tree.Read(shows.Markup, why) && (sheet.Read(Ui::UserAgentSheet()), true) &&
           (shows.Style.empty() ? true : (sheet.Read(shows.Style), true)) &&
           (sheet.Read(tree.StyleText()), true) &&
           placed.Build(tree, sheet, shows.WidthFrac * widthPx, shows.HeightFrac * heightPx, font,
                        why) &&
           painted.Build(placed, font, why);
  };

  Browser browser;
  browser.Cases = View::Cases();
  browser.Recount();
  CHECK(!browser.Cases.empty(), "the tree declares cases, so this browser has something to show");
  std::printf("NOTE cases the tree declares = %zu across %zu suites\n", browser.Cases.size(),
              View::Suites(browser.Cases).size());

  std::string error;
  CHECK(lay(browser.Surfaces()[0], browser.Face, kWidth, kHeight, error),
        "the browser's own declaration reads, lays out and paints");
  if (!painted.Quads().empty()) {
    std::printf("NOTE the browser is %zu boxes and %zu rectangles at %dx%d\n",
                placed.Boxes().size(), painted.Quads().size(), kWidth, kHeight);
  } else {
    std::printf("       %s\n", error.c_str());
    return Report();
  }

  {
    int outside = 0;
    for (const Ui::Quad &quad : painted.Quads()) {
      const bool inside = quad.X >= 0 && quad.Y >= 0 && quad.X + quad.Width <= kWidth &&
                          quad.Y + quad.Height <= kHeight;
      outside += inside ? 0 : 1;
    }
    CHECK(outside == 0, "every rectangle the browser declares is inside the surface it declared");
    CHECK(painted.QuadsBeyondTheBound() == 0,
          "and it asks for no rectangle past the library's bound");
  }

  {
    const int overStage = Covering(painted, kWidth - 40, kHeight / 2);
    CHECK(overStage < 0,
          "nothing is painted over the stage, so the picture the library drew is what is seen there");
    const int overPanel = Covering(painted, 20, 400);
    CHECK(overPanel >= 0, "and the panel beside it is painted");
  }

  {

    const double em = View::RootEmPx(kHeight);
    const int firstRow = (int)(1.8 * em + 0.65 * em);
    const int caseColumn = (int)(View::ColumnsWidth(kWidth) - 0.1 * kWidth);
    browser.Touched(Ui::Under(placed, tree, caseColumn, firstRow));
    CHECK(browser.Showing.Selected == 0, "a pointer on the first row of the case column selects it");
    CHECK(lay(browser.Surfaces()[0], browser.Face, kWidth, kHeight, error),
          "and the declaration recomposes with the selection in it");

    const std::vector<std::string> suites = View::Suites(browser.Cases);
    CHECK(!suites.empty(), "the corpus column is derived from the listing");
    browser.Touched(Ui::Under(placed, tree, 20, 1.8 * em + 1.3 * em + 0.65 * em));
    CHECK(!browser.Showing.Suite.empty(), "a pointer on a corpus filters to it");
    CHECK(browser.Showing.Selected == -1, "and clears a selection that belonged to another listing");
    const size_t narrowed = View::Filtered(browser.Cases, browser.Showing).size();
    CHECK(narrowed > 0 && narrowed < browser.Cases.size(),
          "the filter admits some of the tree and not all of it");
    browser.Recount();
    CHECK(lay(browser.Surfaces()[0], browser.Face, kWidth, kHeight, error),
          "and the filtered declaration composes");

    browser.Touched(Ui::Under(placed, tree, 20, firstRow));
    CHECK(browser.Showing.Suite.empty(), "and the row above them all is the way back to the whole tree");
    CHECK(View::Filtered(browser.Cases, browser.Showing).size() == browser.Cases.size(),
          "which admits every case the tree declares");
  }

  {
    browser.Showing.Suite.clear();
    browser.Showing.Selected = -1;
    browser.Showing.ScrolledRows = 0;
    browser.Recount();
    CHECK(lay(browser.Surfaces()[0], browser.Face, kWidth, kHeight, error),
          "the unfiltered listing composes");
    const double em = View::RootEmPx(kHeight);
    const int row = (int)(1.8 * em + 0.65 * em);
    const int column = (int)(View::ColumnsWidth(kWidth) - 0.1 * kWidth);
    const int before = Covering(painted, column, row);
    const double firstY = before >= 0 ? painted.Quads()[(size_t)before].Y : -1.0;

    browser.Scrolled(1);
    CHECK(browser.Showing.ScrolledRows == 1, "one notch scrolls one row");
    CHECK(lay(browser.Surfaces()[0], browser.Face, kWidth, kHeight, error),
          "the scrolled listing composes");
    const int after = Covering(painted, column, row);
    const double nextY = after >= 0 ? painted.Quads()[(size_t)after].Y : -1.0;
    CHECK(before >= 0 && after >= 0, "a row is under the pointer before and after the scroll");
    CHECK(firstY == nextY,
          "the row under the pointer is at the same place -- what moved is WHICH row is there, which "
          "is what a clip and an offset do and what a resized list would not");

    browser.Scrolled(-9999);
    CHECK(browser.Showing.ScrolledRows == 0, "and the scroll is bounded at its own top");
    browser.Scrolled(9999);
    const int shown = (int)View::Filtered(browser.Cases, browser.Showing).size();
    CHECK(browser.Showing.ScrolledRows == std::max(0, shown - View::RowsThatFit(kHeight)),
          "and at its own end, which is the listing's length less what fits");
    browser.Showing.ScrolledRows = 0;
  }

  {
    constexpr int kWarmup = 20;
    constexpr int kFrames = 200;
    std::vector<double> samples;
    samples.reserve(kFrames);
    size_t quads = 0;
    for (int frame = 0; frame < kWarmup + kFrames; ++frame) {
      browser.Showing.ScrolledRows = frame % 40;
      browser.Recount();
      const auto began = std::chrono::steady_clock::now();
      const bool composed = lay(browser.Surfaces()[0], browser.Face, kWidth, kHeight, error);
      const double ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
      if (!composed) { break; }
      if (frame >= kWarmup) {
        samples.push_back(ms);
        quads = painted.Quads().size();
      }
    }
    const Distribution cost = Over(samples);
    const double allowed = kFrameBudgetMs * kChromeShare;
    std::printf("NOTE population = %zu frames, the listing re-declared and re-laid every one, "
                "%zu rectangles at %dx%d\n",
                samples.size(), quads, kWidth, kHeight);
    std::printf("NOTE chrome cpu ms  p50 %.4f  p95 %.4f  p99 %.4f  max %.4f\n", cost.P50Ms,
                cost.P95Ms, cost.P99Ms, cost.MaxMs);
    std::printf("NOTE budget %.2f ms, share [SET] %.0f%%, allowed %.4f ms, p99 uses %.1f%% of it\n",
                kFrameBudgetMs, kChromeShare * 100.0, allowed, 100.0 * cost.P99Ms / allowed);
    CHECK(cost.P99Ms < allowed,
          "the browser's own chrome fits inside the share of the frame it was given, at p99");
    browser.Showing.ScrolledRows = 0;
  }

  {
    outshine::Render::Renderer renderer;
    browser.Showing = View::Showing();
    browser.Recount();
    std::unique_ptr<outshine::Clients::Live> live;
    const bool stood =
        outshine::Clients::Live::Open(renderer, browser.Declare(), &browser.Face, live, error);
    CHECK(stood, "the browser's declaration stands a scenario up on the device");
    if (stood) {
      CHECK(live->Advance(error), "and the scenario draws a frame with nobody telling it what is in it");
      std::vector<uint8_t> rgba;
      const bool read = renderer.ReadPixels(rgba) == outshine::Render::ReadState::Ready;
      CHECK(read, "the frame comes back off the device");
      if (read && rgba.size() >= (size_t)kWidth * kHeight * 4u) {
        const auto codeAt = [&rgba](int x, int y, int channel) {
          return (int)rgba[(((size_t)y * kWidth) + (size_t)x) * 4u + (size_t)channel];
        };

        const int r = codeAt(300, 400, 0), g = codeAt(300, 400, 1), b = codeAt(300, 400, 2);
        std::printf("NOTE the case column reads back as %d %d %d, declared 18 22 27\n", r, g, b);
        CHECK(std::abs(r - 0x12) <= 1 && std::abs(g - 0x16) <= 1 && std::abs(b - 0x1b) <= 1,
              "a panel declared #12161b comes back as #12161b -- the declaration IS the colour, "
              "which is what the transfer would break if the two ever met twice");
      }
    }
  }

  {
    const std::vector<int> all = View::Filtered(browser.Cases, browser.Showing);
    int documentAt = -1, scriptAt = -1;
    for (int at = 0; at < (int)all.size(); ++at) {
      const View::Listed &one = browser.Cases[(size_t)all[(size_t)at]];
      if (one.Document && documentAt < 0) { documentAt = at; }
      if (one.Script && scriptAt < 0) { scriptAt = at; }
    }
    CHECK(documentAt >= 0 && scriptAt >= 0, "the tree declares a document case and a script case");

    browser.Showing.Selected = -1;
    CHECK(lay(browser.Surfaces()[0], browser.Face, kWidth, kHeight, error),
          "with nothing selected the browser composes");
    CHECK(browser.Surfaces().size() == 1, "and declares one surface -- its own columns");

    browser.Showing.Selected = documentAt;
    CHECK(browser.Surfaces().size() == 2,
          "a document case puts rectangles in the pane -- drawn by the engine that drew the columns "
          "around them, which is the property this browser exists to hold");

    browser.Showing.Selected = scriptAt;
    CHECK(browser.Surfaces().size() == 2,
          "a program is shown as a console: its own text and what this engine made of it, because a "
          "case with no picture still has something to say");
    browser.Showing.Selected = -1;
    CHECK(browser.Surfaces().size() == 1, "and the browser returns to nothing selected");
  }

  {
    outshine::Render::Renderer renderer;
    std::unique_ptr<outshine::Clients::Live> live;
    int withBody = -1;
    for (int at = 0; at < (int)browser.Cases.size(); ++at) {
      const View::Listed &one = browser.Cases[(size_t)at];
      if (one.Ready && !one.Document && !one.Script) { withBody = at; break; }
    }
    CHECK(withBody >= 0, "the tree declares a prepared case with a body in it");
    if (withBody >= 0) {
      browser.Showing = View::Showing();
      browser.Showing.Suite = browser.Cases[(size_t)withBody].Suite;
      const std::vector<int> shown = View::Filtered(browser.Cases, browser.Showing);
      for (int row = 0; row < (int)shown.size(); ++row) {
        if (shown[(size_t)row] == withBody) { browser.Showing.Selected = row; }
      }
      browser.Recount();
      const bool stood =
          outshine::Clients::Live::Open(renderer, browser.Declare(), &browser.Face, live, error);
      CHECK(stood, "a case with a body stands a scenario up");
      std::vector<uint8_t> withIt, without;
      if (stood && live->Advance(error)) {
        (void)(renderer.ReadPixels(withIt) == outshine::Render::ReadState::Ready);
      }

      browser.Showing = View::Showing();
      browser.Recount();
      const bool bare =
          outshine::Clients::Live::Open(renderer, browser.Declare(), &browser.Face, live, error);
      CHECK(bare, "and a case with none stands one up in its place");
      if (bare && live->Advance(error)) {
        (void)(renderer.ReadPixels(without) == outshine::Render::ReadState::Ready);
      }
      const size_t pane = (size_t)(View::ColumnsWidth(kWidth) + 40.0);
      size_t drawn = 0;
      if (withIt.size() == without.size() && !withIt.empty()) {
        for (size_t y = 100; y < (size_t)kHeight; y += 5) {
          for (size_t x = pane; x < (size_t)kWidth; x += 5) {
            const size_t at = ((y * (size_t)kWidth) + x) * 4u;
            drawn += without[at] != 0 || without[at + 1] != 0 || without[at + 2] != 0 ? 1 : 0;
          }
        }
        size_t before = 0;
        for (size_t y = 100; y < (size_t)kHeight; y += 5) {
          for (size_t x = pane; x < (size_t)kWidth; x += 5) {
            const size_t at = ((y * (size_t)kWidth) + x) * 4u;
            before += withIt[at] != 0 || withIt[at + 1] != 0 || withIt[at + 2] != 0 ? 1 : 0;
          }
        }
        std::printf("NOTE samples with ink over the pane: with a body %zu, with none %zu\n", before,
                    drawn);
        CHECK(before > 0, "the body was drawn where the browser said it would be");
      }
      CHECK(drawn == 0,
            "and nothing of it survives into the scenario that replaced it -- a body outliving the "
            "scenario that placed it is drawn behind everything shown afterwards");
    }
  }

  {
    const std::vector<uint8_t> face = View::Sheet();
    size_t ink = 0;
    for (size_t at = 3; at < face.size(); at += 4) { ink += face[at] > 0 ? 1 : 0; }
    CHECK(ink > 0, "the browser's own face carries ink");
    CHECK(ink < face.size() / 4,
          "and it is not a filled sheet -- a face whose every texel is ink is Ahem with more steps");
    const Ui::Glyph glyph = browser.Face.Shape(U'A', 16.0);
    CHECK(glyph.Drawn && glyph.WidthPx < glyph.AdvancePx,
          "a glyph is narrower than the advance that follows it, which is where the spacing lives");
    CHECK(!browser.Face.Shape(U' ', 16.0).Drawn, "and a space draws nothing");
  }

  return Report();
}

namespace {

int Windowed(Browser &browser, int frames) {
  Spoken spoken;
  outshine::Log::SetSink(&spoken);
  outshine::Render::Renderer renderer;

  std::unique_ptr<outshine::Clients::Live> live;
  std::string error;
  int unshown = 0;
  std::string unshownWhy;

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
          std::printf("the window was refused: %s\n", standing.error().c_str());
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
      ++unshown;
      if (unshownWhy.empty()) { unshownWhy = std::move(presented).error(); }
    } else {
      const outshine::Render::Renderer::Shown shown = *presented;
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
  if (unshown > 0) {
    std::printf("%d frame(s) never reached the surface: %s\n", unshown, unshownWhy.c_str());
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
