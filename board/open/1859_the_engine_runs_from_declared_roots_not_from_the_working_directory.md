Type: bug
Parent: 1803
Area: clients
Tags: driver, paths, shipping

# The engine finds its shaders and its assets through declared roots, not through the process's working directory

`apps/driver/src/main.cpp` is 146 lines, links against `build/liboutshine.a` alone, and drives
`Engine::Read → Declare → Assemble → RenderTo → Advance`. It runs. What it cannot do is start,
and the two refusals it produces are mutually exclusive:

```
$ outshine-driver --from 48.1371,11.5754 --to 48.1583,11.5033 --headless      # at the repo root
REFUSED scene.gltf: cannot be opened

$ cd $TMPDIR/outshine-prepared/apps-driver-f31 && outshine-driver --scenario /abs/f31.scenario ...
REFUSED the stage 'subjects' did not configure: the shader source
        src/render/shaders/subjectBindings.msl is not readable from here -- the engine reads
        its shaders from the tree, so the process must start at the repository root
```

**The asset resolves relative to the working directory and the shader resolves relative to the
repository root, so no working directory satisfies both.** The refusal even says the rule out
loud: *"the process must start at the repository root"* — which is a sentence a library may not
say. A shipped engine has no repository.

This is why `apps/driver/src/` held two declarations and no program (board:1803): every consumer
so far has been a TEST, and a test starts at the repo root and resolves the corpus itself
through `test/harness/shared/PreparedRoot.h`. That header is the missing library feature wearing
a test's clothes.

## What will be true

- [ ] The engine takes its roots as DECLARATIONS, not as ambient state: where the shaders are,
      where the assets are. A scenario names its assets; something the client hands the engine
      says where that name resolves.
- [ ] No refusal in `src/` names the repository, the tree, or a working directory. The three
      spellings of that assumption today are `src/render/shaders/...` in the stage
      configuration, the relative `Uri` in `Scenario::Asset`, and `src/assets` passed as a
      literal by every caller of `Sim::Provision`.
- [ ] `apps/driver` runs from any working directory, with `--from`/`--to`/`--headless`, and
      writes a still — which is what makes the hourly review's screenshot possible at all.
- [ ] Proving test: a case that runs the driver's own entry point from a working directory that
      is NOT the repository root and gets a frame. Negative control: the declared shader root
      removed -> the refusal returns, naming what was not declared rather than where the process
      stands.

## Comments

- 2026-08-25 -- found by writing the entry point board:1803 says is missing. The program itself
  is not the hard part; it compiled and linked first try against `include/outshine/` alone. What
  it exposed is that the LIBRARY is not shippable: it reads its own source tree at runtime.
