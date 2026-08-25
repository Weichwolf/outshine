#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"

namespace {

int Run(const std::string &cmd, std::string &said) {
  std::FILE *const pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) { return -1; }
  char block[512];
  while (std::fgets(block, sizeof block, pipe) != nullptr) { said += block; }
  return pclose(pipe);
}

[[nodiscard]] std::string Marker(const std::string &where) {
  std::string said;
  Run("cat " + where + "/.prepared-by 2>/dev/null", said);
  return said;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *tmp = std::getenv("TMPDIR");
  const std::string root =
      std::string(tmp != nullptr && *tmp != 0 ? tmp : "/tmp") + "/outshine-owner-claim";
  std::string ignored;

  // board:1839: run.sh's PruneCase deletes only what THIS nest prepared (board:1789), and the
  // marker it compares against was written by exactly one route -- the rebuild. Every other
  // way into the corpus, including the documented offline one, wrote nothing, so the prune
  // silently stopped pruning and the 26 GB it existed to hold down became the floor.
  Run("rm -rf " + root, ignored);
  const std::string prepare =
      "OUTSHINE_CORPUS_OWNER=nest-under-test python3 test/harness/shared/corpus/prepare.py "
      "dry-run --manifest test/render/khronos/glTF/Box/manifest.json --dest " +
      root + " >/dev/null 2>&1";
  Run(prepare, ignored);

  const std::string owned = Marker(root);
  std::printf("NOTE a case prepared under a nest is owned by: '%s'\n", owned.c_str());
  CHECK(owned == "nest-under-test",
        "**A CASE PREPARED THROUGH prepare.py CARRIES ITS OWNER**, so the prune has something "
        "to compare against and deletes what it fetched rather than nothing at all "
        "(board:1839)");

  Run("rm -rf " + root, ignored);
  const std::string offline =
      "env -u OUTSHINE_CORPUS_OWNER python3 test/harness/shared/corpus/prepare.py dry-run "
      "--manifest test/render/khronos/glTF/Box/manifest.json --dest " +
      root + " >/dev/null 2>&1";
  Run(offline, ignored);

  const std::string unowned = Marker(root);
  std::printf("NOTE a case prepared by no runner is owned by: '%s'\n", unowned.c_str());
  CHECK(unowned == "no-runner",
        "**AND ONE PREPARED BY NO RUNNER SAYS SO**, which is a fact a runner can act on -- "
        "'nobody claims this' and 'this is mine' are different, and a marker that is simply "
        "absent is the third thing that reads like neither");

  Run("rm -rf " + root, ignored);

  Covers("IV.16 a case prepared through prepare.py names the nest that prepared it, or names "
         "that no runner did, so the corpus prune has an owner to compare against on every "
         "route into the corpus and not only the rebuild (board:1839)");
  return Report();
}
