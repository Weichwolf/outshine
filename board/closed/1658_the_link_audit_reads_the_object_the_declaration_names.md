Type: bug
Area: test
Tags: build, audit

**The link audit reads exactly the object the current declaration names**

1656 closed on "the audit reads the gate's own objects". The resolver (test/run.sh:457-461)
strips `\.[0-9]+\.o$` and takes the FIRST object per stem in `ls` order — the setId that
1603 put into the name precisely to distinguish include sets is discarded at the moment of
selection. Nothing purges superseded setIds (no rm in the gate; Prune.cpp never touches
obj/), so after any GroupIncludes or toolchain edit both `stem.old.o` and `stem.new.o`
stand in $BUILD/obj and the audit may pick the stale sibling: it then proves closure over
yesterday's symbol set — a vacuous pass over a since-lost unit, or a false alarm over a
since-removed symbol. The exact "measures the past" class the trailer rule exists for.

The comment's own defence (run.sh:438-439, "symbol NAMES do not depend on the include
set") holds for EXTRA_DEFINES and SAN only by directory separation — obj-validated
(run.sh:925) and obj-sanitised (:895) never land in $BUILD/obj — but it does not hold
across time within $BUILD/obj.

Demanded: the audit computes each unit's setId exactly as BuildGroup does
(run.sh:285, `cksum` of includes|std) and matches `stem.$setId.o` alone; a stem whose
current-setId object is absent is the cold case that already exists (BuildGroup at :466).
One line of awk input gains the expected setId; stale siblings become invisible.

---

Closed, and the exact matching immediately caught a second defect the loose matching hid:
the audit now computes each group's setId exactly as BuildGroup does and reads
<stem>.<setId>.o -- a stale object from an old include set is unspellable; a missing exact
object triggers one BuildGroup of that group; and a DECLARED SOURCE THAT DOES NOT EXIST
refuses as "a ghost in the listing" -- which src/clients/SceneWeather.cpp was: deleted from
the tree on 2026-08-20, still declared by render/outshine/world and its include row, skipped
in silence by every [ -e ] || continue since. The ghost is out of the declaration. Proving
test: EveryDeclaredSuiteResolvesItsOwnSymbols -- three controls now (seeded loss, seeded
ghost, and the real tree must close), all inside the exported nest. Gate 130/130 at 55.2 s.
