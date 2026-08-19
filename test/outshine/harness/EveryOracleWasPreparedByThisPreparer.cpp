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

/* EVERY `.py` OF THE PREPARER THAT CAN REACH THIS CASE, ENUMERATED RATHER THAN LISTED. A named list
 * here is a second copy of the one `jobs.py` keeps, and the two drifted the first time a file was added
 * to one of them. Both sides derive the set the same way, so there is nothing to keep in step.
 *
 * **AND THE SET IS PER CASE** (board:1451). `test/harness/` entire was one population and one claim,
 * and the claim was wider than what it decides: adding a fetch step to one corpus invalidated another's
 * 148 cases, which is hours of Cycles paid for a coupling that does not exist. What a case is digested
 * against is now the SHARED preparer plus the harness serving its own corpus -- the same lookup that
 * CHOOSES its steps, so what runs for it is what is digested for it.
 *
 * **THE LOOKUP IS POSITIONAL AND THERE IS NO TABLE**, which is `vendor.py`'s own rule: the harness for a
 * case at `test/<a>/<b>/<case>` is the deepest existing directory under `test/harness/` matching a
 * prefix of its path, so a new corpus adds a directory and nothing else. A list here would be the third
 * copy of a fact and the one nobody updates. */
const char *const kHarnessRoot = "test/harness";
const char *const kSharedHarness = "test/harness/shared";

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


/* The preparer concatenates its sources in sorted path order and hashes the result once. The order is
 * part of the digest, so it is stated in both places identically or the two never agree. */
void SourcesUnder(const std::string &directory, std::vector<std::string> &into) {
  if (!std::filesystem::is_directory(directory)) { return; }
  const size_t first = into.size();
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".py") {
      into.push_back(entry.path().string());
    }
  }
  std::sort(into.begin() + (long)first, into.end());
}

/* THE HARNESS SERVING A CASE, by position: the deepest existing directory under `test/harness/` whose
 * path is a prefix of the case's own, relative to `test/`. `vendor.harness_of` is this same walk. */
std::string HarnessOf(const std::string &caseDirectory) {
  const std::string prefix = "test/";
  std::string relative = caseDirectory;
  if (relative.rfind(prefix, 0) == 0) { relative = relative.substr(prefix.size()); }
  std::string found;
  std::string walked = kHarnessRoot;
  size_t at = 0;
  while (at <= relative.size()) {
    const size_t slash = relative.find('/', at);
    const std::string part = relative.substr(at, slash == std::string::npos ? std::string::npos
                                                                            : slash - at);
    if (part.empty()) { break; }
    walked += "/" + part;
    if (std::filesystem::is_directory(walked)) { found = walked; }
    if (slash == std::string::npos) { break; }
    at = slash + 1;
  }
  return found;
}

std::string PreparerDigest(const std::string &caseDirectory, bool &complete) {
  std::string material;
  complete = true;
  std::vector<std::string> sources;
  SourcesUnder(kSharedHarness, sources);
  const std::string vendor = HarnessOf(caseDirectory);
  if (!vendor.empty() && vendor != kSharedHarness) { SourcesUnder(vendor, sources); }
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

/* **THE TRACKED CASES, AND THE PREPARED DIRECTORY DERIVED FROM EACH** (board:1451). The walk starts in
 * the TREE and not in the prepared root, because a case's digest is a function of its own tracked path
 * -- which harness serves it -- and the prepared leaf is that path with its separators flattened, so
 * one end gives the other without a table (`PreparedRoot.h` states the rule).
 *
 * A tracked case with nothing prepared is skipped rather than failed: it is a corpus nobody has run the
 * preparer over, which is what `UNPREPARED` means everywhere else in this tree. */
struct Case {
  std::string Tracked;
  std::string Prepared;
};

std::vector<Case> PreparedCases() {
  std::vector<Case> cases;
  if (!std::filesystem::is_directory("test")) { return cases; }
  const std::string root = outshine::Test::PreparedRoot() + "/";
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator("test")) {
    if (!entry.is_regular_file() || entry.path().filename() != "manifest.json") { continue; }
    std::string tracked = entry.path().parent_path().generic_string();
    if (tracked.rfind("test/harness", 0) == 0) { continue; }
    std::string flattened = tracked;
    for (char &letter : flattened) {
      if (letter == '/') { letter = '-'; }
    }
    const std::string prepared = root + flattened + "/";
    std::string document;
    if (Slurp(prepared + "provenance.json", document)) {
      cases.push_back({tracked, prepared});
    }
  }
  std::sort(cases.begin(), cases.end(),
            [](const Case &left, const Case &right) { return left.Tracked < right.Tracked; });
  return cases;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<Case> cases = PreparedCases();
  Note("prepared cases carrying a provenance document", (double)cases.size(), "cases");
  if (cases.empty()) {
    Unprepared("test/harness/shared/corpus/prepare.py has produced no provenance.json to check a digest against");
    return Report();
  }

  size_t agreeing = 0, silent = 0, incomplete = 0;
  std::string sawKhronos, sawGrown;
  for (const Case &one : cases) {
    std::string document;
    if (!Slurp(one.Prepared + "provenance.json", document)) { continue; }
    std::string recorded;
    if (!RecordedDigest(document, recorded)) {
      ++silent;
      continue;
    }
    bool complete = false;
    const std::string current = PreparerDigest(one.Tracked, complete);
    incomplete += complete ? 0u : 1u;
    if (one.Tracked.rfind("test/khronos", 0) == 0) { sawKhronos = current; }
    if (one.Tracked.rfind("test/outshine", 0) == 0) { sawGrown = current; }
    const bool same = recorded == current;
    CHECK(same, (one.Tracked + " names the preparer that can reach it").c_str());
    if (!same) {
      Note((one.Tracked + ": prepared by " + recorded + ", the tree digests to " + current).c_str());
    } else {
      ++agreeing;
    }
  }
  CHECK(incomplete == 0, "every source a case's preparer digests is present to be digested here");

  /* **TWO CORPORA DIGEST DIFFERENTLY, WHICH IS THE WHOLE OF THIS CHANGE** (board:1451). If they agreed
   * the population would still be `test/harness/` entire and a vendor's edit would still invalidate
   * every other vendor's cases -- so this is the claim, not a curiosity. */
  if (!sawKhronos.empty() && !sawGrown.empty()) {
    Note(("khronos digests to " + sawKhronos).c_str());
    Note(("grown digests to " + sawGrown).c_str());
    CHECK(sawKhronos != sawGrown,
          "two corpora served by different harnesses digest to different preparers, so an edit to one "
          "vendor's steps cannot invalidate the other's cases");
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
