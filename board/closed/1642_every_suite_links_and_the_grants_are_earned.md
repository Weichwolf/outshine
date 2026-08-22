Type: task
Parent: 1641
Area: tests

# Every declared suite links its sources, and the include grants are earned

The world suite gets Wayfinding.cpp back in its sources; the drive suite loses the duplicate
listing (the directory already carries it); dead include grants go -- and actor/path's own
grant shrinks to what its headers actually spell, so a tomorrow-import of ground refuses at
compile (1525's proof restored). Proven by linking the two named suites once and by the fast
gate.


---

Closed: the world suite links (Wayfinding.cpp restored to its sources) and passes; the drive
suite's duplicate listing is gone, its dead -Isrc/ground grant removed, and actor/path's grant
shrank to -Isrc/actor/path alone -- a tomorrow-import of ground refuses at compile, restoring
1525's proof. Both named suites were linked and run once: world 1/1, drive 2/3 with the one
failure adjudicated as a pre-existing superseded-door member (filed 1645). The Wshadow the
relink exposed in ACarDrives (the Lie-lift's aheadM against the loop's) is fixed and the
member passes. Fast gate 123/123.