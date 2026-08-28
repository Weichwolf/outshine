Type: bug
State: active
Area: render
Tags: residency, staging, refusal
Regresses: 2001

# a refusal reads the state it refused on, not the state it left behind

**Benchmark** — Unreal: `FRDGBuilder` validation reports the pass and the resource by name at the
moment the check fails, before any teardown runs. RAGE: an `Assertf` formats its arguments at the
failing site. **Taking both, because they agree** — a diagnostic is a READ of the state that
caused the refusal, so nothing that changes that state may run first.

`SubjectResidency::Cross` refuses a hand that does not fit the staging ring like this:

    if (StagingBytes_ < wanted || !Staging_[StagingAt_]) {
      DropStaged();
      ...
      error = ... std::to_string(StagingUsed_) ...

`DropStaged()` sets `StagingUsed_ = 0`. The message then prints that zero as if it were the
occupancy that caused the refusal. `wanted` is correct — it was computed one line above the drop
— so the message contradicts itself, and board:2001 read the contradiction as an unexplained
8192 bytes:

    45056 over 43616 -- 0 already staged and this hand adds 36864

45056 = **8192** + 36864. The 8192 was really staged; the refusal erased it and then reported the
erasure. This is the `empty vs identity` trap in CLAUDE.md's own table, applied to a diagnostic
rather than a table: a state nobody can read is not a statement.

The consequence is not cosmetic. It cost board:2001 two attempts and one wrong conclusion — the
refusal was read as evidence that the accounting was broken, when the accounting was right and the
REPORT was wrong. A refusal that lies about why it refused is worse than one that says nothing,
because it is acted on.

- [ ] the refusal reads `StagingUsed_` before any teardown, and its two numbers reconcile
- [ ] the 8192 is named for what it is: a hand already staged this frame

**The measurement that shows I am wrong:** rebuild with the canonical-slot dedupe restored and read
the refusal again. If the printed occupancy is still 0 while `wanted` exceeds `total`, the cause
is elsewhere and this item is withdrawn. Negative control: revert the fix and the same run must
print 0 again.
