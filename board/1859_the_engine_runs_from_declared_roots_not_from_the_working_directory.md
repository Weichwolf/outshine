Type: bug
State: open
Parent: 1803
Area: clients
Tags: driver, paths, shipping, shaders

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

## Where the shaders live is part of it

Measured at HEAD: **no shader is embedded as a string** -- 25 `.msl` files stand under
`src/render/shaders/` and no `R"(` appears anywhere in `src/`. That half is done.

What is not: they are read at runtime through a path spelled into the code,
`LoadShaderText("src/render/shaders/subjectDepthOnly.msl")` and twenty-four like it, which is a
path into the SOURCE TREE. So a shipped library needs its own source checked out beside the
binary, and `src/assets/` -- CLAUDE.md's name for the library's declared data -- holds every
other declared file while the shaders sit outside it.

They belong in `src/assets/shaders/`, under the SAME declared root as the ground materials, the
vegetation table and the sky data. One root, not two: a second root is a second thing an
installer can get wrong, and it is the one the engine cannot start without.

## What will be true

- [ ] The shaders live under `src/assets/`, resolved through the same declared root as every
      other shipped file, and no stage names a directory.
- [ ] Roots are DECLARED — shaders, assets, cache — with a defaulting rule that works for an
      installed binary, and a missing root refuses by NAME at stand-up.
- [ ] No source in `src/` mentions a repository, a working directory or a relative climb.
- [ ] Proving test: the driver runs with its working directory set to `/`, and a root removed
      from the declaration refuses instead of half-starting. Negative control: the shader path
      spelled into a stage as it stands today, and the same run cannot find it.
- [ ] A claim walks `src/` for a literal path into the tree -- `"src/` in a string -- and refuses
      when one stands, the way `TheSourceCarriesNoCommentary` walks for comments. Twenty-five of
      them got in one at a time because nothing was counting.
