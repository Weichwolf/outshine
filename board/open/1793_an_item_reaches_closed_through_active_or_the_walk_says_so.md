Type: bug
Area: board
Tags: process
Regresses: 1783

# An item reaches board/closed through board/active, and the walk says so

`board:1783` repaid the *"`board/active/` mirrors what is being worked on right now"* rule with
a claim that asks one thing: **is every item standing in `board/active/` fresh?** That claim
has been green since it was born. It is green because `board/active/` has held exactly one
entry -- 1783 itself -- and that entry was fresh.

What it cannot see is the case the rule exists for: an item worked and closed **without ever
standing in `board/active/`**. 1783's own body names the gap and leaves it:

> *"The stronger check the body asks for -- no item CLOSED whose parent commit had it outside
> `board/active/` -- is left for a round that can spend the git walk."*

The walk, spent, over every closure since 1783's proof was born:

| commit | item closed | stood in `board/active/` at the parent |
|---|---|---|
| `1c8619e5` | 1787 a note table proves it names every note | **no** |
| `1c8619e5` | 1790 no claim in the gate turns red by the clock | **no** |
| `84198793` | 1792 a straight synthetic route lays without refusing | **no** |

Three closures under the rule, three violations, claim green throughout. The last of them is
the hour's own work, closed one commit after the queue congratulated itself for obeying its own
rule. A gate whose only witness is the drawer it is supposed to police measures nothing: an
empty `board/active/` is legal (`board:1790`, and rightly -- the finish line has nothing in
flight), so the cheapest way to stay green is to never use the drawer at all.

That is the shape of the defect. The freshness claim rewards **abstention**. The transition
claim cannot be satisfied by abstaining, because it is anchored to the closure, and a closure
is the one event the queue cannot avoid producing.

## What must be true

- [ ] A claim walks every commit that ADDED a file under `board/closed/` and asserts the same
      file stood under `board/active/` in that commit's parent tree.
- [ ] Its window starts at its OWN proof's birth commit, derived, not chosen: the three
      violations above happened while nothing could see them, and history is not rewritten to
      make a new gate green. The rule binds work done after the rule can be checked.
- [ ] Negative control: close an item straight out of `board/open/` and the claim goes red
      naming it.
- [ ] The queue then obeys it -- `git mv` into `board/active/` in the commit that starts the
      work, out of it in the commit that closes.
