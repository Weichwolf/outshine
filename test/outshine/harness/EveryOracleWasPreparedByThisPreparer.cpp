/* THE ORACLE ON DISK WAS PRODUCED BY THE PREPARER IN THE TREE (board:1120).
 *
 * THE DEFECT THIS EXISTS FOR. The oracle key covered Blender, the subject pins, the declared scene and
 * the recipe -- everything the manifest SAYS and nothing the preparer DOES. `in_blender_render.py`
 * decides which passes are enabled, how materials are built and where lights and cameras sit; `exr.py`
 * decides how the products are decoded on the way out. A change to either moves what Cycles renders
 * while every field of the key stays identical, so the store serves the old bytes indefinitely.
 *
 * IT WAS FOUND BY HASHING BEFORE AND AFTER ON A HUNCH, which is not an instrument. The digest in the
 * key means a preparer change MISSES; this test means a corpus prepared by a different preparer is
 * VISIBLE rather than silently trusted -- the two are not the same guarantee. The key protects the
 * next run; this protects the run that already happened.
 *
 * WHAT IT COMPARES. Each case's `provenance.json` records the digest of the preparer that produced it.
 * This recomputes that digest from the files in the tree and holds the two against each other. It
 * hashes the same bytes the preparer hashes -- the whole file, not a version string, because a version
 * somebody has to remember to bump goes stale exactly when it matters.
 *
 * A TREE WITH NOTHING PREPARED IS UNPREPARED AND SAYS SO. `test/run.sh` does not prepare, so a corpus
 * that has never been built is a missing input rather than a failure, and never a silent pass. */
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>

#include "Check.h"

#include "PreparedRoot.h"

#include "Sha256.h"

namespace {

/* EVERY `.py` UNDER THE PREPARER, ENUMERATED RATHER THAN LISTED. A named list here is a second copy of
 * the one `jobs.py` keeps, and the two drifted the first time a file was added to one of them -- this
 * test went red naming 34 cases, which was the divergence and not a stale corpus. Both sides now derive
 * the set the same way, so there is nothing to keep in step. */
/* EVERY HARNESS SOURCE, NOT ONE DIRECTORY (board:1196): the preparer is split by vendor, so
 * `fetch.py` and `grown.py` sit beside the corpus they serve while still deciding what lands on
 * disk. The preparer digests this same tree, recursively, in sorted path order. */
const char *const kPreparerDirectory = "test/harness";

bool Slurp(const std::string &path, std::string &into) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) { return false; }
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  return true;
}

bool IsDirectory(const std::string &path) {
  struct stat entry {};
  return stat(path.c_str(), &entry) == 0 && S_ISDIR(entry.st_mode);
}

/* The preparer concatenates its sources in sorted path order and hashes the result once. The order is
 * part of the digest, so it is stated in both places identically or the two never agree. */
std::string PreparerDigest(bool &complete) {
  std::string material;
  complete = true;
  std::vector<std::string> sources;
  if (std::filesystem::is_directory(kPreparerDirectory)) {
    for (const std::filesystem::directory_entry &entry :
         std::filesystem::recursive_directory_iterator(kPreparerDirectory)) {
      if (entry.is_regular_file() && entry.path().extension() == ".py") {
        sources.push_back(entry.path().string());
      }
    }
  }
  std::sort(sources.begin(), sources.end());
  for (const std::string &source : sources) {
    if (!Slurp(source, material)) { complete = false; }
  }
  return outshine::Sha256Hex(material);
}

/* The value of `"preparerDigest"` in a provenance document, without a JSON parser: the field is a flat
 * string written by `json.dump`, and this test's whole business with the file is one key. */
bool RecordedDigest(const std::string &document, std::string &out) {
  const std::string key = "\"preparerDigest\":";
  const size_t at = document.find(key);
  if (at == std::string::npos) { return false; }
  const size_t open = document.find('"', at + key.size());
  if (open == std::string::npos) { return false; }
  const size_t close = document.find('"', open + 1);
  if (close == std::string::npos) { return false; }
  out = document.substr(open + 1, close - open - 1);
  return true;
}

/* THE PREPARED ROOT AND NOT THE TREE (board:1364). A case's products live under the system temp root,
 * so the two-level walk of `test/khronos/glTF/` this replaces now finds manifests and nothing else --
 * which reports as an EMPTY population rather than as a failure, and an empty population is a green
 * test about nothing. The root is flat and spelled once in `PreparedRoot.h`, so one level is the whole
 * walk and BOTH corpora arrive rather than only the fetched one. */
std::vector<std::string> PreparedCases() {
  std::vector<std::string> cases;
  const std::string root = outshine::Test::PreparedRoot() + "/";
  DIR *prepared = opendir(root.c_str());
  if (!prepared) { return cases; }
  for (dirent *one = readdir(prepared); one; one = readdir(prepared)) {
    const std::string name = one->d_name;
    if (name == "." || name == "..") { continue; }
    const std::string directory = root + name + "/";
    std::string document;
    if (IsDirectory(directory) && Slurp(directory + "provenance.json", document)) {
      cases.push_back(directory);
    }
  }
  closedir(prepared);
  return cases;
}

} // namespace

int main() {
  using namespace outshine::Test;

  bool complete = false;
  const std::string current = PreparerDigest(complete);
  CHECK(complete, "every source the preparer digests is present in the tree to be digested here");
  Note(("the preparer in the tree digests to " + current).c_str());

  const std::vector<std::string> cases = PreparedCases();
  Note("prepared cases carrying a provenance document", (double)cases.size(), "cases");
  if (cases.empty()) {
    Unprepared("test/harness/shared/corpus/prepare.py has produced no provenance.json to check a digest against");
    return Report();
  }

  size_t agreeing = 0, silent = 0;
  for (const std::string &directory : cases) {
    std::string document;
    if (!Slurp(directory + "provenance.json", document)) { continue; }
    std::string recorded;
    if (!RecordedDigest(document, recorded)) {
      ++silent;
      continue;
    }
    const bool same = recorded == current;
    CHECK(same, (directory + "provenance.json names the preparer that is in the tree").c_str());
    if (!same) {
      Note((directory + ": prepared by " + recorded).c_str());
    } else {
      ++agreeing;
    }
  }
  /* A DOCUMENT WITHOUT THE FIELD IS A CORPUS FROM BEFORE THE FIELD EXISTED, and it is a failure rather
   * than a skip: it is exactly the state this test was written to make visible. */
  CHECK(silent == 0,
        "every provenance document records the preparer that produced it, so a corpus older than the "
        "field is reported rather than passed over");
  Note("cases whose oracle was produced by this preparer", (double)agreeing, "cases");

  Covers("board:1120 the oracle key covers the preparer's own code, so a preparer change invalidates "
         "the corpus instead of being found by hashing on a hunch");
  return Report();
}
