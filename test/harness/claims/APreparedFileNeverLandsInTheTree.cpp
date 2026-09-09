#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

const char *const kCaseTrees[] = {
    "test/khronos/glTF", "test/khronos/generator", "test/test262/js", "test/wpt/css"};

bool Declared(const std::filesystem::path &file) {
  const std::string name = file.filename().string();
  if (file.generic_string().starts_with("test/khronos/")) {
    return name == "manifest.json" || name == ".gitignore";
  }
  if (name == "manifest.json" || name == "reference.png" || name == ".gitignore") { return true; }
  // a reference per FRAME of an animated case, reference.f0000.png .. as prep/manifest.py names it
  return name.size() == 19 && name.starts_with("reference.f") && name.ends_with(".png") &&
         std::all_of(
             name.begin() + 11, name.begin() + 15, [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace

int main() {
  using namespace outshine::Test;

  std::vector<std::string> strays;
  size_t manifests = 0, looked = 0;
  for (const char *tree : kCaseTrees) {
    std::error_code failed;
    if (!std::filesystem::is_directory(tree, failed)) {
      CHECK(false, "every case tree this test names is a directory in this repository");
      std::printf("       %s\n", tree);
      continue;
    }
    for (const auto &entry : std::filesystem::recursive_directory_iterator(tree, failed)) {
      if (!entry.is_regular_file()) { continue; }
      ++looked;
      if (entry.path().filename() == "manifest.json") { ++manifests; }
      if (Declared(entry.path())) { continue; }
      strays.push_back(entry.path().string());
    }
  }

  for (const std::string &stray : strays) { std::printf("NOTE in the tree: %s\n", stray.c_str()); }

  Note("files under the case trees", (double)looked, "files");
  Note("manifests, which is the case count", (double)manifests, "files");
  Note("files that are neither a manifest, a reference nor a .gitignore",
       (double)strays.size(),
       "files");

  CHECK(manifests > 0,
        "the case trees hold cases at all, so the emptiness below is a measurement "
        "over a population rather than a statement about an empty one");

  CHECK(strays.empty(),
        "Khronos cases contain JSON setup and reference hashes, never reference pixels; "
        "prepared inputs and content-addressed reference images stay outside the repository");

  Covers("I.61 a repository is what is declared and what is built from it: the case trees carry "
         "declarations and never products");
  return Report();
}
