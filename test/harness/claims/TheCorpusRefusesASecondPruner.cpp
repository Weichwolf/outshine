#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Run;

namespace {

// The guard is READ OUT OF run.sh and called, so the two cannot diverge: a copy of it as a
// string literal here would stay green the day the guard left the runner (board:1838).
[[nodiscard]] std::string DrivesTheRunnersGuard(const std::string &root, const std::string &where,
                                                const char *nest) {
  const std::string carved = root + "/guard.sh";
  std::string ignored;
  Run("{ echo 'Guard() {'; awk '/prunePreparer=/,/^  fi$/' test/run.sh; echo '  printf PRUNES'; "
      "echo '}'; } > " + carved, ignored);
  std::string said;
  Run("sh -c '. " + carved + "; prunePrepared=" + where + "; NEST=" + std::string(nest) +
          "; notMine=0; Guard; [ \"$notMine\" -gt 0 ] && printf LEFT-ALONE' 2>/dev/null",
      said);
  return said;
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
      std::string(tmp != nullptr && *tmp != 0 ? tmp : "/tmp") + "/outshine-pruner-claim";
  std::string ignored;
  Run("rm -rf " + root + " && mkdir -p " + root + "/mine " + root + "/theirs " + root + "/nobodys",
      ignored);
  Run("printf aaaaaaaaaaaa > " + root + "/mine/.prepared-by", ignored);
  Run("printf bbbbbbbbbbbb > " + root + "/theirs/.prepared-by", ignored);

  const auto asks = [&](const char *where, const char *nest) {
    return DrivesTheRunnersGuard(root, root + "/" + where, nest);
  };

  const std::string mine = asks("mine", "aaaaaaaaaaaa");
  const std::string theirs = asks("theirs", "aaaaaaaaaaaa");
  const std::string nobodys = asks("nobodys", "aaaaaaaaaaaa");

  std::printf("NOTE a case this nest prepared:      %s\n", mine.c_str());
  std::printf("NOTE a case another nest prepared:   %s\n", theirs.c_str());
  std::printf("NOTE a case nobody claims:           %s\n", nobodys.c_str());

  CHECK(mine == "PRUNES",
        "a runner prunes what it prepared itself, which is what the prune is for -- holding the "
        "26 GB peak down over its own run");
  CHECK(theirs == "LEFT-ALONE",
        "**AND IT LEAVES ALONE WHAT ANOTHER NEST PREPARED**: the corpus is one directory for "
        "every checkout and the hourly review is MANDATED to run in its own worktree, so a "
        "prune scoped only by a lock decides which runner loses its subjects mid-run rather "
        "than that neither does (board:1789)");
  CHECK(nobodys == "LEFT-ALONE",
        "**AND IT LEAVES ALONE WHAT NOBODY CLAIMS**, because 'first to delete owns it' is the "
        "same race with a shorter fuse");

  Run("rm -rf " + root, ignored);

  // The marker the guard above compares against is written by EVERY route into the corpus,
  // not only by the rebuild: one route that wrote nothing stopped the prune silently
  // (board:1839).
  const std::string made = root + "-owner";
  Run("rm -rf " + made, ignored);
  Run("OUTSHINE_CORPUS_OWNER=nest-under-test python3 test/harness/shared/corpus/prepare.py "
      "dry-run --manifest test/render/khronos/glTF/Box/manifest.json --dest " +
          made + " >/dev/null 2>&1",
      ignored);
  const std::string owned = Marker(made);
  std::printf("NOTE a case prepared under a nest is owned by: '%s'\n", owned.c_str());
  CHECK(owned == "nest-under-test",
        "**A CASE PREPARED THROUGH prepare.py CARRIES ITS OWNER**, so the guard above has "
        "something to compare against and deletes what it fetched rather than nothing at all "
        "(board:1839)");

  Run("rm -rf " + made, ignored);
  Run("env -u OUTSHINE_CORPUS_OWNER python3 test/harness/shared/corpus/prepare.py dry-run "
      "--manifest test/render/khronos/glTF/Box/manifest.json --dest " +
          made + " >/dev/null 2>&1",
      ignored);
  const std::string unowned = Marker(made);
  std::printf("NOTE a case prepared by no runner is owned by: '%s'\n", unowned.c_str());
  CHECK(unowned == "no-runner",
        "**AND ONE PREPARED BY NO RUNNER SAYS SO**: 'nobody claims this' and 'this is mine' are "
        "different answers, and a marker that is simply absent reads like neither");

  Run("rm -rf " + made, ignored);

  Covers("IV.22 the prepared corpus is shared and the right to delete from it is not: every "
         "route into the corpus writes an owner, and the runner's guard prunes what this nest "
         "prepared and leaves every other case standing (board:1789, 1839)");
  return Report();
}
