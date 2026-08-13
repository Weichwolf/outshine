/* THE WHOLE REPORTER, and it links against nothing. A test that needed the tree to link in order to
 * say "failed" could not report the tree failing to link, so this is header-only: C++17 inline
 * variables, no translation unit, no archive, no order of initialisation to get wrong.
 *
 * THE VERDICT IS THE TRAILER THIS PRINTS, never the exit status. A POSIX status is eight bits: 256
 * failed claims come back as 0 and 77 of them used to read as "skipped", so the count a reader sees
 * is the count the harness judges by, and the status carries one bit that is cross-checked against
 * it.
 *
 * THE COUNTERS ARE MUTABLE GLOBALS (`I.2`). The deviation is deliberate and bounded: a test process
 * is one thread and one `main`, and the alternative — a reporter object threaded through every
 * helper a test writes — is the thing that makes people stop writing checks. What it does not buy is
 * a writable score: a `Tally` counts up and cannot be assigned, so `Failures = 0` has no spelling. */
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

/* WHAT THE TREE DOES NOT CARRY YET, AND THAT IS NOT THE SAME AS A SKIP. The corpus is untracked by
 * design (board:0083: a case directory's only tracked file is its manifest), so a
 * fresh clone has the declarations and none of the fetched, converted or rendered inputs. A test that
 * SKIPPED there would be indistinguishable from a test that passed having compared nothing -- the
 * same vacuous shape as two empty images, one level up. So this is its own word, it is RED, and the
 * harness counts it in the trailer, which is what makes a run over an empty corpus unable to read as
 * a clean run. */
inline void Unprepared(const char *what) {
  ++Unprepareds;
  std::printf("UNPREPARED %s -- run test/corpus/prepare.py\n", what);
}

/* A SKIP IS SPELLED HERE OR NOWHERE. It used to be an exit status the harness recognised, which made
 * it indistinguishable from that many failed claims; it is a declared word in the trailer now. */
inline void Skip(const char *why) {
  ++Skips;
  std::printf("SKIP %s\n", why);
}

/* A test that checked nothing and skipped nothing fails: it is indistinguishable from a test whose
 * body was compiled away, and both are silence wearing a green hat. The return value is one bit
 * because the trailer already carries the count — a status that tried to carry it lied at 256. */
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
