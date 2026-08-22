Type: bug
Area: test
Tags: build, layering

**Every declared suite links its sources — and the gate would notice when one cannot**

The c0109aff path move edited the run.sh sources lists mechanically and broke the LINK of two
named-only suites. The fast gate compiles every source (1602's repayment) but never links the
named-only suites, so 123/123 was structurally blind to both:

- **render/outshine/world cannot resolve `Path::Network::Lay`.** Its sources (test/run.sh:167)
  list `src/ground` — which contains RoadHarvest.cpp — but not `src/actor/path/Wayfinding.cpp`.
  Before the move, `src/ground` swept Wayfinding.cpp up automatically; now RoadHarvest.o
  carries the undefined symbol `__ZN8outshine4Path7Network3LayEPKdmddi` (proven with
  `nm -u` on a fresh compile under the group's own include set) and nothing in the link
  provides it. The include grant `-Isrc/actor/path` (run.sh:89) was updated; the sources
  list was not.
- **render/outshine/drive links Wayfinding twice.** Its sources (test/run.sh:168) list both
  the directory `src/actor/path` — whose `find -maxdepth 1` expansion contains
  Wayfinding.cpp — and the explicit `src/actor/path/Wayfinding.cpp`, a leftover from when the
  file lived in src/ground and needed naming. BuildGroup (run.sh:263-281) computes a
  different setId per group, so TWO objects defining every `outshine::Path` symbol reach the
  one link line (run.sh:772) and ld refuses on duplicate symbols.
- Dead grants ride along: `-Isrc/ground` for render/outshine/drive (run.sh:90) serves no
  include in test/render/outshine/drive/ — both cases include only actor/path, body and mind
  headers.

Demanded: drop the doubled `src/actor/path/Wayfinding.cpp` from the drive list; give the world
list the Wayfinding unit (or the actor/path directory) its ground objects reference; delete the
dead `-Isrc/ground` grant on drive. And the systemic half: the gate that already compiles every
source gains a cheap link truth for every DECLARED suite — at minimum a claim that no sources
list names a unit its own directory entries already expand to, and that each suite's object set
is closed over its undefined symbols. A declared suite that cannot build is a lie the trailer
never gets to print.

---

Sharpened (review 2026-08-22 late): the audit stands (run.sh:406-434) and the two shipped
shapes are dead, but the claims test overstates itself. EverySuiteListsEachSourceOnce...cpp:24
asserts the detectors are "negative-controlled against a seeded duplicate and a seeded orphan"
— no such control exists in the test or in run.sh; the control was manual, once. As written
the test proves only that the CURRENT tree is clean per the detector, not that the detector
detects: a broken `uniq -d` or `grep -qx` still prints "AUDIT clean" and the gate stays green.
Demanded before this closes: the test seeds a duplicate and an orphan (env-injected extra list
entry, or a copied run.sh with one line patched) and asserts the audit verdict flips for each.
The link-closure half (object set closed over undefined symbols) also remains open.

---

Progress (board queue, same day): the negative controls are no longer manual. The claims test
seeds both defect shapes on EVERY run -- a copy of run.sh in the temp dir with ROOT pinned and
one declaration line patched (Store.cpp listed beside src/scene; Sim.cpp struck from world's
closure, the only one that compiles it) -- and asserts the audit verdict flips and names the
defect for each. A broken `uniq -d` or `grep -qx` now fails the gate instead of printing
clean. The seeds guard themselves: if the patched line drifts, the seed-took CHECK fails
loudly. Remaining before close: the link-closure half (each declared suite's object set closed
over its undefined symbols).

---

Verified + one cost note (review round 2, 2026-08-22): the negative controls are the demanded
shape — seeded copies with ROOT pinned, self-guarding seed-took CHECKs, both verdicts flip
with the defect named; PASS in an isolated worktree run. Cost: the claims test now runs the
audit three times and measures 6873 ms [MEASURED, one cold worktree run] — the heaviest single
gate member. If the audit reports ALL defects before its verdict, one copy seeded with both
defects proves both detectors in two audit runs instead of three; worth taking when the warm
gate nears its 90 s bound. The link-closure half stays the open remainder.

---

Closed: the systemic half stands. `run.sh --audit-link` builds every declared suite's object
set under its own declaration (warm: stat-walks the cache) and refuses any outshine symbol
the set cannot resolve from itself, via nm closure; an optional suite argument narrows the
walk. Proving test: test/harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols.cpp -- the
real tree must audit closed, AND a copy with Wayfinding struck from world's declaration must
refuse naming __ZN8outshine4Path7Network3Lay, the very symbol whose silent absence filed this
item. The cost note is also repaid: the listing controls run as ONE dually-seeded copy
(6.9 s -> 4.7 s [MEASURED warm]), the closure control narrows to the seeded suite; the
closure's real run costs 13.5 s warm [MEASURED] -- the price of a link truth the gate never
had, inside a 68.4 s warm gate against the 90 s bound.
