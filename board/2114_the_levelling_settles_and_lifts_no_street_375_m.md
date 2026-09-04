# The levelling settles, and no street is lifted 375 m

State: open

Where ways meet, `LevelsWhereWaysMeet` averages the grade at the shared node and pulls every lane
toward that average by a uniform offset. It is Jacobi relaxation with a FIXED round count
(`kLevelPasses = 24`) and a settling bar (`kLevelledM = 0.01 m`).

Measured on OldTown, 2026-09-04, by a trace built for the question and removed again:

```
met=16191  meetings=6667  lanes=5419  shifted=5024  biggest= 23.23 m  last shift 0.2474 m
met=16297  meetings=6666  lanes=5419  shifted=5021  biggest=375.45 m  last shift 5.6682 m
met=16192  meetings=6666  lanes=5419  shifted=5022  biggest= 23.23 m  last shift 0.2474 m
```

**Two things are wrong and they are not the same thing.**

**IT DOES NOT SETTLE.** The bar is 0.01 m and the last round still moves 5.67 m -- 567 times the
bar, and 0.247 m even in the quiet case. So the loop always runs all 24 rounds and stops because it
ran out of rounds, never because it agreed. The ROUND COUNT is the answer, which means the answer
has no defence: 24 is not derived from anything, and 25 would give a different world. A number
without its origin, which this tree does not allow.

**A LANE IS LIFTED 375 m.** That is not levelling, it is a lane dragged into the sky by a node it
should never have shared -- or by a chain of nodes passing a pull along. The ledger already has the
field that would have shown it (`streets: vertices FLYING, over the bar`) and it was never read
against this pass. 23 m in the quiet case is already too much for a junction.

The two are linked: an unsettled Jacobi sweep spreads a bad pull FURTHER with every round instead
of damping it, so the second defect is what the first one fails to contain.

## What Unreal does, what RAGE does

**NEITHER FACES THIS AT RUN TIME, and that is the finding.** Unreal's Landscape Splines and RAGE's
road network are AUTHORED: a person places the junction, the tool deforms the landscape under it,
and the result is baked into the map. There is no solver in the frame because the answer was
settled in the studio. outshine takes its world over the wire, so what they bake has to happen
during preload -- the choice is MINE and this item has to say why.

## What will be true

- the loop stops because it AGREED, and the round count is a ceiling that is not normally reached
- the residual is published, so "it did not settle" is visible instead of silent
- no lane moves further than a junction can justify, and the bar is derived rather than set
- `streets: vertices FLYING` is read against this pass, because it is the oracle that was already
  there

## What will show I was wrong

The residual after the last round, published beside the shift. If it stays above `kLevelledM` on
any of the eight places, the pass has not settled and the number it produced is the round count
speaking. Today: 5.67 m against a bar of 0.01 m.

## Not in this item

The DATA LAYOUT was repaired on 2026-09-04 (flat, node-sorted vector and one offset per lane
instead of a hash map walked 24 times, and the uniform shift applied once at the end rather than
per round). All eight digests stayed bit-identical, so that move is settled and is not what this
item is about. This item is about the VERDICT the pass reaches, not what it costs to reach it.
