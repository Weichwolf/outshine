#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const std::string &path) {
  std::ifstream reading(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(reading)), std::istreambuf_iterator<char>());
}

[[nodiscard]] std::string Field(const std::string &text, size_t from, const std::string &key) {
  const size_t at = text.find("\"" + key + "\":", from);
  if (at == std::string::npos) { return std::string(); }
  size_t begins = text.find_first_not_of(" \t", at + key.size() + 3);
  if (begins == std::string::npos) { return std::string(); }
  const size_t ends = text.find_first_of(",\n", begins);
  return text.substr(begins, ends - begins);
}

[[nodiscard]] std::string Block(const std::string &text, const std::string &key) {
  const size_t at = text.find("\"" + key + "\":");
  if (at == std::string::npos) { return std::string(); }
  const size_t opens = text.find('"', at + key.size() + 3);
  if (opens == std::string::npos) { return std::string(); }
  const size_t closes = text.find("\",\n", opens + 1);
  return text.substr(opens, closes - opens);
}

// 4.5 -> "4.5 %", 8.0 -> "8.0 %": the origin block prints a percentage with one decimal, and
// the rule declares a rise over run.
[[nodiscard]] std::string AsPercent(double share) {
  char said[32];
  std::snprintf(said, sizeof said, "%.1f %%", share * 100.0);
  return std::string(said);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1794: the asset carried maxGradient 6.0 % for primary and 7.0 % for secondary while
  // citing RAL, and RAL prints neither -- its ladder is 4.5 / 5.5 / 6.5 / 8.0. A citation that
  // does not reproduce the number it justifies is not an origin, and the only thing that keeps
  // one from drifting away again is a walk.
  const std::string asset = Slurp("src/assets/world/vegetation.json");
  CHECK(!asset.empty(), "the shipped class table is where the tree keeps it");

  const std::string gradients = Block(asset, "osmGradientOrigin");
  const std::string radii = Block(asset, "osmRadiusOrigin");
  Note("characters of gradient origin", (double)gradients.size(), "chars");
  Note("characters of radius origin", (double)radii.size(), "chars");
  CHECK(gradients.size() > 400 && radii.size() > 400,
        "and both numbers it declares per road class carry an origin block long enough to be "
        "one");

  // the five carriageway kinds: every one of them declares both numbers, and both must appear
  // in the block that justifies them.
  static const char *const kCarriageways[] = {
      "motorway", "trunk", "primary", "secondary", "tertiary"};
  std::vector<std::string> silent;
  size_t walked = 0;
  for (const char *const kind : kCarriageways) {
    const size_t at = asset.find(std::string("\"kind\": \"") + kind + "\",");
    if (at == std::string::npos) {
      silent.push_back(std::string(kind) + " is not in the table at all");
      continue;
    }
    const std::string grade = Field(asset, at, "maxGradient");
    const std::string radius = Field(asset, at, "minRadiusM");
    if (grade.empty() || radius.empty()) {
      silent.push_back(std::string(kind) + " declares no gradient or no radius");
      continue;
    }
    ++walked;
    const std::string percent = AsPercent(std::atof(grade.c_str()));
    const std::string metres = radius + " m";
    const bool gradeSaid = gradients.find(percent) != std::string::npos;
    const bool radiusSaid = radii.find(metres) != std::string::npos;
    std::printf("NOTE %-10s gradient %-7s -> \"%s\" %s   radius %-4s -> \"%s\" %s\n",
                kind,
                grade.c_str(),
                percent.c_str(),
                gradeSaid ? "printed" : "ABSENT",
                radius.c_str(),
                metres.c_str(),
                radiusSaid ? "printed" : "ABSENT");
    if (!gradeSaid) {
      silent.push_back(std::string(kind) + " declares " + percent +
                       " and its origin block prints no such figure");
    }
    if (!radiusSaid) {
      silent.push_back(std::string(kind) + " declares " + metres +
                       " and its origin block prints no such figure");
    }
  }
  Note("carriageway kinds walked", (double)walked, "of 5");
  for (const std::string &one : silent) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(walked == 5,
        "all five carriageway kinds declare both a gradient and a design minimum radius");
  CHECK(silent.empty(),
        "**EVERY CLASS NUMBER IS PRINTED BY ITS OWN ORIGIN**: a number whose citation does not "
        "reproduce it is not a number with an origin, it is a number with a paragraph beside "
        "it -- and this asset carried three of those, one of them cited to a table that has no "
        "such row (board:1794)");

  Covers("IV.19 the gradient and the design minimum radius each carriageway class declares are "
         "figures its own origin block prints, so a value cannot drift away from the table it "
         "cites (board:1794)");
  return Report();
}
