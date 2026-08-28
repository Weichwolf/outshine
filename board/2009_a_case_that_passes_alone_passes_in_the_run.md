Type: bug
State: active
Area: test
Tags: measured, harness

# a case that passes alone passes in the run, and one does not

**Benchmark** — Unreal: an automation test is expected to be independent, and the runner reports
which test left the editor dirty. RAGE's build farm the same. **Both agree**, and the rule is
older than either: a suite whose verdict depends on what ran before it has no verdict.

`outshine/door/ScoreWhatAMovingSceneResends`:

    sh test/run.sh outshine/door     PASS in 3342 ms
    sh test/run.sh outshine          TIMEOUT at 122060 ms

Reproduced twice. **The runner is SEQUENTIAL** -- `test/run.sh:2178` walks cases one at a time
with a watchdog each -- so this is not contention for the machine or the GPU. Something an earlier
suite leaves behind makes this case hang, and `outshine/audio`, `outshine/content` and
`outshine/fuzz` run before `outshine/door` in that order.

It is not the orphaned-driver defect (board:2006): that one is fixed, `ps aux | grep outshine-`
prints nothing after the run, and the case that hangs is the same one every time rather than
whichever is longest.

- [ ] the case passes in `sh test/run.sh outshine` as it does alone
- [ ] whatever an earlier suite leaves behind is named, and the runner either clears it or refuses

**THE PAIRWISE WALK WAS RUN AND MY PREMISE DID NOT SURVIVE IT.** `outshine/audio outshine/door`
reproduced the timeout once and then PASSED on the next run of the same pair. So it is not suite
order and nothing is being left behind; the item's own stated cause is withdrawn.

**What it is, measured with `sample(1)` on the hung process.** The case does not BLOCK -- it
computes. With `/tmp/outshine-drive-cache` moved aside it reproduces every time, and the stack is
always the same shape:

    Engine::Assemble -> Routes -> AssembleDrive -> OsmField::Build -> TilePool::Bytes -> ...

Four hot spots, each uncovered by fixing the one in front of it:

| samples of ~2300 | where | why |
|---|---|---|
| 1040 | `snprintf` under `Address::Text` | a cache key FORMATTED per request |
| 764 | `memcmp` under `TilePool::Lookup` | the cache scanned LINEARLY per request |
| 1025 | `clock_gettime` under `Lookup` | the clock read on every hit to check a refusal |
| — | `OsmField::Build` above all three | the ring re-asked, because a REFUSED tile never settles |

Three are fixed and the fourth is the cause.

`Address::Text` uses `std::to_chars` -- no locale, no format parse. Unreal's `FIoChunkId` is a
fixed binary id and RAGE hashes a resource name once; **neither formats text on the streaming
path**, and a filename is the only reason text is needed here at all.

`TilePool` carries an `unordered_map<string, size_t>` beside `Cache_`, so a lookup is a hash
rather than a walk of every entry. `FIoDispatcher` indexes chunks the same way and so does RAGE's
resource table. Eviction keeps the index by swapping the victim with the last entry.

`Lookup` reads the clock only when a refusal deadline actually stands.

- [ ] the case passes in `sh test/run.sh outshine` as it does alone
- [ ] **a REFUSED tile settles**: `OsmField::Build` walks its ring once per call and settles only
      tiles that DECODED, so a tile the pool has refused is asked again on every call, and
      `AssembleDrive` calls `Build` until it has data it will never get. The refusal is remembered
      (board:1985) and the asking does not stop, which is the other half of the same rule.

**The measurement that would show I am wrong:** with the cache moved aside the case must FINISH --
refusing the drive is a fine outcome, hanging is not. It is killed at 120 s today.
