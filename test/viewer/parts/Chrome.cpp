#include "Chrome.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "GlyphSheet.h"
#include "PreparedRoot.h"

namespace outshine::Viewer {
namespace {

/* THE ROW'S HEIGHT AND THE LIST'S TOP, in the same pixels the stylesheet states them in. They are
 * here because the scroll arithmetic reads them too, and two spellings of one number is how a list
 * scrolls one row further than it draws. */
constexpr int kRowPx = 18;
constexpr int kBarPx = 26;
constexpr int kStatusPx = 22;

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
    one.Document = held.str().find("\"outshine/declared-case-manifest\"") != std::string::npos;
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
  const int room = heightPx - kBarPx - kStatusPx;
  return room > 0 ? room / kRowPx : 0;
}

const char *Style(void) {
  /* THE WHOLE APPEARANCE, AND NOTHING OUTSIDE THE DECLARED SUBSET. Block flow, flexbox, the box model,
   * `overflow: hidden`, colour, border and the cascade -- every one of them a thing the corpus holds
   * this engine to, so the browser cannot look right through a capability nobody measured. */
  return "body { margin: 0; font-family: sheet; color: #c8d0d8; font-size: 16px }\n"
         ".frame { display: flex; flex-direction: row }\n"
         ".side { display: flex; flex-direction: column; background: #12161b;"
         " border-width: 0 1px 0 0; border-color: #232c35 }\n"
         ".bar { background: #1a222a; padding: 5px 8px; box-sizing: border-box;"
         " display: flex; flex-direction: row; gap: 6px }\n"
         ".tab { padding: 0 6px; background: #232c35; color: #93a1ad }\n"
         ".tab-on { padding: 0 6px; background: #2f6f9f; color: #f2f6fa }\n"
         ".list { flex: 1 1 0%; overflow: hidden }\n"
         ".row { padding: 2px 8px; box-sizing: border-box; color: #9fb0bf }\n"
         ".row-on { padding: 2px 8px; box-sizing: border-box; background: #2f6f9f; color: #f2f6fa }\n"
         ".row-out { padding: 2px 8px; box-sizing: border-box; color: #6a7683 }\n"
         ".status { background: #1a222a; padding: 4px 8px; box-sizing: border-box; color: #93a1ad;"
         " border-width: 1px 0 0 0; border-color: #232c35 }\n"
         ".stage { flex: 1 1 0% }\n"
         ".plate { background: #10141880; padding: 4px 8px; box-sizing: border-box; color: #cfe0ee }\n";
}

std::string Declaration(const std::vector<Listed> &cases, const Showing &showing, int widthPx,
                        int heightPx) {
  const std::vector<int> shown = Filtered(cases, showing);
  const std::vector<std::string> suites = Suites(cases);
  const int rows = RowsThatFit(heightPx);
  const int sidePx = 300;

  std::string out = "<style>";
  out += Style();
  out += "</style><body><div class=frame style=\"width:" + std::to_string(widthPx) +
         "px;height:" + std::to_string(heightPx) + "px\">";

  out += "<div class=side style=\"width:" + std::to_string(sidePx) +
         "px;height:" + std::to_string(heightPx) + "px\">";
  out += "<div class=bar style=\"height:" + std::to_string(kBarPx) + "px\">";
  for (const std::string &suite : suites) {
    const bool on = showing.Suite == suite;
    /* THE ACTION IS A SCRIPT AND NOT A TOKEN THE BROWSER TAKES APART. `suite("wpt/css/flexbox")` is
     * read by the interpreter and answered by a host this browser implements, so what the word means
     * lives in one place -- and adding a control is adding a native rather than another prefix to
     * pick off a string. */
    out += "<div class=\"" + std::string(on ? "tab-on" : "tab") + "\" data-action=\"suite('" +
           Quoted(suite) + "')\">" + Quoted(suite) + "</div>";
  }
  out += "</div>";

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
    out += "<div class=" + std::string(style) + " style=\"height:" + std::to_string(kRowPx) +
           "px\" data-action=\"select(" + std::to_string(at) + ")\">" + Quoted(one.Name) +
           "</div>";
  }
  out += "</div></div>";

  out += "<div class=status style=\"height:" + std::to_string(kStatusPx) + "px\">" +
         Quoted(showing.Note) + "</div></div>";

  /* THE STAGE DECLARES NO BACKGROUND, so nothing is painted over the picture the renderer drew. A
   * quad with no alpha reaches no pixel and costs no rectangle, which is what makes *transparent* a
   * value here rather than a mode. */
  out += "<div class=stage style=\"height:" + std::to_string(heightPx) + "px\">";
  if (showing.Selected >= 0 && showing.Selected < (int)shown.size()) {
    const Listed &one = cases[(size_t)shown[(size_t)showing.Selected]];
    out += "<div class=plate style=\"height:" + std::to_string(kBarPx) + "px\">" +
           Quoted(one.Suite) + " / " + Quoted(one.Name) + "</div>";
  }
  out += "</div>";

  out += "</div></body>";
  return out;
}

std::vector<Render::OverlayQuad> AsOverlay(const std::vector<Ui::Quad> &quads) {
  std::vector<Render::OverlayQuad> out;
  out.reserve(quads.size());
  for (const Ui::Quad &from : quads) {
    Render::OverlayQuad to;
    to.LeftPx = (float)from.X;
    to.TopPx = (float)from.Y;
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
    to.ClipLeftPx = (float)from.ClipX;
    to.ClipTopPx = (float)from.ClipY;
    to.ClipWidthPx = (float)from.ClipWidth;
    to.ClipHeightPx = (float)from.ClipHeight;
    to.RadiusPx = (float)from.Radius;
    to.Opacity = (float)from.Opacity;
    out.push_back(to);
  }
  return out;
}

} // namespace outshine::Viewer
