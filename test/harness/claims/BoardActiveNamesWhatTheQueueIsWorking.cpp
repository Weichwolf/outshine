#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;
using outshine::Test::Run;

namespace {

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board/ is one flat directory and State is an attribute of the item, so the drawer that
  // used to vanish when git could not carry it empty (board:1808) cannot vanish: there is
  // nothing to carry but the items themselves.
  std::vector<std::string> standing;
  for (const std::string &one : Lines(Ask("grep -l '^State: active' board/*.md 2>/dev/null"))) {
    const size_t slash = one.rfind('/');
    standing.push_back(slash == std::string::npos ? one : one.substr(slash + 1));
  }
  Note("items whose State is active", (double)standing.size(), "items");
  for (const std::string &one : standing) { std::printf("NOTE active: %s\n", one.c_str()); }

  // board:1790: the first version of this claim asked whether board/ was NON-EMPTY and
  // how many WALL-CLOCK hours had passed since each entry moved. Both were wrong. A tree
  // nobody touched for three hours went red against itself, and the tree's own terminal state
  // -- no open item, nothing in flight -- made the claim red forever. A gate that cannot be
  // green at the finish line is not a gate.
  //
  // What is actually checkable is a relation INSIDE the repository: an item standing in
  // board/ was moved there by a commit near HEAD. kRecentCommits [SET] = 20: the queue
  // spends one to three commits per item, and two review rounds run about ten, so an entry
  // untouched across twenty commits is parked rather than worked. No clock is read.
  constexpr int kRecentCommits = 20;
  const std::string touched =
      Ask("git log -n " + std::to_string(kRecentCommits) +
          " --name-only --format= -- board/ 2>/dev/null");
  CHECK(!touched.empty(), "the walk can read the board's own history");

  std::vector<std::string> parked;
  for (const std::string &one : standing) {
    // the PATH must be board//<name>: a file name alone also matches the commit that
    // moved the item OUT of active, which is the opposite of being in flight.
    const bool moved = touched.find("board//" + one) != std::string::npos;
    std::printf("NOTE %s moved by one of the last %d commits: %s\n", one.c_str(),
                kRecentCommits, moved ? "yes" : "no");
    if (!moved) {
      parked.push_back(one + " stands in board/ and no commit in the last " +
                       std::to_string(kRecentCommits) + " touched it");
    }
  }

  for (const std::string &one : parked) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(parked.empty(),
        "**WHAT board/ NAMES IS ACTUALLY IN FLIGHT**: an item there was moved there by "
        "a commit near HEAD, so an agent asking what the queue is working is told the truth "
        "-- and an EMPTY board/ is a legal answer, because a tree with nothing left to "
        "work has nothing in flight (board:1783, 1790)");

  Covers("IV.14 every item standing in board/ was moved there by a recent commit -- a "
         "relation inside the repository, never a relation to the wall clock, so the gate "
         "cannot go red because time passed (board:1783, 1790)");
  return Report();
}
