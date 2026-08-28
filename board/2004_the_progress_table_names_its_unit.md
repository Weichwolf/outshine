Type: bug
State: active
Area: test
Tags: state, legibility

# the progress table names its unit, so `5/15` cannot be read as fifteen items

**Benchmark** — neither engine has this table; it is outshine's own. **The choice is mine and the
standard is CLAUDE.md's**: "an abstraction that needs the author present to be understood is
unfinished", and "the reader I am writing for is a competent stranger".

`STATE.md`'s Progress table prints

    | area | held | share | tickets | note |
    | `gpu-driven` | 5/15 | 33% | [1943](...), [1985](...) | |

`5/15` is PREDICATES -- ticked boxes across the listed items -- and the column beside it lists
two items. The reader of this tree read that cell as "fifteen tickets open" **twice in one
session**, which is the whole evidence needed: a cell whose unit lives only in the generator is
not legible, and `held` names the state of the number without naming what is counted.

- [ ] the header names predicates and items, and the cell cannot be read as a count of items

**The measurement that would show I am wrong:** none is possible from the tree -- legibility is
read, not measured. What stands in for it: the header must answer "fifteen what?" without the
generator open beside it. If it still cannot, the fix failed.
