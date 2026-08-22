Type: bug
Area: test
Tags: build, concurrency
Depends: 1659

**The nest lock is atomic in claim and identity, and its refusal has a test**

1659's fix (test/run.sh:54-67) claims the nest by `mkdir "$NESTLOCK"` and writes the
holder's pid AFTERWARDS (run.sh:66). The claim and the identity are two steps, and the gap
between them is spellable in exactly the two-simultaneous-gates shape that filed 1659:

- **Empty-pid window** (run.sh:56 vs 66): runner A's mkdir succeeds; before A's
  `printf '%s' "$$" > "$NESTLOCK/pid"` lands, runner B's mkdir fails, B cats an absent pid
  (run.sh:57), `[ -n "$otherPid" ]` is false, B takes the STALE path, `rm -rf`s A's live
  lock (run.sh:61) and mkdirs its own. Both runners proceed in one nest — the corruption
  1659 documented, back through the lock's own front door.
- **Two-loser stale race** (run.sh:61-62): two runners both find a genuinely dead pid; A
  rm+mkdir, then B's `rm -rf` lands on A's FRESH lock and B mkdirs over it. Both hold; on
  exit each `rm -rf "$NESTLOCK"` (run.sh:44) removes whatever stands there, including the
  other's claim.
- **No proving test**: 1659's own body demanded "a second run.sh started while the lock is
  held exits with the refusal, and a stale lock (dead pid) is broken with a named message"
  as a test beside TheLayeringIsDeclaredOnce. The closure says "proven live" — a live probe
  is not a regression gate; the next edit to the lock block regresses silently.

Demanded: claim and pid become ONE atomic step — POSIX noclobber does it in sh:
`(set -C; printf '%s' "$$" > "$NESTLOCK") 2>/dev/null` creates the lock FILE with the pid
already inside; a loser always reads a complete identity, and the empty-pid path is
unspellable. Stale-breaking re-verifies after the break (bounded retry: break, attempt the
noclobber claim, on failure re-read the pid — never `rm` followed by a blind second claim).
And the refusal gets its claims test: plant a lock with a live pid → the gate refuses
naming it; plant a dead pid → the gate breaks it with the named message and runs.

---

Closed, and the demanded regression test immediately earned its keep: the claim and the pid
are ONE atomic noclobber write (a lock FILE, its content the pid, created under set -C in a
single step -- no window with an empty claim); a stale break retries under the same
noclobber so exactly one of two breakers wins, bounded at three attempts. The new claims
test TheNestRefusesASecondRunner -- an env-stripped child against the gate's own live lock,
plus the nested passthrough -- went RED on first run and exposed lock bug number FOUR, which
no review had seen: the release lived in KillRunning, which RunWithTimeout calls after EVERY
test, so the parent dropped its own lock one test into the gate. The release now lives in
ReleaseNest, called only from the exit traps. Both arms live-proven (live pid refuses naming
it, dead pid breaks stale), 16/16 claims, gate green.
