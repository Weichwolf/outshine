Type: bug
State: active
Parent: 1953
Area: engine

# A redeclaration leaves nothing of the last one

**Benchmark** — Unreal: `LoadMap` tears the world down before standing the next one. RAGE: a new map unloads the old. **Both agree** — a redeclaration leaves nothing of the last one standing.

Measured while writing `ScoreWhatThreeProducersAgreeOn`, which stood the same face three ways
through ONE engine:

    MEAN RED   file 7.628   client 0.000   maker 0.000
    LIT PIXELS file   169   client     0   maker     0

The file arm drew. The two arms after it drew NOTHING -- not a wrong picture, an empty one -- and
each of them draws correctly when it is the first thing an engine stands. So a declaration that
follows another does not fully replace it: something the file arm left standing survives a
`Declare` that names no asset, and whatever the client or the generator hands in afterwards never
reaches the frame.

**MEASURED FURTHER, AND THE SYMPTOM IS NOT "NOTHING STANDS".** The second declaration stands and
DRAWS -- `batches the picture draws = 1` -- and shades to black:

    the brightest the scene's linear buffer reached    alone 6528.000000   shared 0.000000
    the exposure the picture applied                   alone 0.000052      shared 0.000100
    batches the shadow casts                           shared 1
    texels above the clear                             shared 0
    frames the subject drew shadowed                   shared 3

So the geometry is present, placed and drawn, and contributes nothing in LINEAR space -- before
exposure, which rules the tone chain out. The last two lines are the shape of it: the subject is
marked SHADOWED for three frames while the atlas holds nothing above its clear, which is exactly
the defect `ScoreWhatTheShadowCasts` was written to guard -- *with nothing to cast, nothing writes
the atlas, so nothing may read it*. Something cast (1 batch) and wrote no texel, so the caster is
outside the light's frustum while the receiver still samples.

The trail stops there rather than being guessed further: it is a debugging session of its own and
the numbers above are what it should start from.

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
