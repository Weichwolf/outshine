Type: bug
Area: core
Tags: instrument

**Four source comments cite the deleted architecture document, deleted before this migration, and nothing was catching it**

`src/core/Material.h:56` and three others cited a document removed weeks before the board migration.
The citation test existed and was green throughout, because **its domain was documents** — it read the old doc tree, `CLAUDE.md` and later `board/`, and **never the comments in `src/` and `test/`**.

**This is the week's own class again: an instrument reporting success by not looking.** The number was
right — every path cited *in a document* resolved — and it was about a smaller population than the
sentence *every path this repository cites resolves* implies.

**Right:** the checker's domain includes source comments, so a comment citing a path is held to the same
rule as a document citing one. **Fixed when** a deleted file named in a `.h` or `.cpp` comment turns the
run red.
