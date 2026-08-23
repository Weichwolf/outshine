Type: bug
Area: ui
Tags: style, selector

**A child link after a descendant gap cannot refuse a matching tree**

`Selects` (`src/ui/Style.cpp:835-852`) matches the compound chain by walking ancestors
greedily: the FIRST ancestor that holds a compound consumes it, and a failed `Child` link
then returns false — no backtracking. CSS requires the match to succeed if ANY assignment of
compounds to ancestors satisfies the links.

Proven false negative:

    <div class="outer"><div class="mid"><div><div class="mid">
      <span class="leaf">x</span></div></div></div></div>

    .outer > .mid .leaf { color: red }

→ `selects=0`. The greedy walk binds `.mid` to the inner ancestor, whose parent is not
`.outer`, and refuses — though the outer `.mid` IS a child of `.outer` and a browser (and
the WPT oracle) selects the leaf. `test/unit/ui/ARuleRanksByTheSpecificityCssDeclares.cpp`
exercises specificity, not combinator completeness, so the gate holds nothing here.

Demanded: on a failed `Child` link, resume the search for the LAST-consumed compound at the
next ancestor (standard retry-from-descendant matching — the chain is short, the walk stays
bounded by tree depth), plus the unit case above as the regression proof.

---

Closed: Selects matches right-to-left WITH BACKTRACKING -- a descendant link tries every
ancestor candidate instead of marrying the first, so `.outer > .mid .leaf` selects through
the UPPER .mid when the lower one's parent is not .outer (CSS's own semantics; the recursion
is bounded by chain length x tree depth, both declared small). Negative-controlled: the
greedy walk restored, the two-rung proof goes red. WPT CSS corpus 162/162 beside it.
