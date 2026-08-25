Type: issue
Area: process
Tags: claude.md, diagrams, review

# CLAUDE.md's CURRENT diagrams are corrected by whoever moved the tree, and judged only by the review

Two standing instructions disagree, and the disagreement is worked around every session rather
than settled.

`CLAUDE.md:555`:

> the review is the only writer of the diagrams in this file

The owner's standing `/goal`, restated every turn by the Stop hook:

> CLAUDE.md stays current: every structural change lands in its CURRENT diagrams the same
> session, and a diagram that lies about the tree is itself a finding.

The review fires at `:17` each hour. The queue works continuously between rounds. So a class
renamed at `:20` leaves CLAUDE.md lying about the tree for 57 minutes under the first rule, and
the second rule calls that lie a finding. Both cannot hold as written, and the queue has been
resolving it by hand -- writing CURRENT rows and then noting the breach.

## The line that makes both true

The two rules are not about the same thing. One is about MEASUREMENT, the other about JUDGEMENT:

| what | who may write it | why |
|---|---|---|
| a CURRENT node's existence, name, edges | whoever moved the tree, the same session | a node that is gone is not an opinion; leaving it is a lie with a timestamp |
| a CURRENT `file:line` citation, a count, a measured byte size | the same | the citation is a measurement of HEAD and nothing else |
| a CURRENT node's COLOUR, and the row that argues it | the review only | green/amber/red is a principal engineer's verdict on whether a shape is right, and a repair grading its own work is the conflict the hourly review exists to prevent |
| anything in TARGET | the review only | where the tree is going is not decided by the item in flight |

A repair that believes its node has earned a colour change says so in the item's closure; the
review reads the delta and grades it. That is already how a closure reaches the report.

## What will be true

- [ ] `CLAUDE.md:555` states the split above rather than a rule the owner's own instruction
      contradicts.
- [ ] Proving test: a claim that reads CLAUDE.md's CURRENT class diagram, extracts every
      `file:line` citation in the red/amber tables, and checks that the cited line still says
      what the row claims -- a lying diagram becomes RED rather than a finding somebody has to
      notice. Negative control: a row's line number shifted by one -> the claim goes red.

## Comments

- 2026-08-25 -- filed by the queue against itself: this session edited CURRENT rows in CLAUDE.md
  while `:555` says it may not, and the alternative -- leaving the file wrong until `:17` -- is
  what the owner's instruction forbids. Filing rather than continuing to choose silently.
