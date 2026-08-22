Type: bug
Area: core
Tags: instrument

**One include-set declaration feeds the Makefile and the runner, and `make` compiles the library entire**

**`make` is red at HEAD, and the failure is structural rather than a typo.**

```
src/world/Wayfinding.cpp:3:10: fatal error: 'Fit.h' file not found
```

`src/world/Wayfinding.cpp:3` includes `Fit.h` from `src/corridor`; the Makefile's `INC_WORLD`
(`Makefile:42`) carries no `-Isrc/corridor`. `test/run.sh` knows better: its `GroupIncludes` for
`src/world` (`test/run.sh:214`) says `-Isrc/corridor` explicitly. So the tests pass while the
build the README of the Makefile calls "the library entire" (`Makefile:9`) does not compile.

**Three layers are missing from the library outright.** The Makefile declares no source group and
no include set for `src/corridor`, `src/physics` or `src/pilot` — `grep -n 'corridor\|physics\|pilot'
Makefile` returns nothing, and `Makefile:66-83` lists every group there is. `build/liboutshine.a`
has never contained the corridor, the physics or the pilot; every consumer of those layers got them
only through `test/run.sh`'s own compiles (`test/run.sh:141-143,163`).

**The root cause is that the layering is declared twice.** `Makefile:13` says THE LAYERING IS THE
BUILD — but the include sets exist once as `INC_*` (`Makefile:37-61`) and once as
`LayerIncludes`/`GroupIncludes` (`test/run.sh:61-231`), with nothing keeping them equal. They have
already diverged beyond the red compile:

| sources | Makefile | test/run.sh |
|---|---|---|
| `src/world` | no `-Isrc/corridor` (`Makefile:42`) | `-Isrc/corridor` (`run.sh:214`) |
| `src/clients/Sim.cpp` | no `-Isrc/corridor`, no `-Isrc/generators/draw` (`Makefile:58`) | both (`run.sh:227`) |
| corridor · physics · pilot | absent | own layers (`run.sh:65-67`) |

Two build systems each holding a private copy of the layer graph is the exact shape the Makefile's
own preamble condemns in runners: "a second runner with a second verdict, and this repository has
already paid for having two" (`Makefile:4-7`). RAGE and Unreal both generate the build from one
module declaration (Unreal: `*.Build.cs` per module is the single truth for public/private include
paths); here the single truth exists in neither file.

## Done when

- [ ] the layer → include-set map lives ONCE (one file both read, or one generates the other)
- [ ] `make` compiles `src/corridor`, `src/physics`, `src/pilot` into `build/liboutshine.a` and is
      green at HEAD
- [ ] a divergence cannot pass silently: either the shared source makes it impossible, or a claims
      test compares the two maps and fails loudly

---

**Closed.** The Makefile lost its private copy of the layering: `make` delegates to
`test/run.sh --library`, which builds every source under `src/` from the runner's own
`GroupIncludes`/`GroupToolchain` declarations -- a file arm when one is named, the directory arm
otherwise, and a source neither names is a loud refusal. `build/liboutshine.a` now carries 138
objects including corridor, physics and pilot, and `make` is green at HEAD. Proving test:
`test/harness/claims/TheLayeringIsDeclaredOnce` -- the Makefile spells no `-Isrc` at all, so the
divergence is unspellable rather than compared.
