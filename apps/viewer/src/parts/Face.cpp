#include "Face.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace outshine::Viewer {

constexpr double kCorpusShare = 0.15;
constexpr double kCaseShare = 0.22;
constexpr double kRowEm = 1.3;
constexpr double kHeadEm = 1.8;
constexpr double kStatusEm = 1.6;

constexpr double kLinesTall = 45.0;

namespace {

std::string Flattened(std::string path) {
  for (char &c : path) {
    if (c == '/') { c = '-'; }
  }
  return path;
}

std::string Quoted(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) { out.push_back((c == '"' || c == '<' || c == '&') ? '_' : c); }
  return out;
}

}

std::vector<Listed> Cases(const std::string &under, const std::string &prepared) {
  std::vector<Listed> found;
  std::error_code walking;
  for (std::filesystem::recursive_directory_iterator it(under, walking), end; it != end;
       it.increment(walking)) {
    if (walking) { break; }
    if (!it->is_regular_file(walking) || it->path().filename() != "manifest.json") { continue; }
    const std::string relative = it->path().parent_path().string();

    if (relative.find("/prepare") != std::string::npos) { continue; }
    Listed one;
    one.Name = it->path().parent_path().filename().string();
    one.Prepared = prepared + "/" + Flattened(relative);

    const std::string inside =
        relative.size() > under.size() + 1 ? relative.substr(under.size() + 1) : relative;
    const size_t first = inside.find('/');
    const size_t second = first == std::string::npos ? first : inside.find('/', first + 1);
    one.Suite = second == std::string::npos ? inside : inside.substr(0, second);
    one.Ready =
        std::filesystem::exists(std::filesystem::path(one.Prepared) / "manifest.json", walking);
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

double RootEmPx(int heightPx) {
  return (double)heightPx / kLinesTall;
}

double ColumnsWidth(int widthPx) {
  return (kCorpusShare + kCaseShare) * (double)widthPx;
}

std::string Style(void) {
  const auto share = [](double of) { return std::to_string(of * 100.0) + "%"; };
  const auto em = [](double of) { return std::to_string(of) + "em"; };

  return std::string("html, body { height: 100% }\n") +
         "body { margin: 0; font-family: sheet; color: #c8d0d8 }\n" +
         ".frame { display: flex; flex-direction: row; width: 100%; height: 100% }\n" +
         ".corpora { display: flex; flex-direction: column; background: #0f1317; width: " +
         share(kCorpusShare) +
         "; height: 100%; border-width: 0 1px 0 0; border-color: #232c35 }\n"
         ".cases { display: flex; flex-direction: column; background: #12161b; width: " +
         share(kCaseShare) +
         "; height: 100%; border-width: 0 1px 0 0; border-color: #232c35 }\n"
         ".head { background: #1a222a; padding: 0.3em 0.6em; box-sizing: border-box; color: "
         "#6f8090;"
         " height: " +
         em(kHeadEm) +
         "; border-width: 0 0 1px 0; border-color: #232c35 }\n"
         ".list { flex: 1 1 0%; overflow: auto }\n"
         ".row { padding: 0.1em 0.6em; box-sizing: border-box; height: " +
         em(kRowEm) +
         "; color: #9fb0bf }\n"
         ".row-on { padding: 0.1em 0.6em; box-sizing: border-box; height: " +
         em(kRowEm) +
         "; background: #2f6f9f; color: #f2f6fa }\n"
         ".row-out { padding: 0.1em 0.6em; box-sizing: border-box; height: " +
         em(kRowEm) +
         "; color: #6a7683 }\n"
         ".status { background: #1a222a; padding: 0.25em 0.6em; box-sizing: border-box; height: " +
         em(kStatusEm) +
         "; color: #93a1ad; border-width: 1px 0 0 0; border-color: #232c35 }\n"
         ".stage { flex: 1 1 0%; height: 100% }\n"
         ".plate { background: #10141880; padding: 0.25em 0.6em; box-sizing: border-box; height: " +
         em(kHeadEm) +
         "; color: #cfe0ee }\n"
         ".console { display: flex; flex-direction: column; background: #0b0e11; width: 100%;"
         " height: 100% }\n"
         ".code { padding: 0 0.6em; box-sizing: border-box; height: " +
         em(kRowEm) +
         "; color: #9fb0bf; white-space: pre }\n"
         ".verdict { background: #1a222a; padding: 0.3em 0.6em; box-sizing: border-box; height: " +
         em(kHeadEm) + "; color: #cfe0ee; border-width: 1px 0 0 0; border-color: #232c35 }\n";
}

std::string
Declaration(const std::vector<Listed> &cases, const Showing &showing, int widthPx, int heightPx) {
  const std::vector<int> shown = Filtered(cases, showing);
  const std::vector<std::string> suites = Suites(cases);
  (void)widthPx;

  std::string out = "<style>html { font-size: " + std::to_string(RootEmPx(heightPx)) + "px }\n";
  out += Style();
  out += "</style><body><div class=frame>";

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

  out += "<div class=cases><div class=head>CASE (" + std::to_string(shown.size()) + ")</div>";

  out += "<div class=list><div>";
  for (int at = 0; at < (int)shown.size(); ++at) {
    const Listed &one = cases[(size_t)shown[(size_t)at]];
    const char *style = at == showing.Selected ? "row-on" : (one.Ready ? "row" : "row-out");
    out += "<div class=" + std::string(style) + " data-action=\"select(" + std::to_string(at) +
           ")\">" + Quoted(one.Name) + "</div>";
  }
  out += "</div></div><div class=status>" +
         (shown.empty() ? std::string("NO CASES") : std::to_string(shown.size()) + " CASES") +
         "</div></div>";

  out += "<div class=stage>";
  if (showing.Selected >= 0 && showing.Selected < (int)shown.size()) {
    const Listed &one = cases[(size_t)shown[(size_t)showing.Selected]];
    out += "<div class=plate>" + Quoted(one.Suite) + " / " + Quoted(one.Name) + "</div>";
  }
  out += "</div></div></body>";
  return out;
}

outshine::Patch StageRegion(int widthPx, int heightPx) {
  outshine::Patch out;
  if (widthPx <= 0 || heightPx <= 0) { return out; }
  out.LeftFrac = kCorpusShare + kCaseShare;
  out.TopFrac = kHeadEm * RootEmPx(heightPx) / (double)heightPx;
  out.WidthFrac = 1.0 - out.LeftFrac;
  out.HeightFrac = 1.0 - out.TopFrac;
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

}

std::string EntryPath(const std::string &prepared) {
  bool haveManifest = false;
  const std::string manifest = Slurp(prepared + "/manifest.json", haveManifest);
  if (!haveManifest) { return {}; }
  const size_t at = manifest.find("\"entry\"");
  if (at == std::string::npos) { return {}; }
  const size_t open = manifest.find('"', manifest.find(':', at));
  const size_t close = manifest.find('"', open + 1);
  if (open == std::string::npos || close == std::string::npos) { return {}; }
  return prepared + "/" + manifest.substr(open + 1, close - open - 1);
}

std::string EntryOf(const std::string &prepared, bool &found) {
  bool haveManifest = false;
  const std::string manifest = Slurp(prepared + "/manifest.json", haveManifest);
  found = false;
  if (!haveManifest) { return {}; }

  const size_t at = manifest.find("\"entry\"");
  if (at == std::string::npos) { return {}; }
  const size_t open = manifest.find('"', manifest.find(':', at));
  const size_t close = manifest.find('"', open + 1);
  if (open == std::string::npos || close == std::string::npos) { return {}; }
  return Slurp(prepared + "/" + manifest.substr(open + 1, close - open - 1), found);
}

std::string LinkedSheets(const std::string &prepared) {
  std::string out;
  std::error_code walking;
  for (const auto &entry : std::filesystem::directory_iterator(prepared, walking)) {
    if (entry.path().extension() != ".css") { continue; }
    bool found = false;
    const std::string text = Slurp(entry.path().string(), found);
    if (found) { out += text + "\n"; }
  }
  return out;
}

Stands StandOf(const Listed &one) {
  Stands out;
  out.Under = one.Prepared;
  const std::string entry = EntryPath(one.Prepared);
  if (entry.empty()) {
    out.Why = "no manifest names an entry under " + one.Prepared;
    return out;
  }
  const std::filesystem::path named(entry);
  const std::string suffix = named.extension().string();
  if (suffix == ".gltf" || suffix == ".glb") {
    out.Uri = named.filename().string();
    return out;
  }

  bool found = false;
  const std::string held = EntryOf(one.Prepared, found);
  if (!found) {
    out.Why = named.filename().string() + " is named by the manifest and did not open";
    return out;
  }
  if (suffix == ".js") {
    out.Programme = held;
    return out;
  }
  out.Document = held;
  out.Style = LinkedSheets(one.Prepared);
  return out;
}

std::string Console(const std::string &title,
                    const std::string &source,
                    const std::string &verdict,
                    const char *why,
                    int widthPx,
                    int heightPx) {
  (void)widthPx;
  std::string out = "<style>html { font-size: " + std::to_string(RootEmPx(heightPx)) + "px }\n";
  out += Style();
  out += "</style><body><div class=console><div class=head>" + Quoted(title) +
         "</div><div class=list><div>";

  size_t at = 0;
  int lines = 0;
  const int most = RowsThatFit(heightPx);
  while (at < source.size() && lines < most) {
    const size_t end = source.find('\n', at);
    const std::string line =
        source.substr(at, (end == std::string::npos ? source.size() : end) - at);
    at = end == std::string::npos ? source.size() : end + 1;
    out += "<div class=code>" + Quoted(line) + "</div>";
    ++lines;
  }
  out += "</div></div><div class=verdict>" + Quoted(verdict) +
         (why != nullptr ? " -- " + Quoted(why) : "") + "</div></div></body>";
  return out;
}

}
