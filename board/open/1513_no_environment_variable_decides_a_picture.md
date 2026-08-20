Type: bug
Area: render
Tags: bug, instrument

**No environment variable decides a picture**

```cpp
inline float SseTauPx(void) {
  static const float tau = []() { const char *e = getenv("FB_TAU"); return e ? (float)atof(e) : 1.0f; }();
  return tau;
}
```

**`FB_TAU` is the projected-error threshold the LOD ladder selects by.** `export FB_TAU=4` changes every
picture this engine has ever produced, and **no manifest, no report and no digest would show it.**

`CLAUDE.md`: *the picture is a function of the DECLARATION, not of the machine.* This is the exact
sentence, broken, in the one place where breaking it is invisible -- and it is `static`, so it is read
once per process and cannot even be varied deliberately.

## The others, because it is a class and not a case

| | |
|---|---|
| `src/core/ClusterDag.h` | **`FB_TAU`** -- the pixel error a rung is chosen by. **A picture** |
| `src/world/TerrainLoader.cpp` | `FB_TILEWORKERS` -- how many threads stream tiles. A pace, and pace must not decide a result |
| `src/world/World.cpp` | `FB_DAGLOG` -- logging, and harmless |
| `src/clients/Env.h` | the reader itself |

## HOW THE SHIPPED ENGINES DO IT, looked up rather than recalled

**Not one of them reads the environment.** Unreal has three places and every one is visible:

| where | what it is | why it is not an environment variable |
|---|---|---|
| **per asset** | *Nanite Settings* on the mesh -- the precision its representation is built at | it travels WITH the content, so a different asset can want a different answer |
| **per run** | `DefaultScalability.ini`, `DefaultEngine.ini` | a FILE the application loads, which can be diffed, shipped and pinned |
| **at runtime** | a **console variable** | **registered, enumerable, queryable and loggable** -- a deliberate act with a name |

**And Nanite has no LOD threshold at all**: it targets a pixel error and scales the triangle count to
it, which is the currency this engine already declares.

**THE DIFFERENCE IS NOT WHERE THE NUMBER LIVES, IT IS WHETHER ANYBODY CAN SEE IT.** A CVar can be
listed, printed into a report and compared between two runs. `getenv` is ambient: it is read once,
appears in no output, and a shell that set it three days ago changes today's measurement with nothing to
grep for. *That is the whole defect, and it is why `FB_TAU` is worse than a hard-coded constant would
be -- a constant at least shows up in a diff.*

## THE PIXEL IS THE NATURAL THRESHOLD, so the default is DERIVED and not chosen

**One pixel is what *no visible difference* MEANS.** A rung whose projected error is under a pixel
cannot be told from the rung above it by anyone looking at the screen -- that is not a preference, it is
what the display is. **So `tau = 1 px` is a derivation and the existing default is already right**; what
is wrong is only how it can be changed.

*This is Nanite's own claim restated: it carries no LOD threshold because it targets a pixel, and
`CLAUDE.md` already demands the same currency -- **a budget is a screen-space error in pixels, because
it is the only currency comparable across terrain, trunk, facade and crown**.*

## THREE LEVELS AND A FIXED PRECEDENCE, which is the owner's ruling

| | who sets it | when |
|---|---|---|
| **the engine's default** | outshine, **derived** -- one pixel | always, and it is sensible on its own |
| **the scenario** | the declaration (`board:1480`'s render row) | where a world wants a different bar |
| **the client** | the consumer, where it makes sense | at run time, deliberately |

**A later level overrides an earlier one and the value in force is published.** *That is the same
later-overrides-earlier rule `board:1493`'s layers follow, and the same shape Unreal has as
default -> ini -> console variable -- with the environment removed from the chain entirely.*

**AND THE ENGINE MUST WORK WITH NOTHING DECLARED.** A scenario that says nothing about pixel error gets
one pixel and a correct picture; a client that overrides nothing gets the scenario's. *An engine whose
defaults are absent is an engine every consumer has to configure before it draws anything, which is the
opposite of `create -> load -> run -> destroy`.*

## What must be true

- [ ] **The pixel error is one pixel unless something says otherwise**, and what said otherwise is
      published -- so two runs of one declaration are one picture whatever the shell holds
- [ ] **Every default the engine ships is DERIVED or `[SET]` with a reason**, because a default nobody
      can argue with is a magic number with better manners
- [ ] **The worker count is declared too**, because *if pace decides the result, the coupling is a bug*
      -- and a streamer with a different number of workers must produce the same world
- [x] **A test holds that no `getenv` reaches a picture.**
      `test/harness/claims/NoEnvironmentVariableDecidesAPicture.cpp` reads **298 files across all ten
      layers** and finds **0 undeclared readers**. The three that remain PRINT THEIR REASON, so a new
      one anywhere is red without anybody remembering to look
- [ ] **The value in force is PUBLISHED with every measurement**, so a number taken under a different
      tau is not silently comparable with one that was not -- *this is the property a CVar has and an
      environment variable cannot*
- [ ] **A per-ASSET precision may refine the per-run one**, which is Unreal's Nanite Settings and is the
      right shape: the content knows things the run does not

## Comments

**This was found by asking a different question.** The owner pointed out that the F31 is a test of the
LOD pipeline; reading whether the subject path uses the ladder at all (`board:1512`, it does not) put
this line on the screen. *A defect that has been sitting in the one place nobody looks was found by
following a feature request into it.*

## `FB_TAU` is gone, and it was DEAD

`SseTauPx()` was called by nothing. `DagSelect` already takes `tau` as a parameter -- the right shape
all along -- so the function was a reader nobody read, sitting in the header where the ladder lives.
**A dead path is worse than a missing one**: it looked like the way tau was configured, and it was not
even that. It is now `kPixelTau = 1.0f`, `constexpr`, with `board:1513`'s derivation beside it.

## And writing the claim caught the claim

**The first version of the test listed seven layers and left out `src/world`** -- which is exactly where
both surviving readers live. It passed. **A test whose population excludes the thing it is looking for
is a rubber stamp**, and this repository has now made that mistake twice in one session (`board:1473`'s
stale log, and this). *The tell was the same both times: a check that went green on the first run.*

## The two that remain, each with its reason in the test's own output

| | |
|---|---|
| `FB_DAGLOG` | logging -- changes what is printed, never what is drawn |
| `FB_TILEWORKERS` | **a pace and not a picture**, excused only while the streamer's result is worker-count independent -- **which this item still owes a test for**, and the default beneath it is ALREADY machine-dependent: `hardware_concurrency - 2` |

- [ ] **The streamer's result is worker-count independent, and that is MEASURED** rather than assumed --
      one worker and eight must build the same world, or `FB_TILEWORKERS` is a picture after all and so
      is the machine it runs on

