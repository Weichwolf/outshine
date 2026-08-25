#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const std::string &path) {
  std::ifstream reading(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(reading)),
                     std::istreambuf_iterator<char>());
}

// every -Wsomething and -std=something in a chunk of text, in the order it stands.
[[nodiscard]] std::vector<std::string> Flags(const std::string &text) {
  std::vector<std::string> found;
  for (size_t at = text.find('-'); at != std::string::npos; at = text.find('-', at + 1)) {
    const bool warning = text.compare(at, 2, "-W") == 0;
    const bool standard = text.compare(at, 5, "-std=") == 0;
    if (!warning && !standard) { continue; }
    size_t ends = at;
    while (ends < text.size() && text[ends] != '"' && text[ends] != ' ' && text[ends] != '\n' &&
           text[ends] != ',' && text[ends] != '\'') {
      ++ends;
    }
    found.push_back(text.substr(at, ends - at));
    at = ends;
  }
  std::sort(found.begin(), found.end());
  found.erase(std::unique(found.begin(), found.end()), found.end());
  return found;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1786: the preparer stood at -std=c++17 in a C++23 tree until a repair to something
  // else happened to touch it. The gate's toolchain has ONE spelling or the corpus is built by
  // a compiler the gate never uses -- and nothing says so.
  const std::string runner = Slurp("test/run.sh");
  CHECK(!runner.empty(), "the runner, which is the one spelling of the toolchain, is readable");

  const size_t stdAt = runner.find("CXXSTD=");
  const size_t warnAt = runner.find("WARN=");
  CHECK(stdAt != std::string::npos && warnAt != std::string::npos,
        "the runner declares a standard and a warning set");
  if (stdAt == std::string::npos || warnAt == std::string::npos) { return Report(); }
  const std::vector<std::string> runnerFlags =
      Flags(runner.substr(stdAt, runner.find('\n', stdAt) - stdAt) + " " +
            runner.substr(warnAt, runner.find('\n', warnAt) - warnAt));

  std::vector<std::string> others;
  for (const auto &entry : Sources("test")) {
    if (entry.extension() != ".py" && entry.extension() != ".sh") { continue; }
    if (entry.string() == "test/run.sh") { continue; }
    const std::string text = Slurp(entry);
    for (const char *spelled : {"clang++", "-std=c++", "g++ "}) {
      if (text.find(spelled) != std::string::npos) {
        others.push_back(entry.string() + " spells " + spelled);
      }
    }
  }
  for (const std::string &one : others) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(others.empty(),
        "**ONE TOOLCHAIN IS SPELLED ONCE**: test/run.sh is the only place in test/ that names a "
        "compiler or a standard, so the corpus cannot be built by a compiler the gate never uses "
        "(board:1786)");

  for (const std::string &one : runnerFlags) { std::printf("NOTE the runner: %s\n", one.c_str()); }
  Note("flags the runner declares", (double)runnerFlags.size(), "flags");

  Covers("IV.20 one toolchain is spelled once: test/run.sh is the only place in test/ that names "
         "a compiler or a standard, so no corpus can be built by a compiler the gate never uses "
         "(board:1786)");
  return Report();
}
