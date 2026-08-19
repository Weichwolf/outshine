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

  Ui::Markup Tree;
  Ui::Stylesheet Sheet;
  Ui::Layout Placed;
  Ui::Painting Painted;
  /* **WHAT IS IN THE PANE WHEN THE CASE HAS NO PICTURE.** A document is laid out and painted by the
   * same engine that draws these columns, and a program is shown as a console -- the browser has one
   * way to draw, and a blank pane would say nothing about either. */
  Ui::Markup PaneTree;
  Ui::Stylesheet PaneSheet;
  Ui::Layout PanePlaced;
  Ui::Painting PanePainted;
  std::vector<outshine::Render::OverlayQuad> Ready;

  [[nodiscard]] bool Compose(std::string &error) {
    const std::string document = View::Declaration(Cases, Showing, WidthPx, HeightPx);
    Tree = Ui::Markup();
    Sheet = Ui::Stylesheet();
    if (!Tree.Read(document, error)) { return false; }
    Sheet.Read(Ui::UserAgentSheet());
    Sheet.Read(Tree.StyleText());
    if (!Placed.Build(Tree, Sheet, WidthPx, HeightPx, Face, error)) { return false; }
    if (!Painted.Build(Placed, Face, error)) { return false; }
    Ready = View::AsOverlay(Painted.Quads());
    Pane(error);
    return true;
  }

  /* THE PANE'S OWN CONTENT, appended to the chrome's rectangles. A render case leaves it empty -- the
   * library drew there -- and the other two kinds fill it with a picture the UI engine made. */
  void Pane(std::string &error) {
    const std::vector<int> shown = View::Filtered(Cases, Showing);
    if (Showing.Selected < 0 || Showing.Selected >= (int)shown.size()) { return; }
    const View::Listed &one = Cases[(size_t)shown[(size_t)Showing.Selected]];
    if (!one.Document && !one.Script) { return; }

    const int left = (int)View::ColumnsWidth(WidthPx);
    const int top = 0;
    const int w = PaneWidth(), h = HeightPx;
    if (w <= 0 || h <= 0) { return; }

    bool found = false;
    const std::string entry = View::EntryOf(one.Prepared, found);
    std::string document;
    if (one.Script) {
      /* WHAT THIS ENGINE MAKES OF THE PROGRAM, which for most of the corpus is *outside the subset*
       * with the boundary that says why -- the same answer the suite publishes, shown where a reader
       * is looking. */
      outshine::Script::Program program;
      std::string why;
      const bool reads = found && program.Read(entry, why);
      const std::string token =
          program.Stopped().empty() ? std::string() : "token:" + program.Stopped();
      document = View::Console(one.Name, found ? entry : "the case is not prepared",
                               reads ? "READS -- " + std::to_string(program.NodeCount()) + " NODES"
                                     : "REFUSED: " + why,
                               token.empty() ? nullptr : outshine::Script::WhyOutside(token), w, h);
    } else if (found) {
      document = entry;
    } else {
      return;
    }

    PaneTree = Ui::Markup();
    PaneSheet = Ui::Stylesheet();
    if (!PaneTree.Read(document, error)) { return; }
    PaneSheet.Read(Ui::UserAgentSheet());
    if (one.Document) { View::AddLinkedSheets(one.Prepared, PaneSheet); }
    PaneSheet.Read(PaneTree.StyleText());
    if (!PanePlaced.Build(PaneTree, PaneSheet, w, h, Face, error)) { return; }
    if (!PanePainted.Build(PanePlaced, Face, error)) { return; }
    const std::vector<outshine::Render::OverlayQuad> inside =
        View::AsOverlay(PanePainted.Quads(), left, top);
    Ready.insert(Ready.end(), inside.begin(), inside.end());
  }

  [[nodiscard]] int PaneWidth(void) const { return WidthPx - (int)View::ColumnsWidth(WidthPx); }

  /* A POINTER EVENT IS A HIT AND A DECLARED ACTION, AND THE ACTION IS A SCRIPT. The library hands
   * back the text the element carried and decides nothing about it; the interpreter reads it and the
   * host below answers its words, so what `select` MEANS lives in one place. Adding a control to this
   * browser is adding a native here -- not another prefix to pick off a string. */
  void Touched(double x, double y) {
    const Ui::Touched found = Ui::Under(Placed, Tree, x, y);
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
  bool windowed = false;
  for (int at = 1; at < argc; ++at) {
    if (std::strcmp(argv[at], "--window") == 0) { windowed = true; }
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

  Browser browser;
  browser.Cases = View::Cases();
  browser.Recount();
  CHECK(!browser.Cases.empty(), "the tree declares cases, so this browser has something to show");
  std::printf("NOTE cases the tree declares = %zu across %zu suites\n", browser.Cases.size(),
              View::Suites(browser.Cases).size());

  std::string error;
  CHECK(browser.Compose(error), "the browser's own declaration reads, lays out and paints");
  if (!browser.Painted.Quads().empty()) {
    std::printf("NOTE the browser is %zu boxes and %zu rectangles at %dx%d\n",
                browser.Placed.Boxes().size(), browser.Painted.Quads().size(), kWidth, kHeight);
  } else {
    std::printf("       %s\n", error.c_str());
    return Report();
  }

  /* EVERY RECTANGLE IS INSIDE THE SURFACE. A quad past the edge is one the client asked the library to
   * draw where nobody can see it, and it costs the same as one that shows. */
  {
    int outside = 0;
    for (const Ui::Quad &quad : browser.Painted.Quads()) {
      const bool inside = quad.X >= 0 && quad.Y >= 0 && quad.X + quad.Width <= kWidth &&
                          quad.Y + quad.Height <= kHeight;
      outside += inside ? 0 : 1;
    }
    CHECK(outside == 0, "every rectangle the browser declares is inside the surface it declared");
    CHECK(browser.Painted.QuadsBeyondTheBound() == 0,
          "and it asks for no rectangle past the library's bound");
  }

  /* THE STAGE IS TRANSPARENT, WHICH IS WHAT LETS ONE RENDERER SERVE TWO CONTRIBUTIONS. If the browser
   * painted a panel over the right-hand side, the case's picture would be behind it and the claim
   * *two targets of one renderer* would be a claim about a picture nobody sees. */
  {
    const int overStage = Covering(browser.Painted, kWidth - 40, kHeight / 2);
    CHECK(overStage < 0,
          "nothing is painted over the stage, so the picture the library drew is what is seen there");
    const int overPanel = Covering(browser.Painted, 20, 400);
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
    browser.Touched(caseColumn, firstRow);
    CHECK(browser.Showing.Selected == 0, "a pointer on the first row of the case column selects it");
    CHECK(browser.Compose(error), "and the declaration recomposes with the selection in it");

    const std::vector<std::string> suites = View::Suites(browser.Cases);
    CHECK(!suites.empty(), "the corpus column is derived from the listing");
    browser.Touched(20, (int)(1.8 * em + 1.3 * em + 0.65 * em));
    CHECK(!browser.Showing.Suite.empty(), "a pointer on a corpus filters to it");
    CHECK(browser.Showing.Selected == -1, "and clears a selection that belonged to another listing");
    const size_t narrowed = View::Filtered(browser.Cases, browser.Showing).size();
    CHECK(narrowed > 0 && narrowed < browser.Cases.size(),
          "the filter admits some of the tree and not all of it");
    browser.Recount();
    CHECK(browser.Compose(error), "and the filtered declaration composes");

    /* THE FIRST ROW OF THE CORPUS COLUMN IS *ALL*, which is the way back and is a row like any other. */
    browser.Touched(20, firstRow);
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
    CHECK(browser.Compose(error), "the unfiltered listing composes");
    const double em = View::RootEmPx(kHeight);
    const int row = (int)(1.8 * em + 0.65 * em);
    const int column = (int)(View::ColumnsWidth(kWidth) - 0.1 * kWidth);
    const int before = Covering(browser.Painted, column, row);
    const double firstY = before >= 0 ? browser.Painted.Quads()[(size_t)before].Y : -1.0;

    browser.Scrolled(1);
    CHECK(browser.Showing.ScrolledRows == 1, "one notch scrolls one row");
    CHECK(browser.Compose(error), "the scrolled listing composes");
    const int after = Covering(browser.Painted, column, row);
    const double nextY = after >= 0 ? browser.Painted.Quads()[(size_t)after].Y : -1.0;
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
      const bool composed = browser.Compose(error);
      const std::vector<outshine::Render::OverlayQuad> &ready = browser.Ready;
      const double ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
      if (!composed) { break; }
      if (frame >= kWarmup) {
        samples.push_back(ms);
        quads = ready.size();
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
    outshine::Render::PlanSpec declaration;
    declaration.Outputs.push_back(outshine::Render::Resource::FrameTex);
    declaration.Content.push_back(outshine::Render::Stage::Subjects);
    declaration.Content.push_back(outshine::Render::Stage::Overlay);
    std::shared_ptr<const outshine::Render::RenderPlan> plan;
    const bool compiled = outshine::Render::RenderPlan::Compile(declaration, &plan, error);
    CHECK(compiled, "the browser's own plan compiles");
    if (compiled) {
      renderer.Init(kWidth, kHeight, plan);
      CHECK(renderer.DeviceUsable(), "and the device comes up on it");
      if (renderer.DeviceUsable()) {
        const double eye[3] = {0, 0, 0}, fwd[3] = {0, 0, -1}, right[3] = {1, 0, 0}, up[3] = {0, 1, 0};
        renderer.SetCameraBasis(eye, fwd, right, up);
        const std::vector<uint8_t> sheet = View::Sheet();
        CHECK(renderer.SetOverlayAtlas(sheet.data(), View::AtlasWidth(), View::AtlasHeight(), error),
              "the browser's own face is uploaded");
        browser.Showing = View::Showing();
        browser.Recount();
        CHECK(browser.Compose(error), "and its declaration composes");
        const std::vector<outshine::Render::OverlayQuad> ready =
            View::AsOverlay(browser.Painted.Quads());
        CHECK(renderer.SetOverlay(ready.data(), ready.size(), error),
              "and the library takes every rectangle of it");
        renderer.RenderFrame();
        std::vector<uint8_t> rgba;
        const bool read = renderer.ReadPixels(rgba) == outshine::Render::ReadState::Ready;
        CHECK(read, "the frame comes back off the device");
        if (read && rgba.size() >= (size_t)kWidth * kHeight * 4u) {
          const auto codeAt = [&rgba](int x, int y, int channel) {
            return (int)rgba[(((size_t)y * kWidth) + (size_t)x) * 4u + (size_t)channel];
          };
          /* The panel's own colour, `#12161b`, read where nothing else is drawn over it. */
          /* The CASE column's own colour, `#12161b`, read where nothing else is drawn over it. */
          const int r = codeAt(300, 400, 0), g = codeAt(300, 400, 1), b = codeAt(300, 400, 2);
          std::printf("NOTE the case column reads back as %d %d %d, declared 18 22 27\n", r, g, b);
          CHECK(std::abs(r - 0x12) <= 1 && std::abs(g - 0x16) <= 1 && std::abs(b - 0x1b) <= 1,
                "a panel declared #12161b comes back as #12161b -- the declaration IS the colour, "
                "which is what the transfer would break if the two ever met twice");
          /* The stage, where the browser declares nothing, is the picture the library drew. */
          int ink = 0;
          for (int y = 100; y < 600; y += 7) {
            for (int x = 460; x < 1270; x += 7) {
              ink += codeAt(x, y, 0) != r || codeAt(x, y, 1) != g || codeAt(x, y, 2) != b;
            }
          }
          std::printf("NOTE samples over the stage that are not the panel's colour = %d\n", ink);
        }
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
    CHECK(browser.Compose(error), "with nothing selected the browser composes");
    const size_t bare = browser.Ready.size();

    browser.Showing.Selected = documentAt;
    CHECK(browser.Compose(error), "and with a document selected it composes too");
    CHECK(browser.Ready.size() > bare,
          "a document case puts rectangles in the pane -- drawn by the engine that drew the columns "
          "around them, which is the property this browser exists to hold");

    browser.Showing.Selected = scriptAt;
    CHECK(browser.Compose(error), "and with a program selected it composes");
    CHECK(browser.Ready.size() > bare,
          "a program is shown as a console: its own text and what this engine made of it, because a "
          "case with no picture still has something to say");
    browser.Showing.Selected = -1;
    CHECK(browser.Compose(error), "and the browser returns to nothing selected");
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
  /* THE CASE BEING SHOWN IS HELD BY POINTER because a configured case owns a device's worth of state
   * and is neither copied nor moved -- swapping one for another is releasing the first and building
   * the second, which is what selecting a different case IS. */
  std::unique_ptr<ConfiguredCase> live;
  int shownAt = -1;

  /* THE BROWSER WITH NO CASE UP IS A SKY AND ITS OWN CHROME. A plan whose picture has no contributor
   * is REFUSED -- *nothing draws into it, so the plan would compile, run and render black* -- and that
   * refusal is right: it is the guard against a consumer who forgot a stage. This consumer did not
   * forget one; it declares the SAME stage that will draw the case it is about to show, and hands it
   * no subject -- so the picture is the pass's own clear with the interface over it, and selecting a
   * case changes what that stage is given rather than which stages exist.
   *
   * `Stage::Sky` WOULD BE THE PRETTIER ANSWER AND IT IS NOT AVAILABLE: the catalogue declares it and
   * this device layer does not execute it yet, which `Renderer::Executable` says out loud. Naming that
   * here rather than working around it silently is the difference between a gap and a mystery. */
  outshine::Render::PlanSpec chromeOnly;
  chromeOnly.Outputs.push_back(outshine::Render::Resource::Surface);
  chromeOnly.Content.push_back(outshine::Render::Stage::Subjects);
  chromeOnly.Content.push_back(outshine::Render::Stage::Overlay);
  std::shared_ptr<const outshine::Render::RenderPlan> plan;
  std::string error;
  if (!outshine::Render::RenderPlan::Compile(chromeOnly, &plan, error)) {
    std::printf("the browser's own plan was refused: %s\n", error.c_str());
    return 1;
  }
  renderer.Init(browser.WidthPx, browser.HeightPx, plan);
  if (!renderer.DeviceUsable()) {
    std::printf("the device did not come up\n");
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow("outshine cases", browser.WidthPx, browser.HeightPx,
                                        SDL_WINDOW_RESIZABLE);
  if (window == nullptr || !SDL_ClaimWindowForGPUDevice(renderer.Device(), window)) {
    std::printf("the window was refused: %s\n", SDL_GetError());
    return 1;
  }
  /* A CAMERA, BECAUSE THIS BROWSER DECLARES A GEOMETRY STAGE. A frame is not rendered without one --
   * and that is right: a stage that PLACES things needs to know from where. With no case up nothing is
   * placed, and the camera is still the honest thing to hand over rather than a special case in the
   * library for a plan that happens to be empty. */
  const double eye[3] = {0.0, 0.0, 0.0};
  const double fwd[3] = {0.0, 0.0, -1.0};
  const double right[3] = {1.0, 0.0, 0.0};
  const double up[3] = {0.0, 1.0, 0.0};
  renderer.SetCameraBasis(eye, fwd, right, up);

  const std::vector<uint8_t> sheet = View::Sheet();
  if (!renderer.SetOverlayAtlas(sheet.data(), View::AtlasWidth(), View::AtlasHeight(), error)) {
    std::printf("the face was refused: %s\n", error.c_str());
    return 1;
  }

  bool running = true;
  int drawn = 0;
  while (running) {
    if (frames >= 0 && drawn >= frames) { break; }
    ++drawn;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) { running = false; }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        browser.Touched(event.button.x, event.button.y);
      }
      if (event.type == SDL_EVENT_MOUSE_WHEEL) { browser.Scrolled(-(int)event.wheel.y); }
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) { running = false; }
      if (event.type == SDL_EVENT_WINDOW_RESIZED) {
        browser.WidthPx = event.window.data1;
        browser.HeightPx = event.window.data2;
      }
    }

    /* THE CASE THE BROWSER IS SHOWING BRINGS THE RENDERER UP ON ITS OWN PLAN, WITH THE OVERLAY IN IT.
     * That is one plan and one renderer; the browser adds a content stage and decides nothing else. */
    const std::vector<int> shown = View::Filtered(browser.Cases, browser.Showing);
    const int wanted = browser.Showing.Selected >= 0 && browser.Showing.Selected < (int)shown.size()
                           ? shown[(size_t)browser.Showing.Selected]
                           : -1;
    if (wanted != shownAt) {
      shownAt = wanted;
      live.reset();
      std::string why;
      if (wanted >= 0 && browser.Cases[(size_t)wanted].Ready) {
        auto fresh = std::make_unique<ConfiguredCase>();
        /* THE SURFACE IS THE WINDOW'S AND THE PICTURE'S RECTANGLE IS THE PANE'S. The case is framed
         * for its own aspect, centred in the room left of the two columns, and everything outside it
         * keeps the frame's clear -- which is where the browser's own interface is drawn. */
        if (fresh->Read(browser.Cases[(size_t)wanted].Prepared, why) &&
            fresh->Start(renderer, why, {outshine::Render::Stage::Overlay}, browser.WidthPx,
                         browser.HeightPx) &&
            fresh->PoseAt(0, why)) {
          /* **THE BROWSER FRAMES THE MODEL AND DOES NOT REPRODUCE THE SHOT.** A case's own camera is
           * what an oracle is compared against; a person opening a browser wants to SEE the thing, and
           * a subject that fills a tenth of its frame because its author stood far back is a subject
           * nobody can look at. A failure here is not fatal -- a subject with no extent keeps the
           * camera it declared. */
          std::string framing;
          (void)fresh->FrameToFill(kBrowserFill, framing);
          live = std::move(fresh);
          const View::Region where = View::StageRegion(browser.WidthPx, browser.HeightPx);
          renderer.SetPictureRegion(where.X, where.Y, where.Width, where.Height,
                                    (double)live->WidthPx() / (double)live->HeightPx());
          browser.Showing.Note = "SHOWING " + browser.Cases[(size_t)wanted].Name;
        } else {
          browser.Showing.Note = "DECLINED " + why;
        }
      }
      /* WITH NO CASE UP, THE BROWSER IS ITS OWN PLAN AGAIN. `Init` replaces every target the last
       * plan owned, so the atlas is handed over afterwards each time -- it lives in the stage the
       * plan just rebuilt. */
      if (!live) {
        renderer.SetPictureRegion(0, 0, 0, 0, 0);
        renderer.Init(browser.WidthPx, browser.HeightPx, plan);
        browser.Recount();
      }
      if (!renderer.SetOverlayAtlas(sheet.data(), View::AtlasWidth(), View::AtlasHeight(), error)) {
        browser.Showing.Note = "THE FACE WAS REFUSED";
      }
    }

    if (!browser.Compose(error)) { browser.Showing.Note = "THE DECLARATION WAS REFUSED"; }
    if (!renderer.SetOverlay(browser.Ready.data(), browser.Ready.size(), error)) { running = false; }

    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(renderer.Device());
    SDL_GPUTexture *surface = nullptr;
    Uint32 gotW = 0, gotH = 0;
    if (SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &surface, &gotW, &gotH) &&
        surface != nullptr) {
      /* **THE SWAPCHAIN'S OWN SIZE IS THE SURFACE'S**, and the window's is only what was asked for:
       * a system may grant less, and a display may hand back more device pixels than points. The
       * browser lays itself out for what it was GIVEN, or its columns are placed for a frame that does
       * not exist. */
      if ((int)gotW != browser.WidthPx || (int)gotH != browser.HeightPx) {
        browser.WidthPx = (int)gotW;
        browser.HeightPx = (int)gotH;
      }
      renderer.PresentInto(surface);
      SDL_SubmitGPUCommandBuffer(commands);
      /* THE CASE DRAWS ITSELF THROUGH THE LIBRARY, or the library draws the chrome alone. Either way
       * this program has issued no draw: it named a surface and asked for a frame. */
      std::string why;
      if (live) {
        if (!live->Draw(renderer, why)) { browser.Showing.Note = "REFUSED " + why; }
      } else {
        renderer.RenderFrame();
      }
    } else {
      SDL_SubmitGPUCommandBuffer(commands);
    }
  }
  renderer.WaitForGpu();
  SDL_ReleaseWindowFromGPUDevice(renderer.Device(), window);
  SDL_DestroyWindow(window);
  std::printf("the browser drew %d frame(s) into a window it owns\n", drawn);
  return 0;
}

}  // namespace
