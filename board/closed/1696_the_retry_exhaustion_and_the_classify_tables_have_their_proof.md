Type: bug
Area: test
Tags: data, mirror, backoff

**The retry exhaustion and the classify tables have their proof**

1692's closing note claims "a fallthrough flip or a lost Attempts_ reset can no longer pass
green". Neither claim holds. `test/unit/data/ARetryWaitsOnTheTransportsClock.cpp` drives
429-429-200 against a budget of 4: the budget never exhausts, so the
`[[fallthrough]]`-to-Refused path (`src/data/SourceSet.cpp:127`) never executes — flipping it
to a handover (`continue`) passes the whole suite today. And with a single registered source,
`Attempts_ = 0` at `SourceSet.cpp:64` is reached exactly once — deleting the reset also passes.
1692's own demanded cases are still absent: Retry-forever → Refused with `Begin` called
budget+1 times and `Retried == budget`; the classify tables (200-with-zero-bytes → Retry at
`TerrariumDem.cpp`, 403-is-Absent) remain unpinned.

Two defects in the shipped backoff ride along:

- `SourceSet.cpp:124`: `1 << (query.Attempts_ - 1)` is clamped only AFTER the shift by
  `fmin`; a source declaring `RetryBudget >= 33` (nothing validates `SourceDecl::RetryBudget`)
  makes the shift itself UB. Clamp the exponent, not the product — or refuse the budget at
  `Add`.
- `SourceSet.cpp:12`: kRetryCapMs 4000 claims "four doublings then hold", but with the shipped
  budgets of 4 the ladder is 250·500·1000·2000 — the cap is unreachable and the comment
  miscounts. Either the cap earns a population that reaches it or the number and its prose
  agree.

Demanded: the exhaustion test 1692 specified, the two classify pins, an exponent clamp (or
assembly refusal) with its own check, and the cap's origin corrected.

---

Closed: the backoff doubles through std::ldexp (no shift UB at any budget), the 4000 ms cap
is REACHABLE at the fifth attempt and its prose says so; and the budget-exhaust arm exists --
a never-recovering host walks all four retries into the refusal path with exactly
budget-many begins counted, so a flipped fallthrough or a lost Attempts_ reset goes red. The
1692 overclaim is repaid with the arm it claimed.
