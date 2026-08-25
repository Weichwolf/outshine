Type: bug
Parent: 1841
Area: test
Tags: claims, duplication, shell

# The claims ask the shell through one door

**NINE** harness sources spawn processes, and each spells the spawning itself -- the first
count in this item said four, which was what `grep 'Ask('` found rather than what
`grep 'popen('` found:

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

## Closed 2026-08-25 -- the count was nine, not four

`grep -rln 'popen(' test/` over the tree at filing:

| shape | sources |
|---|---|
| `Ask(cmd)` returning trimmed text | `AnItemReachesClosedThroughActive`, `BoardActiveNamesWhatTheQueueIsWorking` |
| `Ask(cmd)` returning untrimmed text | `ACommitCarriesTheItemItNames` |
| `Ask(cmd, int &verdict)` | `TheCorpusIsRebuiltByOneCommand` |
| `Run(cmd, std::string &said)` returning the verdict | `AGreenTrailerNamesWhatItDidNotJudge`, `APreparedCaseCarriesItsOwner`, `EveryDeclaredSuiteResolvesItsOwnSymbols`, `EverySuiteListsEachSourceOnceAndEverySourceHasASuite`, `TheCorpusRefusesASecondPruner`, `TheNestRefusesASecondRunner`, `TheCorpusIsPrunedByOneRunnerOnly` |
| a raw `popen` inline | `TheSourceCarriesNoCommentary`, `Layering.h`'s `Compile` |

`test/harness/shared/Shell.h` now holds all three verbs, and `Run` is the one implementation:

```cpp
inline int Run(const std::string &command, std::string &said);
[[nodiscard]] inline std::string Ask(const std::string &command, int *verdict = nullptr);
[[nodiscard]] inline std::vector<std::string> Lines(std::string_view block);
```

The trim is GONE rather than shared, and that is a measured decision, not a preference: the
control this item first wrote -- *"the shared Ask made to drop its trim -> the cases that read a
bare answer change what they say"* -- was run and **nothing changed**. Every caller either feeds
`Lines`, which drops empty lines anyway, or reads through separator bytes. A contract nothing
observes is not a contract, so the door answers what the process wrote.

**The proving test is new, because the item's own was not one.** A repair that removes eight
copies is proven by a rule that forbids the ninth, not by the suite staying green:
`test/harness/claims/TheHarnessSpawnsThroughOneDoor` (IV.32) walks all 295 harness sources and
finds any that start a process outside the door.

Its needle is ASSEMBLED (`std::string("po") + "pen("`) and the reason is in the source: a file
spelling what it forbids finds itself, and the exemption that fixes it -- *skip my own path* --
is a rule that stops applying to its own author.

Negative control, run: `TheNestRefusesASecondRunner` given back its own private spawn ->
`FOUND test/harness/claims/TheNestRefusesASecondRunner.cpp spawns a process itself` and
`FAIL ...:56 THE HARNESS SPAWNS THROUGH ONE DOOR`.
