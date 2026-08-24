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

// The runner's own guard, extracted verbatim from test/run.sh's PruneCase. Driving it here
// rather than reading it is what makes this a measurement: a claim that quotes a shell
// fragment and never runs it proves the quote, not the behaviour.
constexpr const char *kGuard =
    "prunePreparer=$1/.prepared-by; NEST=$2; "
    "if [ \"$(cat \"$prunePreparer\" 2>/dev/null)\" != \"$NEST\" ]; "
    "then printf LEFT-ALONE; else printf PRUNES; fi";

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
    std::string said;
    Run("sh -c '" + std::string(kGuard) + "' guard " + root + "/" + where + " " + nest, said);
    return said;
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
        "every checkout, and the hourly review is MANDATED to run in its own worktree, so a "
        "prune scoped only by a lock decides which runner loses its subjects mid-run rather "
        "than that neither does. Measured once at 4 UNPREPARED on a gate that had 0 an hour "
        "earlier, with no glTF change in the delta (board:1789)");
  CHECK(nobodys == "LEFT-ALONE",
        "**AND IT LEAVES ALONE WHAT NOBODY CLAIMS**, because 'first to delete owns it' is the "
        "same race with a shorter fuse -- an unclaimed case is one prepare.py made outside any "
        "runner, and the runner that did not make it cannot know who is reading it");

  Run("rm -rf " + root, ignored);

  Covers("IV.14 the prepared corpus is shared and the right to delete from it is not: a runner "
         "prunes the cases it prepared and leaves every other case standing (board:1789)");
  return Report();
}
