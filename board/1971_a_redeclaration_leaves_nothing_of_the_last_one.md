Type: bug
State: open
Parent: 1953
Area: engine

# A redeclaration leaves nothing of the last one

Measured while writing `ScoreWhatThreeProducersAgreeOn`, which stood the same face three ways
through ONE engine:

    MEAN RED   file 7.628   client 0.000   maker 0.000
    LIT PIXELS file   169   client     0   maker     0

The file arm drew. The two arms after it drew NOTHING -- not a wrong picture, an empty one -- and
each of them draws correctly when it is the first thing an engine stands. So a declaration that
follows another does not fully replace it: something the file arm left standing survives a
`Declare` that names no asset, and whatever the client or the generator hands in afterwards never
reaches the frame.

**CLAUDE.md's own sentence is the specification**: *A SCENARIO IS A STREAM, not a value that is
re-declared. `Declare` seeds; after that parts enter and leave.* Parts leaving is exactly what does
not happen here. Unreal's level transition tears down the world it replaces and RAGE's map swap
unloads the nodes it leaves; neither carries a resident from one declaration into the next by
accident.

The case works around it by standing each arm in its own engine, which is honest for a producer
comparison and hides nothing -- the comparison wants the same starting state, and an engine that
has already stood something is not that state. But the leak is real and it is the client's problem
the moment a client declares twice.

- [ ] a declaration that names no asset leaves nothing of the one before it standing
- [ ] geometry handed in after a file declaration reaches the frame, proven by the three-producer
      case running all three arms through ONE engine
