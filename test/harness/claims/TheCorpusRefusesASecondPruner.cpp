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

// board:1838: this claim used to carry a COPY of the guard as a string literal -- four lines of
// sh inside C++, which the tree forbids, and which meant deleting the guard from run.sh left
// the claim green. It reads PruneCase out of the runner now and calls it, so the two cannot
// diverge: run.sh is sourced with the surrounding machinery stubbed to nothing.
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
