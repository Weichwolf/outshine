Type: bug
State: open
Area: engine, import
Tags: measured

# A pose moves matrices, not meshes

**Benchmark** — Unreal: a skeletal mesh's render data is built ONCE; animation writes a bone
matrix palette per frame and the vertices are skinned from it, so `FSkeletalMeshRenderData` never
re-reads the asset. RAGE: the same shape -- `grmGeometry` is immutable and `crSkeleton` supplies
the matrices `rmcDrawable` skins with. **They agree**, so the matter is closed: a pose is a
transform stream, never a rebuild of the geometry.

`Core::Posed::Poses` calls `Subject::Build(File_, pose, weights, variant)` on EVERY frame. That
re-reads the document, re-decodes every accessor and re-assembles every part, and it has two
consequences beyond the cost:

- **Anything APPENDED onto the carrier is silently dropped.** `Live::Build` joins a declared body
  onto a file's subject and sizes the surface table to the joined count; the next pose returns the
  file's parts alone, and the proxy then stands over three parts while its table names nine. The
  refusal reads `the subject proxy stands over 3 parts and the surface table names a slot for 9`
  and it names the symptom, not the cause.
- **The render shape has to be re-formed per pose.** `Live::Pose` now calls `Reshape()` because
  there is exactly one shape and the proxy stands on it. That is correct for as long as a pose
  moves vertices; once a pose moves MATRICES the re-form disappears with it.

This was met while cutting the world path off the importer's carrier (board:1547, board:1949). It
did not exist because of that cut -- the drop was always there -- but the cut is what made it
visible, because one shape cannot paper over a carrier that changes shape underneath it.

- [ ] A pose writes a transform palette and leaves the assembled geometry standing
- [ ] Appending survives a pose, and the proxy's part count and its surface table cannot disagree
- [ ] `Live::Pose` no longer re-forms the shape, because there is nothing new to form
      proof: the Khronos animation cases are the only reference this tree has for a pose -- an
      `outshine/` case here would state agreement with ourselves about a design that is changing
