#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;
using outshine::Test::Run;

namespace {

[[nodiscard]] bool HoldsAFetchedSubject(const std::string &prepared, const std::string &stem) {
  std::error_code why;
  for (const auto &entry : std::filesystem::directory_iterator(prepared, why)) {
    if (why) { return false; }
    if (!entry.is_directory()) { continue; }
    const std::string name = entry.path().filename().string();
    if (name.rfind(stem + "-", 0) != 0) { continue; }
    for (const auto &held : std::filesystem::directory_iterator(entry.path(), why)) {
      if (why) { continue; }
      const std::string file = held.path().filename().string();
      if (file != "manifest.json" && file != "provenance.json") { return true; }
    }
  }
  return false;
}

bool HoldsACase(const std::filesystem::path &family) {
  std::error_code failed;
  for (const auto &held : std::filesystem::recursive_directory_iterator(family, failed)) {
    if (held.is_regular_file() && held.path().filename() == "manifest.json") { return true; }
  }
  return false;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  CHECK(nest != nullptr && *nest != 0, "this test runs under a runner that holds the nest");
  if (nest == nullptr) { return Report(); }

  const char *tmp = std::getenv("TMPDIR");
  std::string root = tmp == nullptr || tmp[0] == '\0' ? "/tmp" : tmp;
  while (root.size() > 1 && root.back() == '/') { root.pop_back(); }
  const std::string prepared = root + "/outshine-prepared";
  const std::string where =
      std::filesystem::exists(prepared) ? prepared : std::string("/tmp/outshine-prepared");

  std::string said;
  const int verdict = Run("sh test/run.sh --corpus 2>&1", said);
  std::printf("NOTE the runner answered:\n%s", said.empty() ? "  (nothing)\n" : said.c_str());
  CHECK(verdict == 0, "the runner answers what a missing corpus judges, on the inherited nest");

  // board:1765: a conformance corpus is FETCHED, never carried. A family whose case
  // directories hold only their manifest holds no subject to score -- and a reader who sees
  // a green trailer must not be left to assume the flex core was judged against wpt/css.
  size_t families = 0;
  std::vector<std::string> silent;
  for (const auto &entry : std::filesystem::directory_iterator("test")) {
    if (!entry.is_directory()) { continue; }
    const std::string family = entry.path().filename().string();
    if (family == "harness" || family == "outshine") { continue; }
    if (!HoldsACase(entry.path())) { continue; }
    ++families;
    const std::string stem = "test-" + family;
    const bool fetched = HoldsAFetchedSubject(where, stem);
    const bool named = said.find("test/" + family + " declares") != std::string::npos;
    std::printf("NOTE test/%s: fetched subject %s, named by the runner %s\n",
                family.c_str(),
                fetched ? "yes" : "no",
                named ? "yes" : "no");
    if (fetched == named) {
      silent.push_back("test/" + family +
                       (fetched ? " holds subjects and the runner "
                                  "names it anyway"
                                : " holds none and the runner is silent"));
    }
  }

  Note("case families judged", (double)families, "families");
  for (const std::string &one : silent) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(families >= 3, "the walk saw the declared corpora, not a corner of them");
  CHECK(silent.empty(),
        "**A GREEN TRAILER NAMES WHAT IT DID NOT JUDGE**: every case family whose corpus "
        "holds no fetched subject is named by the runner, and every family that does hold "
        "one is not -- so a green run cannot be read as coverage it never had (board:1765)");

  Covers("IV.13 the runner publishes, per run, which declared case families hold no fetched "
         "subject, so a green trailer is never mistaken for conformance it did not measure "
         "(board:1765)");
  return Report();
}
