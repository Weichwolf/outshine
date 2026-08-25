#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

namespace {

struct Citation {
  std::string Said;
  std::string File;
  size_t Line = 0;
};

[[nodiscard]] std::string Slurp(const std::filesystem::path &path) {
  std::ifstream reading(path, std::ios::binary);
  std::ostringstream all;
  all << reading.rdbuf();
  return all.str();
}

// Lines() drops empty lines, which is right for a command's answer and wrong for a file: a
// citation is a line NUMBER, and every blank one counts.
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

[[nodiscard]] std::string Found(const std::string &basename) {
  for (const auto &entry : std::filesystem::recursive_directory_iterator(".")) {
    if (!entry.is_regular_file()) { continue; }
    const std::string path = entry.path().string();
    if (path.find("/.git/") != std::string::npos) { continue; }
    if (entry.path().filename().string() == basename) { return path; }
  }
  return std::string();
}

[[nodiscard]] std::vector<Citation> CitedBy(const std::string &map) {
  std::vector<Citation> out;
  for (size_t at = map.find("` ("); at != std::string::npos; at = map.find("` (", at + 1)) {
    const size_t opens = map.rfind('`', at - 1);
    if (opens == std::string::npos) { continue; }
    const size_t colon = map.find(':', at);
    const size_t closes = map.find(')', at);
    if (colon == std::string::npos || closes == std::string::npos || colon > closes) { continue; }
    const std::string file = map.substr(at + 3, colon - at - 3);
    if (file.find('.') == std::string::npos || file.find(' ') != std::string::npos) { continue; }
    std::string digits;
    for (size_t step = colon + 1; step < closes; ++step) {
      if (map[step] < '0' || map[step] > '9') { break; }
      digits += map[step];
    }
    if (digits.empty()) { continue; }
    out.push_back(Citation{map.substr(opens + 1, at - opens - 1), file,
                           (size_t)std::stoul(digits)});
  }
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string map = Slurp("CLAUDE.md");
  CHECK(!map.empty(), "the map this claim judges is where it is declared to be");

  const std::vector<Citation> cited = CitedBy(map);
  std::vector<std::string> lying;
  size_t reached = 0;
  for (const Citation &one : cited) {
    const std::string path = Found(one.File);
    if (path.empty()) {
      lying.push_back(one.File + " is cited and no file of that name is in the tree");
      continue;
    }
    const std::vector<std::string> held = Rows(Slurp(path));
    if (one.Line == 0 || one.Line > held.size()) {
      lying.push_back(one.File + ":" + std::to_string(one.Line) + " is past the file's " +
                      std::to_string(held.size()) + " lines");
      continue;
    }
    ++reached;
    if (held[one.Line - 1].find(one.Said) != std::string::npos) { continue; }
    lying.push_back(one.File + ":" + std::to_string(one.Line) + " is cited as '" + one.Said +
                    "' and says '" + held[one.Line - 1] + "'");
  }

  Note("citations the map carries", (double)cited.size(), "citations");
  Note("citations whose file and line exist", (double)reached, "citations");
  for (const std::string &one : lying) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(cited.size() >= 20,
        "the walk read the map's citations rather than a corner of them -- CLAUDE.md's CURRENT "
        "tables cite one line per red and per amber node, and a walk finding a handful has "
        "stopped parsing the shape they are written in");
  CHECK(lying.empty(),
        "**AND EVERY LINE THE MAP CITES SAYS WHAT THE MAP CLAIMS**: CLAUDE.md's CURRENT tables "
        "argue each colour from a file:line, and a citation that has drifted turns the argument "
        "into an assertion nobody can check -- the map is the work list, and a work list that "
        "lies about the tree is itself a finding (board:1855)");

  Covers("IV.34 every file:line CLAUDE.md's CURRENT tables cite exists and carries the text the "
         "table quotes, so a node's colour stays argued from the tree rather than from a "
         "citation that has drifted (board:1855)");
  return Report();
}
