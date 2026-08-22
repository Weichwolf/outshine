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
