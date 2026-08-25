Type: bug
State: active
Parent: 1803
Area: clients
Tags: driver, paths, shipping

# The engine finds its shaders and its assets through declared roots, not through the process's working directory

`apps/driver/src/main.cpp` links `build/liboutshine.a` alone and drives
`Engine::Read -> Declare -> Assemble -> RenderTo -> Advance`. What it cannot do is start from
anywhere, and the two refusals are mutually exclusive: the asset resolves relative to the
WORKING DIRECTORY, the shader relative to the REPOSITORY ROOT, so no directory satisfies both.
`src/render/stages/ShaderFile.cpp:13` says it out loud — *"process must start at the repository
root"* — which is a sentence a library may not say. A shipped engine has no repository.

`test/harness/shared/PreparedRoot.h` is the missing library feature wearing a test's clothes:
every consumer so far has been a test, and a test starts at the root and resolves the corpus
itself.

## What will be true

- [ ] Roots are DECLARED — shaders, assets, cache — with a defaulting rule that works for an
      installed binary, and a missing root refuses by NAME at stand-up.
- [ ] No source in `src/` mentions a repository, a working directory or a relative climb.
- [ ] Proving test: the driver runs with its working directory set to `/`, and a root removed
      from the declaration refuses instead of half-starting.
