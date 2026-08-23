Type: bug
Area: clients
Tags: engine, persistence, boundary

**A save survives the locale, the disk, and a refused restore**

1492's Save/Restore (`src/clients/Engine.cpp:229-330`) is the right shape — declared rows
only, sorted, versioned header, one arrival route — but four seams leak:

- **The writer speaks the locale the reader refuses.** :255 `snprintf("%s %.17g", …)` renders
  the decimal point from LC_NUMERIC; :306 `from_chars` reads only `.`. The engine is a
  library — one `setlocale(LC_ALL, "")` in a host (SDL apps do this) and every save writes
  `0,5`, which every Restore then refuses. The tree spent 1621/1694 evicting exactly this
  class; the newest boundary reintroduced it. `std::to_chars` writes the shortest
  round-tripping form and no locale.
- **A truncated save reports success.** :265-266 ignore the `fwrite` and `fclose` results —
  disk-full writes a half savefile and returns true. The refusal culture demands the failed
  write be loud (check both, or write-to-temp-then-rename).
- **A refused Restore leaves the scene half-restored.** :296-327 apply each row via
  `Kinds.Put` as it parses; a bad row mid-file returns false with every earlier row already
  committed. Validate the whole file, then apply — or the refusal text is a lie about the
  state it leaves.
- **The proof proves a no-op.** `test/render/outshine/client/ASaveIsAFunctionOfTheDeclaration.cpp:64-66`
  saves the DECLARED DEFAULT (fillL 0.5) and checks Restore puts back… 0.5, which assembly
  already set — `Restore(){return true;}` passes every check in the file. Nothing mutates the
  trait before Save, so the `%.17g` round-trip is never exercised on a value that needs its
  17 digits (0.1+0.2), and the partial-apply and short-write behaviours are untested.

Also :255 truncates silently into `char line[192]` when `row.What` is long — the row saves
mutilated and Restore blames "the declaration moved on". Refuse over-long rows at Save.

---

Closed: the save WRITES locale-free (std::to_chars, the mirror of the from_chars read) --
a host's setlocale can no longer corrupt a save; fwrite and fclose are checked ("a full disk
is a refusal, never a successful save"); Restore validates EVERY line into a staging list
before ONE value moves, proven by a save whose last line is broken leaving the first
untouched; and the proof discriminates -- the save is tampered to 0.75 before restoring, so
a Restore that does nothing goes red.
