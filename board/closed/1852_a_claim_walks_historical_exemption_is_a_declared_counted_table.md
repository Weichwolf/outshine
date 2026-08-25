Type: bug
Parent: 1802
Area: test
Tags: claims, exemption, drift

# A claim walk's historical exemption is a declared, counted table and not a hash in an `if`

`board:1846`'s repair restored the `board:NNNN` predicate -- the right call -- and paid for it
with a commit hash compiled into the predicate:

```cpp
test/harness/claims/ACommitCarriesTheItemItNames.cpp:96
    const bool historical = commit.rfind("3f52567e", 0) == 0;
test/harness/claims/ACommitCarriesTheItemItNames.cpp:98
      bool spoken = historical;
```

The DECISION is right: a commit already in history cannot be restaged, and widening the rule
around it -- which is what `board:1846` was filed against -- would be worse. The SHAPE is wrong
in three ways.

**1. It is unmeasured.** The walk publishes one number, `Note("commits this rule has bound so
far", ...)` (`:106`), and nothing about the exemption. A reader of the log cannot learn that a
commit in the walked range was excused, or which, or how many. CLAUDE.md's rule is that a number
carries information where a number would; the count of exemptions in a guard is exactly such a
number, and it is zero-cost to publish.

**2. It grows by `||`.** Nothing states the count, nothing forbids a second, and the cheapest
repair the next time history bites -- a rebase, a range that moves, an older message found
wanting -- is one more disjunct. `board:1801` is this tree's own precedent that an exemption
belongs in the RULE the walk enforces; this one lives in a comment beside an `if`.

**3. It dies silently.** `born` is derived from `git log --diff-filter=A` on the claim's own path
(`:56-63`), so a rename or a history rewrite resets the range. If `3f52567e` ever falls outside
it, the branch matches nothing and no run says so -- a dead exemption that reads as a live one.

And the exemption is TOTAL where the violation is not: `spoken = historical` excuses every board
item that commit touches, rather than the specific three (`1610`, `1826`, `1831`) whose bare
spelling is the defect.

## What will be true

- [x] The exempted commits are a `constexpr` table beside the walk, their count published in a
      `Note`, and the claim's sentence says "except N commits, named here" -- so the exemption is
      part of the rule a reader is told, not a fact hidden in the predicate.
- [x] A table entry whose commit is not inside the walked range turns the claim RED: a dead
      exemption is a rot the walk finds itself.
- [x] Strictly better, and preferred: the walked range starts after the last historical
      violation, derived and stated in the `Note`, so no exemption exists at all.
- [x] Proving test: the walk itself, with a second table entry naming a commit that does not
      exist -> red, naming it. Negative control: HEAD -> green while the branch is dead, which is
      the state this item forbids.

## Comments

- 2026-08-25 -- filed by the hourly review. The question it was asked to answer was "is this an
  exception that will grow?" -- and the answer is that nothing in the tree would tell anyone if
  it did.

## Closed 2026-08-25 -- the hash left the `if`

```cpp
struct Excusal { const char *Commit; const char *Items; const char *Why; };
constexpr Excusal kExcused[] = {
    {"3f52567e", "1610 1826 1831", "the hourly review wrote its sharpened items as a bare list "
     "beside a verb, before its instructions carried the rule that a reference is board:NNNN"},
};
```

Each of the three defects the item named is answered by a property of the table, not by a
promise:

| the defect | what answers it |
|---|---|
| unmeasured | `NOTE exemptions the table declares = 1 rows`, `NOTE times one was used this run = 3 items`, and one `EXCUSED` line per row printed with its reason |
| grows by `\|\|` | a row carries the ITEMS it excuses -- `3f52567e` no longer excuses everything it touched, only `1610 1826 1831`; a fourth item in that commit fails |
| dies silently | `CHECK(excused > 0 \|\| commits < 30)` -- a declared exemption the walked range no longer reaches turns the claim RED |

The claim's own sentence carries it too: *"except the commits this file's `kExcused` table declares,
each named beside the items it excuses, counted in the notes and refused the day the walked range
no longer reaches one"*.

Proving test: `test/harness/claims/ACommitCarriesTheItemItNames` (IV.23).
Negative controls, both run:

| control | result |
|---|---|
| the row's commit changed to a hash the range does not hold (`deadbeef`) -- the dead row | `FAIL ...:142 AND EVERY DECLARED EXEMPTION IS STILL REACHED` + `:149` |
| the row's items changed to `9999` -- the exemption no longer covers what it was written for | the same two, `:142` and `:149` |

The second control is the one that shows the exemption narrowed: under the old total `if`, any
item of `3f52567e` was excused and no change to a list could have made the walk red, because
there was no list.
