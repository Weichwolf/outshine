#include <cstdio>
#include <string>

#include "Check.h"

namespace {

bool Slurp(const char *path, std::string &into) {
  std::FILE *file = std::fopen(path, "rb");
  if (file == nullptr) { return false; }
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  return true;
}

size_t Sites(const std::string &text, const std::string &needle) {
  size_t found = 0;
  for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
    ++found;
  }
  return found;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string makefile;
  std::string runner;
  CHECK(Slurp("Makefile", makefile) && Slurp("test/run.sh", runner),
        "the two build surfaces exist where the repository says they do");

  Note("include spellings in the Makefile", (double)Sites(makefile, "-Isrc"), "sites");
  CHECK(Sites(makefile, "-Isrc") == 0,
        "**THE MAKEFILE SPELLS NO INCLUDE SET.** A second map of which source compiles with which "
        "includes is a copy of the layering, and a copy drifts: the Makefile's went stale, left "
        "corridor, physics and pilot out of the archive entire, and broke `make` at HEAD while "
        "every test stayed green (board:1584). What cannot be spelled cannot go stale");

  CHECK(Sites(makefile, "sh test/run.sh --library") == 1,
        "and `make` builds the library by delegating to the runner's own declarations, so the one "
        "spelling serves both surfaces");

  CHECK(Sites(runner, "GroupIncludes()") == 1 && Sites(runner, "BuildLibrary()") == 1,
        "which live in test/run.sh: GroupIncludes is the single spelling of the layering, and "
        "BuildLibrary is the archive built from it");

  CHECK(Sites(runner, ".$setId.o") == 1,
        "**AND ONE OBJECT PATH IS BUILT BY ONE INCLUDE SET**: the object name carries the "
        "checksum of its group's includes and standard, so a narrow unit build and the wide "
        "library build of the same source can never share an artefact -- the path IS the flag "
        "identity, and UpToDate needs no flag awareness (board:1603)");

  CHECK(Sites(runner, "apps/driver/src) printf '%s' \"-Iinclude\" ;;") == 1 &&
            Sites(runner, "apps/viewer/src) printf '%s' \"-Iinclude -Iapps/viewer/src/parts\" ;;") == 1,
        "**AND THE DOOR PROOF COMPILES AGAINST THE DOOR ALONE**: the programmes whose whole "
        "purpose is to show that a client needs nothing but include/ are declared with -Iinclude "
        "and no -Isrc entry, so the proof cannot be retired by widening the set it stands on "
        "(board:1582)");

  Covers("I.83 the layering is declared once: the runner's group declarations are the only "
         "spelling of which source compiles with which includes, and `make` builds the library "
         "entire from them, and the client suite's set is the public door alone");
  return Report();
}
