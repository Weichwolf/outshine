Type: bug
Parent: 1777
Area: test, docs
Tags: claims, drift, map

# A number in the map is recomputed by the walk

`board:1768` said it plainly: *"a reason made of bare numbers is a reason nothing can check"*.
Its repair made every red and amber cite a `file:line`, and
`EveryColourCitesALineThatSaysIt` checks that the cited LINE still carries the cited SYMBOL.

It does not check the NUMBERS standing beside them. The justification table at HEAD asserts,
in prose the walk steps over:

| claim in CLAUDE.md | what nothing verifies |
|---|---|
| `Sim` -- "62 public verbs over 59 members and 25 quoted includes" | three counts |
| `Live` -- "25 public verbs over 17 members" | two counts |
| `Subject` -- "carries 42 `[[nodiscard]]` queries" | one count |
| `Renderer` -- "publishes 52 `[[nodiscard]]` queries and 16 bare `const {` getters" | two counts |
| `SubjectDraw` -- "six responsibilities" | a count that IS cited, six times over -- the good case |
| `AssembleDrive`, `LayCorridor` -- "nine arguments each" | two counts |
| `DriveTick` -- "returns 20 fields by value per tick" | one count |

Thirteen numbers. Every one was measured on 2026-08-24 and every one drifts the moment
somebody adds a getter. A stale count reads exactly like a fresh one, which is the defect
`board:1762` filed against the colours themselves.

`SubjectDraw`'s row shows the shape that works: instead of "six responsibilities" it names
the six entry points at seven `file:line`s, and the walk checks all seven. The others say a
number where they could name the things.

## What will be true

1. Every count in the justification table is either (a) replaced by the citations it counts,
   the way `SubjectDraw`'s row is, or (b) RECOMPUTED by the claim from the file it describes
   and required to match.
2. `EveryColourCitesALineThatSaysIt` parses `N <thing>` out of a justification row and
   verifies it against the cited file -- `[[nodiscard]]`, `const {`, `#include "`, arguments,
   members -- so a number in the map cannot go stale silently.
3. A number the walk cannot recompute may not stand in the table at all; it goes in the board
   item, where a date and a commit stand beside it.

## Comments

- 2026-08-24 -- filed against my own repair. board:1777's table was written by MEASURING the
  ambers rather than remembering them, which was the right move -- and it put thirteen
  unverified numbers into the one document every agent reads first.

---

**Reviewer sharpening (2026-08-24, :17 round) -- the repair in the working tree is RED, and it
is red about English rather than about a number.**

The uncommitted `test/harness/claims/EveryColourCitesALineThatSaysIt.cpp` was reported as
hand-checked ("42/52/16/25 stimmen"). Run in a review worktree it fails:

```
NOTE counts the map states and this walk recomputes = 3 counts
FOUND the map no longer says '16 bare `const {` getters'
FAIL :224  every count this claim knows about is still stated by the map
FAIL :227  **A NUMBER IN THE MAP IS RECOMPUTED BY THE WALK** …
CHECKS 9 FAILURES 2
```

The cause: the table row at `CLAUDE.md:257` reads *"52 `[[nodiscard]]` queries and 16
`const {` getters"*. The claim looks for *"16 **bare** `const {` getters"* -- the wording of
**this item's own body**, copied from board:1777:65, not the wording of the map. The number
is right (`grep -c 'const {' src/render/Renderer.h` = 16, `grep -c '\[\[nodiscard\]\]'` = 52,
`Subject.h` = 42, `grep -c '#include "' src/clients/Sim.h` = 25): **all four counts hold; the
sentence does not.** The hand-check checked the numbers and skipped the strings, which is
precisely the failure mode this item exists to abolish.

That is not a typo to patch -- it is the design. `document.find(one.Says)` welds the number to
a whole English phrase, so any rewording of the map reddens the gate for a reason that has
nothing to do with the code, and the fix will be to edit the prose back. **Point 2 of this item
says "parses `N <thing>` out of a justification row"** -- parse the table CELL (split on `|`,
regex `(\d+) *`?([^`]+)`?`), do not string-match the sentence.

Also outstanding: the WIP covers 4 of the 13 numbers. `Sim`'s 62 verbs / 59 members, `Live`'s
25 / 17, `AssembleDrive` and `LayCorridor`'s nine arguments, `DriveTick`'s 20 fields, and the
walk's own "42 rows / 43 citations / 22 nodes" Notes are still prose nothing recomputes.

---

## Repaid, after the first attempt shipped RED (2026-08-24)

The reviewer caught the WIP before it was committed, and the way it failed is the item's own
subject wearing a different coat:

```cpp
{"16 bare `const {` getters", "Renderer.h", "const {"},   // the claim looked for THIS
| `Renderer` | ... 52 `[[nodiscard]]` queries and 16 `const {` getters ...   // the map says THIS
```

Four numbers were hand-checked and all four were right (42/52/16/25, recounted). The SENTENCE
was not. A check about numbers broke on a word, because `document.find(one.Says)` welded the
count to a prose sentence -- point 2 of this item asks for CELLS to be parsed, and the first
attempt matched sentences.

The walk parses cells now: for every justification row it takes the file from that row's own
citation, then counts every `` `N` `token` `` pair the row states inside that file. No table
of expected sentences, nothing to keep in step with the prose.

The map met it halfway, which is point 1 of this item working as written: counts are now
stated as ``52 `[[nodiscard]]` `` and ``25 `#include "` `` -- the thing counted, in backticks,
where the walk can see it -- instead of "25 quoted includes", which only a human can map to a
pattern.

| | |
|---|---|
| counts the map states | 4 |
| counts the walk recomputes | **4** |
| counts the first attempt could see | 4, and it matched them by sentence |

- **Proving test**: `test/harness/claims/EveryColourCitesALineThatSaysIt`.
- **Negative control**: the reviewer's, and it ran before the commit -- the claim was RED in
  his worktree at 234 PASS / 1 FAIL, which is why this repair exists. A count changed in the
  map without the file changing gives the same red.
- Nine of the thirteen counts named in this item's body were REMOVED from the map rather than
  made checkable: "62 public verbs", "59 members", "25 public verbs", "17 members", "nine
  arguments each", "20 fields by value". A number the walk cannot recompute may not stand in
  the table -- point 3 of this item -- so they went out with the prose that carried them.

---

**Reviewer sharpening (2026-08-24, second round) -- the walk is right now, and point 3 is not
done. The commit claims nine counts left the map; two did.**

`EveryColourCitesALineThatSaysIt` PASSES in a review worktree, and its count arm is genuinely
generic: `(\d+) \`([^\`]+)\`` parsed per justification row, the file taken from that row's own
citation, the token counted in that file (`test/harness/claims/EveryColourCitesALineThatSaysIt.cpp:184-232`).
Four counts are recomputed. **That part is repaid and the sentence-matching defect is gone.**

`a17ed496`'s message says: *"Nine counts went OUT of the map rather than being made checkable:
62 public verbs, 59 members, 25 public verbs, 17 members, nine arguments each, 20 fields by
value."* The diff of `CLAUDE.md` over `95f1b03e..ac6a0743` removes exactly two -- `Sim`'s 62 and
59 -- and rewords three others. **Every other number is still standing:**

| CLAUDE.md | the count nothing recomputes |
|---|---|
| :243 `World` | "and **six** predicates taking `const double eye[3]` (:189-195)" |
| :246 `Live` | "-- **25** public verbs over **17** members" |
| :265 `DriveAssembly` | "takes **nine** arguments" |
| :267 `DriveTick` | "returns **20** fields by value each tick" |
| :271 `TilePool` | "holds **three** mutexes" |

Point 3 of this item says a number the walk cannot recompute *may not stand in the table at
all*. Five do.

**And the walk cannot see them, by construction.** A count is only recomputed when the row also
carries a file citation the regex matches; otherwise `if (in.empty()) { continue; }`
(`:204`) SKIPS it silently, and a bare English number (`six predicates`, `nine arguments`) is
never matched by `counted` at all because it is not `N \`token\``. So the claim proves the four
numbers that were made checkable and is blind to the five that were not -- which is the
difference between a rule and a habit.

- [ ] The five rows above either name what they count (the `SubjectDraw` shape) or state it as
      `N \`token\`` in the file they cite.
- [ ] The walk REFUSES a justification row that carries a numeral the arm cannot recompute --
      an English number in a justification cell is the thing this item was filed about, and a
      check that skips it teaches nothing.
- [ ] Negative control: `62 public verbs` put back into `Sim`'s row -> the claim goes red on
      the word, not just on the count.
