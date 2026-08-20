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

}

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
    if (one.Tracked.rfind("test/render/khronos", 0) == 0) { sawKhronos = current; }
    if (one.Tracked.rfind("test/render/outshine", 0) == 0) { sawGrown = current; }
    const bool same = recorded == current;
    CHECK(same, (one.Tracked + " names the preparer that can reach it").c_str());
    if (!same) {
      Note((one.Tracked + ": prepared by " + recorded + ", the tree digests to " + current).c_str());
    } else {
      ++agreeing;
    }
  }
  CHECK(incomplete == 0, "every source a case's preparer digests is present to be digested here");

  if (!sawKhronos.empty() && !sawGrown.empty()) {
    Note(("khronos digests to " + sawKhronos).c_str());
    Note(("grown digests to " + sawGrown).c_str());
    CHECK(sawKhronos != sawGrown,
          "two corpora served by different harnesses digest to different preparers, so an edit to one "
          "vendor's steps cannot invalidate the other's cases");
  }

  CHECK(silent == 0,
        "every provenance document records the preparer that produced it, so a corpus older than the "
        "field is reported rather than passed over");
  Note("cases whose oracle was produced by this preparer", (double)agreeing, "cases");

  Covers("the oracle key covers the preparer's own code, so a preparer change invalidates "
         "the corpus instead of being found by hashing on a hunch");
  return Report();
}
