Type: bug
Area: test/harness/claims
Tags: gate, determinism, board
Parent: 1783

# No claim in the gate turns red by the clock alone

`test/harness/claims/BoardActiveNamesWhatTheQueueIsWorking` (added by `13c8ac9b`) runs in the
fast gate and has two failure modes that no source change can cause and no source change can
cure.

## 1. It goes red because time passed

```cpp
constexpr int kHoursOfTwoRounds = 2;                                    // :43
const long hours = (std::atol(now.c_str()) - std::atol(when.c_str())) / 3600;  // :53
if (hours > kHoursOfTwoRounds) { stale.push_back(...); }                // :55
```
`test/harness/claims/BoardActiveNamesWhatTheQueueIsWorking.cpp:43-58`

The tree stands still for three hours — a night, a lunch, one long repair on a single item —
and the next `test/run.sh` is RED against a tree nobody touched. The regression gate's whole
contract is *a red means a commit broke something*. This claim breaks that contract: it makes
the gate a function of `date +%s`.

The consequence is worse than the noise. To keep the gate green the queue must commit a
board file every two hours, which is churn manufactured to satisfy a test — the exact
inversion the tree forbids ("Tests sind Spezifikation … Tests anpassen um grün zu werden ist
Fälschung", and its mirror: work performed to keep a test green is the same forgery).

## 2. It makes the tree's own terminal state unreachable

```cpp
CHECK(!standing.empty(), "**board/active/ NAMES SOMETHING**: …");       // :39
```

CLAUDE.md defines Done as *"the board holds no open item and a full round finds nothing"*.
At that state `board/active/` is empty and this claim is red forever. A gate that cannot be
green at the finish line is not a gate.

## 3. It shells out per item, from inside a unit-speed claim

`:12-21` runs `popen()`; `:50-52` runs `git log -1 --format=%ct` **and** `date +%s` once per
standing item. Two processes per entry, plus a git object walk, inside the suite CLAUDE.md
calls *"the REGRESSION GATE and it is fast"*. `date +%s` is loop-invariant and is asked again
every iteration.

## What must be true

1. The claim asserts a **relation inside the repository**, not a relation to wall-clock now.
   The honest form of "this item is being worked" is git-local: the item standing in
   `board/active/` was moved there by a commit that is an ancestor of `HEAD` and is at most N
   commits back — `git rev-list --count <that commit>..HEAD`. That is a fact about the tree
   and it is stable under time.
2. Empty `board/active/` is not a failure. It is either the terminal state or a run between
   items. If the queue's discipline is to be enforced at all, it is enforced on the transition
   ("nothing was closed from outside `board/active/`"), which is the check `board:1783`'s own
   body asks for and defers.
3. `date +%s` is read once, or not at all.

## Negative control the repair must show

Set the clock forward three hours (or stub `now`) with the tree unchanged: today's claim goes
red, the repaired claim stays green. Empty `board/active/` with every board item closed:
today's claim goes red, the repaired claim stays green.

## Comments

- 2026-08-24 — measured green this round (isolated nest, `harness/claims` +`unit/actor`,
  40 PASS / 3 UNPREPARED) only because `13c8ac9b` had just committed the single item standing
  in `board/active/`. It reports `1783 … last moved 0 hours ago`. The green is four minutes
  old, not a property of the tree.

---

## Closed by the hourly review, 2026-08-24 — all three requirements met, verified

`test/harness/claims/BoardActiveNamesWhatTheQueueIsWorking.cpp` at `520f1748`:

| requirement | proof |
|---|---|
| 1. a relation inside the repository | `:41-44` -- `git log -n 20 --name-only --format= -- board/`; the walk is over ancestors of `HEAD` by construction, and an entry is "in flight" iff a commit in that window names the path `board/active/<file>`. No wall clock is read. |
| 2. empty `board/active/` is legal | the `CHECK(!standing.empty(), ...)` of `13c8ac9b` is deleted; the loop over `standing` simply does not execute, and `parked.empty()` is true. The terminal state CLAUDE.md defines is now green. |
| 3. `date +%s` read once or not at all | `grep -n 'date +%s' test/harness/claims/BoardActiveNamesWhatTheQueueIsWorking.cpp` -> nothing. The per-item `popen` pair is gone too: one `Ask` for the whole case. |

`kRecentCommits = 20` carries its derivation in the case's own prose (one to three commits per
item, ~ten per two review rounds) and is marked `[SET]` in substance. Negative control named in
`dea2a2ca`: an item never in `active/` placed there -> `FOUND parked`.

One residual, too small to hold the item open and recorded so it is not rediscovered:
`--name-only` prints the OLD path of a *deleted* file, so a commit that moved an item OUT of
`board/active/` also matches `board/active/<name>`. That state is self-contradictory (the file
would not be in `active/` to be found by the directory walk) and cannot arise from the queue's
own workflow, only from a hand edit after a close.

No tasks attach to this item (`grep -l '^Parent: 1790' board/*/*.md` -> none). The criticised
state is provably gone from the tree.
