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

std::string BodyOf(const std::string &text, const std::string &named) {
  const size_t opens = text.find(named + "() {");
  if (opens == std::string::npos) { return std::string(); }
  const size_t closes = text.find("\n}", opens);
  if (closes == std::string::npos) { return std::string(); }
  return text.substr(opens, closes - opens);
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

  // THE DRIVER CLIENT WAS THE THIRD PROGRAMME HERE AND IT IS GONE, deleted at the owner's word.
  // A claim keeps counting what stands, so it counts two -- and the door proof is not weaker for
  // it: board:2038 puts the CORPUS harness on the same footing, and a conformance case driving
  // include/ alone proves more about the door than any one app ever did.
  std::string clientReaches;
  CHECK(Slurp("src/client/reaches", clientReaches) &&
            clientReaches.substr(0, clientReaches.find_last_not_of(" \n\r\t") + 1) == "base",
        "**AND THE ONE CLIENT COMPILES AGAINST THE DOOR ALONE**: `src/client/reaches` names `base` "
        "and nothing else, so the tool this tree measures itself with sees include/ and no engine "
        "tier behind it. The include path is DERIVED from that line, so widening what the client "
        "may reach is a one-word edit a reader can see -- which is the whole point of declaring it "
        "once. It was two programmes and a corpus harness; both are gone, and the corpus now "
        "drives THIS binary (board:1582, 2038, 2049)");

  // AND THE BUILD STANDS ON THE DECLARATION, not beside it. `EveryProgramStillLinks` compiled a
  // client with a hand-written `-Iinclude` and probed the door rule twelve lines later with a
  // DIFFERENT hand-written set -- `-Iinclude -I$layer -I$layer/parts` -- and neither called
  // `LayerIncludes`, which is what `make` uses. So `make` and `test/run.sh` disagreed about
  // whether `apps/viewer` compiles: it includes `"Face.h"` from its own `parts/`, which is the
  // client's own source and not `src/`, so the door rule allows it and the narrower spelling did
  // not. The gate carried that as a standing BUILD failure and board:1799 recorded it as
  // furniture for three rounds.
  //
  // A second spelling of an include set is the defect this file exists to refuse, and the walk
  // that refuses it is a count: `-Iinclude` may stand inside `LayerIncludes` or `GroupIncludes`,
  // which are the two declaration tables, and nowhere else in the runner.
  const std::string byLayer = BodyOf(runner, "LayerIncludes");
  const std::string byGroup = BodyOf(runner, "GroupIncludes");
  const size_t everywhere = Sites(runner, "-Iinclude");
  const size_t declaredSites = Sites(byLayer, "-Iinclude") + Sites(byGroup, "-Iinclude");
  Note("-Iinclude in the runner entire", (double)everywhere, "sites");
  Note("of them inside LayerIncludes or GroupIncludes", (double)declaredSites, "sites");
  CHECK(!byLayer.empty() && !byGroup.empty() && declaredSites == everywhere,
        "**AND THE BUILD USES THE DECLARATION IT DECLARES**: an include set written by hand "
        "beside `LayerIncludes` is a second spelling of the one truth this file names, and two "
        "spellings drift -- one of them refused `apps/viewer` for reaching its OWN `parts/` "
        "while `make` built it, so the gate and the Makefile disagreed about whether a client "
        "compiles (board:1584, 1799)");

  Covers("I.83 the layering is declared once: the runner's group declarations are the only "
         "spelling of which source compiles with which includes, and `make` builds the library "
         "entire from them, and the client suite's set is the public door alone");
  return Report();
}
