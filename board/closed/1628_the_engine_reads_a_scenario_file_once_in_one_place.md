Type: bug
Area: clients

**The engine reads a scenario file once, in one place**

src/clients/Engine.cpp:124-147 (Read) and 149-168 (Load) carry the identical
fopen/fread/ReadScenario block verbatim -- twenty lines twice, introduced by e110343 while
converting both to string_view. They differ only in the tail: Read keeps the declaration,
Load stands it up via Declare. A future fix to the slurp (an fread error path, a size cap,
an mmap) lands in one and silently misses the other.

Demanded: one private helper (path -> Scenario or refusal) both tails consume; the twin that
proves Read and Load agree on the same malformed file catches the drift.

---

Closed: the helper already stood at HEAD -- Engine::ReadInto (private, path -> Scenario or
refusal) is the one slurp+parse both verbs consume, Read keeping the declaration and Load
standing it up. What was missing was the drift-catcher, and it now exists: proving test
test/render/outshine/client/AReadAndALoadRefuseTheSameMalformedFile.cpp -- the same malformed
file through both doors must refuse with IDENTICAL text, and a missing file refuses naming
its path. 4/4 in the client suite.
