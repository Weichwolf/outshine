Type: issue
State: open
Area: test
Tags: claims, identity, ledger

# A claim number names one claim

Every proof in this tree ends in `Covers("<number> <sentence>")`, and the number is the claim's
identity -- it is how a scorer, a trailer and a reader tie a case to what it proves. There is
no ledger anywhere in the tree that says which numbers exist:

```
$ find . -name '*.md' -not -path './board/*' -not -path './.git/*'
./CLAUDE.md
./.claude/agents/architecture-reviewer.md
./test/harness/shared/corpus/README.md
./apps/driver/test/stills/parts_probe.md
```

None of them lists a claim. The numbers are chosen by hand, per file, and they collide. Forty-two of the 106 numbers
in use carry two or more UNRELATED sentences. The three added this session collide
with three that already stood:

| number | sentence A | sentence B |
|---|---|---|
| `IV.13` | *the device does not leave the render layer* (new, `TheDeviceLeavesTheLibraryOnlyForItsOwnTwins.cpp:80`) | *the runner publishes, per run, which declared case families hold no fetched subject* |
| `IV.14` | *the prepared corpus is shared and the right to delete from it is not* (new, `TheCorpusRefusesASecondPruner.cpp:69`) | *every item standing in board/active was moved there by a recorded transition* |
| `IV.15` | *a commit that touches a board item names it* (new, `ACommitCarriesTheItemItNames.cpp:89`) | *the shared corpus is pruned by the runner holding its claim and by no other* (`TheCorpusIsPrunedByOneRunnerOnly.cpp:114`) |

`IV.14` and `IV.15` are the worse pair: the corpus's own two claims now sit under two numbers
that each also name something else, and `TheCorpusRefusesASecondPruner` and
`TheCorpusIsPrunedByOneRunnerOnly` -- one item's two proofs -- carry `IV.14` and `IV.15`
respectively, so the corpus question cannot be asked by number at all.

Thirty-nine more collisions stand from before this session. `I.26` is the worst: twelve
sentences under one number, spanning the glTF reader (`AGlbCarriesWhatItDeclares.cpp`,
`ACameraCrossesWithItsConventions.cpp`, five more), mip upload
(`EveryLevelOfAnUploadedChainReadsBackAsItself.cpp`), device LOD selection
(`TheDeviceSelectsALodAcrossAUvDiscontinuity.cpp`) and *"the engine is a library"*
(`TheEngineNamesNoTestOfItsOwn.cpp`). `I.4` carries seven, `IV.7` three.

This is CLAUDE.md's own rule one level up: *every number carries its origin*. A claim number's
origin is the claim it names, and today it names several.

## What will be true

- [ ] The claim numbers are DECLARED in one place the build reads -- a table beside `Check.h`,
      or the claim text keyed by number in a header the cases include, so a number is spelled
      once and a typo is a compile error rather than a collision.
- [ ] `Covers()` takes the number as a value from that catalogue, not as free text.
- [x] The standing collisions are resolved: each sentence gets its own number, or two
      cases proving ONE claim share the number deliberately and the catalogue says so.
- [x] Proving test: a `harness/claims` walk that reads every `Covers("` in `test/`, groups by
      number, and refuses a number carrying two different sentences. Negative control: HEAD ->
      red, printing all forty-two.

## Comments

- 2026-08-25 -- filed by the hourly review, after three new claims each took a number that was
  already in use. The mechanism is the same one `board:1824` built for the map's citations: a
  number that nothing recomputes drifts, and here it drifts into another claim's identity.

## Two boxes closed, two named (2026-08-25)

`harness/claims/AClaimNumberNamesOneClaim` reads every `Covers(` under `test/`, joins the
adjacent string literals the way the compiler does, groups by number and refuses a number
carrying two DIFFERENT sentences. Two cases sharing a number for the SAME sentence stay legal:
that is one claim proven twice, which the tree does deliberately.

Measured at HEAD before the repair: **44 numbers carried more than one sentence**, across 209
`Covers` statements and 107 distinct numbers. Five of the 44 were this session's own -- the new
claims took `IV.13` through `IV.17`, each of which already stood -- and those moved to the free
end of the prefix rather than displacing the older claim.

The 39 standing ones were resolved mechanically: the first sentence keeps the number, every
other takes the next free number in its own prefix. **94 sentences were renumbered; 107 numbers
became 206, all distinct.** Gate 271/271 after.

Negative control, run: `IV.24` written back to `IV.7` ->

```
NOTE numbers carrying more than one sentence = 1 numbers
FOUND IV.7 carries 2 different sentences over 2 cases
```

**The two boxes that stay open are the catalogue**, and the reason is that renumbering is not
what they ask for:

- a table beside `Check.h` the build reads, so a number is spelled once and a typo is a compile
  error;
- `Covers()` taking the number as a VALUE from that catalogue rather than as free text.

What landed makes every number unique and keeps it that way. What it does not do is make a
number unspellable-if-wrong, and the mechanical assignment above means the numbering carries no
meaning beyond uniqueness -- `I.26`'s twelve sentences are now twelve numbers whose ordering
says nothing about what they cover. A catalogue is where that meaning would live.
