Type: bug
Area: test
Tags: build, concurrency
Regresses: 1662

**A stale break never removes a live claim**

1662 closed on "a stale break retries under the same noclobber so exactly one of two
breakers wins". The retry is under noclobber; the BREAK is not — `rm -f "$NESTLOCK"`
(test/run.sh:75) is blind to what the file holds at the moment of removal. Two windows,
both the exact shape 1662's own body spelled and its close claims shut:

- **Two-breaker steal** (run.sh:69-76): A and B both read a genuinely dead pid X. A
  removes, claims, holds, runs. B — whose read of X predates A's claim — then executes
  its `rm -f`, deleting A's LIVE lock, loops back to the noclobber, and wins the claim of
  a file that no longer exists. Both gates run in one nest; on exit each ReleaseNest
  (run.sh:45-50) removes whatever stands there, including the other's claim. The
  1662 close's "exactly one of two breakers wins" holds only when both rm's precede
  either claim; the interleave rm(A) · claim(A) · rm(B) · claim(B) defeats it.
- **Empty-content read** (run.sh:65 vs 69): `(set -C; printf '%s' "$$" > "$NESTLOCK")` is
  atomic in CREATION only — the O_EXCL open publishes an empty file, the printf lands the
  pid a syscall later. A loser reading in that gap sees an empty `otherPid` (run.sh:69),
  `[ -n ]` fails (run.sh:70), and it walks the break path against a claim that is alive.
  The close note's "no window in which a second runner reads an empty claim" is false as
  stated: no window in which the WINNER's claim lacks a pid at rest, but a reader can
  still catch it in flight.

Both windows are microseconds and both matter only when two gates start together — which
is precisely the population the lock exists for.

Demanded: the break becomes TAKE-then-INSPECT, never read-then-rm. Break by atomic
rename — `mv "$NESTLOCK" "$NESTLOCK.broke.$$"` — so at most one breaker captures any
given lock instance (the loser's mv fails ENOENT and loops back to the noclobber, where
it refuses or claims correctly). Then inspect the CAPTURED file: dead or stale-empty →
discard and retry the claim; a live pid → the breaker stole a claim taken between its
read and its mv, and it puts the file back best-effort and REFUSES loudly rather than
running (refuse over corrupt — a spurious refusal under a nanosecond race is acceptable,
a shared nest is not). Alternatively close the empty window at the source: write the pid
to a unique temp and publish by `ln` (the hardlink is atomic and the content is complete
before the name exists). Proof: a claims-test arm that plants a stale lock and starts two
env-stripped runners simultaneously, repeated a stated N — the assertion is "never two
PASS trailers", the counterpart of TheNestRefusesASecondRunner's single-runner arms.

Minor, same block, on record: `kill -0` (run.sh:70) answers "signallable", not "is a
gate" — a recycled pid belonging to any live process of this user turns a stale lock into
a permanent refusal until someone removes the file by hand. A lock that carries pid AND
start-time (ps -o lstart= -p) or simply an age bound would keep the break reachable.
