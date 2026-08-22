Type: bug
Area: test
Tags: build, concurrency
Regresses: 1649

**Two gates in one checkout refuse to share the nest**

1649 closed on "the concurrent-gate interleaving that filed this item cannot recur -- the
second runner's nest is a different path". Tonight, 2026-08-23 00:33, it recurred in the
exact filed shape: two gates in the SAME checkout (the working mode of this repository --
parallel agents, one tree) shared ${TMPDIR}/outshine-tests.e112a0838f88 and produced

- `harness-claims-EveryDeclaredSuiteResolvesItsOwnSymbols.log` with TWO trailer lines and a
  mid-line duplicated COVERS fragment -> the outer gate Died with "printed 2 verdict lines"
  (run.sh:341), reporting the reporter instead of the collision;
- the nested `--audit-link` inside that claims test read objects the neighbour gate was
  writing and printed "cannot resolve: 1char_traitsIcEENS9_..." -- a garbage fragment from a
  half-written object file, blamed on harness/render/outshine/grown and
  render/outshine/world, both of which resolve cleanly alone;
- `unit-sim-*`/`unit-ui-*` logs with 00:33 mtimes from a run that was not the one that died.

1649's key (sha256 of $ROOT, run.sh:14) separates CHECKOUTS; it separates nothing within
one. $OBJDIR objects are written in place by $CXX (run.sh:288, no compile-to-temp-then-mv),
log paths are keyed by suite name only, build/liboutshine.a is `rm -f` + `ar` (run.sh:396).

Demanded, in the house's own order -- refusal at assembly over runtime checks: the gate
takes the nest at startup (mkdir "$BUILD/gate.lock" as the atomic claim, holder pid inside)
and a second gate in the same checkout REFUSES LOUDLY by name and pid instead of
interleaving; the lock is released on the EXIT/INT/TERM traps that already exist
(run.sh:43-46). Alternatively objects and logs become collision-proof (write-temp-then-mv,
pid-keyed logs) -- but the lock is one mkdir and makes the whole class unspellable. The
proving test extends TheLayeringIsDeclaredOnce or stands beside it: a second run.sh started
while the lock is held exits with the refusal, and a stale lock (dead pid) is broken with a
named message, not a hang.
