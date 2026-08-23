Type: bug
Area: render
Tags: frame-path, memory, gpu
Regresses: 1463

# A mid-frame staging regrow never orphans what is already staged

`src/render/stages/SubjectResidency.cpp:92-105` — the deferred `Cross` path grows the
staging ring by reassigning ALL `kStagingRing` slots (`Staging_[slot] = OwnedTransfer(...)`)
whenever `StagingUsed_ + total > StagingBytes_`. But `Staged_` entries recorded by an
EARLIER `Cross` call of the SAME frame hold the raw `SDL_GPUTransferBuffer*` of the slot
just destroyed (`SubjectResidency.cpp:129-130`), and their bytes were written into the old
mapping. `FlushCrossings` (line 174-186) then encodes uploads from a RELEASED transfer
buffer, and the pose bytes are gone either way.

Reachable today, not hypothetical: `SubjectDraw::SetPose` issues TWO deferred crossings per
animated frame — `HandStreams(pose, true, ...)` at SubjectDraw.cpp:625, then
`HandVisibility(true, ...)` at :633 after the BVH refit. On the first deferred frame
(`StagingBytes_` still sized for one of the two) and on every frame where a larger pose or
refit BVH arrives, the second call regrows and orphans the first call's staged runs.

The same lines carry the frame-path allocation discipline 1463 established and this file
now breaks in three places:

- growth is to EXACTLY `wanted` (line 96-97), no headroom factor, so a slowly growing
  pose regrows the whole ring repeatedly — device allocations on the tick path;
- `SubjectResidency.cpp:74-85` recreates the destination buffer whenever `*one.Held !=
  one.Bytes` — a SHRINK also reallocates, so fluctuating stream sizes buy heap per frame;
- `SDL_MapGPUTransferBuffer` results are fed to `memcpy` unchecked at
  `SubjectResidency.cpp:51` (`Fill`) and `:244` (`Upload`) — a device out-of-memory is a
  segfault, not a refusal ("a failure is loud" means an error string, not SIGSEGV).

Demand: the staging capacity is opened ONCE, at residency establishment, from the declared
mesh's own stream sizes (the numbers exist in `HandStreams`); a frame that wants more than
the opened capacity is a REFUSAL naming both numbers, like `kStagedCrossings` already is
(line 124-127). If regrowth is kept for load-time, it must first flush or re-copy the
already-staged bytes and patch `Staged_[].Staging`. Null-check both maps.

`src/render/stages/SubjectResidency.cpp` has NO unit twin (test/unit/render/stages/ holds
none) — the proving test is the twin: stage two crossings whose second forces the regrow,
flush, and assert the first crossing's bytes arrived intact.
