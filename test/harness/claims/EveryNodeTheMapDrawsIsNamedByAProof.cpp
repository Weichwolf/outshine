#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const std::string &path) {
  std::ifstream reading(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(reading)),
                     std::istreambuf_iterator<char>());
}

// board:1806: CLAUDE.md's CURRENT class diagram is the tree's own map, and a node it draws that
// no proof ever names is a layer nobody has checked. Seven such nodes stood when this was
// written -- OsmField, BuildingField, WaterField, RegionForge, StreetField, Ephemeris,
// WebTileSource, 1410 lines between them -- and the reason they could stand is exactly that
// nothing walked the map.
//
// The node list is DERIVED from the file rather than copied beside it, so a node added to the
// diagram is covered the moment it is drawn.
[[nodiscard]] std::vector<std::string> NodesOfTheCurrentMap(const std::string &text) {
  // only the mermaid FENCE, not the section: the justification tables under the graph are
  // prose, and a parser that read them would call every capitalised word a node.
  const size_t section = text.find("## Class structure (CURRENT)");
  std::vector<std::string> names;
  if (section == std::string::npos) { return names; }
  const size_t opens = text.find("```mermaid", section);
  if (opens == std::string::npos) { return names; }
  const size_t from = text.find('\n', opens);
  const size_t to = text.find("```", from);
  if (from == std::string::npos || to == std::string::npos) { return names; }
  const std::string block = text.substr(from, to - from);

  // mermaid draws a node either bare (`Ground --> Forest`) or with a label
  // (`World["World -- quadtree LOD"]`). Both spellings start with a capital and run on in
  // letters and digits; everything else on a line is punctuation, a class directive or prose.
  // a node's LABEL is prose -- `World["World -- quadtree LOD - admission - kerbs"]` -- and the
  // words in it are not nodes. Only the identifier is, and it is what stands before the
  // bracket. Skipping the label is what tells BVH, LOD and XML from a class (board:1806).
  for (size_t at = 0; at < block.size();) {
    if (block[at] == '[') {
      const size_t closes = block.find(']', at);
      at = closes == std::string::npos ? block.size() : closes + 1;
      continue;
    }
    if (block[at] == '"') {
      const size_t closes = block.find('"', at + 1);
      at = closes == std::string::npos ? block.size() : closes + 1;
      continue;
    }
    if (!(block[at] >= 'A' && block[at] <= 'Z')) {
      ++at;
      continue;
    }
    const size_t begins = at;
    while (at < block.size() && ((block[at] >= 'A' && block[at] <= 'Z') ||
                                 (block[at] >= 'a' && block[at] <= 'z') ||
                                 (block[at] >= '0' && block[at] <= '9'))) {
      ++at;
    }
    const std::string word = block.substr(begins, at - begins);
    const bool before = begins == 0 || block[begins - 1] == '\n' || block[begins - 1] == ' ' ||
                        block[begins - 1] == '>' || block[begins - 1] == '&' ||
                        block[begins - 1] == '|';
    const bool after = at >= block.size() || block[at] == '[' || block[at] == ' ' ||
                       block[at] == '\n' || block[at] == '-' || block[at] == '&';
    if (before && after && word.size() > 1) { names.push_back(word); }
  }
  return names;
}

// mermaid's own vocabulary and this diagram's display labels: `TD` is the direction token, and
// two nodes carry a label that is not the class's name because the name is taken in the same
// graph. They are excluded BY NAME so the exclusion is visible rather than a silent filter.
[[nodiscard]] bool IsTheDiagramsOwnWord(const std::string &word) {
  static const char *const kMermaid[] = {"TD", "LR", "TB", "RL", "BT"};
  static const char *const kLabels[] = {"LayoutUi", "SceneStore"};
  for (const char *const one : kMermaid) {
    if (word == one) { return true; }
  }
  for (const char *const one : kLabels) {
    if (word == one) { return true; }
  }
  return false;
}

// board:1806, reopened: this claim walked every source under test/ INCLUDING ITSELF, and its
// own comment names five of the eight classes it was written for. Six twins removed from the
// tree and it reported one unproven node. Naming a class in PROSE is not proving it -- a proof
// names a class by USING it -- so the search runs over code with the commentary and the string
// literals taken out. That fixes the self-reference and every other file's prose at once.
[[nodiscard]] std::string CodeOnly(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (size_t at = 0; at < text.size();) {
    // an #include is CODE, and the strongest evidence a proof reaches a file at all -- it is
    // kept whole even though the path it names is a string literal.
    const bool lineStart = at == 0 || text[at - 1] == '\n';
    if (lineStart && text.compare(at, 8, "#include") == 0) {
      while (at < text.size() && text[at] != '\n') { out.push_back(text[at++]); }
      continue;
    }
    if (text[at] == '/' && at + 1 < text.size() && text[at + 1] == '/') {
      while (at < text.size() && text[at] != '\n') { ++at; }
      continue;
    }
    if (text[at] == '/' && at + 1 < text.size() && text[at + 1] == '*') {
      at += 2;
      while (at + 1 < text.size() && !(text[at] == '*' && text[at + 1] == '/')) { ++at; }
      at = at + 2 <= text.size() ? at + 2 : text.size();
      out.push_back(' ');
      continue;
    }
    if (text[at] == 'R' && at + 1 < text.size() && text[at + 1] == '"') {
      const size_t opens = text.find('(', at + 2);
      if (opens != std::string::npos) {
        const std::string tag = text.substr(at + 2, opens - (at + 2));
        const size_t closes = text.find(")" + tag + "\"", opens);
        at = closes == std::string::npos ? text.size() : closes + tag.size() + 2;
        out.push_back(' ');
        continue;
      }
    }
    if (text[at] == '"' || text[at] == '\'') {
      const char quote = text[at];
      ++at;
      while (at < text.size() && text[at] != quote) {
        at += text[at] == '\\' ? 2 : 1;
      }
      at = at < text.size() ? at + 1 : at;
      out.push_back(' ');
      continue;
    }
    out.push_back(text[at]);
    ++at;
  }
  return out;
}

[[nodiscard]] bool NamedUnderTest(const std::string &node) {
  for (const auto &entry : std::filesystem::recursive_directory_iterator("test")) {
    if (!entry.is_regular_file()) { continue; }
    const std::string suffix = entry.path().extension().string();
    if (suffix != ".cpp" && suffix != ".h") { continue; }
    const std::string text = CodeOnly(Slurp(entry.path().string()));
    for (size_t at = text.find(node); at != std::string::npos; at = text.find(node, at + 1)) {
      const bool before = at == 0 || !(std::isalnum((unsigned char)text[at - 1]) ||
                                       text[at - 1] == '_');
      const size_t ends = at + node.size();
      const bool after = ends >= text.size() || !(std::isalnum((unsigned char)text[ends]) ||
                                                  text[ends] == '_');
      if (before && after) { return true; }
    }
  }
  return false;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string map = Slurp("CLAUDE.md");
  CHECK(!map.empty(), "the map is where the tree keeps it");

  std::vector<std::string> nodes = NodesOfTheCurrentMap(map);
  std::sort(nodes.begin(), nodes.end());
  nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
  Note("nodes the CURRENT class map draws", (double)nodes.size(), "nodes");
  CHECK(nodes.size() > 40,
        "the walk read the map rather than a corner of it -- a parse that found a handful "
        "would pass this claim by seeing nothing");

  std::vector<std::string> unproven, excluded;
  for (const std::string &node : nodes) {
    if (IsTheDiagramsOwnWord(node)) {
      excluded.push_back(node);
      continue;
    }
    if (!NamedUnderTest(node)) { unproven.push_back(node); }
  }
  Note("of them, the diagram's own words and display labels", (double)excluded.size(), "words");
  for (const std::string &one : excluded) { std::printf("NOTE excluded by name: %s\n", one.c_str()); }
  Note("nodes no source under test/ names", (double)unproven.size(), "nodes");
  for (const std::string &one : unproven) { std::printf("FOUND %s is drawn and unproven\n", one.c_str()); }

  CHECK(unproven.empty(),
        "**EVERY NODE THE MAP DRAWS IS NAMED BY A PROOF**: the unit mirror IS the layering "
        "proof, so a class the tree draws in its own architecture and no test ever mentions is "
        "a layer nobody has checked -- and it looks exactly like a layer that works "
        "(board:1806)");

  Covers("IV.18 every node of CLAUDE.md's CURRENT class map is named by at least one source "
         "under test/, with the node list derived from the map itself and the diagram's own "
         "vocabulary excluded by name (board:1806)");
  return Report();
}
