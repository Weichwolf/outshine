Type: bug
Area: doc
Tags: claude-md, map, regression

# The map's red table does not eat the rule beneath it

board:1768's repair added a justification table under the render-plan CURRENT diagram and
left the paragraph that followed it half-swallowed. `CLAUDE.md` at fda0d090:

```
152 | red | what makes it red, at HEAD |
153 |---|---|
154 | `SUBJ` | one stage carrying six responsibilities -- ... ; nothing culls |
155 | `GLASS` | `{Stage::SubjectsTransmissive, ...` (RenderCatalogue.h:268) is a full clone of
      `{Stage::Subjects, ...` (:263) -- transmissive draws belong in the one subject stage | One
      `Writes` producer per derived resource (`static_assert`); missing
156 contributor = picture choice, **published** as `-> neutral`; load/store ops derived from the
157 plan (`Stored()`).
```

Line 155's second cell ends with a `|` and then continues into the render plan's own rule.
The table gets a phantom third column reading "One `Writes` producer per derived resource
(`static_assert`); missing", and lines 156-157 dangle as an orphan paragraph that begins
mid-sentence with "contributor = picture choice".

The rule this destroyed is not decoration -- it is the render plan's binding invariant:

> One `Writes` producer per derived resource (`static_assert`); missing contributor = picture
> choice, **published** as `-> neutral`; load/store ops derived from the plan (`Stored()`).

The map is the tree's constitution and every agent reads it first. A rule a reader cannot
parse is a rule that is not there.

## Evidence

- CLAUDE.md:155-157 (introduced by 5fb183f0, unchanged at fda0d090)
- Introduced while closing board:1768, whose whole subject is the map telling the truth.

## What will be true

1. The `GLASS` row ends at its own `|`, and the `Writes`-producer rule stands as its own
   paragraph, whole, below the table.
2. `test/harness/claims/EveryColourCitesALineThatSaysIt` -- or a sibling -- refuses a table
   row that carries more cells than the header declares, so the next hand that edits this
   table cannot ship the same shape green. Today the walk parses `| `Node` |` rows and never
   counts columns, which is why 206/206 passed over a broken document.
