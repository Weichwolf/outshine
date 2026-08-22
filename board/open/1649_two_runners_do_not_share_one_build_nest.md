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
