Type: task
State: open
Area: build
Tags: tooling, hygiene, lint, doc

# The tools are ESTABLISHED ones, and `make` is the only door

**Benchmark** — Unreal: `UnrealBuildTool` plus a house style enforced by review, its own header parser for reflection, and no external linter in the loop. RAGE: the same shape. **Taking NEITHER**, and the reason is that this is not a question they answer: both chose in the 2000s, before clang-tidy and a compilation database existed, and both employ a team to hold a house style. This tree has one author, C++23, and a build that already DERIVES every include set -- so the established tool is cheaper here than the hand-written one, which is the opposite of their situation.

## What is being replaced and by what

    hand-written                          established
    test/harness/claims (40 cases)        clang-tidy where it applies; `make lint` for the rest
    prose in src/ (1606 lines)            Doxygen over the DOOR; commits and items for the reasons
    reaching past make into run.sh        make, and nothing else

**THE CLAIMS ARE 15 PER CENT REPLACEABLE AND 100 PER CENT MISPLACED.** Counted: about 12 check the
BOARD (an id is issued once, an item names its benchmark, every edge points at an item), about 10
check the HARNESS (no prepared file lands in the tree, a green trailer names what it did not
judge), about 14 check invariants only this tree knows (every type name declared once, every
framing constant agrees with the engine), and about 4 are generic lints clang-tidy already ships.
No off-the-shelf tool knows what `board/NNNN_*.md` is, so most of them stay -- what stops is their
pretending to be PROOFS about the engine. They are a linter over the repository and they belong in
`make lint` beside clang-tidy, printing the same way.

## What stands

- [x] `make db` -> `compile_commands.json`, derived from the tier graph rather than kept beside it
- [x] `make` is the only entrance: db, lint, doc, shots, test, suite, clean, spotless
- [x] `build/outshine-client` -- the engine through its own door, `src/client/reaches` naming `base`
      alone so it is held to the same door a stranger gets

## What is left, in order

- [ ] **`.clang-format` and `.clang-tidy`, strict, with a baseline that may only SHRINK.**
      `WarningsAsErrors: '*'` over 57 113 lines yields thousands of findings on day one and gets
      switched off within a week; the version that survives is hard errors on touched files and a
      checked-in count for the rest that a commit may lower and never raise.
- [ ] **Doxygen over `include/` AND `src/client`**, `WARN_AS_ERROR`, every public entity
      documented. The client is the library's official command line and belongs on the same page as
      the door it drives.
- [ ] **The claims move out of `test/` into `make lint`**; the four clang-tidy covers are deleted.
- [ ] **`src/`, `include/` and `apps/` carry no comments** -- the rule CLAUDE.md already states and
      1606 lines break. The migration is the work: every surviving block moves into the item or the
      commit that owns it, and a block that names no decision is deleted rather than moved.
- [ ] **The place cases go THROUGH `build/outshine-client`**, not around it. Six binaries that
      duplicate the client's own path are six chances to disagree with the thing a person runs.
- [ ] **THE BLENDER ORACLE'S OWN DIGEST IS PINNED IN THE TREE.** `prepare.py` already digests the
      INPUT assets -- "the scenario pins the digest, so a wrong file is refused by name" -- and does
      not digest what Blender PRODUCES. So a case compares our render against whatever oracle
      happens to be in the nest: a version bump, a re-render with different sampling or another
      machine's output all grade silently. The manifest records the oracle's sha256 beside the
      Blender version it already records, the prepare step refuses a mismatch by NAME, and a
      deliberate re-render is a commit that changes the digest.

## What this does NOT cover

`test/run.sh` is 2357 lines and 60 functions -- the build, the tests, the layer audit, the corpus
preparation, the prune and STATE.md in one file. That is the actual monstrum (the Makefile it was
blamed on is 36 lines) and taking it apart is its own item. `make` being the only entrance is what
makes that possible without a caller noticing, which is why it came first.
