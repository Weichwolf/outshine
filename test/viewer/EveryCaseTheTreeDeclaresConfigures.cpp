/* EVERY CASE THIS TREE DECLARES CONFIGURES FOR OUTSHINE, AND A BROWSER IS WHAT WILL SHOW THEM.
 *
 * **The renderer is the library and this program renders nothing.** It reads a case's manifest and
 * configures outshine from it -- exactly what `test/harness/shared/render/Parity.cpp` does before it
 * scores anything -- through `ConfiguredCase`, which is the one place a manifest becomes a studio. A
 * second reading of the same declaration would be a second answer to *what is this case*, and the two
 * would drift on the first field either of them learned.
 *
 * **A SUITE IS A FOLDER WITH MANIFESTS IN IT**, which is `test/run.sh`'s own rule, so the generator and
 * scenario suites still ahead appear here by existing rather than by being listed.
 *
 * **What is NOT here yet is the window**, and that is a decision rather than an omission: the browser's
 * own interface is a document, and `board:1442` is the engine capability that draws one. Until it
 * exists there is nothing to draw a list WITH that would not be a second UI thrown away on the day the
 * first one lands. What stands today is the half that is already a claim: every case configures.
 *
 * **No Blender and no Cycles.** Nothing here reads an oracle product; a case needs its manifest and the
 * subject the preparer fetched. */
#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"
#include "RenderCase.h"

namespace {

struct Listed {
  std::string Suite;
  std::string Name;
  std::string Prepared;
  bool Ready = false;
};

std::string Flattened(std::string path) {
  for (char &c : path) {
    if (c == '/') { c = '-'; }
  }
  return path;
}

/* EVERY DIRECTORY UNDER `test/` THAT CARRIES A MANIFEST, and its suite is the path above it. A case
 * whose preparation has not run is listed and counted apart, never hidden: a browser that showed only
 * what happened to be on disk would answer a different question than *what does this tree declare*. */
std::vector<Listed> Cases(void) {
  std::vector<Listed> found;
  std::error_code walking;
  for (std::filesystem::recursive_directory_iterator it("test", walking), end; it != end;
       it.increment(walking)) {
    if (walking) { break; }
    if (!it->is_regular_file(walking) || it->path().filename() != "manifest.json") { continue; }
    const std::string relative = it->path().parent_path().string();
    /* The preparer's own inputs live beside the corpora and declare no case. */
    if (relative.find("/prepare") != std::string::npos) { continue; }
    Listed one;
    one.Name = it->path().parent_path().filename().string();
    one.Prepared = outshine::Test::PreparedRoot() + "/" + Flattened(relative);
    const std::string inside = relative.substr(5);
    const size_t cut = inside.rfind('/');
    one.Suite = cut == std::string::npos ? inside : inside.substr(0, cut);
    one.Ready =
        std::filesystem::exists(std::filesystem::path(one.Prepared) / "manifest.json", walking);
    found.push_back(std::move(one));
  }
  std::sort(found.begin(), found.end(), [](const Listed &a, const Listed &b) {
    return a.Suite == b.Suite ? a.Name < b.Name : a.Suite < b.Suite;
  });
  return found;
}

}  // namespace

int main(void) {
  using namespace outshine::Test;

  const std::vector<Listed> listed = Cases();
  CHECK(!listed.empty(), "the tree declares at least one case, so this browser has something to show");

  int configured = 0, unprepared = 0, declined = 0;
  std::string suite;
  for (const Listed &one : listed) {
    if (one.Suite != suite) {
      suite = one.Suite;
      std::printf("SUITE %s\n", suite.c_str());
    }
    if (!one.Ready) {
      ++unprepared;
      std::printf("  UNPREPARED %s\n", one.Name.c_str());
      continue;
    }
    ConfiguredCase held;
    std::string why;
    const bool reads = held.Read(one.Prepared, why);
    /* A CASE WHOSE DECLARED VERDICT IS THAT THIS ENGINE DECLINES IT IS ANNOUNCED, NOT FAILED
     * (`board:1424`). `SpecGlossVsMetalRough` names `KHR_materials_pbrSpecularGlossiness` in
     * `extensionsRequired`; Khronos archived that extension and this engine does not implement it, so a
     * conforming reader MUST refuse the file -- and refusing it is the case doing exactly what its own
     * manifest requires. **It is announced and never skipped**: a silence here could not be told from a
     * case that configured. */
    if (!reads && held.Declines()) {
      ++declined;
      std::printf("  DECLINED %s -- %s\n", one.Name.c_str(), why.c_str());
      continue;
    }
    /* THE VERDICT IS PER CASE AND NAMES THE CASE, because a count of failures over a hundred and fifty
     * directories says nothing about which declaration is wrong. */
    CHECK(reads, one.Name.c_str());
    if (!reads) {
      std::printf("       %s\n", why.c_str());
      continue;
    }
    ++configured;
    std::printf("  %-38s %4d x %-4d  %d frame(s)\n", one.Name.c_str(), held.WidthPx(),
                held.HeightPx(), held.Frames());
  }
  std::printf("NOTE cases the tree declares = %zu\n", listed.size());
  std::printf("NOTE cases configured = %d\n", configured);
  std::printf("NOTE cases whose preparation has not run = %d\n", unprepared);
  std::printf("NOTE cases this engine declines by their own declaration = %d\n", declined);
  Covers("board:1443 a suite is a folder with manifests in it, and every case it declares configures "
         "for outshine through the one reader the runner uses");
  return Report();
}
