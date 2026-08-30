#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

// A SHELL `case` TAKES THE FIRST MATCH, so a label listed twice has one live branch and one
// that can never run. `test/run.sh` resolves every suite's include set, link groups, toolchain
// and libraries through such statements, and it carried two dead branches: `outshine/scenario`
// appeared in both the narrow arm and the wide engine-linking arm of LayerIncludes and of
// LayerGroups. Reading the file said that suite links the whole engine. It links four things.
//
// Unreal errors on a duplicate module in a Build.cs dependency list and RAGE keeps one entry per
// library in the project. Both agree, and the reason is exactly this: a build declaration naming
// the same thing twice has one live answer and one lie, and which is which must not be left to
// reading order.
//
// WHAT THIS COSTS WHEN IT IS MISSED is not a wrong answer but a confusing one. A new case in
// outshine/scenario failed to link against ReadScenario while the file plainly said that suite
// links src/scenario -- an hour available to anyone who trusts the wide branch. The refusal is
// cheap and the confusion is not.
//
// The walk tracks `case ... in` / `esac` rather than function boundaries, so two statements
// inside one function are judged apart, and it reads EVERY case in the runner rather than the
// four this defect happened to live in.

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<std::string> twice =
      Lines(Ask("awk '"
                "/case .* in$/ { ++deep; next } "
                "/^ *esac/ { for (k in seen) { if (index(k, deep \"\\t\") == 1) { delete seen[k] } "
                "} --deep; next } "
                "deep > 0 && /^ *[^ #].*\\) / { "
                "  line = $0; sub(/\\).*$/, \"\", line); gsub(/^ +/, \"\", line); "
                "  n = split(line, labels, \"[|]\"); "
                "  for (i = 1; i <= n; i++) { "
                "    lab = labels[i]; gsub(/^ +| +$/, \"\", lab); "
                "    if (lab == \"*\" || lab == \"\") { continue } "
                "    key = deep \"\\t\" lab; "
                "    if (key in seen) { printf \"%s at line %d, already at line %d\\n\", lab, FNR, "
                "seen[key] } "
                "    else { seen[key] = FNR } "
                "  } "
                "}' test/run.sh"));

  std::printf("  case labels declared twice in test/run.sh: %zu\n", twice.size());
  for (const std::string &one : twice) { std::printf("    %s\n", one.c_str()); }

  CHECK(twice.empty(),
        "**A SUITE IS DECLARED ONCE**: a shell `case` takes the first match, so a second branch "
        "for a label already listed can never run. The file then states two answers and only "
        "reading order says which one the build uses -- which is how a suite that appeared to "
        "link the whole engine actually linked four things");

  Covers("the runner's own declarations: no suite, layer or group name is listed twice in a "
         "`case`, so what the file says a suite reaches is what the build gives it");
  return Report();
}
