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
      stale.push_back(carried + ":" + one[3].str() + " is cited for `" + symbol +
                      "', and that line reads: " + lines[first - 1]);
    }
  }

  // board:1768: a threshold fitted to the count that happens to exist judges nothing. The
  // bar is the map's OWN claim -- every node the diagram paints red must appear in the
  // paragraph that justifies the reds, and every justification must cite code.
  const std::regex painted(R"(\n\s*class ([A-Za-z0-9_,]+) wrong\b)");
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

  Note("citations judged", (double)citations, "citations");
  Note("nodes painted red", (double)nodes, "nodes");
  for (const std::string &one : stale) { std::printf("FOUND %s\n", one.c_str()); }
  for (const std::string &one : unjustified) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(nodes >= 3, "the diagram paints reds for this walk to judge");
  CHECK(unjustified.empty(),
        "**EVERY NON-GREEN NODE CARRIES A CITATION THE CLAIM WALKS**: a node the map paints "
        "red is named in the paragraph that justifies the reds, and that justification cites "
        "code by file:line -- a reason made of bare numbers is a reason nothing can check "
        "(board:1768)");
  CHECK(citations >= nodes,
        "and there is at least one citation per red, so the bar follows the map rather than "
        "the count that happens to exist");
  CHECK(stale.empty(),
        "**EVERY COLOUR CITES A LINE THAT SAYS IT**: a red node names what makes it red at "
        "file:line, and that line still spells it -- a justification that has drifted is "
        "itself a finding (board:1762)");

  Covers("IV.12 every file:line a document cites resolves to a line that carries the symbol "
         "cited, so a stale justification cannot outlive the code it judges (board:1762)");
  return Report();
}
