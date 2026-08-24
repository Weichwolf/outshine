#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Ask(const std::string &command) {
  std::string said;
  std::FILE *const pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) { return said; }
  char block[512];
  while (std::fgets(block, sizeof block, pipe) != nullptr) { said += block; }
  pclose(pipe);
  while (!said.empty() && (said.back() == '\n' || said.back() == ' ')) { said.pop_back(); }
  return said;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1808: the throwing overload, on a path this claim does not own. A directory the
  // repository requires but git cannot carry empty is a directory that vanishes the moment the
  // queue finishes -- and a claim that ABORTS there reports no verdict at all, which is worse
  // than either answer it could have given. board/active/README.md keeps the drawer in the
  // tree; this reads it without a throw either way.
  std::error_code walking;
  std::vector<std::string> standing;
  for (const auto &entry : std::filesystem::directory_iterator("board/active", walking)) {
    if (entry.path().extension() != ".md") { continue; }
    if (entry.path().filename() == "README.md") { continue; }
    standing.push_back(entry.path().filename().string());
  }
  std::printf("NOTE the drawer %s\n", walking ? "is not in the tree" : "stands");
  CHECK(!walking,
        "**THE DRAWER THE QUEUE USES IS PART OF THE TREE**: git carries no empty directory, so "
        "without a tracked file board/active vanishes the moment the last item closes -- and "
        "every path that cites it stops resolving in a fresh clone while the nest, holding an "
        "untracked leftover, disagrees (board:1808)");
  Note("items standing in board/active", (double)standing.size(), "items");
  for (const std::string &one : standing) { std::printf("NOTE active: %s\n", one.c_str()); }

  // board:1790: the first version of this claim asked whether board/active was NON-EMPTY and
  // how many WALL-CLOCK hours had passed since each entry moved. Both were wrong. A tree
  // nobody touched for three hours went red against itself, and the tree's own terminal state
  // -- no open item, nothing in flight -- made the claim red forever. A gate that cannot be
  // green at the finish line is not a gate.
  //
  // What is actually checkable is a relation INSIDE the repository: an item standing in
  // board/active was moved there by a commit near HEAD. kRecentCommits [SET] = 20: the queue
  // spends one to three commits per item, and two review rounds run about ten, so an entry
  // untouched across twenty commits is parked rather than worked. No clock is read.
  constexpr int kRecentCommits = 20;
  const std::string touched =
      Ask("git log -n " + std::to_string(kRecentCommits) +
          " --name-only --format= -- board/ 2>/dev/null");
  CHECK(!touched.empty(), "the walk can read the board's own history");

  std::vector<std::string> parked;
  for (const std::string &one : standing) {
    // the PATH must be board/active/<name>: a file name alone also matches the commit that
    // moved the item OUT of active, which is the opposite of being in flight.
    const bool moved = touched.find("board/active/" + one) != std::string::npos;
    std::printf("NOTE %s moved by one of the last %d commits: %s\n", one.c_str(),
                kRecentCommits, moved ? "yes" : "no");
    if (!moved) {
      parked.push_back(one + " stands in board/active and no commit in the last " +
                       std::to_string(kRecentCommits) + " touched it");
    }
  }

  for (const std::string &one : parked) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(parked.empty(),
        "**WHAT board/active NAMES IS ACTUALLY IN FLIGHT**: an item there was moved there by "
        "a commit near HEAD, so an agent asking what the queue is working is told the truth "
        "-- and an EMPTY board/active is a legal answer, because a tree with nothing left to "
        "work has nothing in flight (board:1783, 1790)");

  Covers("IV.14 every item standing in board/active was moved there by a recent commit -- a "
         "relation inside the repository, never a relation to the wall clock, so the gate "
         "cannot go red because time passed (board:1783, 1790)");
  return Report();
}
