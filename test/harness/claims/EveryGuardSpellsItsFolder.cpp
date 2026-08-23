#include <algorithm>
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

[[nodiscard]] std::string Wanted(const std::filesystem::path &header) {
  std::string spelling = "OUTSHINE";
  const std::filesystem::path rel = std::filesystem::relative(header, "src");
  for (const auto &part : rel.parent_path()) {
    std::string folder = part.string();
    for (char &c : folder) { c = (char)std::toupper((unsigned char)c); }
    spelling += "_" + folder;
  }
  std::string name = header.stem().string();
  for (char &c : name) { c = (char)std::toupper((unsigned char)c); }
  return spelling + "_" + name + "_H";
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // a guard that does not spell its folder is one innocently-named header away from a
  // silent empty translation unit -- BODY_H and FOREST_H were exactly that (board:1748)
  const std::regex guard(R"(#ifndef\s+([A-Za-z0-9_]+)\s*\n\s*#define\s+\1\s*\n)");
  size_t headers = 0;
  std::vector<std::string> misspelt;
  std::vector<std::string> guards;
  for (const auto &entry : std::filesystem::recursive_directory_iterator("src")) {
    if (!entry.is_regular_file() || entry.path().extension() != ".h") { continue; }
    ++headers;
    const std::string text = Slurp(entry.path());
    std::smatch found;
    if (!std::regex_search(text, found, guard)) {
      misspelt.push_back(entry.path().string() + " carries no include guard at all");
      continue;
    }
    guards.push_back(found[1].str());
    const std::string wanted = Wanted(entry.path());
    if (found[1].str() != wanted) {
      misspelt.push_back(entry.path().string() + " guards as " + found[1].str() +
                         " where its folder spells " + wanted);
    }
  }

  Note("headers walked", (double)headers, "headers");
  for (const std::string &one : misspelt) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(headers >= 150, "the walk saw the tree, not a corner of it");
  CHECK(misspelt.empty(),
        "**EVERY GUARD SPELLS ITS FOLDER**: OUTSHINE_<FOLDERS>_<NAME>_H, so a second "
        "header of the same short name cannot silently empty a translation unit "
        "(board:1748)");

  std::sort(guards.begin(), guards.end());
  const auto twice = std::adjacent_find(guards.begin(), guards.end());
  if (twice != guards.end()) { std::printf("FOUND the guard %s is spelt twice\n", twice->c_str()); }
  CHECK(twice == guards.end(), "and no two headers claim one guard");

  Covers("IV.10 every include guard in src/ spells its folder and no two collide -- the "
         "rule is a walk, not a habit repeated per layer (board:1643, 1748)");
  return Report();
}
