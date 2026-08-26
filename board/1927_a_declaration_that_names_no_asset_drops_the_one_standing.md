Type: bug
State: active
Area: engine, door
Tags: declaration, measured

# A declaration that names no asset drops the one that was standing

`Engine::Declare` with an empty `Assets` list leaves the previous subject standing. Measured
while working board:1922: a scenario declaring one glTF was stood and drawn, then re-declared as
the same scenario with `Assets.clear()`, and `batches the picture draws` still reported **1**.
The shadow radius derived from the first subject's extent still stood at 0.7071 m, because
`Live::Build` -- which derives it -- is reached through `Restands`, and `Restands` is never
called when the new declaration names nothing to stand.

CLAUDE.md: *a scenario is a STREAM, not a value that is re-declared. `Declare` seeds; after that
parts enter and leave.* Parts entering works (`ScoreWhatASubjectSwapRebuilds` proves a swap).
Parts LEAVING does not: the only way a part leaves today is by being replaced.

This is why board:1922 cannot build its control arm, and it is worth more than that: a scenario
that clears its assets is asking for an empty stage, and getting the last one back is a lie about
what was declared.

## What will be true

- [ ] A declaration whose asset list is empty stands nothing, and `batches the picture draws`
      is 0.
- [ ] Every derived default that hangs off the subject -- the shadow radius is the one found so
      far -- goes with it rather than outliving it.
- [ ] Proving case: declare a subject, render, re-declare with an empty asset list, render, and
      the picture draws nothing. Negative control: the declaration path as it stands, and the
      second render draws the first subject.
