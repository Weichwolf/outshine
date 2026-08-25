Type: bug
Parent: 1841
Area: test
Tags: claims, duplication, shell

# The claims ask the shell through one door

Four claim cases spawn processes, and each spells the spawning itself:

| case | block | trims the tail | carries the verdict |
|---|---|---|---|
| `AnItemReachesClosedThroughActive.cpp` | 512 | yes | no |
| `BoardActiveNamesWhatTheQueueIsWorking.cpp` | 512 | yes | no |
| `ACommitCarriesTheItemItNames.cpp` | 4096 | **no** | no |
| `TheCorpusIsRebuiltByOneCommand.cpp` | 4096 | no | **yes** |

Two of them also carry a `Lines` that differ in shape while agreeing in result. That is one
truth in four spellings, and the divergence is already live: a case reading a trimmed answer and
a case reading an untrimmed one are two different contracts under one verb name, so a helper
lifted from one case into another silently changes what its caller sees.

The tree already refuses this one level down -- `test/harness/shared/` is on every case's include
path and holds `Check.h` and, since board:1846, `BoardNames.h`.

## What will be true

- [x] `test/harness/shared/Shell.h` holds ONE `Ask` and ONE `Lines`; the four cases include it
      and spell neither.
- [x] `Ask` answers the tail-trimmed text and takes the verdict by optional out-parameter, so the
      corpus case's `pclose` result is not a second function.
- [x] Proving test: the four cases keep their verdicts, and
      `harness/claims/TheSourceCarriesNoCommentary` and the fast gate stay green. Negative
      control: the shared `Ask` made to drop its trim -> the cases that read a bare answer change
      what they say.

## Comments

- 2026-08-25 -- filed while working board:1844, which needs a shell answer to derive the floor
  the review found originless. Filing rather than adding a fifth spelling.
