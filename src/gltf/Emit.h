/* THE THIRD EDGE OF THE MODEL, AND THE ONLY ONE THAT WAS MISSING. `bytes -> Document -> Subject` is
 * the reader; `request -> Subject` is what a generator does; this is `Subject -> bytes`, and what it
 * buys is that a GENERATED part gets a number -- Cycles renders it and the picture bound decides it
 * (board:0105). Without it a grown tree is judged by eye for as long as it exists.
 *
 * THIS WRITER ANSWERS TO ONE PRODUCER, WHICH IS WHY IT IS A FRACTION OF THE READER. `Document.cpp`
 * is 46 kB because it must accept every legal file; this emits exactly what a `Subject` carries and
 * refuses everything else BY NAME: one buffer, tightly packed, one accessor per attribute per part,
 * 32-bit indices, no sparse accessor, no stride variety, no animation, no camera, no image.
 *
 * IT IS A FIXED POINT OF THE FLATTEN AND NOT OF THE DOCUMENT. `Subject(Emit(S)) == S`; `Document ->
 * Document` is NOT identity, because the flatten discards the hierarchy and a round-trip test
 * written the obvious way would be asserting something false. One node per part with an identity
 * transform is what makes the second flatten reproduce the first exactly.
 *
 * AND IT NARROWS TO f32, BECAUSE THAT IS WHAT THE FORMAT'S POSITION IS. A `Subject` holds doubles;
 * glTF's POSITION, NORMAL, TEXCOORD_0 and TANGENT are float. So the fixed point holds exactly for
 * every subject whose numbers are f32-representable -- which is every subject the reader produced
 * from a file under identity transforms, and every subject the emitter itself produced. A subject
 * carrying a node rotation reaches the fixed point after ONE emit rather than at zero, and that is a
 * property of the format and not of this code.
 *
 * IT IS A TEST PATH AND NEVER A FRAME PATH. Nothing in a running frame serialises; a generator that
 * could only produce bytes would have to be parsed back before it could be drawn, which is the
 * translation the model refuses. */
#ifndef GLTF_EMIT_H
#define GLTF_EMIT_H

#include <cstdint>
#include <string>
#include <vector>

#include "Span.h"

#include "Subject.h"
#include "Types.h"

namespace outshine::Gltf {

/* THE WHOLE OF WHAT IS WRITTEN, as one parameter object rather than four arguments (`I.23`).
 *
 * THE MATERIAL TABLE TRAVELS WITH THE GEOMETRY AND HAS NO SHORTER SPELLING. `Part::Material` is an
 * INDEX into a table a `Subject` does not own, so a writer handed the subject alone could either
 * drop the material -- and a Cycles render of a part with no material is not a render of that part
 * -- or invent placeholders to keep the indices, which is a table nobody declared. The caller owns
 * the table because the caller is who resolved the index in the first place.
 *
 * `Generator` IS `asset.generator`, the format's own field for who wrote the file. It is here rather
 * than defaulted because a generated part's provenance is the one thing a reader of the file cannot
 * recompute from it. */
struct Emission {
  const Subject *Geometry = nullptr;
  Span<const MaterialRef> Materials;
  std::string Generator;
};

/* One `.glb` -- one file, one hash, no relative path for a consumer to resolve. `false` leaves
 * `error` holding the sentence, and every refusal names what it refused: a texture the writer has no
 * image bytes for, a transmissive surface the core format cannot state, an emission above the
 * format's own ceiling, a placed light, a generated tangent basis, a part naming a material outside
 * the table. */
[[nodiscard]] bool Emit(const Emission &what, std::vector<uint8_t> &glb, std::string &error);

} // namespace outshine::Gltf
#endif
