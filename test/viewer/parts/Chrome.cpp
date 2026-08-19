#include "Chrome.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "GlyphSheet.h"
#include "PreparedRoot.h"

namespace outshine::Viewer {

/* THE TWO COLUMNS AND THE ROW, in the same pixels the stylesheet states them in. They are here because
 * the stage's own rectangle is derived from them, and two spellings of one number is how a picture
 * ends up beside the pane it was meant to be centred in. */
/* **THE WHOLE GEOMETRY OF THIS BROWSER, AS RATIOS, AND EACH IS WRITTEN ONCE.** The stylesheet is built
 * from them and so is the arithmetic that decides how many rows to declare -- two spellings of one
 * number is how a list scrolls one row further than it draws, and a stylesheet and a scroll that
 * disagree is that defect wearing two hats. */
constexpr double kCorpusShare = 0.15;   /* of the surface's width */
constexpr double kCaseShare = 0.22;
constexpr double kRowEm = 1.3;          /* of the root text */
constexpr double kHeadEm = 1.8;
constexpr double kStatusEm = 1.6;
/* [SET] HOW MANY LINES OF TEXT THE SURFACE IS TALL. It is the only absolute in the browser, and it is
 * absolute because a glyph is drawn from texels: something has to say how many of them a line gets. */
constexpr double kLinesTall = 45.0;

namespace {

std::string Flattened(std::string path) {
  for (char &c : path) {
    if (c == '/') { c = '-'; }
  }
  return path;
}

/* A DECLARATION IS TEXT AND A CASE NAME COMES FROM A DIRECTORY, so the one character that could end an
 * attribute early is removed. It is a refusal to produce broken markup rather than a sanitiser: no
 * name in this tree carries a quote, and if one ever does this turns it into an underscore instead of
 * a document that reads as something else. */
std::string Quoted(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) { out.push_back((c == '"' || c == '<' || c == '&') ? '_' : c); }
  return out;
}

} // namespace

std::vector<Listed> Cases(void) {
  std::vector<Listed> found;
  std::error_code walking;
  for (std::filesystem::recursive_directory_iterator it("test", walking), end; it != end;
       it.increment(walking)) {
    if (walking) { break; }
    if (!it->is_regular_file(walking) || it->path().filename() != "manifest.json") { continue; }
    const std::string relative = it->path().parent_path().string();
    /* The preparer's own inputs live beside the corpora and declare no case. */
    if (relative.find("/prepare") != std::string::npos) { continue; }
    Listed one;
    one.Name = it->path().parent_path().filename().string();
    one.Prepared = Test::PreparedRoot() + "/" + Flattened(relative);
    /* THE SUITE IS THE CORPUS AND NOT THE PATH ABOVE THE CASE. [MEASURED] taking the parent
     * directory reported SIX suites where the tree holds three: a corpus whose cases nest one level
     * deeper -- `khronos/glTF/Cameras/cameras-orthographic` -- grew a suite per model. Two components
     * is what a vendor and its suite are, and it is derived rather than listed. */
    const std::string inside = relative.substr(5);
    const size_t first = inside.find('/');
    const size_t second = first == std::string::npos ? first : inside.find('/', first + 1);
    one.Suite = second == std::string::npos ? inside : inside.substr(0, second);
    one.Ready = std::filesystem::exists(std::filesystem::path(one.Prepared) / "manifest.json", walking);
    std::ifstream declaration(it->path());
    std::stringstream held;
    held << declaration.rdbuf();
    const std::string stated = held.str();
    one.Document = stated.find("\"outshine/declared-case-manifest\"") != std::string::npos;
    one.Script = stated.find("\"kind\": \"script\"") != std::string::npos;
    found.push_back(std::move(one));
  }
  std::sort(found.begin(), found.end(), [](const Listed &a, const Listed &b) {
    return a.Suite == b.Suite ? a.Name < b.Name : a.Suite < b.Suite;
  });
  return found;
}

std::vector<std::string> Suites(const std::vector<Listed> &cases) {
  std::vector<std::string> out;
  for (const Listed &one : cases) {
    if (std::find(out.begin(), out.end(), one.Suite) == out.end()) { out.push_back(one.Suite); }
  }
  return out;
}

std::vector<int> Filtered(const std::vector<Listed> &cases, const Showing &showing) {
  std::vector<int> out;
  for (int at = 0; at < (int)cases.size(); ++at) {
    if (showing.Suite.empty() || cases[(size_t)at].Suite == showing.Suite) { out.push_back(at); }
  }
  return out;
}

int RowsThatFit(int heightPx) {
  const double em = RootEmPx(heightPx);
  const double room = (double)heightPx - (kHeadEm + kStatusEm) * em;
  return room > 0 ? (int)(room / (kRowEm * em)) : 0;
}

double RootEmPx(int heightPx) { return (double)heightPx / kLinesTall; }
double ColumnsWidth(int widthPx) { return (kCorpusShare + kCaseShare) * (double)widthPx; }

std::string Style(void) {
  /* **THE WHOLE FASSADE IN RATIOS, and the one absolute number is the root's font size** -- which the
   * declaration sets from the surface's own height, because a text size is the single place a ratio
   * has to become a device pixel. Everything else is a percentage of its parent, a multiple of the
   * text, or a share of what is left: a window twice as large shows the same interface twice as big,
   * and no number here has to be revisited.
   *
   * Every property and every value below is inside the subset the corpus holds this engine to, so the
   * browser cannot look right through a capability nobody measured. */
  const auto share = [](double of) { return std::to_string(of * 100.0) + "%"; };
  const auto em = [](double of) { return std::to_string(of) + "em"; };
  /* **A FULL-HEIGHT COLUMN NEEDS A DEFINITE HEIGHT ALL THE WAY UP**, and a user-agent sheet gives the
   * root none -- which is CSS's own arrangement and not a gap: `html { height: 100% }` resolves against
   * the viewport, and a page that wants the whole of it says so. Every page that has ever had a
   * full-height sidebar has written this line. */
  return std::string("html, body { height: 100% }\n")
         + "body { margin: 0; font-family: sheet; color: #c8d0d8 }\n"
         + ".frame { display: flex; flex-direction: row; width: 100%; height: 100% }\n"
         + ".corpora { display: flex; flex-direction: column; background: #0f1317; width: " +
         share(kCorpusShare) +
         "; height: 100%; border-width: 0 1px 0 0; border-color: #232c35 }\n"
         ".cases { display: flex; flex-direction: column; background: #12161b; width: " +
         share(kCaseShare) +
         "; height: 100%; border-width: 0 1px 0 0; border-color: #232c35 }\n"
         ".head { background: #1a222a; padding: 0.3em 0.6em; box-sizing: border-box; color: #6f8090;"
         " height: " + em(kHeadEm) + "; border-width: 0 0 1px 0; border-color: #232c35 }\n"
         ".list { flex: 1 1 0%; overflow: hidden }\n"
         ".row { padding: 0.1em 0.6em; box-sizing: border-box; height: " + em(kRowEm) +
         "; color: #9fb0bf }\n"
         ".row-on { padding: 0.1em 0.6em; box-sizing: border-box; height: " + em(kRowEm) +
         "; background: #2f6f9f; color: #f2f6fa }\n"
         ".row-out { padding: 0.1em 0.6em; box-sizing: border-box; height: " + em(kRowEm) +
         "; color: #6a7683 }\n"
         ".status { background: #1a222a; padding: 0.25em 0.6em; box-sizing: border-box; height: " +
         em(kStatusEm) + "; color: #93a1ad; border-width: 1px 0 0 0; border-color: #232c35 }\n"
         ".stage { flex: 1 1 0%; height: 100% }\n"
         ".plate { background: #10141880; padding: 0.25em 0.6em; box-sizing: border-box; height: " +
         em(kHeadEm) + "; color: #cfe0ee }\n"
         ".console { display: flex; flex-direction: column; background: #0b0e11; width: 100%;"
         " height: 100% }\n"
         ".code { padding: 0 0.6em; box-sizing: border-box; height: " + em(kRowEm) +
         "; color: #9fb0bf; white-space: pre }\n"
         ".verdict { background: #1a222a; padding: 0.3em 0.6em; box-sizing: border-box; height: " +
         em(kHeadEm) + "; color: #cfe0ee; border-width: 1px 0 0 0; border-color: #232c35 }\n";
}

std::string Declaration(const std::vector<Listed> &cases, const Showing &showing, int widthPx,
                        int heightPx) {
  const std::vector<int> shown = Filtered(cases, showing);
  const std::vector<std::string> suites = Suites(cases);
  const int rows = RowsThatFit(heightPx);
  (void)widthPx;

  /* **THE ONE ABSOLUTE NUMBER, AND IT IS THE ROOT'S TEXT SIZE.** Everything below is a percentage of a
   * parent, a multiple of this, or a share of what is left -- so nothing in this declaration has to be
   * revisited when the surface changes size, and the whole interface scales with it. */
  std::string out = "<style>html { font-size: " + std::to_string(RootEmPx(heightPx)) + "px }\n";
  out += Style();
  out += "</style><body><div class=frame>";

  /* **TWO COLUMNS AND THEN THE VIEW**, which is how a file browser has shown a hierarchy since long
   * before this one: the corpus on the left decides what the middle holds, and the middle decides what
   * the right shows. Each column is its own clipped pane, so a listing of any length scrolls without
   * moving the one beside it. */
  out += "<div class=corpora><div class=head>CORPUS</div><div class=list><div>";
  {
    const bool all = showing.Suite.empty();
    out += "<div class=\"" + std::string(all ? "row-on" : "row") +
           "\" data-action=\"suite('')\">ALL (" + std::to_string(cases.size()) + ")</div>";
  }
  for (const std::string &suite : suites) {
    int count = 0;
    for (const Listed &one : cases) { count += one.Suite == suite ? 1 : 0; }
    const bool on = showing.Suite == suite;
    out += "<div class=\"" + std::string(on ? "row-on" : "row") + "\" data-action=\"suite('" +
           Quoted(suite) + "')\">" + Quoted(suite) + " (" + std::to_string(count) + ")</div>";
  }
  out += "</div></div><div class=status>" + Quoted(showing.Note) + "</div></div>";

  out += "<div class=cases><div class=head>CASE (" + std::to_string(shown.size()) +
         ")</div>";

  /* THE LIST IS CLIPPED AND ONLY WHAT CAN BE SEEN IS DECLARED. [MEASURED] declaring all 309 rows and
   * offsetting them under a clip cost p50 1.34 ms and 641 boxes -- 94.5 % of the share this browser
   * was given -- for a picture that shows about forty. **The engine laying out a box nobody can see is
   * the engine doing what it was asked**, so the repair belongs here: a client decides WHAT it
   * declares, and a listing of any length declares one screenful.
   *
   * THE ROW'S ACTION CARRIES ITS INDEX IN THE FILTERED LISTING AND NOT IN THE SLICE, or a pointer
   * would select a different case after every scroll. */
  const int from = std::max(0, showing.ScrolledRows);
  const int upTo = std::min((int)shown.size(), from + rows + 1);
  out += "<div class=list><div>";
  for (int at = from; at < upTo; ++at) {
    const Listed &one = cases[(size_t)shown[(size_t)at]];
    const char *style = at == showing.Selected ? "row-on" : (one.Ready ? "row" : "row-out");
    out += "<div class=" + std::string(style) + " data-action=\"select(" + std::to_string(at) +
           ")\">" + Quoted(one.Name) + "</div>";
  }
  out += "</div></div><div class=status>" +
         (shown.empty() ? std::string("NO CASES")
                        : std::to_string(from + 1) + "-" + std::to_string(upTo) + " OF " +
                              std::to_string(shown.size())) +
         "</div></div>";

  /* THE STAGE DECLARES NO BACKGROUND, so nothing is painted over the picture the renderer drew. A
   * quad with no alpha reaches no pixel and costs no rectangle, which is what makes *transparent* a
   * value here rather than a mode. */
  out += "<div class=stage>";
  if (showing.Selected >= 0 && showing.Selected < (int)shown.size()) {
    const Listed &one = cases[(size_t)shown[(size_t)showing.Selected]];
    out += "<div class=plate>" + Quoted(one.Suite) + " / " + Quoted(one.Name) + "</div>";
  }
  out += "</div></div></body>";
  return out;
}

Region StageRegion(int widthPx, int heightPx) {
  /* **THE ROOM THE PICTURE MAY HAVE, AS FRACTIONS.** What shape the picture takes inside it is the
   * library's, because the library is what knows the case's aspect and what knows how many pixels
   * there are -- this browser only says *here, and not over my lists*. */
  Region out;
  if (widthPx <= 0 || heightPx <= 0) { return out; }
  out.X = kCorpusShare + kCaseShare;
  out.Y = kHeadEm * RootEmPx(heightPx) / (double)heightPx;
  out.Width = 1.0 - out.X;
  out.Height = 1.0 - out.Y;
  return out;
}

namespace {

std::string Slurp(const std::string &path, bool &found) {
  std::ifstream file(path);
  found = file.good();
  std::stringstream held;
  held << file.rdbuf();
  return held.str();
}

}  // namespace

std::string EntryOf(const std::string &prepared, bool &found) {
  bool haveManifest = false;
  const std::string manifest = Slurp(prepared + "/manifest.json", haveManifest);
  found = false;
  if (!haveManifest) { return {}; }
  /* THE ENTRY IS WHAT THE MANIFEST NAMES, read without a JSON reader because one field of one shape is
   * not a document model -- and the browser already links no JSON parser of its own. */
  const size_t at = manifest.find("\"entry\"");
  if (at == std::string::npos) { return {}; }
  const size_t open = manifest.find('"', manifest.find(':', at));
  const size_t close = manifest.find('"', open + 1);
  if (open == std::string::npos || close == std::string::npos) { return {}; }
  return Slurp(prepared + "/" + manifest.substr(open + 1, close - open - 1), found);
}

void AddLinkedSheets(const std::string &prepared, Ui::Stylesheet &into) {
  std::error_code walking;
  for (const auto &entry : std::filesystem::directory_iterator(prepared, walking)) {
    if (entry.path().extension() != ".css") { continue; }
    bool found = false;
    const std::string text = Slurp(entry.path().string(), found);
    if (found) { into.Read(text); }
  }
}

std::string Console(const std::string &title, const std::string &source, const std::string &verdict,
                    const char *why, int widthPx, int heightPx) {
  (void)widthPx;
  std::string out = "<style>html { font-size: " + std::to_string(RootEmPx(heightPx)) + "px }\n";
  out += Style();
  out += "</style><body><div class=console><div class=head>" + Quoted(title) +
         "</div><div class=list><div>";
  /* THE PROGRAM AS IT IS WRITTEN, one row per line, and only what fits -- a client declares one
   * screenful for the same reason the case list does. */
  size_t at = 0;
  int lines = 0;
  const int most = RowsThatFit(heightPx);
  while (at < source.size() && lines < most) {
    const size_t end = source.find('\n', at);
    const std::string line = source.substr(at, (end == std::string::npos ? source.size() : end) - at);
    at = end == std::string::npos ? source.size() : end + 1;
    out += "<div class=code>" + Quoted(line) + "</div>";
    ++lines;
  }
  out += "</div></div><div class=verdict>" + Quoted(verdict) +
         (why != nullptr ? " -- " + Quoted(why) : "") + "</div></div></body>";
  return out;
}

std::vector<Render::OverlayQuad> AsOverlay(const std::vector<Ui::Quad> &quads, double offsetX,
                                           double offsetY) {
  std::vector<Render::OverlayQuad> out;
  out.reserve(quads.size());
  for (const Ui::Quad &from : quads) {
    Render::OverlayQuad to;
    to.LeftPx = (float)(from.X + offsetX);
    to.TopPx = (float)(from.Y + offsetY);
    to.WidthPx = (float)from.Width;
    to.HeightPx = (float)from.Height;
    to.U0 = (float)from.U0;
    to.V0 = (float)from.V0;
    to.U1 = (float)from.U1;
    to.V1 = (float)from.V1;
    /* THE COLOUR IS RGBA8 ON ONE SIDE AND FOUR FLOATS ON THE OTHER, and this is the only place the
     * two meet. A shift written twice is a shift that will disagree with itself. */
    to.Red = (float)((from.Colour >> 24) & 0xFFu) / 255.0f;
    to.Green = (float)((from.Colour >> 16) & 0xFFu) / 255.0f;
    to.Blue = (float)((from.Colour >> 8) & 0xFFu) / 255.0f;
    to.Alpha = (float)(from.Colour & 0xFFu) / 255.0f;
    to.ClipLeftPx = (float)(from.ClipX + offsetX);
    to.ClipTopPx = (float)(from.ClipY + offsetY);
    to.ClipWidthPx = (float)from.ClipWidth;
    to.ClipHeightPx = (float)from.ClipHeight;
    to.RadiusPx = (float)from.Radius;
    to.Opacity = (float)from.Opacity;
    out.push_back(to);
  }
  return out;
}

} // namespace outshine::Viewer
