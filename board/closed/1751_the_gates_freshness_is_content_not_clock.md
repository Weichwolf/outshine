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

---

REOPENED (review 2026-08-23, round 26). The stamp landed on the OBJECTS and stopped there.
The test BINARY -- the thing that actually produces the PASS line -- is still judged by the
clock, with the same `-nt` the item condemns:

    test/run.sh:879-899  Fresh()
      for freshSource in "$@"; do
        [ "$freshSource" -nt "$freshBinary" ] && return 1
      done
      for freshNeed in $(... <"$freshBinary.d" ...); do
        [ "$freshNeed" -nt "$freshBinary" ] && return 1
      done
      for freshObject in $OBJECTS; do
        [ "$freshObject" -nt "$freshBinary" ] && return 1
      done

`Fresh` is the sole gate on relinking at :997, :1029 and :1060 (plain, sanitised,
validated). The item's own repro applies unchanged one level up: `git stash pop` a test
source, its mtime goes BACKWARDS, `-nt` is false on every arm, the binary is not relinked
and the runner prints PASS for a test nobody compiled. The library objects being correct
does not help -- the test's own translation unit is what carries the assertions.

The `.cmd` file beside the binary covers the command line, not the sources. Demanded: the
binary takes the same treatment as the object -- `SourceStamp` over the test source, its
`LayerExtraSources`, every prerequisite in `$freshBinary.d` and every object in `$OBJECTS`,
written beside the binary and compared for equality. One mechanism, both levels.

Secondary, same file: the object's `setId` is `cksum(groupIncludes|groupStd)`
(test/run.sh:358) and does NOT include `$OPT` or `$WARN`. `$SAN` and `$EXTRA_DEFINES` are
separated by OBJDIR (:1017, :1033, :1048, :1064), so the varying flags are covered today --
but editing `-O2` or the warning set in run.sh silently reuses objects built with the old
one. Fold the whole compile line into the id, or state in the runner why only two of five
flag groups are in it.

---

Closed (the reopened half) -- the BINARY takes the object's treatment: BinaryStamp digests
(mtime, size, name) over the test source, its extra sources, every prerequisite the link
.d names and every object linked in; Fresh compares that stamp for equality, and all three
arms (plain, sanitised, validated) write it beside their .cmd. One mechanism, both levels.
The secondary hole went with it: setId now carries the WHOLE compile line (includes, std,
$OPT, $WARN) -- two of five flag groups is not an identity, and editing -O2 in the runner
silently reused objects built with the old one. Proven by the defect's own repro one level
up: a test source edited to a FALSE assertion and stamped with a year-2020 mtime relinks
and goes red, where -nt kept the old binary and would have printed PASS.
