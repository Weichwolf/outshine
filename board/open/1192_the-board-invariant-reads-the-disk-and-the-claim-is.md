Type: bug
Area: harness
Tags: instrument, scope

**The board invariant reads the disk, and the claim people read it as is about the repository**

`board:1188` was committed under **both** `board/open/` and `board/active/` — a `git mv` whose deletion was
staged and not committed — and **the harness reported green throughout, because the working tree was
correct.** The invariant *an id in any edge resolves to exactly one file* is checked over the filesystem;
**the duplicate existed only in `HEAD`.**

**Its population is the disk. The claim it is read as making is about the repository.** Those are the same
set only while nothing is half-landed, which is exactly the condition it exists to detect.

**THIS IS THE THIRD HALF-LANDED `git mv` and that makes it a mechanism rather than three slips.**
`CLAUDE.md` already carries the neighbouring hazard in the other direction — *a new file must be staged
before `git grep` can see it*, so a work item proven only by an unstaged test reads as unproven. **Both
failures are one cause: the board's state lives in paths, and a path change is two operations in git.**

## The repair is not to swap one population for the other

**Reading `HEAD` alone inverts the blindness**: a newly written item, correct on disk and not yet
committed, would read as absent — so an invariant that only ever saw `HEAD` would be blind to the state a
round is actually in.

> **Both populations are read, and a DISAGREEMENT between them is itself the finding** — because a
> disagreement between the working tree and `HEAD` **is** a half-landed move, named directly instead of
> inferred from a duplicate.

- [ ] **The five invariants run over `git ls-tree HEAD` as well as over the filesystem**, and the two
  results are published side by side. **Neither is the authority; their agreement is**
- [ ] **The disagreement is reported by id and by path**, so *`1188` is at `board/open/` on disk and at
  `board/open/` and `board/active/` in `HEAD`* is one line rather than a puzzle
- [ ] **It generalises past the id invariant.** Every board check that walks the filesystem has this
  blindness — the ready query, the parent rules, the `closed` cited-by-nothing check. **The population is
  a property of the checker, not of the invariant**, so it is fixed once
- [ ] **The `git grep` hazard is the same defect and is already documented rather than checked.**
  Confirmed live this round: a recursive grep for `board:1188` **silently omitted `Parity.cpp`'s 3 hits**
  because `test/render/.gitignore` opens with `*`. `CLAUDE.md` warns of it in prose; **nothing enforces
  it**, and the enforcement is the same one line — read the tracked population

**Why it is worth an item rather than a habit.** A convention that has to be remembered at the moment of
committing is the one this repository has now failed three times, in three disguises, by three different
hands. **The board's state is its path, and the checker must therefore ask git what the paths are.**

**Done when** the invariants read both the working tree and the committed tree, a disagreement between
them is a named failure rather than a silent pass, and a half-landed `git mv` cannot be green.
