Type: bug
Area: harness
Tags: instrument, corpus, perf

**The content store grows without bound, and nothing can tell a live entry from a dead one**

[MEASURED] `/var/folders/…/T/outshine-content`: **54 GB over 18 655 entries.** Of that, roughly 500 MB
is fetched upstream assets and the rest is cached oracle renders.

**`CLAUDE.md` now carries the rule this breaks** -- *no unbounded loop, no unbounded queue, no unbounded
growth; everything that can grow states what bounds it.* **The store states nothing.** Its key covers the
host, the subject's bytes, the whole declared scene and the recipe, so **every manifest edit orphans
every product that case had** -- and this session alone rewrote more than a dozen manifests.

## Why it cannot be cleaned by inspection, measured rather than assumed

| candidate criterion | why it fails |
|---|---|
| **references in `provenance.json`** | 231 of 18 655 entries appear there. **Not a liveness signal**: provenance records the fetch digests, not the derived render keys, so 18 424 "unreferenced" includes every live cached render |
| **access time** | [MEASURED] this filesystem does not maintain it -- an entry read at 17:16 still reported an atime of 17:05. **A cache HIT leaves no trace at all**, so a live entry is indistinguishable from a dead one |
| **modification time** | same defect facing the other way: a hit does not touch mtime either, so an entry created a week ago and hit every day since looks exactly like an orphan |

**So there is no safe deletion without deriving the live set**, and that is the repair rather than a
heuristic over timestamps.

## The repair, and the preparer already holds every piece of it

- [ ] **A `collect` job in `prepare.py`.** It is the one script the constraints allow and it already
  computes every key it writes: walk every case, derive the fetch digests and the render keys the same
  way the run does, union them, and delete what is not in the union. **`--every-case` exists now**, so
  the walk is already written
- [ ] **It must be a JOB and not a flag on `all`.** A sweep that collected as a side effect of preparing
  would delete another case's products the moment somebody prepared one case alone
- [ ] **It publishes what it removed, in entries and bytes**, the way the prune already does per case.
  *A collector that reported nothing would be a second silent truncation.*

## What it costs to leave open, so the choice is the owner's and priced

| | |
|---|---|
| **keep it** | 54 GB and growing by roughly the size of a case's products on every manifest edit. The disk is at **26 GiB free** |
| **delete it entire** | frees ~53 GB and costs **a full re-render of every oracle** -- 56 cases, several recipes each, some over a 31-frame grid. Hours of Cycles, and the only thing lost is time |
| **build the collector** | bounded work, and it is the only option that does not have to be taken again next month |

## Closed by a ruling rather than by a collector: a Cycles render is not cached

**The owner: this machine has more CPU than disk.** So the render cache goes, and the collector this
item was going to ask for is never needed — there is nothing to collect when nothing is kept.

**The two halves are separated and only one of them goes**, because they cost different things:

| | what a miss costs |
|---|---|
| **fetched upstream bytes** | the **network**, and `raw.githubusercontent.com` rate-limits by IP — measured this session at HTTP 429 through both `urllib` and `curl`. **Kept** |
| **Cycles renders** | the **CPU**, which is what this machine has. **Not kept**: `_render_one` copies its products out, reads its account back, and calls `store.forget` on every key it wrote |

**[MEASURED] the effect, before and after:**

| | before | after |
|---|---|---|
| store size | **54 GB** | **165 MB** |
| store entries | **18 655** | **233**, which is exactly the fetch digests the manifests name |
| free disk | 26 GiB | **80 GiB** |
| a case prepared after the change | — | 14 writes, and the store still holds **233** entries |

**What was deleted and why it was safe to.** Everything whose name is not a `sha256` a manifest states —
53.4 GB. Renders are not cached going forward, so their liveness stopped mattering; the fetch entries
are named by the tracked manifests and were kept by that rule rather than by a timestamp.

**The bound this now has, which is what the item was about.** The store holds one entry per file the
corpus declares, and it grows only when a case declares a new file. *That is a bound somebody chose,
which is what `CLAUDE.md`'s no-unbounded-growth rule asks for and what the store did not have.*
