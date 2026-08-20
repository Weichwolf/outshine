Type: feature
Area: gltf
Tags: khronos

**A primitive without normals is flat-shaded, as the format requires**

A subject read from a file carries a normal on every drawn vertex. Where the file declares none, the
reader calculates the flat normal of each triangle -- which is what glTF 2.0 says a client MUST do --
so a lit scene draws such a body instead of refusing it.

## Why it was a refusal, and why that stopped being right

`Clients::Show` refused a lit scene over a part with no NORMAL, deliberately and by name: *falling back
to the emitted arm would draw that part in a radiance nothing declared -- black, in a scene where every
other body is lit -- which reads as a shading bug rather than as the missing attribute it is.* That
reasoning is sound and it was the right guard while nothing calculated the normal.

**A GAME ENGINE HAS TO DISPLAY ITS ASSETS.** [MEASURED] **16 of the 34** models of the generator's two
skinning groups declare no NORMAL at all -- every `Animation_Skin_*` and every `Animation_SkinType_*` --
so the viewer declined half the corpus and the group a reader most wants to look at.

## What the format says, fetched rather than recalled

glTF 2.0, meshes: *When normals are not specified, client implementations **MUST** calculate flat
normals and the provided tangents (if present) MUST be ignored.*
https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes

## What it is

`Subject::FlatNormalsFor` runs after a primitive's index run is built and **before** the tangent basis,
because a basis is defined against a normal and `BuildTangentsFor` refuses a part without one -- the
order is load-bearing rather than tidy.

**IT SPLITS AT A CREASE AND NOWHERE ELSE.** Flat shading is one normal per TRIANGLE and a shared vertex
holds one normal, so the naive answer is to de-index every primitive -- which doubles the vertex count
of a body that was already unshared, for nothing. The first triangle to reach a vertex writes its
normal; a later one that agrees reuses it; only a disagreement costs a copy. A quad, a strip and any
flat surface therefore keep exactly the vertices the file declared. It is the same bargain
`BuildTangentsFor` already makes for a seam, and the same split machinery.

**A DEGENERATE TRIANGLE'S NORMAL IS ZERO**, which is the answer this reader already gives a node scaled
to nothing (`board:1439`): the surface has no orientation left, and a substituted direction would be a
picture nobody declared.

## What it cost the tests, and each repair carries its reason

Three unit tests pinned `VertexCount()` on a normal-less triangle and were fixed by splitting at a
crease rather than everywhere -- **the better design, found because the tests refused the first one.**
Two more pinned the old contract directly:

- `EmittingASubjectIsAFixedPointOfTheFlatten` used a POSITION-only part to prove the writer states
  attributes per PRIMITIVE. A part read out of a file never carries POSITION alone again, so the
  discriminator is the uv set, which the rule does not touch and which catches the same writer
- `AProducedSubjectIsTheOneItStated` asserted `Subject(Emit(P)) == P` over a produced piece with no
  normals. It now asserts what the rule promises: every normal the producer stated survives EXACTLY,
  every vertex it stated none for comes back carrying a unit normal, and `HasNormal` is the one field
  a round trip is allowed to gain

## Comments

**The corpus could not have found this and the viewer found it in a minute.** Every render case declares
`light.kind: none` and an emitting material, so nothing in 181 criteria ever needed a normal --
`CLAUDE.md`'s *the number was right and about something else*, with the domain too narrow. `board:1472`
carries that, because it is the larger finding.
