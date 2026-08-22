Type: bug
Area: test
Tags: build, tests

# The negative control's copy lives in its own nest

1649 keyed the build nest by the checkout — and its own fix moved the audit-control copies
OUT of the nest into the shared temp root. At HEAD:

- test/harness/claims/EverySuiteListsEachSourceOnceAndEverySourceHasASuite.cpp:23 writes
  `${TMPDIR}/audit-control-<name>.sh` (ca4e3ee8 deleted the `/outshine-tests/` segment
  instead of re-keying it).
- test/harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols.cpp:36 writes
  `${TMPDIR}/audit-link-control.sh` — same shared root, same fixed name.

Two parallel checkouts running the fast gate overwrite each other's control script between
the write and the `sh $at` that executes it — the exact collision class 1649 was filed on,
reintroduced by its closure commit. Worse than an interleaved log: the copy carries
`ROOT="$PWD"`, so a neighbour's overwrite makes THIS gate execute the NEIGHBOUR's run.sh
(a different commit's declaration) or a truncated file, and the negative control's verdict
stops being about this tree.

Demanded: the control copies land inside the checkout-keyed nest. run.sh already derives it
(`${TMPDIR}/outshine-tests.$(sha256 of $ROOT | 12 hex)`); the claims tests derive the same
twelve hex from their own cwd, or run.sh exports the nest path to the tests it spawns —
either way a fixed name in the shared temp root is unspellable.

---

Closed: run.sh exports OUTSHINE_NEST (the one place the formula lives -- no re-derivation),
and both control writers land their copies inside it; the symbols test CHECKs the export
exists before writing. A fixed name in the shared temp root is unspellable again. Proving
tests: the two claims tests themselves -- EverySuiteListsEachSourceOnceAndEverySourceHasASuite
and EveryDeclaredSuiteResolvesItsOwnSymbols, whose controls now execute only a copy inside
this checkout's nest. 129/129 warm.
