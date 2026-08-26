Type: task
State: open
Area: test
Tags: precision, claims, render, measured

# A float that holds a world position is refused by the gate

CLAUDE.md: *"Precision has ONE boundary and it is the camera — `Anchor - Eye` in `double`, the
model-view-projection product in `double`, and the cast to `float` only at the uniform push
(`src/render/stages/SubjectDraw.cpp:841,846,854`)."*

**Nothing walks it, and the citation is dead.** Measured at 84115df7:

    wc -l src/render/stages/SubjectDraw.cpp   ->  816

All three cited lines are past the end of the file. The casts the sentence describes are at
**731** (`uniform[i] = (float)placed[i]`), **733** (`(float)(Anchor[i] - ctx.Eye[i])`) and
**736** (`(float)(PrevAnchor[i] - ctx.PrevEye[i])`). The rule that decides whether a car a
thousand kilometres from the origin has a visible wheelbase points at three line numbers that
do not exist.

## AND THE GUARD BUILT TO CATCH THAT REPORTS AN EMPTY WINDOW

`harness/claims/TheMapCitesLinesThatSayWhatItClaims` is GREEN and its own note says why:

    NOTE file:line citations the map carries = 0 citations
    NOTE paths the map cites into this tree = 39 citations
    CHECKS 7 FAILURES 0

Zero. `CHECK(lying.empty(), ...)` passes over an empty set. Two lines make it blind:

- `CitedBy` (`TheMapCitesLinesThatSayWhatItClaims.cpp:58`) recognises only
  `` `symbol` (File.cpp:123) `` — backtick, space, paren. CLAUDE.md:27 writes the citation the
  other way round, `` (`src/.../SubjectDraw.cpp:841,846,854`) ``, with the path INSIDE the
  backticks, and the scan never fires;
- the second walk sees that span and calls `WithoutLineReference` on it
  (`TheMapCitesLinesThatSayWhatItClaims.cpp:119`), which strips `:841` before the existence
  test. The path exists, so it passes.

A guard that stops guarding goes GREEN, not red (board:1857). This one has been green over a
dead citation for as long as the sentence has stood.

## What will be true

- [ ] `CitedBy` reads BOTH forms, and a citation carrying a list — `:841,846,854` — is three
      citations, not one. Negative control: the citation as it stands today turns the claim red.
- [ ] The claim REFUSES when it walks zero citations, by that name: a map with no checkable
      citation is a map that argues from nothing.
- [ ] The declared cast sites are read from the code, not from a constant in the claim.
- [ ] No signature in `src/` takes a world position as `float` — a `float lat`, `float lon`,
      `float eastM/northM` or `float x, y, z` naming a position rather than an offset is a
      defect, and an offset must say so in its name. Negative control: a `float` anchor in a
      fresh `src/render` header turns it red.
