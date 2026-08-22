Type: bug
Area: core
Tags: scope

**The repair policy, and it is not a cadence**

*Stated 2026-08-12 because it had been unstated and was being improvised. The argument for it is below;
if it is wrong, the alternative is a repair round on a fixed cadence and the owner decides.*

- **A defect is repaired by the round that needs it**, not by a round scheduled to repair defects. A
  repair round has no acceptance of its own: it cannot tell a fix from a change, because the thing that
  would have told it — the work that meets the defect — is what was deferred.
- **A fixed defect is deleted in the round that fixes it.** Later is never: this audit found **21** dead
  entries, and the file overstated the debt by 14 %.
- **Cheap defects are batched**, because eleven one-line repairs in one round cost one review and eleven
  rounds cost eleven. Band 2 below is that batch.
- **The exception, and it is the only one: a defect that blocks the next round is not deferred to it.**
  Band 1 is repaired before the work that would meet it starts, because meeting it inside that work
  makes the work's own acceptance unreadable.


---

**Closed as superseded (2026-08-22).** CLAUDE.md's board section codifies the repair policy: defect found = item, same round; open->active->closed is the machine.
