Type: bug
Area: test
Tags: build

**Two runners do not share one build nest**

Observed tonight, reproducibly: a fast gate in the checkout and a second gate in a reviewer's
worktree ran concurrently and BOTH wrote ${TMPDIR}/outshine-tests -- one log file carried two
interleaved verdict trailers (unit-data-TheStoreNamesBytesByTheirKey.log: "printed 2 verdict
lines"), and the content-store test's fixture directory was swept by the neighbour mid-run,
failing checks that pass alone. The build nest, the log directory and every test fixture
under $BUILD are keyed by nothing: not by checkout path, not by process. Demanded: the nest
carries the checkout's identity (hash of $ROOT in the path, the way objects already carry
their include-set checksum), so parallel checkouts get parallel nests and a collision is
unspellable; the fixture sweep then needs no lock at all.

---

Closed: run.sh keys the nest by the checkout -- BUILD becomes
${TMPDIR}/outshine-tests.$(sha256 of $ROOT, 12 hex) -- so parallel checkouts and worktrees
build, log and sweep beside each other; the audit-control copies moved out of the nest they
no longer own. The prepared corpus stays shared BY DESIGN (it is keyed by content, not by
run). Proving test: test/harness/claims/TheLayeringIsDeclaredOnce.cpp holds the keyed
spelling in run.sh; the concurrent-gate interleaving that filed this item cannot recur --
the second runner's nest is a different path. 127/127 warm at 56.3 s in the fresh nest.
