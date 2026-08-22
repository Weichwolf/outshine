#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

struct Excused {
  const char *Layer;
  const char *Why;
};

const Excused kExcused[] = {
    {"src/render", "the device layer itself: its unit mirror is the device suite "
                   "render/outshine/shader, where MSL and its C++ twin are one test"},
    {"src/ground/tiles", "folded into unit/world -- the tiles are the world's own plumbing and "
                        "unit/world compiles them in its groups"},
    {"src/core/io", "folded into unit/core -- one layer, two directories by include depth"},
    {"src/assets", "declared data, not code"},
};

bool Excuse(const std::string &layer, std::string &why) {
  for (const Excused &one : kExcused) {
    if (layer == one.Layer) {
      why = one.Why;
      return true;
    }
  }
  return false;
}

bool HoldsSources(const std::filesystem::path &dir) {
  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cpp") { return true; }
  }
  return false;
}

bool HoldsTests(const std::filesystem::path &dir) {
  if (!std::filesystem::is_directory(dir)) { return false; }
  for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cpp") { return true; }
  }
  return false;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t layers = 0;
  size_t mirrored = 0;
  std::vector<std::string> naked;
  for (const auto &entry : std::filesystem::recursive_directory_iterator("src")) {
    if (!entry.is_directory()) { continue; }
    const std::string layer = entry.path().string();
    if (!HoldsSources(entry.path())) { continue; }
    ++layers;
    std::string why;
    if (Excuse(layer, why)) {
      std::printf("NOTE %s is excused -- %s\n", layer.c_str(), why.c_str());
      ++mirrored;
      continue;
    }
    const std::string mirror = "test/unit/" + layer.substr(4);
    if (HoldsTests(mirror)) {
      ++mirrored;
    } else {
      naked.push_back(layer);
    }
  }

  Note("source layers holding code", (double)layers, "directories");
  Note("layers mirrored or excused", (double)mirrored, "directories");
  for (const std::string &layer : naked) {
    std::printf("FOUND %s has no unit mirror and no declared excuse\n", layer.c_str());
  }
  CHECK(layers >= 15, "the walk saw the tree, not a corner of it");
  CHECK(naked.empty(),
        "**EVERY SOURCE LAYER HAS ITS UNIT MIRROR.** unit/ mirrors src/ ALWAYS -- the mirror "
        "is the regression gate (board:1601), so a layer without one is a layer whose breakage "
        "waits for the long suites. An excuse exists only with a named reason beside it");

  Covers("I.28 unit/ mirrors src/ and the mirror is walked, never listed: every source layer "
         "carries a unit suite or a declared excuse with its reason");
  return Report();
}
