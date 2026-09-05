#ifndef CHECK_H
#define CHECK_H

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <string>

#include <unistd.h>

namespace outshine::Test {

class Tally {
public:
  Tally() = default;
  Tally(const Tally &) = delete;
  Tally &operator=(const Tally &) = delete;
  Tally(Tally &&) = delete;
  Tally &operator=(Tally &&) = delete;

  Tally &operator++() {
    ++Count_;
    return *this;
  }

  int Value() const { return Count_; }

private:
  int Count_ = 0;
};

inline Tally Checks;
inline Tally Failures;
inline Tally Skips;
inline Tally Unprepareds;
inline Tally Partials;

inline void
Checked(bool held, const char *expression, const char *claim, const char *file, int line) {
  ++Checks;
  if (held) { return; }
  ++Failures;
  std::printf("FAIL %s:%d  %s\n       CHECK(%s)\n", file, line, claim, expression);
}

inline void CheckedNear(double got,
                        double want,
                        double tolerance,
                        const char *unit,
                        const char *claim,
                        const char *expression,
                        const char *file,
                        int line) {
  ++Checks;
  const double off = std::fabs(got - want);
  if (off <= tolerance) { return; }
  ++Failures;
  std::printf("FAIL %s:%d  %s\n       %s = %.9g %s, want %.9g %s, off by %.9g %s > %.9g %s\n",
              file,
              line,
              claim,
              expression,
              got,
              unit,
              want,
              unit,
              off,
              unit,
              tolerance,
              unit);
}

inline void Note(const char *what, double value, const char *unit) {
  std::printf("NOTE %s = %.9g %s\n", what, value, unit);
}

inline void Note(const char *what) {
  std::printf("NOTE %s\n", what);
}

inline void Covers(const char *requirement) {
  std::printf("COVERS %s\n", requirement);
}

// where a test plants an artefact: inside the checkout-keyed nest run.sh exports, or -- run
// standalone -- under a pid-keyed name in the temp root, so two runners never share a path
[[nodiscard]] inline std::string PlantedPath(const char *name) {
  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest != nullptr && *nest != 0) { return std::string(nest) + "/" + name; }
  const char *tmp = std::getenv("TMPDIR");
  std::string root = (tmp != nullptr && *tmp != 0) ? tmp : "/tmp";
  if (root.back() == '/') { root.pop_back(); }
  return root + "/outshine-" + std::to_string(getpid()) + "-" + name;
}

// board:1810: a case that stops on its own wall-clock budget (board:1778) reported the
// truncation in its LOG and returned PASS, so the trailer -- the line CLAUDE.md tells a reader
// to read FIRST -- could not tell a whole route from a seventh of one. UNPREPARED already means
// "this run judged nothing here"; PARTIAL is the third member of that family and it carries the
// share, because the truncation point moves with the machine.
inline void Partial(double share, const char *ofWhat) {
  ++Partials;
  std::printf("PARTIAL %.6f %s\n", share, ofWhat);
}

inline void Unprepared(const char *what) {
  ++Unprepareds;
  std::printf("UNPREPARED %s\n", what);
}

inline void Skip(const char *why) {
  ++Skips;
  std::printf("SKIP %s\n", why);
}

[[nodiscard]] inline std::vector<std::filesystem::path> Sources(const char *root) {
  std::vector<std::filesystem::path> found;
  std::error_code why;
  auto walk = std::filesystem::recursive_directory_iterator(root, why);
  if (why) {
    ++Failures;
    std::printf(
        "FAIL the tree '%s' this claim walks is not there: %s\n", root, why.message().c_str());
    return found;
  }
  // a directory whose name starts with a dot is somebody else's: the lab's .venv under test/
  // carries a numpy .cpp and a scipy .py that spell a compiler, and neither is this tree's word
  for (auto at = std::filesystem::begin(walk); at != std::filesystem::end(walk); ++at) {
    if (at->is_directory() && at->path().filename().string().starts_with(".")) {
      at.disable_recursion_pending();
      continue;
    }
    if (at->is_regular_file()) { found.push_back(at->path()); }
  }
  return found;
}

[[nodiscard]] inline int Report() {
  // board:1845: a case that judged NONE of its subject says so with Partial, and that is a
  // legal answer -- so the vacuum guard counts it beside the other two ways of saying nothing
  // was judged. Without it a case survives on whatever unrelated CHECK it happened to make.
  if (Checks.Value() == 0 && Skips.Value() == 0 && Unprepareds.Value() == 0 &&
      Partials.Value() == 0) {
    ++Failures;
    std::printf("FAIL no claim was checked\n");
  }
  std::printf("CHECKS %d FAILURES %d SKIPPED %d UNPREPARED %d PARTIAL %d\n",
              Checks.Value(),
              Failures.Value(),
              Skips.Value(),
              Unprepareds.Value(),
              Partials.Value());
  std::fflush(stdout);
  return (Failures.Value() == 0 && Unprepareds.Value() == 0) ? 0 : 1;
}

} // namespace outshine::Test

#define CHECK(expression, claim)                                                                   \
  ::outshine::Test::Checked((expression), #expression, (claim), __FILE__, __LINE__)
#define CHECK_NEAR(got, want, tolerance, unit, claim)                                              \
  ::outshine::Test::CheckedNear(                                                                   \
      (got), (want), (tolerance), (unit), (claim), #got, __FILE__, __LINE__)

#endif
