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
