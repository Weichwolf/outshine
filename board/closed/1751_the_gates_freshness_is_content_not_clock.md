Type: bug
Area: test
Tags: gate-honesty

**The gate's freshness is content, not clock**

Found while proving 1746/1750: `git stash pop` restored a source file whose object had been
built from the OTHER version, and `UpToDate` (test/run.sh) reported it current -- the check
was `[ "$source" -nt "$object" ]`, and a checkout, stash pop, revert or `git switch` moves
an mtime BACKWARDS. The gate then ran the old object against the new source and printed
green: a verdict about code nobody compiled. Measured: the same suite answered 120.3125 m
(stale) and 0 m (fresh) on identical sources, minutes apart.

This is the same class the tree already refuses elsewhere -- a measurement that may report
the past is not a measurement -- and the runner is the one instrument every other proof
stands on.

Repaired in the same hour: SourceStamp digests the source and every prerequisite the
compiler named into a `.stamp` beside the object; UpToDate compares digests. A backwards
mtime cannot fool it, and a prerequisite that vanished refuses.

---

Closed -- the freshness check is a sha256 over the source and its named prerequisites,
stamped beside every object as it is built. Proven by the defect's own repro: revert a
source under a built object (mtime backwards) and the object rebuilds, where the -nt check
kept it. The whole gate runs on the new stamp: 205 arms.
