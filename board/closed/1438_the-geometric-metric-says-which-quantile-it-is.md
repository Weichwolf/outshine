Type: bug
Area: harness
Tags: instrument

**The geometric metric says which quantile it is**

`worst_disagreement_px` was **not the worst disagreement**. It is the disagreement at `kBoundFraction`,
which is 0.99 -- and the largest one had its own row beside it under the name
`worst_disagreement_max_px`. So the metric that decides carried the word `worst` and the metric that is
the worst carried the word `max`, and a reader had no way to tell from either name which was which.

| was | is |
|---|---|
| `worst_disagreement_px` | **`disagreement_p99_px`** |
| `worst_disagreement_max_px` | **`disagreement_max_px`** |
| `worst_disagreement_samples` | **`disagreement_samples`** |

**Both names are recorded here on purpose**: every earlier board item quotes a log line under the old
name, and a quotation is not edited to match a later rename. This row is where the two spellings meet.

## It cost a round, and the round is this session's own

While measuring `board:1430` I read `worst_disagreement_px 0.1353173` as the largest disagreement and
reasoned about it as a maximum for several steps -- deriving an edge offset from it, checking that
against an area-over-perimeter estimate, and puzzling at the mismatch. **The two numbers are 0.1353173
and 0.17261918**, so the arithmetic survived; the reasoning was about the wrong quantity and only the
name said so.

*`CLAUDE.md` states the failure in one sentence -- the number was right and about something else -- and
lists four faces for it. This is a fifth: **the name was about something else**.*

## The convention it now follows was already in the tree

`picture_p99_delta_code` has carried its quantile in its own name since `board:1367` revised the picture
bound to p99. The geometric metric now matches it, so the two verdicts a case publishes read the same
way, and the maximum stands beside each of them as a reported row rather than as a rival name.
