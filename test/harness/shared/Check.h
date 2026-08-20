#ifndef CHECK_H
#define CHECK_H

#include <cmath>
#include <cstdio>

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

inline void Checked(bool held, const char *expression, const char *claim, const char *file,
                    int line) {
  ++Checks;
  if (held) { return; }
  ++Failures;
  std::printf("FAIL %s:%d  %s\n       CHECK(%s)\n", file, line, claim, expression);
}

inline void CheckedNear(double got, double want, double tolerance, const char *unit,
                        const char *claim, const char *expression, const char *file, int line) {
  ++Checks;
  const double off = std::fabs(got - want);
  if (off <= tolerance) { return; }
  ++Failures;
  std::printf("FAIL %s:%d  %s\n       %s = %.9g %s, want %.9g %s, off by %.9g %s > %.9g %s\n", file,
              line, claim, expression, got, unit, want, unit, off, unit, tolerance, unit);
}

inline void Note(const char *what, double value, const char *unit) {
  std::printf("NOTE %s = %.9g %s\n", what, value, unit);
}
inline void Note(const char *what) { std::printf("NOTE %s\n", what); }

inline void Covers(const char *requirement) { std::printf("COVERS %s\n", requirement); }

inline void Unprepared(const char *what) {
  ++Unprepareds;
  std::printf("UNPREPARED %s -- run test/harness/shared/corpus/prepare.py\n", what);
}

inline void Skip(const char *why) {
  ++Skips;
  std::printf("SKIP %s\n", why);
}

[[nodiscard]] inline int Report() {
  if (Checks.Value() == 0 && Skips.Value() == 0 && Unprepareds.Value() == 0) {
    ++Failures;
    std::printf("FAIL no claim was checked\n");
  }
  std::printf("CHECKS %d FAILURES %d SKIPPED %d UNPREPARED %d\n", Checks.Value(),
              Failures.Value(), Skips.Value(), Unprepareds.Value());
  std::fflush(stdout);
  return (Failures.Value() == 0 && Unprepareds.Value() == 0) ? 0 : 1;
}

}

#define CHECK(expression, claim) \
  ::outshine::Test::Checked((expression), #expression, (claim), __FILE__, __LINE__)
#define CHECK_NEAR(got, want, tolerance, unit, claim)                                       \
  ::outshine::Test::CheckedNear((got), (want), (tolerance), (unit), (claim), #got, __FILE__, \
                                __LINE__)

#endif
