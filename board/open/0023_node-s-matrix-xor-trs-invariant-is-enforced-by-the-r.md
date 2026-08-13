Type: bug
Area: gltf
Tags: khronos

**`Node`'s *matrix XOR TRS* invariant is enforced by the reader, 250 lines from the type it protects**

`src/gltf/Types.h:107-119`. `Node` is a `struct` carrying `bool HasMatrix`, `double Matrix[16]`,
`double Translation[3]`, `double Rotation[4]`, `double Scale[3]`, with the invariant written in a comment
— *"A node carries a matrix or a TRS triple, never both"* — and enforced by an `if` in another file
(`src/gltf/Document.cpp:369-386`).

**The reason stated at the site is that the reader refuses the file that carries both. It is true and it
is not the question.** `C.2` — use `class` if the class has an invariant, `struct` if the members can
vary independently — and `C.40`, define a constructor if a class has an invariant. Here the members
demonstrably cannot vary independently: `Matrix` is meaningless when `HasMatrix` is false and the TRS
triple is meaningless when it is true. **A rule a reader enforces can be broken by the next writer of a
`Node`; a rule a `std::variant<Trs, Matrix4>` carries does not compile.** The "both set" state loses its
spelling, the "neither" state is `Trs{}` and is identity, `HasMatrix` disappears, and the branch that
reads it (`Document.cpp:564`) becomes `node.Local()` — which also deletes the possibility of a *second*
consumer reading `Matrix` without checking the flag and receiving the default identity, i.e. a mesh at
the origin.

**Worth a round now rather than after the format widens, and the argument is consumer count.** Today
`Node` has exactly one branch on `HasMatrix` and one test assertion, both inside `src/gltf/`. The bridge
from the reader to `core/ChunkVtx.h` is the next round (`board/` § I.26) and is a second
consumer; materials, skins and animations bring more. The edit costs a type, a `Transform Local() const`,
and one line of `test/unit/gltf/AMatrixNodeAndItsTrsAgree`, and it never costs less than it does now. It
also shrinks the record from 26 doubles (208 B) to a variant of 136 B (`Per.16`, `Per.18`).

**The caveat, sought and answered.** Is the invariant really exclusive? A node carrying neither is legal
and means identity — that is the `Trs{}` alternative with its own defaults, so the variant is exhaustive
and nothing is lost; and glTF forbids animating a `matrix` node, so no later arm needs to decompose one
back into a TRS triple.

**Two smaller defects in the same declaration, fixed by the same edit:** `double Matrix[16]` and
`double Translation[3]` are C arrays where `std::array` is the rule (`SL.con.1`), and they decay to
`const double *` at the `Transform::FromColumnMajor(step.Matrix)` call (`Bounds.3`).

**Fixed when** a `Node` with both a matrix and a translation does not compile.
