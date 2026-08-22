Type: bug
Area: test
Tags: build, audit

**The link audit refuses an object older than its source**

1658 made the audit read exactly `<stem>.<setId>.o` and refuse ghosts. One staleness axis
survives: the audit accepts a present object on existence alone (test/run.sh:485,
`[ -f "$OBJDIR/$objName" ]`) -- `UpToDate` (run.sh:259, dep-walking, one call away) is
never asked. Inside the gate this is masked because BuildLibrary refreshes every src group
first; standalone `test/run.sh --audit-link` after a source edit reads the pre-edit object
and proves yesterday's symbol set -- the same "measures the past" class 1658 named, on the
source axis instead of the include axis.

Also stale: the block comment run.sh:437-440 still says "no compile, no relink; a source
with no object yet is the cold case" -- the cold path has compiled via BuildGroup since
1658 (:491-497). The comment describes the predecessor.

Demanded: the existence check becomes `[ -f ... ] && UpToDate "$OBJDIR/$objName" "$unit"`
(a stale object is the cold case, which already rebuilds the group once); the comment
either tells the truth or goes. Minor, same block: after a cold BuildGroup the group's
objects enter $OBJECTS twice (BuildGroup appends at :290, the loop appends again at
:486/:500) -- harmless under `nm | sort -u`, but the list should be built once.

---

Closed: the audit asks UpToDate -- an existing but stale object (source or any prerequisite
newer) is rebuilt through the same straggler path as a missing one, so `--audit-link` after
a source edit measures the present; the straggler BuildGroup no longer doubles OBJECTS (the
group build's own list is discarded, the exact per-unit object is appended once); the block
comment says what the audit does since 1658 instead of what it did before. Gate 131/131.
