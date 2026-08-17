/* A REPOSITORY IS WHAT IS DECLARED AND WHAT IS BUILT FROM IT, AND THIS IS THE INSTRUMENT FOR THAT.
 *
 * `CLAUDE.md` states it as a constraint -- *every artefact goes to the system temp directory, never
 * into the tree* -- and until now nothing measured it. The preparer wrote each case's fetched buffers,
 * its images, its converted `.blend` and every oracle `.exr` and `.raw` **into the case's own
 * directory**, so the working tree carried **158 MB of untracked artefacts** while `git status` stayed
 * clean: two `.gitignore` files, each opening with `*`, made the whole class invisible.
 *
 * WHY THE INVISIBILITY IS THE DEFECT AND NOT THE SIZE. Nothing was ever committed, so no diff could
 * show it and no reviewer could notice. A rule that is stated in prose, satisfied by a `.gitignore`
 * and contradicted on disk is a rule the tree does not carry -- which is the shape this repository
 * spends its rounds removing.
 *
 * WHAT THIS DOES NOT CLAIM. It says nothing about the store, the build directory or the prepared
 * root; those are all under the system temp directory and are exactly where the constraint puts them.
 * It says nothing about SIZE either: a small artefact in the tree is the same defect as a large one,
 * so the predicate is what a file IS and never how many bytes it has. */
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

/* THE DECLARATIVE SUITES, AND THEY ARE THE WHOLE POPULATION. `test/outshine/unit/` and the rest hold
 * source, which is declared; these two hold CASES, whose only declared file is a manifest and whose
 * every other file is produced by the preparer. */
const char *const kCaseTrees[] = {"test/khronos/glTF", "test/outshine/render"};

/* WHAT A CASE DIRECTORY MAY CONTAIN, named rather than counted so a file added by a later round is
 * visible as a name that is not here. `.gitignore` stays because `CLAUDE.md` cites it by path and the
 * `git grep` hazard it documents is real whether or not artefacts land beside it. */
bool Declared(const std::filesystem::path &file) {
  const std::string name = file.filename().string();
  return name == "manifest.json" || name == ".gitignore";
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
  Note("files that are neither a manifest nor a .gitignore", (double)strays.size(), "files");

  /* A CASE COUNT OF ZERO WOULD MAKE THE CLAIM ABOVE VACUOUS, so the population is asserted before the
   * predicate over it. A tree that had been emptied would otherwise report a clean pass. */
  CHECK(manifests > 0, "the case trees hold cases at all, so the emptiness below is a measurement "
                       "over a population rather than a statement about an empty one");

  CHECK(strays.empty(),
        "no prepared file stands in the tree -- a case directory holds its manifest and nothing "
        "else, and every fetched buffer, image, .blend, .exr and .raw is under the system temp root "
        "where CLAUDE.md puts it");

  Covers("I.26.10 a repository is what is declared and what is built from it: the case trees carry "
         "declarations and never products");
  return Report();
}
