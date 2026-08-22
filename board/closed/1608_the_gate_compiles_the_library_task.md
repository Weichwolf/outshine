Type: task
Parent: 1602
Area: test

**The fast gate compiles every source under its one include truth**

Plan: the FAST_GATE branch calls BuildLibrary before the tests (incremental; warm cost is
seconds and joins the measured bound), the dead render/outshine/grown entry leaves NAMED_ONLY
(its suite id carries the harness/render prefix), the Makefile's test target says what the
gate is, and the bound is re-measured with warm and cold populations named.

---

**Closed.** The gate compiles the library entire before the test clock starts (warm 1.4 s,
printed per run), the dead NAMED_ONLY entry is gone, the Makefile says what the gate is, and
the bound's populations are named in the runner itself -- warm 48.6 s + 1.4 s over 119 tests,
cold ~126 s by design with the overrun message saying "run again warm". Proof: cold run exits
red by its own judgement, warm run 119/119 exit 0.
