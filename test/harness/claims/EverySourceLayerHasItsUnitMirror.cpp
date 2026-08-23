#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

struct Excused {
  const char *Layer;
  const char *Why;
  const char *Folds; // the suite the excuse names, which MUST hold tests -- or nullptr
};

// an excuse that points at an empty directory is a lie the gate would otherwise print in
// green: src/ground/tiles was excused into "unit/world", which has never existed
// (board:1745). Every folding excuse now names its target and the target is checked.
const Excused kExcused[] = {
    {"src/core/io", "folded into unit/core -- one layer, two directories by include depth",
     "test/unit/core"},
    {"src/assets", "declared data, not code", nullptr},
};

const Excused *Excuse(const std::string &layer) {
  for (const Excused &one : kExcused) {
    if (layer == one.Layer) { return &one; }
  }
  return nullptr;
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
  std::vector<std::string> stale;
  for (const auto &entry : std::filesystem::recursive_directory_iterator("src")) {
    if (!entry.is_directory()) { continue; }
    const std::string layer = entry.path().string();
    if (!HoldsSources(entry.path())) { continue; }
    ++layers;
    if (const Excused *excused = Excuse(layer)) {
      std::printf("NOTE %s is excused -- %s\n", layer.c_str(), excused->Why);
      if (excused->Folds != nullptr && !HoldsTests(excused->Folds)) {
        stale.push_back(std::string(layer) + " folds into " + excused->Folds +
                        ", which holds no test");
        continue;
      }
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
  for (const std::string &lie : stale) { std::printf("FOUND %s\n", lie.c_str()); }
  CHECK(stale.empty(),
        "**AN EXCUSE NAMES A SUITE THAT HOLDS TESTS**: a folding excuse pointing at an "
        "empty or absent directory is a lie this gate would print in green (board:1745)");
  CHECK(layers >= 15, "the walk saw the tree, not a corner of it");
  CHECK(naked.empty(),
        "**EVERY SOURCE LAYER HAS ITS UNIT MIRROR.** unit/ mirrors src/ ALWAYS -- the mirror "
        "is the regression gate (board:1601), so a layer without one is a layer whose breakage "
        "waits for the long suites. An excuse exists only with a named reason beside it");

  Covers("I.28 unit/ mirrors src/ and the mirror is walked, never listed: every source layer "
         "carries a unit suite or a declared excuse with its reason");
  return Report();
}
