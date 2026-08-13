Type: bug
Area: generators
Tags: instrument

**`board/active/` describes a tree that three commits ago stopped existing — **Band 2****

`board/active/` is *the current work item* and at `81d4db1` it is a record of finished work presented as
future work. Measured against the tree in this round:

| the old todo says | the tree at `81d4db1` |
|---|---|
| *"Then: the renderer stage, which leads from here"* | done at `0161f88` — `src/render/` is 21 files on SDL_GPU |
| *"`vendor/` **stays until the SDL_GPU port** — it holds Dawn"* | `vendor/` does not exist |
| *"The tree ends with three directories … `tiles/` 58, `tools/` 4, `mods/` 4, `assets/` 34"* | none of the four exists; the tree is `doc/ src/ test/ build/` |
| *"**450 lines, 16 targets** — `help walk walk-asan world treebench` and eight `verify-*`"* | `Makefile` is **114 lines, 3 targets** — `all`, `test`, `clean` |
| *"**Emscripten gone** — 20 conditionals and 6 includes"* | `grep -rl emscripten src/ test/` returns nothing |
| *"No Python in the engine or the tests … `verify_clients.py` remains"* | no Python outside `test/corpus/`, which is the offline preparation `CLAUDE.md` permits |
| *"**Blocked:** the harness — four demonstrated defects"* | the harness runs: 106 tests, 88 PASS / 18 FAIL, one verdict per test |

**Why it is a defect and not merely out of date.** `CLAUDE.md` makes `board/active/` the answer to *what
next*, and a round that opens it is told to delete a container, a Python proxy and an Emscripten target
that are already gone — so the first act of the next round is to discover that its instructions are
false. That is the same cost as a miscited rule number, paid at the start of every round instead of once.

**Right:** the file states the work that follows `81d4db1`. **Fixed when** every claim in it resolves
against the tree — which, for the seven rows above, is seven one-line checks.
