/* THE WHOLE REPORTER, and it links against nothing. A test that needed the tree to link in order to
 * say "failed" could not report the tree failing to link, so this is header-only: C++17 inline
 * variables, no translation unit, no archive, no order of initialisation to get wrong.
 *
 * THE COUNTERS ARE MUTABLE GLOBALS (`I.2`). The deviation is deliberate and bounded: a test process
 * is one thread and one `main`, and the alternative — a reporter object threaded through every
 * helper a test writes — is the thing that makes people stop writing checks. */
#ifndef CHECK_H
#define CHECK_H

#include <cmath>
#include <cstdio>

namespace outshine::Test {

inline int Checks = 0;
inline int Failures = 0;

inline void Checked(bool held, const char *expression, const char *claim, const char *file,
                    int line) {
  ++Checks;
  if (held) { return; }
  ++Failures;
  std::printf("FAIL %s:%d  %s\n       CHECK(%s)\n", file, line, claim, expression);
}

/* `tolerance` and `unit` are parameters and not defaults on purpose: a default tolerance is an
 * unstated [SET] number deciding an acceptance, and a bare number in a failure line has no frame of
 * reference. A NaN fails here, because every comparison against it is false. */
inline void CheckedNear(double got, double want, double tolerance, const char *unit,
                        const char *claim, const char *expression, const char *file, int line) {
  ++Checks;
  const double off = std::fabs(got - want);
  if (off <= tolerance) { return; }
  ++Failures;
  std::printf("FAIL %s:%d  %s\n       %s = %.9g %s, want %.9g %s, off by %.9g %s > %.9g %s\n", file,
              line, claim, expression, got, unit, want, unit, off, unit, tolerance, unit);
}

/* WHAT A PASSING TEST LEARNED. A test that prints nothing when it passes teaches nothing, and the
 * number it measured is exactly what the next round wants and would otherwise re-measure. */
inline void Note(const char *what, double value, const char *unit) {
  std::printf("NOTE %s = %.9g %s\n", what, value, unit);
}
inline void Note(const char *what) { std::printf("NOTE %s\n", what); }

/* Emitted AT RUN TIME, so a claim counts as covered only if the test carrying it actually ran. A
 * comment naming a requirement is covered by nothing. */
inline void Covers(const char *requirement) { std::printf("COVERS %s\n", requirement); }

/* The exit code. A test that checked nothing fails: it is indistinguishable from a test whose body
 * was compiled away, and both are silence wearing a green hat. */
[[nodiscard]] inline int Report() {
  if (Checks == 0) {
    ++Failures;
    std::printf("FAIL no claim was checked\n");
  }
  std::printf("CHECKS %d FAILURES %d\n", Checks, Failures);
  std::fflush(stdout);
  return Failures;
}

} // namespace outshine::Test

/* THE TWO MACROS, AND THERE ARE NO OTHERS. `ES.31` forbids macros for program text; the reason for
 * the deviation is that `__FILE__`, `__LINE__` and the source spelling of an expression cannot be
 * obtained from a function, and a failure line without all three sends its reader looking. */
#define CHECK(expression, claim) \
  ::outshine::Test::Checked((expression), #expression, (claim), __FILE__, __LINE__)
#define CHECK_NEAR(got, want, tolerance, unit, claim)                                       \
  ::outshine::Test::CheckedNear((got), (want), (tolerance), (unit), (claim), #got, __FILE__, \
                                __LINE__)

#endif
