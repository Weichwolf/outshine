Type: bug
Parent: 1856
Regresses: 1854
Area: test
Tags: claims, regression, silence

# A walk anchored to its own birth says whether it found it

`board:1854` dropped `Ask`'s tail trim, on a control that ran and showed nothing changing. The
control was blind, and this is what it missed:

```cpp
test/harness/claims/AnItemReachesClosedThroughActive.cpp:33
  const std::string birth = Ask("git log --diff-filter=A --format=%H -- " + self + " | tail -1");
test/harness/claims/AnItemReachesClosedThroughActive.cpp:34
  const bool born = birth.size() == 40;
```

git prints a hash and a newline. With the trim, `birth.size()` is 40 and the walk runs. Without
it, 41 -- and the case reports

```
NOTE the rule binds from (uncommitted -- nothing yet in window)
NOTE closures inside the window = 0 items
```

and **PASSES**, because a walk over nothing collides with nothing. IV.16 has judged no closure
since board:1854 landed, and the trailer said PASS every time.

The blindness of the control is the sharper half. It ran the suite, saw 34 PASS, and concluded
the trim was unobserved. A guard that stops guarding does not go red -- it goes green faster.
A control over a suite can only see cases that FAIL when the thing they guard is removed.

## What will be true

- [x] `Ask` answers tail-trimmed text again, and the reason is stated where the contract is: a
      command's answer is what it said, not what it said plus the separator that ended it.
- [x] Every walk anchored to its own birth commit CHECKS that it found that anchor -- derived
      from `git ls-files`, so a versioned proof that reports an empty window is RED, not green.
- [x] Proving test: `harness/claims/AnItemReachesClosedThroughActive` and
      `harness/claims/ARepairFindsItsItemInActive`, both of which currently report an empty
      window over a versioned file. Negative control: the trim dropped again -> both go red
      instead of both going quiet.

## Comments

- 2026-08-25 -- filed by the queue against its own closure, found while writing board:1856's
  negative control: the control could not make the new walk red, and the reason was that the
  walk was not walking.
