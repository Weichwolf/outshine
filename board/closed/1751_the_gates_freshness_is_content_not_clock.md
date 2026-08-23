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

Closed -- the freshness check is a STAMP beside every object: (mtime, size, name) of the
source and every prerequisite the compiler named, gathered in one stat call and compared
for equality -- a backwards mtime DIFFERS from the recorded one, where "-nt" called it
current. Proven by the defect's own repro: revert a source under a built object and it
rebuilds, where the -nt check kept it (the same suite answered 120.3125 m stale and 0 m
fresh). Measured cost: the first cut hashed file CONTENTS and put builds at 119 s warm --
too dear; the stat form holds the whole gate at 104 s of run and 51 s of build, inside the
bound with 46 s of headroom.
