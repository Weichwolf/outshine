#include <cstdio>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const std::filesystem::path &path) {
  std::FILE *file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) { return std::string(); }
  std::string into;
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  return into;
}

[[nodiscard]] std::filesystem::path Resolve(const std::string &name) {
  for (const auto &entry : std::filesystem::recursive_directory_iterator("src")) {
    if (entry.is_regular_file() && entry.path().filename() == name) { return entry.path(); }
  }
  return {};
}

[[nodiscard]] std::vector<std::string> Lines(const std::filesystem::path &path) {
  std::vector<std::string> lines;
  std::string held = Slurp(path);
  size_t from = 0;
  for (size_t at = held.find('\n'); at != std::string::npos; at = held.find('\n', from)) {
    lines.push_back(held.substr(from, at - from));
    from = at + 1;
  }
  lines.push_back(held.substr(from));
  return lines;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // a colour on the map is only worth what its stated reason is worth: board:1762 found
  // TilePool red for "spells camera and LOD" when it spells neither, and a justification
  // that has gone stale teaches the reader to discount the colours that still hold.
  const std::string document = Slurp("CLAUDE.md");
  CHECK(!document.empty(), "the map is where the colours are adjudicated");

  const std::regex cited(R"(`([^`]+)`\s*\((?:([A-Za-z0-9_]+\.(?:h|cpp)))?:(\d+)(?:-(\d+))?\))");
  std::string carried;
  size_t citations = 0;
  std::vector<std::string> stale;

  for (auto found = std::sregex_iterator(document.begin(), document.end(), cited);
       found != std::sregex_iterator(); ++found) {
    const std::smatch &one = *found;
    const std::string symbol = one[1].str();
    if (!one[2].str().empty()) { carried = one[2].str(); }
    if (carried.empty()) { continue; }
    ++citations;

    const std::filesystem::path source = Resolve(carried);
    if (source.empty()) {
      stale.push_back(carried + " is cited and no file of that name stands in src/");
      continue;
    }
    const std::vector<std::string> lines = Lines(source);
    const size_t first = (size_t)std::stoul(one[3].str());
    const size_t last = one[4].str().empty() ? first : (size_t)std::stoul(one[4].str());
    if (last == 0 || last > lines.size()) {
      stale.push_back(carried + ":" + one[3].str() + " is past the end of a " +
                      std::to_string(lines.size()) + "-line file");
      continue;
    }
    bool says = false;
    for (size_t at = first; at <= last && !says; ++at) {
      says = lines[at - 1].find(symbol) != std::string::npos;
    }
    if (!says) {
      // board:1824: a citation goes stale every time an edit above it inserts a line, and this
      // walk reported only that it had. Twenty-four citations, hand-corrected twice in one
      // session. The symbol is still in the file; saying WHERE turns a red into a one-line fix
      // and costs one more pass over lines already in memory.
      size_t moved = 0;
      for (size_t at = 1; at <= lines.size() && moved == 0; ++at) {
        if (lines[at - 1].find(symbol) != std::string::npos) { moved = at; }
      }
      stale.push_back(carried + ":" + one[3].str() + " is cited for `" + symbol +
                      "', and that line reads: " + lines[first - 1] +
                      (moved == 0 ? "  -- and the symbol is nowhere in the file"
                                  : "  -- the symbol is on line " + std::to_string(moved)));
    }
  }

  // board:1768: a threshold fitted to the count that happens to exist judges nothing. The
  // bar is the map's OWN claim -- every node the diagram paints red must appear in the
  // paragraph that justifies the reds, and every justification must cite code.
  // board:1777: amber says the FORM is in question. A question with no line to look at is
  // an opinion, and an opinion on the map reads like a finding.
  const std::regex painted(R"(\n\s*class ([A-Za-z0-9_,]+) (?:wrong|unsure)\b)");
  std::vector<std::string> unjustified;
  size_t nodes = 0;
  for (auto red = std::sregex_iterator(document.begin(), document.end(), painted);
       red != std::sregex_iterator(); ++red) {
    std::string list = (*red)[1].str();
    size_t from = 0;
    while (from <= list.size()) {
      const size_t comma = list.find(',', from);
      const std::string node = list.substr(from, comma == std::string::npos ? comma : comma - from);
      if (!node.empty()) {
        ++nodes;
        const size_t row = document.find("| `" + node + "` |");
        if (row == std::string::npos) {
          unjustified.push_back(node + " is painted red and the paragraph does not name it");
        } else {
          const size_t ends = document.find('\n', row);
          const std::string says = document.substr(row, ends - row);
          if (!std::regex_search(says, cited)) {
            unjustified.push_back(node + " is painted red and its reason cites no file:line");
          }
        }
      }
      if (comma == std::string::npos) { break; }
      from = comma + 1;
    }
  }

  // board:1775: the justification table swallowed the render plan's binding invariant into
  // a phantom third column, because a row that does not end at its own `|` runs into the
  // paragraph beneath it. A rule a reader cannot parse is a rule that is not there.
  size_t rows = 0;
  std::vector<std::string> malformed;
  {
    size_t width = 0;
    size_t from = 0;
    for (size_t line = document.find('\n'); ; line = document.find('\n', from)) {
      const std::string said =
          document.substr(from, line == std::string::npos ? line : line - from);
      if (!said.empty() && said.front() == '|') {
        size_t cells = 0;
        for (const char c : said) { cells += c == '|' ? 1 : 0; }
        if (said.back() != '|') {
          malformed.push_back("a table row runs past its last cell: " + said.substr(0, 60));
        } else if (said.find("---") != std::string::npos) {
          width = cells;
        } else if (width > 0 && cells != width) {
          malformed.push_back("a table row carries " + std::to_string(cells - 1) +
                              " cells under a header of " + std::to_string(width - 1) + ": " +
                              said.substr(0, 60));
        } else {
          ++rows;
        }
      } else if (said.empty()) {
        width = 0;
      }
      if (line == std::string::npos) { break; }
      from = line + 1;
    }
  }
  Note("table rows walked", (double)rows, "rows");
  for (const std::string &one : malformed) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(rows >= 10, "the map carries tables for this walk to judge");
  CHECK(malformed.empty(),
        "**A TABLE ROW ENDS AT ITS OWN CELL**: a row that runs past its last `|` swallows "
        "the paragraph beneath it into a phantom column, and a rule a reader cannot parse "
        "is a rule that is not there (board:1775)");

  Note("citations judged", (double)citations, "citations");
  Note("nodes painted red or amber", (double)nodes, "nodes");
  for (const std::string &one : stale) { std::printf("FOUND %s\n", one.c_str()); }
  for (const std::string &one : unjustified) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(nodes >= 20, "the diagram paints reds and ambers for this walk to judge");
  CHECK(unjustified.empty(),
        "**EVERY NON-GREEN NODE CARRIES A CITATION THE CLAIM WALKS**: a node the map paints "
        "red OR amber is named in a justification row, and that row cites code by file:line "
        "-- a reason made of bare numbers is a reason nothing can check, and a question with "
        "no line to look at is an opinion (board:1768, 1777)");
  CHECK(citations >= nodes,
        "and there is at least one citation per red, so the bar follows the map rather than "
        "the count that happens to exist");
  CHECK(stale.empty(),
        "**EVERY COLOUR CITES A LINE THAT SAYS IT**: a red node names what makes it red at "
        "file:line, and that line still spells it -- a justification that has drifted is "
        "itself a finding (board:1762)");

  // board:1779: a count standing beside a citation is a claim the walk stepped over, and the
  // first attempt at this claim matched the SENTENCE rather than parsing the cell -- so a
  // word changed in the prose broke a check about numbers. The row is parsed now: every
  // `N `token`` in a justification row is counted in the file that row cites.
  const std::regex counted(R"((\d+) `([^`]+)`)");
  size_t recomputed = 0;
  std::vector<std::string> drifted;
  {
    size_t from = 0;
    for (size_t line = document.find('\n'); ; line = document.find('\n', from)) {
      const std::string row =
          document.substr(from, line == std::string::npos ? line : line - from);
      if (!row.empty() && row.front() == '|' && row.find("---") == std::string::npos) {
        std::smatch where;
        std::string in;
        if (std::regex_search(row, where, cited) && !where[2].str().empty()) {
          in = where[2].str();
        }
        for (auto one = std::sregex_iterator(row.begin(), row.end(), counted);
             one != std::sregex_iterator(); ++one) {
          const std::string token = (*one)[2].str();
          if (in.empty()) { continue; }
          ++recomputed;
          const std::filesystem::path source = Resolve(in);
          if (source.empty()) {
            drifted.push_back(in + " is counted and no file of that name stands in src/");
            continue;
          }
          const std::string text = Slurp(source);
          const size_t wanted = (size_t)std::stoul((*one)[1].str());
          size_t found = 0;
          for (size_t at = text.find(token); at != std::string::npos;
               at = text.find(token, at + 1)) {
            ++found;
          }
          if (found != wanted) {
            drifted.push_back(in + " carries " + std::to_string(found) + " of '" + token +
                              "' where the map says " + std::to_string(wanted));
          }
        }
      }
      if (line == std::string::npos) { break; }
      from = line + 1;
    }
  }
  Note("counts the map states and this walk recomputes", (double)recomputed, "counts");
  for (const std::string &one : drifted) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(recomputed >= 4, "the map states counts for this walk to recompute");
  CHECK(drifted.empty(),
        "**A NUMBER IN THE MAP IS RECOMPUTED BY THE WALK**: a count standing beside a "
        "citation is a claim, and a stale count reads exactly like a fresh one (board:1779)");

  Covers("IV.12 every file:line a document cites resolves to a line that carries the symbol "
         "cited, so a stale justification cannot outlive the code it judges (board:1762)");
  return Report();
}
