/* THE LIBRARY DOES NOT KNOW WHAT RUNS IT, AND THIS IS THE INSTRUMENT FOR THAT.
 *
 * `CLAUDE.md`: *the engine is a library and it is platform agnostic -- it declares what it needs from
 * a host and calls nothing else; everything that runs it is a test.* A source file that names a test
 * has reversed that relation, and the reversal is not a matter of taste: **the citation rots**.
 * [MEASURED] when this test was written, `src/` cited seven distinct paths under `test/` and **six of
 * them did not resolve** (board:1378) -- `test/shader/BothHalvesOfTheBrdfAgree.cpp`, `test/unit/render/stages/`,
 * `test/frame/` and their kind had all moved under the suite reorganisation, and nothing moved the
 * comments with them. A citation pointing from the stable tree at the moving one has no owner.
 *
 * It is the same direction argument the board already carries -- *THE CODE CITES THE REQUIREMENT; THE
 * BOARD NEVER NAMES THE CODE* -- applied one level down. A `board:` marker travels with the line it
 * sits on and the relation is derived by `git grep`; a path is a copy that can drift, and did.
 *
 * WHAT THIS DOES NOT FORBID, STATED SO THE INSTRUMENT'S DOMAIN IS PART OF ITS CLAIM. It says nothing
 * about `Blender`, `Cycles` or `oracle` appearing in `src/`, and those are deliberately left alone:
 * where they occur they carry the ORIGIN OF A NUMBER -- *MEASURED against `NormalTangentMirrorTest`,
 * whose TANGENT Blender's own exporter wrote* -- and `CLAUDE.md` requires every number to carry its
 * origin. Scrubbing them would delete provenance to satisfy a layering rule they do not breach: an
 * external renderer that once produced a measurement is a source, not a dependency. The engine is
 * coupled to a test when it NAMES ONE, and that is exactly the predicate below.
 *
 * NOR DOES IT CHECK THE INCLUDE SETS. Those are the build's job and a breach there is a compile error
 * rather than a report, which is stronger than anything this file could say. */
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

namespace {

/* THE WHOLE LIBRARY, WHICH IS THE POPULATION THE CLAIM IS DRAWN FROM. `src/assets/` is included on
 * purpose: declared data is part of the library, and a JSON origin note citing a test file would be
 * the same defect wearing a different extension. */
const char *const kLibrary = "src";

} // namespace

int main() {
  using namespace outshine::Test;

  std::vector<std::string> citations;
  size_t files = 0, bytes = 0;

  std::error_code failed;
  const bool present = std::filesystem::is_directory(kLibrary, failed);
  CHECK(present, "the library is a directory in this repository, so what follows is a measurement "
                 "over a population rather than a statement about an empty one");

  if (present) {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(kLibrary, failed)) {
      if (!entry.is_regular_file()) { continue; }
      ++files;
      std::ifstream source(entry.path(), std::ios::binary);
      std::string line;
      size_t number = 0;
      while (std::getline(source, line)) {
        ++number;
        bytes += line.size();
        /* THE NEEDLE IS `test/` AND NOT THE WORD `test`. A source file may say *this is what a test
         * would check*; what it may not do is name one, and a path separator is what makes the
         * difference between a sentence and a citation. */
        for (size_t at = line.find("test/"); at != std::string::npos;
             at = line.find("test/", at + 1)) {
          /* `latest/`, `fastest/` and their kind end in the same five characters, so the match is
           * anchored: what precedes it must not be a letter. */
          if (at > 0 && (std::isalnum((unsigned char)line[at - 1]) || line[at - 1] == '_')) {
            continue;
          }
          citations.push_back(entry.path().string() + ":" + std::to_string(number));
          break;
        }
      }
    }
  }

  for (const std::string &site : citations) { std::printf("NOTE cites a test: %s\n", site.c_str()); }

  Note("files in the library", (double)files, "files");
  Note("bytes read", (double)bytes, "bytes");
  Note("sites naming a path under test/", (double)citations.size(), "sites");

  CHECK(files > 0, "the library holds files at all");
  CHECK(citations.empty(),
        "no file in the library names a path under test/ -- the engine declares what it needs from a "
        "host and everything that runs it is a test, so the citation goes the other way and travels "
        "as a board: marker that moves with the line it sits on");

  Covers("I.26 the engine is a library: it does not name what runs it, and a claim about it is "
         "proven by a test that cites the requirement rather than by a comment that cites the test");
  return Report();
}
