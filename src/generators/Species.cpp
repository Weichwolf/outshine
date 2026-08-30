#include "Species.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace outshine::Generators {

namespace {

[[nodiscard]] bool Slurp(const std::string &path, std::string &into) {
  FILE *file = fopen(path.c_str(), "rb");
  if (file == nullptr) { return false; }
  char block[8192];
  for (size_t read; (read = fread(block, 1, sizeof block, file)) > 0;) { into.append(block, read); }
  fclose(file);
  return !into.empty();
}

} // namespace

bool ReadSpecies(const char *path, TreeSpecies *out) {
  std::string text;
  return Slurp(path, text) && out->Parse(text.c_str(), text.size());
}

bool ReadSpecies(const char *path, std::vector<TreeSpecies> &out, std::string &error) {
  out.clear();
  if (path == nullptr || *path == 0) {
    error = "a world's species are read from a path and this one is empty";
    return false;
  }
  std::error_code why;
  const std::filesystem::path where(path);
  if (!std::filesystem::is_directory(where, why)) {
    TreeSpecies one;
    if (!ReadSpecies(path, &one)) {
      error = std::string("the species file '") + path +
              "' does not read: " + (one.Error().empty() ? "it could not be opened" : one.Error());
      return false;
    }
    out.push_back(std::move(one));
    return true;
  }

  std::vector<std::string> named;
  const std::filesystem::directory_iterator holds(where, why);
  if (why) {
    error = std::string("the species directory '") + path + "' does not open: " + why.message();
    return false;
  }
  for (const auto &entry : holds) {
    if (entry.path().extension() == ".json") { named.push_back(entry.path().string()); }
  }
  std::sort(named.begin(), named.end());
  if (named.empty()) {
    error = std::string("the species directory '") + path +
            "' holds no .json -- a world carries 0 or 1..N species and this is neither";
    return false;
  }
  for (const std::string &one : named) {
    TreeSpecies grown;
    if (!ReadSpecies(one.c_str(), &grown)) {
      error = "the species file '" + one + "' does not read: " +
              (grown.Error().empty() ? "it could not be opened" : grown.Error());
      return false;
    }
    out.push_back(std::move(grown));
  }
  return true;
}

} // namespace outshine::Generators
