Type: bug
Area: corpus
Tags: oracle, perf, instrument

**The content store lives where the OS deletes things, and a purge was observed this session**

The content store is **15 GB over 2 637 objects** at
`/var/folders/2l/…/T/outshine-content` — a macOS temporary directory. *There is no second cache* is a
design rule, so **that directory holds the only copy of every derived product in this project.**

**This was filed as a bounded hazard and it is now an observed event.** In one session, the orchestrator's
own working directory under `/private/tmp/claude-501/…` **was purged while a full suite run was in
flight**: the log became an unlinked file, the run was unrecoverable, and the harness binaries went with
it. **The store survived only because `/var/folders` is a different temp area on this platform — not
because it is durable.** A hazard with a witness is not a hazard any more.

**And `board:1170` makes it load-bearing.** Pruning a validated case to its PNGs is safe *because the
store holds the bytes* — that is the clause the pruning ruling rests on, verified by content. Prune the
cases and lose the store and the corpus is gone, not degraded.

**The cost of reconstruction, beside the risk, because one without the other decides nothing:**

| | |
|---|---|
| re-render the corpus as it stands today | **83 s** — 55 renders, p50 0.60 s, p95 2.75 s, max 36.99 s |
| at the owner's ~865 content kinds | **≈ 30 min** of Cycles, unannounced, mid-round |
| what is *not* recoverable at any price | nothing — every product is derived, which is why this is a cost and not a loss |

**So the repair is a location, not a mechanism.** The store already does the right things — hash-named
files, no sidecar, a name never preceding its bytes, `os.replace` to publish. **It is in the wrong place**,
and the default came from `tempfile`'s idea of where scratch goes rather than from a decision.

- [ ] **A durable, gitignored directory inside the working tree** is the shape already proven this
  session: the orchestrator moved binaries and logs to `build/orch/` for exactly this reason and they
  survived. The store is the same kind of thing and is larger
- [ ] **The location is declared, not discovered.** Wherever it lands, the preparer publishes it in
  `provenance.json` as it already does, so a run says which store it used
- [ ] **An overridable environment variable stays**, because a machine with a small working volume is a
  real case and this is a 15 GB directory
- [ ] **A purge must be survivable and loud rather than silent.** A missing store today is a cache miss
  and a re-render, which is correct; what must not happen is a *partial* purge read as a hit. The store
  already names bytes by key and `_matches` re-digests on placement, so the guard exists — **it wants a
  test, not a mechanism**

**Done when** the store is somewhere the OS does not empty, the location is declared and overridable, and
a deleted store costs a re-render rather than a wrong answer.
