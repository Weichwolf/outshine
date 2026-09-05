Type: debt
State: open
Area: engine, import, scene, render
Tags: architecture, owner, audit

# The engine holds a SUBJECT and no file format; the importer converts into it

**Benchmark** -- Unreal: a `UStaticMesh`/`USkeletalMesh` knows nothing of FBX or glTF; the
importer (`FbxImporter`, `GLTFImporter`) builds engine assets and is gone. RAGE: `.ydr`/`.yft`
are the engine's own resources, converted offline. Filament: `gltfio` is a separate library
that produces `FilamentAsset` -- the renderer never sees a glTF node. **All agree**: a format
lives in its importer, and the thing the engine holds is the engine's. Decided with the owner
2026-09-05: apart from the importer NOTHING in outshine knows or allows for glTF; it is one
format this tree ships an importer for.

## Where it stands, measured 2026-09-05

```
  src/engine/Live.cpp     17 uses of Gltf::  (Gltf::Shaped, Gltf::Subject, Gltf::Viewpoint,
                             Gltf::FramingFor, Gltf::DeclaredPlacement, ResolveSurfaceTable)
  src/engine/Asset.h/.cpp 15  the asset IS a Gltf::Document + Gltf::Subject + Gltf::Pose +
                             Gltf::VariantSelection + Gltf::Transform
  src/engine/Live.h        3  Restand(const Gltf::Subject &)
  src/engine/Declaring.cpp 2  Kind != "gltf"; Gltf::Subject handed
  src/engine/EngineHeld.h  2  Gltf::Subject Handed; Blocks(const Gltf::Subject &)
  src/engine/Telling.cpp   1
  src/render/SubjectProxy.cpp 4  gltfDirection, gltfPosition
  include/scenario/Scenario.h 3  "a perspective camera as glTF declares one"
  include/generate/Generate.h 1  writeGlb (an exporter in the generators' door)
  removed today            kGltfFrontFace, comments in Mat4.h, Geometry.h, RenderFrame.h
```

## The solution

- a `Subject` in the content tier that is the ENGINE's model: parts (positions, normals, uv,
  tangents, colours, indices, material), materials, skins and joints, clips and poses, cameras,
  variants, the local transforms -- Filament's `FilamentAsset` is the shape; the importer
  (`src/import`) fills one from a glTF file and nothing else reaches the engine
- `Asset`, `Live`, `Declaring`, `Telling` and the proxy take `Subject`; `Gltf::` appears in
  `src/import` only, and a `reaches` rule keeps the import tier out of the engine's include
  path
- a scenario names a FILE, not a format: `Asset.Kind = "gltf"` becomes the importer's decision
  by the file's magic; `writeGlb` stays an exporter beside the importer, out of the door
- the camera's numbers in `Scenario.h` are the door's (Filament's) -- the wording that credits
  glTF goes

## What will be true

- [ ] `grep -rn -i "gltf\|glb" src include test --include='*.h' --include='*.cpp' | grep -v
      '^src/import'` reads 0, and a claim holds it there
- [ ] the nine references bit-identical after the move (a move, not a change)
- [ ] the Khronos corpus imports as before: the vendor cases green when `make test` returns
- [ ] Negative control: an `#include` of an import header from `src/engine` fails at the
      include with a file and a line (the tier's `reaches`)
