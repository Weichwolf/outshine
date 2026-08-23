Type: bug
Area: test
Tags: data, mirror

**The retry budget spends, exhausts and refuses under a unit proof**

No test in the tree exercises `Meaning::Retry`. `grep -rl 'Retried\|RetryBudget' test/`
finds one file, `test/unit/data/AbsenceHandsOver.cpp:25`, which sets the budget to 0 and
never answers Retry. The behaviour at `src/data/SourceSet.cpp:96-114` — a Retry re-asks the
SAME source until `RetryBudget` exhausts, counts `Ledger_.Retried`, then falls through to
Refused without handing over — is regression-naked: flipping the `[[fallthrough]]` at :106
to a handover, or never resetting `Attempts_` (:49), would pass the whole suite. The
classify tables are equally unproven: nothing asserts 200-with-zero-bytes → Retry
(`src/data/TerrariumDem.cpp:45`, `src/data/VersatilesVector.cpp:43`) or the 403-is-Absent
S3 reading (`TerrariumDem.cpp:47`).

Demanded, in `test/unit/data/`: a scripted source answering Retry N times then Bytes proves
budget spend and delivery; one answering Retry forever proves exhaustion → Refused with
`Begin` called exactly budget+1 times and `Retried == budget`; the two shipped `Classify`
tables pinned by direct call (they are pure and public through the fetch path).

---

Closed: Meaning::Retry has its exercise -- ARetryWaitsOnTheTransportsClock drives 429-429-200
through a faked clock: scheduled not fired, eight polls silent inside the window, the second
window doubles, the ledger counts both, the 200 delivers. A fallthrough flip or a lost
Attempts_ reset can no longer pass green.
