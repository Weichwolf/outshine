/* THE TEST-CASE BROWSER, AND ITS OWN CLAIMS (board:1447).
 *
 * **ONE PROGRAM, TWO TARGETS, AND THE CLIENT CHOOSES.** With no argument it owns a texture, draws
 * every frame into it, checks what came back and reports; with `--window` it owns a window, hands the
 * library the swapchain image it acquired, and loops until it is closed. The library cannot tell the
 * two apart -- it is handed a pointer to a surface either way -- which is what makes the headless arm
 * a real test of the windowed one rather than a rehearsal of it.
 *
 * **NOTHING HERE DECIDES A PIXEL.** The browser's appearance is `Chrome::Declaration` -- markup and
 * style -- laid out and painted by the library, and a case's picture is the library rendering the
 * case's own manifest. `TheBrowserSpellsNoDrawingInstruction` reads this file to say so.
 *
 * **THE APPEARANCE IS A PURE FUNCTION OF THE STATE**, which is what lets a window's behaviour be
 * checked without one: give the state, get the document, and every claim below is about that. */
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

/* [SET] THE WINDOW THIS BROWSER ASKS FOR, and the system may give it less. Bigger than the frame the
 * corpus is measured at, because two columns and a picture is what has to fit -- and the picture is
 * what the window is for. */
constexpr int kWidth = 1600;
constexpr int kHeight = 900;

/* [SET] HOW MUCH OF ITS PANE A MODEL SPANS. The corpus frames at 0.6 because an oracle is framed the
 * same way; a browser is not comparing anything, so it fills what it can without touching the edge. */
constexpr double kBrowserFill = 0.92;

/* [SET] WHAT SHARE OF THE FRAME THE BROWSER'S OWN CHROME MAY TAKE ON THE CPU, beside whatever the
 * case it is showing costs. A tenth is the same share the interface was given when it was measured
 * alone, and it is checked here rather than quoted. */
constexpr double kFrameBudgetMs = 16.67;
constexpr double kChromeShare = 0.10;

/* WHAT THE BROWSER IS AND WHAT IT KNOWS, and the state is the whole of what its appearance depends
 * on. Two frames of this struct that compare equal produce the same document, which is the property
 * every claim below rests on. */
struct Browser final : outshine::Script::Host {
  std::vector<View::Listed> Cases;
  View::Showing Showing;
  View::SheetFont Face;
  int WidthPx = kWidth;
  int HeightPx = kHeight;

  /* **WHAT THIS BROWSER HANDS THE ENGINE, AND IT IS THE WHOLE OF WHAT IT DOES.** Two surfaces --
   * its own columns and whatever is in the pane -- a file for the body, and the rectangle the body
   * stands in. It reads no glTF, poses nothing, compiles no plan, frames no camera and paints no
   * quad: every one of those is the engine's, and a consumer that did any of them would be a second
   * implementation of the thing it is here to show off. */
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
      /* **HOW THIS BROWSER LIGHTS WHAT IT SHOWS**, and it is the client's call rather than the
       * engine's: a viewer wants form to read at a glance, so a key light high and to the left over a
       * modest ambient. [SET] 3.0 lux and 0.35 W/(m^2 sr) -- chosen to make a mid-grey body land near
       * the middle of the display range with its shaded side still legible. */
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

  /* THE TWO SURFACES, in fractions of the frame: the columns on the left and the pane beside them.
   * A render case leaves the pane empty -- the engine draws a body there -- and the other two kinds
   * fill it with a page or a console, laid out by the same engine that lays out these columns. */
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
      /* WHAT THIS ENGINE MAKES OF THE PROGRAM, which for most of the corpus is *outside the subset*
       * with the boundary that says why -- the same answer the suite publishes, shown where a reader
       * is looking. */
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

  /* WHETHER THE ENGINE HAS TO STAND A NEW SCENARIO UP, or only to be told what the surfaces say. A
   * body and a plan change when the SELECTION does; scrolling a list and filtering a suite change
   * neither, and re-standing the scenario for one would rebuild every pipeline to move a row. */
  [[nodiscard]] std::string Body(void) const {
    const View::Listed *one = Selected();
    return one != nullptr && one->Ready && !one->Document && !one->Script ? one->Prepared
                                                                         : std::string();
  }

  std::vector<uint8_t> Sheet_ = View::Sheet();

  [[nodiscard]] int PaneWidth(void) const { return WidthPx - (int)View::ColumnsWidth(WidthPx); }

  /* A POINTER EVENT IS A HIT AND A DECLARED ACTION, AND THE ACTION IS A SCRIPT. The library hands
   * back the text the element carried and decides nothing about it; the interpreter reads it and the
   * host below answers its words, so what `select` MEANS lives in one place. Adding a control to this
   * browser is adding a native here -- not another prefix to pick off a string. */
  void Touched(const Ui::Touched &found) {
    if (found.Action.empty()) { return; }
    outshine::Script::Program handler;
    std::string why;
    if (!handler.Read(found.Action, why) || !handler.Run(*this, why)) {
      Showing.Note = "THE DECLARED ACTION WAS REFUSED";
    }
  }

  /* THE BROWSER'S OWN VOCABULARY, AND IT IS THREE WORDS. Nothing in `src/core/Script.cpp` knows any of
   * them; they exist because this file put them there, which is the whole design of the host. */
  [[nodiscard]] outshine::Script::Value Global(const std::string &name) override {
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

  /* THE SENTENCE THE STATUS LINE CARRIES, derived from the listing rather than counted by hand. */
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

/* THE LIBRARY SAYS WHY IT REFUSED AND A CLIENT THAT INSTALLED NO SINK HEARS NOTHING. [MEASURED] the
 * browser's first plan came back as *the device did not come up* with the reason -- a named stage this
 * device layer does not execute -- written to a sink nobody had set. A client owns its own diagnostics
 * and this is the browser's. */
class Spoken final : public outshine::LogSink {
public:
  void Write(double, outshine::LogLevel level, const char *tag, const char *event,
             const std::vector<outshine::LogField> &fields) override {
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

/* WHICH QUAD COVERS A POINT, deepest last, or -1 -- the painter's order IS the depth here. */
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

}  // namespace

int main(int argc, char **argv) {
  /* `--frames N` RUNS THE WINDOWED ARM AND STOPS, which is what makes it checkable at all. A loop
   * that only ends when a person closes it can be compiled and never run, and a windowed path nobody
   * ran is a path that does not work. */
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

  /* `--show` OPENS ON A CASE, which is what makes a cost measurable without a hand on the mouse: the
   * expensive frame is the one with a subject up, and a headless run that never selects one measures
   * the cheap half of the program. It is the same two writes a click makes. */
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

  /* **THE BROWSER'S ONE PRODUCT IS A DECLARATION**, so every claim below is read off one. The layout
   * and the painting are the ENGINE's and are proved in `test/outshine/unit/ui`; what is proved here
   * is that what this consumer declares is well-formed, complete and placed where it says. */
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

  /* EVERY RECTANGLE IS INSIDE THE SURFACE. A quad past the edge is one the client asked the library to
   * draw where nobody can see it, and it costs the same as one that shows. */
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

  /* THE STAGE IS TRANSPARENT, WHICH IS WHAT LETS ONE RENDERER SERVE TWO CONTRIBUTIONS. If the browser
   * painted a panel over the right-hand side, the case's picture would be behind it and the claim
   * *two targets of one renderer* would be a claim about a picture nobody sees. */
  {
    const int overStage = Covering(painted, kWidth - 40, kHeight / 2);
    CHECK(overStage < 0,
          "nothing is painted over the stage, so the picture the library drew is what is seen there");
    const int overPanel = Covering(painted, 20, 400);
    CHECK(overPanel >= 0, "and the panel beside it is painted");
  }

  /* A POINTER IS A HIT AND A DECLARED ACTION. **TWO COLUMNS AND THEN THE VIEW**: the corpus on the
   * left decides what the middle holds, and the middle decides what the right shows -- so a pointer at
   * x = 20 is choosing a corpus and one at x = 300 is choosing a case, and the coordinates below are
   * the declaration's own geometry rather than numbers this test invented. */
  {
    /* THE GEOMETRY IS THE DECLARATION'S OWN: the head is 1.8 em and a row 1.3, and the root em is the
     * surface's height over 45. A test that wrote pixels here would be a second copy of a ratio. */
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

    /* THE FIRST ROW OF THE CORPUS COLUMN IS *ALL*, which is the way back and is a row like any other. */
    browser.Touched(Ui::Under(placed, tree, 20, firstRow));
    CHECK(browser.Showing.Suite.empty(), "and the row above them all is the way back to the whole tree");
    CHECK(View::Filtered(browser.Cases, browser.Showing).size() == browser.Cases.size(),
          "which admits every case the tree declares");
  }

  /* THE SCROLL IS A NEGATIVE MARGIN UNDER A CLIP, which is a pane spelled out of what the subset
   * already holds. One row of scroll moves the content by exactly one row and never by a pixel more. */
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

  /* WHAT THE CHROME COSTS, WITH ITS POPULATION AND ITS DOMAIN. Declaring, reading, cascading, laying
   * out, painting and translating -- everything between the browser's state and the rectangles the
   * library is handed. Uploading and drawing them is the renderer's and is not in this number. */
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

  /* THE WHOLE PATH, THROUGH THE LIBRARY, INTO A TARGET THIS PROGRAM OWNS. Everything above is about a
   * list of rectangles; this is about texels, and it is the same code the windowed arm runs with a
   * swapchain image in place of the texture. **A DECLARED COLOUR MUST COME BACK AS ITSELF**: the frame
   * encodes sRGB on write and CSS states colours in sRGB, so a panel declared `#12161b` that read back
   * lighter would mean every colour in every interface is wrong by the transfer. */
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
        /* The CASE column's own colour, `#12161b`, read where nothing else is drawn over it. */
        const int r = codeAt(300, 400, 0), g = codeAt(300, 400, 1), b = codeAt(300, 400, 2);
        std::printf("NOTE the case column reads back as %d %d %d, declared 18 22 27\n", r, g, b);
        CHECK(std::abs(r - 0x12) <= 1 && std::abs(g - 0x16) <= 1 && std::abs(b - 0x1b) <= 1,
              "a panel declared #12161b comes back as #12161b -- the declaration IS the colour, "
              "which is what the transfer would break if the two ever met twice");
      }
    }
  }

  /* **THE PANE HOLDS WHAT THE CASE IS**, and for two of the three kinds that is a picture this engine
   * made rather than one the renderer drew. A document is laid out and painted by the same engine that
   * draws these columns; a program is shown as a console, because a blank pane says nothing about a
   * case whose subject has no picture at all. */
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

  /* **A SCENARIO TAKES ITS BODY WITH IT WHEN IT GOES** (board:1455), and this is the defect that
   * shape closes rather than a property somebody hoped for. A body was drawn across the whole surface
   * behind every document the browser showed afterwards, because a mesh outlives whatever set it and
   * a picture region of zero means *the whole frame*. Standing a body up, then standing a scenario
   * with none up in its place, must leave nothing of the first one in the picture. */
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
      /* NOTHING SELECTED IS A SCENARIO WITH NO BODY, and it is opened the same way. */
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

  /* THE FACE IS THE CLIENT'S ASSET AND IT IS LEGIBLE. Ahem would put a filled square where every
   * letter goes; this says the sheet has ink in it and that a glyph is narrower than its cell, which
   * is what a readable label needs and a measurement face does not have. */
  {
    const std::vector<uint8_t> sheet = View::Sheet();
    size_t ink = 0;
    for (size_t at = 3; at < sheet.size(); at += 4) { ink += sheet[at] > 0 ? 1 : 0; }
    CHECK(ink > 0, "the browser's own face carries ink");
    CHECK(ink < sheet.size() / 4,
          "and it is not a filled sheet -- a face whose every texel is ink is Ahem with more steps");
    const Ui::Glyph glyph = browser.Face.Shape(U'A', 16.0);
    CHECK(glyph.Drawn && glyph.WidthPx < glyph.AdvancePx,
          "a glyph is narrower than the advance that follows it, which is where the spacing lives");
    CHECK(!browser.Face.Shape(U' ', 16.0).Drawn, "and a space draws nothing");
  }

  return Report();
}

namespace {

/* THE WINDOWED ARM. Everything above ran without one; this adds a window, a swapchain and an event
 * loop, and changes nothing about how a picture is decided. */
int Windowed(Browser &browser, int frames) {
  Spoken spoken;
  outshine::Log::SetSink(&spoken);
  outshine::Render::Renderer renderer;
  /* **THE SCENARIO IS HELD BY POINTER AND SWAPPED WHOLE.** Standing another one up releases this one,
   * and what it put in the renderer -- the body, the picture's rectangle, the rectangles over it --
   * leaves with it. That is the only way a case stops being shown. */
  std::unique_ptr<outshine::Clients::Live> live;
  std::string error;

  SDL_Window *window = SDL_CreateWindow("outshine cases", browser.WidthPx, browser.HeightPx,
                                        SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    std::printf("the window was refused: %s\n", SDL_GetError());
    return 1;
  }

  /* **THIS LOOP IS THE WHOLE OF WHAT A CONSUMER DOES.** It pumps events, and where one changed what
   * the browser IS it hands the engine a new declaration -- a whole scenario when the body changed, a
   * set of surfaces when only the writing did. Everything else is `Advance`, and `Advance` is the
   * engine running. There is no draw call here, no mesh, no camera and no plan. */
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
        browser.Showing.Note = "DECLINED " + error;
        live.reset();
      }
      if (live && !SDL_ClaimWindowForGPUDevice(renderer.Device(), window)) {
        std::printf("the window was refused: %s\n", SDL_GetError());
        return 1;
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

    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(renderer.Device());
    SDL_GPUTexture *surface = nullptr;
    Uint32 gotW = 0, gotH = 0;
    if (SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &surface, &gotW, &gotH) &&
        surface != nullptr) {
      /* **THE SWAPCHAIN'S OWN SIZE IS THE SURFACE'S**, and the window's is only what was asked for: a
       * system may grant less, and a display may hand back more device pixels than points. */
      if ((int)gotW != browser.WidthPx || (int)gotH != browser.HeightPx) {
        browser.WidthPx = (int)gotW;
        browser.HeightPx = (int)gotH;
        restand = true;
      }
      renderer.PresentInto(surface);
      SDL_SubmitGPUCommandBuffer(commands);
      const auto began = std::chrono::steady_clock::now();
      if (!live->Advance(error)) { browser.Showing.Note = "REFUSED " + error; }
      advanceMs.push_back(
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
    } else {
      SDL_SubmitGPUCommandBuffer(commands);
    }
  }
  renderer.WaitForGpu();
  if (frames >= 0) {
    const Distribution advancing = Over(advanceMs);
    std::printf("advance p50 %.3f p95 %.3f p99 %.3f max %.3f ms over %zu frames\n",
                advancing.P50Ms, advancing.P95Ms, advancing.P99Ms, advancing.MaxMs, advanceMs.size());
  }
  SDL_ReleaseWindowFromGPUDevice(renderer.Device(), window);
  SDL_DestroyWindow(window);
  std::printf("the browser drew %d frame(s) into a window it owns\n", drawn);
  return 0;
}

}  // namespace
