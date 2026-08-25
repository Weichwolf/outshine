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
  CHECK(citations >= 30,
        "the walk read the map's citations rather than a corner of them -- the CURRENT tables "
        "cite at least one line per non-green node, and a walk finding a handful has stopped "
        "parsing the shape they are written in");
  CHECK(lying.empty(),
        "**EVERY LINE THE MAP CITES SAYS WHAT THE MAP CLAIMS**: CLAUDE.md's CURRENT tables argue "
        "each colour from a file:line, and a citation that has drifted turns the argument into "
        "an assertion nobody can check -- the map is the work list, and a work list that lies "
        "about the tree is itself a finding (board:1762, 1855)");

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

  const std::regex cited(R"(`([^`]+)`\s*\((?:([A-Za-z0-9_]+\.(?:h|cpp)))?:(\d+))");
  const std::regex painted(R"(\n\s*class ([A-Za-z0-9_,]+) (?:wrong|unsure)\b)");
  std::vector<std::string> unjustified;
  size_t nodes = 0;
  for (auto found = std::sregex_iterator(map.begin(), map.end(), painted);
       found != std::sregex_iterator(); ++found) {
    const std::string list = (*found)[1].str();
    for (size_t from = 0; from <= list.size();) {
      const size_t comma = list.find(',', from);
      const std::string node =
          list.substr(from, comma == std::string::npos ? comma : comma - from);
      if (!node.empty()) {
        ++nodes;
        const size_t row = map.find("| `" + node + "` |");
        if (row == std::string::npos) {
          unjustified.push_back(node + " is painted non-green and no justification row names it");
        } else {
          const std::string says = map.substr(row, map.find('\n', row) - row);
          if (!std::regex_search(says, cited)) {
            unjustified.push_back(node + " is painted non-green and its reason cites no file:line");
          }
        }
      }
      if (comma == std::string::npos) { break; }
      from = comma + 1;
    }
  }
  Note("nodes painted red or amber", (double)nodes, "nodes");
  for (const std::string &one : unjustified) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(nodes >= 20, "the diagrams paint reds and ambers for this walk to judge");
  CHECK(unjustified.empty(),
        "**EVERY NON-GREEN NODE CARRIES A CITATION THE WALK READS**: a node the map paints red "
        "OR amber is named in a justification row and that row cites code by file:line -- a "
        "reason made of bare numbers is a reason nothing can check, and a question with no line "
        "to look at is an opinion (board:1768, 1777)");

  const std::regex counted(R"((\d+) `([^`]+)`)");
  std::vector<std::string> drifted;
  size_t recomputed = 0;
  size_t rows = 0;
  std::vector<std::string> malformed;
  size_t width = 0;
  for (const std::string &row : mapRows) {
    if (row.empty()) {
      width = 0;
      continue;
    }
    if (row.front() != '|') { continue; }
    size_t cells = 0;
    for (const char one : row) { cells += one == '|' ? 1 : 0; }
    if (row.back() != '|') {
      malformed.push_back("a table row runs past its last cell: " + row.substr(0, 60));
    } else if (row.find("---") != std::string::npos) {
      width = cells;
    } else if (width > 0 && cells != width) {
      malformed.push_back("a table row carries " + std::to_string(cells - 1) +
                          " cells under a header of " + std::to_string(width - 1) + ": " +
                          row.substr(0, 60));
    } else {
      ++rows;
    }
    if (row.find("---") != std::string::npos) { continue; }
    std::smatch where;
    if (!std::regex_search(row, where, cited) || where[2].str().empty()) { continue; }
    const std::vector<std::string> *const held = Held(where[2].str());
    if (held == nullptr) { continue; }
    std::string whole;
    for (const std::string &line : *held) { whole += line + "\n"; }
    for (auto one = std::sregex_iterator(row.begin(), row.end(), counted);
         one != std::sregex_iterator(); ++one) {
      ++recomputed;
      const std::string token = (*one)[2].str();
      const size_t wanted = (size_t)std::stoul((*one)[1].str());
      size_t found = 0;
      for (size_t at = whole.find(token); at != std::string::npos;
           at = whole.find(token, at + 1)) {
        ++found;
      }
      if (found != wanted) {
        drifted.push_back(where[2].str() + " carries " + std::to_string(found) + " of '" + token +
                          "' where the map says " + std::to_string(wanted));
      }
    }
  }
  Note("table rows walked", (double)rows, "rows");
  Note("counts the map states and this walk recomputes", (double)recomputed, "counts");
  for (const std::string &one : malformed) { std::printf("FOUND %s\n", one.c_str()); }
  for (const std::string &one : drifted) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(rows >= 10, "the map carries tables for this walk to judge");
  CHECK(malformed.empty(),
        "**A TABLE ROW ENDS AT ITS OWN CELL**: a row that runs past its last `|` swallows the "
        "paragraph beneath it into a phantom column, and a rule a reader cannot parse is a rule "
        "that is not there (board:1775)");
  CHECK(recomputed >= 4, "the map states counts for this walk to recompute");
  CHECK(drifted.empty(),
        "**AND A NUMBER IN THE MAP IS RECOMPUTED BY THE WALK**: a count standing beside a "
        "citation is a claim, and a stale count reads exactly like a fresh one (board:1779)");

  Covers("IV.12 CLAUDE.md argues from the tree: every file:line it cites exists and carries the "
         "text quoted beside it, every path it names is in the tree, every non-green node has a "
         "justification row that cites code, and every count in such a row is recomputed "
         "(board:1762, 1768, 1775, 1777, 1779, 1855)");
  return Report();
}
