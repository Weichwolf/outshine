# board/active

**What is being worked on RIGHT NOW.** One file per item, moved here from `board/open/` by the
commit that starts the work and out of here by the commit that closes or parks it.

An **empty drawer is a legal and desirable answer** -- the board's own definition of done is
that no open item remains. This file is why the directory survives that state: git carries no
empty directories, so without a tracked file `board/active/` vanishes from the tree the moment
the queue finishes, and every path that cites it stops resolving.

Two claims read this directory:

| claim | what it asks |
|---|---|
| `harness/claims/BoardActiveNamesWhatTheQueueIsWorking` | is what stands here fresh -- moved here by a commit near HEAD |
| `harness/claims/AnItemReachesClosedThroughActive` | did every item that arrived in `board/closed/` pass through here |

The first is satisfied by an empty drawer. The second cannot be, which is why both stand.
