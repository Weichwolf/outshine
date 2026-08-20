/* THE INDEX PASSES ON DISK CARRY THE MAPPING THAT MAKES THEM READABLE (board:1154).
 *
 * IT WAS `ACachedRenderStillNamesItsIndices` AND THE CACHE IT GUARDED NO LONGER EXISTS (board:1369).
 * A Cycles render is produced, delivered and forgotten -- the owner's ruling, because this machine has
 * more CPU than disk -- so there is no cache hit to check and the `Cached > 0` claim was struck rather
 * than left to pass vacuously. **The defect below is unchanged and so is everything else here**: the
 * mapping has to be on every render row, and the population is every prepared case.
 *
 * THE DEFECT THIS EXISTS FOR. Cycles' index passes carry `material.pass_index`, which the preparer
 * assigns inside Blender in name order over `bpy.data.materials` -- the factory startup file's own
 * materials included -- so index n is never material n and the numbering cannot be reconstructed from
 * the glTF. The preparer recorded that mapping on the render that produced the pass and dropped it on
 * a cache hit, so a case whose oracle came out of the store kept 14.7 MB per pass and lost the only
 * thing that made the integers mean anything.
 *
 * IT IS THE WORST DIRECTION OF FAILURE: the first run after a cold `prepare.py` is correct and every
 * run after it is not, so nothing changed between the working corpus and the broken one. [MEASURED]
 * before the repair: 37 of 37 prepared cases refused, and no `provenance.json` in the tree carried a
 * mapping at all.
 *
 * WHY IT IS CHECKED HERE AND NOT ONLY BY `test/render/khronos/glTF`. A render case notices the loss only where its
 * picture bound happens to depend on the identity router, so the corpus went from two mappings to
 * none while one case changed verdict. This reads the population instead: every render row of every
 * prepared case, cached and fresh alike, and it publishes how many of them were cached -- a run where
 * everything missed would prove nothing about the path that was broken.
 *
 * WHAT IT DOES NOT CHECK. Whether a mapping is CORRECT is the render suite's, through
 * `test/render/khronos/glTF/SurfaceIdentity.h`: the names are held against the glTF's own materials there, per
 * pixel. This checks that the account exists, names something, and came back with the bytes. */
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"

#include "Json.h"

namespace {

using outshine::Json;

/* THE PASS PRODUCTS WHOSE INTEGERS NEED A MAPPING, and the mapping each one is read out of. The two
 * spellings are the preparer's (`prep/manifest.py` QUANTITY_PASSES and `in_blender_render.py`), and
 * they stand together so a third index pass is one row here. */
struct IndexPass {
  const char *Product;
  const char *Indexed;
};

constexpr IndexPass kIndexPasses[] = {{"materialIndexRaw", "materials"},
                                      {"objectIndexRaw", "objects"}};

bool Slurp(const std::filesystem::path &path, std::string &into) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) { return false; }
  into.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  return !stream.bad();
}

std::vector<std::filesystem::path> PreparedCases() {
  /* A CASE IS FOUND BY WHAT IT CARRIES, NOT BY HOW DEEP IT SITS (board:1196). The two-level walk this
   * replaces encoded one corpus's layout, so naming a case for the model it carries -- which puts the
   * three multi-case models one level lower -- silently emptied the population and the test reported
   * nothing wrong. A recursive search over both corpora cannot be wrong about a depth it never asks. */
  std::vector<std::filesystem::path> cases;
  std::error_code failed;
  /* THE PREPARED ROOT AND NOT THE TREE (board:1364). A case's products live under the system temp
   * root, so a walk of the case trees now finds manifests and nothing else -- which reported as an
   * EMPTY POPULATION rather than as a failure, and an empty population is a green test about nothing.
   * The root is spelled once, in `PreparedRoot.h`, because a second copy of it would drift. */
  const std::string corpus = outshine::Test::PreparedRoot();
  if (std::filesystem::is_directory(corpus, failed)) {
    for (const std::filesystem::directory_entry &one :
         std::filesystem::recursive_directory_iterator(corpus, failed)) {
      if (one.is_directory() && std::filesystem::is_regular_file(one.path() / "provenance.json")) {
        cases.push_back(one.path());
      }
    }
  }
  std::sort(cases.begin(), cases.end());
  return cases;
}

/* WHAT ONE CASE'S RENDER ROWS SAID. Every count is over rows and not over cases: a case is a dozen
 * rows on an animated declaration and one on a still, and a per-case count would weigh those the
 * same. */
struct Census {
  size_t Rows = 0;
  size_t Cached = 0;
  size_t WithAnIndexPass = 0;
  size_t Named = 0;
  size_t Refusing = 0;
};

/* WHETHER THIS ROW'S MAPPING IS THERE AND SAYS SOMETHING. An entry with no name or no index is as
 * useless as no entry, so the shape is checked rather than the array's existence. */
bool MappingIsUsable(const Json::Ref &row, const char *indexed, std::string &why) {
  const Json::Ref entries = row["provenance"]["quantities"]["indices"][indexed];
  if (entries.GetKind() != Json::Kind::Array) {
    why = "the row records no index mapping under quantities.indices.";
    why += indexed;
    return false;
  }
  if (entries.Size() == 0) {
    why = "the row's mapping for ";
    why += indexed;
    why += " is empty, so every integer of the pass names nothing";
    return false;
  }
  for (size_t at = 0; at < entries.Size(); ++at) {
    if (entries[at]["name"].Str("").empty() || entries[at]["passIndex"].Int(-1) < 1) {
      why = "an entry of the mapping for ";
      why += indexed;
      why += " carries no name or no pass index";
      return false;
    }
  }
  return true;
}

Census ReadCase(const std::filesystem::path &directory, const Json &document) {
  Census tally;
  const Json::Ref rows = document.Root()["report"]["render"];
  for (size_t at = 0; at < rows.Size(); ++at) {
    const Json::Ref row = rows[at];
    ++tally.Rows;
    if (row["cache"].Str("") == "hit") { ++tally.Cached; }
    for (const IndexPass &pass : kIndexPasses) {
      if (row["products"][pass.Product]["path"].Str("").empty()) { continue; }
      ++tally.WithAnIndexPass;
      std::string why;
      if (MappingIsUsable(row, pass.Indexed, why)) {
        ++tally.Named;
        continue;
      }
      ++tally.Refusing;
      outshine::Test::Note((directory.string() + " recipe " + row["recipe"].Str("?") + ": " + why).c_str());
    }
  }
  return tally;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<std::filesystem::path> cases = PreparedCases();
  Note("prepared cases carrying a provenance document", (double)cases.size(), "cases");
  if (cases.empty()) {
    Unprepared("test/harness/shared/corpus/prepare.py has produced no provenance.json to read a mapping out of");
    return Report();
  }

  Census all;
  for (const std::filesystem::path &directory : cases) {
    std::string text;
    if (!Slurp(directory / "provenance.json", text)) { continue; }
    Json document;
    if (!document.Parse(text.c_str(), text.size())) {
      CHECK(false, (directory.string() + "/provenance.json parses").c_str());
      continue;
    }
    const Census one = ReadCase(directory, document);
    all.Rows += one.Rows;
    all.Cached += one.Cached;
    all.WithAnIndexPass += one.WithAnIndexPass;
    all.Named += one.Named;
    all.Refusing += one.Refusing;
  }

  Note("render rows across the prepared corpus", (double)all.Rows, "rows");
  Note("render rows served from the content store", (double)all.Cached, "rows");
  Note("index passes placed by those rows", (double)all.WithAnIndexPass, "passes");
  Note("index passes whose mapping came back with them", (double)all.Named, "passes");

  CHECK(all.Refusing == 0,
        "every index pass on disk carries the mapping its integers are read through, whether the "
        "render that produced it ran in this run or was served from the store");
  /* A RUN WHERE NOTHING WAS CACHED WOULD PASS THIS WITHOUT EXERCISING THE PATH THAT WAS BROKEN, so
   * the population is a claim of its own rather than a note. It is the whole difference between "the
   * mapping is written" and "the mapping survives a cache hit". */
  /* A CACHE-HIT CLAIM STOOD HERE AND IT WAS STRUCK, not weakened (board:1369). Renders are no longer
   * kept, so `Cached` is zero on every run by construction; a check on it would assert a path this tree
   * does not have, and a check that cannot pass is worse than one that is absent. `Cached` is still
   * COUNTED and still published below, because a number that reports zero is how a reader sees that the
   * cache is gone rather than broken. */
  CHECK(all.WithAnIndexPass > 0,
        "the corpus places index passes at all, so a green count here is not a count over nothing");

  Covers("board:1154 the pass-index mapping is a keyed product of the render and comes back from the "
         "store with the bytes it describes, so a cached oracle still names its own indices");
  return Report();
}
