#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

namespace {

// EVERY BOARD ITEM SAYS HOW UNREAL AND RAGE DO IT AND WHICH WAY THIS TREE TAKES.
//
// The rule is in CLAUDE.md and it exists because of what happened without it. board:1965 was
// filed on the premise that this tree has no joints at all; it had one, misnamed `Contact`, and
// the item's whole shape followed from the wrong reading. board:1978 was filed on a question
// board:1879 already carried, because nobody had looked. Both cost a round.
//
// A benchmark line is not paperwork. Writing it forces the one question that catches both of
// those: what do the two engines that already shipped this actually do? Where they agree, the
// item was never in doubt. Where they differ, the line has to PICK and give a reason. And where
// neither faces the question -- everything about deriving a world from OSM, because both take
// their world authored -- saying so is itself the finding, because it means there is no vendor to
// check the answer against and the bar has to be argued from first principles.
//
// The walk is deliberately shallow: the line must be there and must name both engines or say
// plainly that neither answers. Judging whether the ANSWER is right is not a string's job.

[[nodiscard]] bool Names(const std::string &body) {
  const size_t at = body.find("**Benchmark**");
  if (at == std::string::npos) { return false; }
  const size_t end = body.find("\n\n", at);
  const std::string line = body.substr(at, end == std::string::npos ? end : end - at);
  const bool named = line.find("Unreal") != std::string::npos && line.find("RAGE") != std::string::npos;
  const bool neither = line.find("Neither") != std::string::npos ||
                       line.find("neither") != std::string::npos ||
                       line.find("The choice is mine") != std::string::npos;
  return named || neither;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<std::string> items = Lines(Ask("ls board/*.md 2>/dev/null"));
  CHECK(!items.empty(), "the board is readable from the tree root");

  std::vector<std::string> silent;
  for (const std::string &path : items) {
    const std::string body = Ask("cat '" + path + "'");
    if (!Names(body)) { silent.push_back(path); }
  }

  std::printf("BOARD %zu item(s), %zu without a benchmark line\n", items.size(), silent.size());
  for (const std::string &one : silent) { std::printf("  %s\n", one.c_str()); }

  CHECK(silent.empty(),
        "**EVERY ITEM NAMES THE BENCHMARK**: what Unreal does, what RAGE does, and which way this "
        "tree takes -- or that neither engine faces the question, which is the finding rather "
        "than an excuse. An item without the line is an item whose premise nobody checked, and "
        "this board has twice paid a round for exactly that");

  Covers("the board: every item carries how Unreal and RAGE answer its question and which answer "
         "this tree takes, so a premise is checked against the two bodies of evidence before work "
         "starts rather than after a round is spent");
  return Report();
}
