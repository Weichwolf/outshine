#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

namespace {

[[nodiscard]] std::string Slurp(const std::filesystem::path &path) {
  std::ifstream reading(path, std::ios::binary);
  std::ostringstream all;
  all << reading.rdbuf();
  return all.str();
}

// Lines() of the harness drops empty lines, which is right for a command's answer and wrong
// for a file: a citation is a line NUMBER and every blank one counts.
[[nodiscard]] std::vector<std::string> Rows(const std::string &text) {
  std::vector<std::string> out;
  size_t at = 0;
  for (;;) {
    const size_t stop = text.find('\n', at);
    out.push_back(text.substr(at, stop == std::string::npos ? stop : stop - at));
    if (stop == std::string::npos) { break; }
    at = stop + 1;
  }
  return out;
}

// One walk of the tree, one index: resolving each citation by its own walk is the tree read
// once per citation.
[[nodiscard]] std::map<std::string, std::string> TheTreesFilesByName() {
  std::map<std::string, std::string> byName;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(".")) {
    if (!entry.is_regular_file()) { continue; }
    const std::string path = entry.path().string();
    if (path.find("/.git/") != std::string::npos) { continue; }
    byName.emplace(entry.path().filename().string(), path);
  }
  return byName;
}

struct Citation {
  std::string Said;
  std::string File;
  size_t Line = 0;
};

// `symbol` (File.cpp:123) -- and the continuation form `symbol` (:456), which carries the file
// from the citation before it on the SAME line, the way a reader of the row does.
[[nodiscard]] std::vector<Citation> CitedBy(const std::string &row) {
  std::vector<Citation> out;
  std::string carried;
  for (size_t at = row.find("` ("); at != std::string::npos; at = row.find("` (", at + 1)) {
    const size_t opens = row.rfind('`', at - 1);
    const size_t colon = row.find(':', at);
    const size_t closes = row.find(')', at);
    if (opens == std::string::npos || colon == std::string::npos || closes == std::string::npos ||
        colon > closes) {
      continue;
    }
    const std::string file = row.substr(at + 3, colon - at - 3);
    if (!file.empty()) {
      if (file.find('.') == std::string::npos || file.find(' ') != std::string::npos) { continue; }
      carried = file;
    }
    if (carried.empty()) { continue; }
    std::string digits;
    for (size_t step = colon + 1; step < closes; ++step) {
      if (row[step] < '0' || row[step] > '9') { break; }
      digits += row[step];
    }
    if (digits.empty()) { continue; }
    out.push_back(Citation{row.substr(opens + 1, at - opens - 1), carried,
                           (size_t)std::stoul(digits)});
  }
  return out;
}

[[nodiscard]] bool NamesThisTree(const std::string &span) {
  for (const char *top : {"src/", "test/", "board/", "apps/", "tools/", "include/"}) {
    if (span.rfind(top, 0) == 0) { return true; }
  }
  return span.find('/') == std::string::npos && span.size() > 3 &&
         span.compare(span.size() - 3, 3, ".md") == 0 && span.find("NNNN") == std::string::npos;
}

[[nodiscard]] bool IsAPlaceholder(const std::string &span) {
  return span.find_first_of("<>* ") != std::string::npos ||
         span.find("\xE2\x80\xA6") != std::string::npos || span.find("://") != std::string::npos;
}

// `src/engine/Sim.{h,cpp}` is two paths in one tick, and a walk that resolves it whole
// resolves neither.
[[nodiscard]] std::vector<std::string> Expanded(const std::string &span) {
  const size_t opens = span.find('{');
  const size_t closes = span.find('}', opens == std::string::npos ? 0 : opens);
  if (opens == std::string::npos || closes == std::string::npos) { return {span}; }
  std::vector<std::string> out;
  const std::string before = span.substr(0, opens);
  const std::string after = span.substr(closes + 1);
  for (size_t at = opens + 1; at <= closes;) {
    const size_t comma = span.find(',', at);
    const size_t ends = comma == std::string::npos || comma > closes ? closes : comma;
    out.push_back(before + span.substr(at, ends - at) + after);
    at = ends + 1;
  }
  return out;
}

[[nodiscard]] std::string WithoutLineReference(const std::string &span) {
  const size_t colon = span.rfind(':');
  if (colon == std::string::npos || colon + 1 == span.size()) { return span; }
  for (size_t at = colon + 1; at < span.size(); ++at) {
    const char one = span[at];
    if (!(one >= '0' && one <= '9') && one != ',' && one != '-') { return span; }
  }
  return span.substr(0, colon);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string map = Slurp("CLAUDE.md");
  CHECK(map.size() > 1000, "the map this claim judges is where it is declared to be");
  const std::vector<std::string> mapRows = Rows(map);
  const std::map<std::string, std::string> byName = TheTreesFilesByName();
  std::map<std::string, std::vector<std::string>> read;

  const auto Held = [&](const std::string &file) -> const std::vector<std::string> * {
    const auto already = read.find(file);
    if (already != read.end()) { return &already->second; }
    const auto where = byName.find(file);
    if (where == byName.end()) { return nullptr; }
    return &read.emplace(file, Rows(Slurp(where->second))).first->second;
  };

  std::vector<std::string> lying;
  size_t citations = 0;
  for (const std::string &row : mapRows) {
    for (const Citation &one : CitedBy(row)) {
      ++citations;
      const std::vector<std::string> *const held = Held(one.File);
      if (held == nullptr) {
        lying.push_back(one.File + " is cited and no file of that name is in the tree");
        continue;
      }
      if (one.Line == 0 || one.Line > held->size()) {
        lying.push_back(one.File + ":" + std::to_string(one.Line) + " is past the file's " +
                        std::to_string(held->size()) + " lines");
        continue;
      }
      if ((*held)[one.Line - 1].find(one.Said) != std::string::npos) { continue; }
      size_t moved = 0;
      for (size_t at = 1; at <= held->size() && moved == 0; ++at) {
        if ((*held)[at - 1].find(one.Said) != std::string::npos) { moved = at; }
      }
      lying.push_back(one.File + ":" + std::to_string(one.Line) + " is cited as '" + one.Said +
                      "' and says '" + (*held)[one.Line - 1] + "'" +
                      (moved == 0 ? "  -- and the symbol is nowhere in the file"
                                  : "  -- the symbol is on line " + std::to_string(moved)));
    }
  }
  Note("file:line citations the map carries", (double)citations, "citations");
  for (const std::string &one : lying) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(lying.empty(),
        "**EVERY LINE THE MAP CITES SAYS WHAT THE MAP CLAIMS**: CLAUDE.md is TARGET now and its "
        "citations are few, but a citation that has drifted turns an argument into an assertion "
        "nobody can check (board:1762, 1855)");

  std::vector<std::string> absent;
  size_t paths = 0;
  for (size_t line = 1; line <= mapRows.size(); ++line) {
    const std::string &text = mapRows[line - 1];
    for (size_t at = text.find('`'); at != std::string::npos; at = text.find('`', at + 1)) {
      const size_t close = text.find('`', at + 1);
      if (close == std::string::npos) { break; }
      const std::string span = text.substr(at + 1, close - at - 1);
      at = close;
      if (!NamesThisTree(span) || IsAPlaceholder(span)) { continue; }
      for (const std::string &cited : Expanded(WithoutLineReference(span))) {
        ++paths;
        if (std::filesystem::exists(cited)) { continue; }
        absent.push_back("CLAUDE.md:" + std::to_string(line) + " cites " + cited +
                         ", which is not in the tree");
      }
    }
  }
  Note("paths the map cites into this tree", (double)paths, "citations");
  for (const std::string &one : absent) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(paths >= 20, "the map names paths into its own tree for this walk to resolve");
  CHECK(absent.empty(),
        "**AND EVERY PATH IT CITES IS IN THE TREE**: a tick that names a file is a claim with "
        "something under it, and a path that has moved reads exactly like one that has not");


  // What the tree IS lives in STATE.md, which every `make` regenerates. Its whole worth is that
  // no hand writes it: a CURRENT map drawn by hand cites file:line and every edit drifts it, so
  // the map spends its life being corrected instead of read. That worth survives exactly as long
  // as the committed file is what the generator produces, and this is the walk that says so.
  std::string generated;
  const int stated = Run("sh test/run.sh --state 2>/dev/null", generated);
  CHECK(stated == 0 && !generated.empty(),
        "the generator answers, so there is something to compare");
  const std::string committed = Slurp("STATE.md");
  CHECK(!committed.empty(),
        "STATE.md stands in the tree, where a reader looks for what outshine is");
  if (committed.empty() || generated.empty()) { return Report(); }

  Note("what the generator produced", (double)generated.size(), "bytes");
  Note("what the tree carries", (double)committed.size(), "bytes");
  CHECK(committed == generated,
        "**STATE.md IS WHAT THE GENERATOR PRODUCES**: it is CURRENT, and CURRENT is only worth "
        "reading while no hand has touched it -- a STATE.md edited by hand, or left behind by a "
        "build that did not run, is the very drift the generated map exists to end");


  // AND EVERY LINE UNDER PROVES IS A CLAIM A CASE MAKES. The page's worth is that it cannot be
  // edited into a lie, and the PROVES walk is the one place that could be told one by accident:
  // it accumulates from a line naming `Covers(` and stops at the next `);`, so a source that
  // merely MENTIONS those characters in a comment contributes prose the tree does not prove.
  // That is not hypothetical -- TheSourceCarriesNoCommentary.cpp:165 names `Covers("` while
  // explaining the rule it enforces, and its sentence stood on the page as a proven capability.
  //
  // So this walk rebuilds the set from the sources, anchored on the claim's own syntax -- a
  // `Covers("` beginning a STATEMENT -- and the page must carry exactly that set. It fails in
  // both directions: prose that is not a claim, and a claim the walk dropped.
  const auto flattened = [](std::string held) {
    const size_t opens = held.find("Covers(");
    if (opens != std::string::npos) { held.erase(0, opens + 7); }
    const size_t shuts = held.find(");");
    if (shuts != std::string::npos) { held.erase(shuts); }
    std::string out;
    bool spaced = true;
    for (const char one : held) {
      if (one == '"') { continue; }
      if (one == ' ' || one == '\t') {
        if (!spaced) { out.push_back(' '); }
        spaced = true;
        continue;
      }
      out.push_back(one);
      spaced = false;
    }
    while (!out.empty() && out.back() == ' ') { out.pop_back(); }
    return out.size() > 150 ? out.substr(0, 150) : out;
  };

  std::vector<std::string> claimed;
  for (const auto &entry : std::filesystem::recursive_directory_iterator("test")) {
    if (!entry.is_regular_file() || entry.path().extension() != ".cpp") { continue; }
    std::ifstream reading(entry.path());
    std::string line, gathering;
    bool holding = false;
    while (std::getline(reading, line)) {
      if (!holding) {
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line.compare(first, 8, "Covers(\"") != 0) { continue; }
        holding = true;
        gathering.clear();
      }
      gathering += line;
      if (line.find(");") != std::string::npos) {
        holding = false;
        const std::string made = flattened(gathering);
        if (!made.empty()) { claimed.push_back(made); }
      }
    }
  }
  std::sort(claimed.begin(), claimed.end());
  claimed.erase(std::unique(claimed.begin(), claimed.end()), claimed.end());

  std::vector<std::string> printed;
  {
    std::istringstream walking(generated);
    std::string line;
    bool under = false;
    while (std::getline(walking, line)) {
      if (line.rfind("PROVES", 0) == 0) { under = true; continue; }
      if (!under) { continue; }
      if (line.empty()) { break; }
      const size_t first = line.find_first_not_of(" \t");
      printed.push_back(first == std::string::npos ? line : line.substr(first));
    }
  }
  std::sort(printed.begin(), printed.end());

  std::vector<std::string> unproven, dropped;
  std::set_difference(printed.begin(), printed.end(), claimed.begin(), claimed.end(),
                      std::back_inserter(unproven));
  std::set_difference(claimed.begin(), claimed.end(), printed.begin(), printed.end(),
                      std::back_inserter(dropped));
  for (const std::string &one : unproven) { std::printf("PROSE  %s\n", one.c_str()); }
  for (const std::string &one : dropped) { std::printf("LOST   %s\n", one.c_str()); }
  Note("claims the sources make", (double)claimed.size(), "claims");
  Note("lines the page prints", (double)printed.size(), "lines");

  CHECK(claimed.size() >= 20,
        "the sources make claims for this walk to find, so a page matching an empty set would "
        "not pass by silence");
  CHECK(unproven.empty(),
        "**EVERY LINE UNDER PROVES IS A CLAIM A CASE MAKES**: the walk accumulates from a line "
        "naming Covers and stops at the next close, so a source that MENTIONS those characters "
        "in a comment contributes prose the tree does not prove. The one page that cannot be "
        "edited into a lie must not be told one by its own extractor");
  CHECK(dropped.empty(),
        "and it fails the other way too: a claim a case makes and the page does not print is a "
        "capability the reader cannot see, which is the same defect facing the other direction");

  Covers("IV.12 CLAUDE.md is TARGET and argues from the tree: every path it names is present and "
         "every line it cites carries the text quoted beside it. STATE.md is CURRENT and is byte "
         "for byte what run.sh --state produces, so the tree's own description cannot drift from "
         "the tree (board:1762, 1768, 1775, 1777, 1779, 1855)");
  return Report();
}
