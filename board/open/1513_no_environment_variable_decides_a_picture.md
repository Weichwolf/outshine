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

## What must be true

- [ ] **The pixel error is DECLARED by the scenario** (`board:1480`'s render row) and defaulted where it
      is absent, so two runs of one declaration are one picture whatever the shell holds
- [ ] **The worker count is declared too**, because *if pace decides the result, the coupling is a bug*
      -- and a streamer with a different number of workers must produce the same world
- [ ] **A test holds that no `getenv` reaches a picture**, which is a grep this repository can make into
      a claim: `src/` may read the environment where a HOST does, and nowhere the frame path can see
- [ ] **The value in force is PUBLISHED with every measurement**, so a number taken under a different
      tau is not silently comparable with one that was not

## Comments

**This was found by asking a different question.** The owner pointed out that the F31 is a test of the
LOD pipeline; reading whether the subject path uses the ladder at all (`board:1512`, it does not) put
this line on the screen. *A defect that has been sitting in the one place nobody looks was found by
following a feature request into it.*
