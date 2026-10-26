#include "Species.h"
#include "ReadTextFile.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>
#include <system_error>
#include <utility>

namespace outshine::Generators {

bool ReadSpecies(const char *path, TreeSpecies *out) {
  if (path == nullptr || out == nullptr) { return false; }
  const auto text = ReadTextFile(path, std::numeric_limits<size_t>::max());
  return text && out->Parse(text->data(), text->size());
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
  std::ranges::sort(named);
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

}
