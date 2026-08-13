/* THE PRODUCER'S EDGE OF THE MODEL: `Assemble` takes what a generator made and yields the same
 * drawable the reader yields, so a generated part can be written, rendered by the oracle and scored
 * (doc/requirements.md I.28).
 *
 * THE FIXED POINT HOLDS AT ZERO APPLICATIONS HERE AND THAT IS WHY THE RUNS ARE f32. A produced
 * subject's numbers are the format's own width, so `Subject(Emit(Assemble(A))) == Assemble(A)` is
 * EXACT -- no narrowing stands between the part a generator grew and the part Cycles is handed.
 *
 * AND EVERY REFUSAL IS ASSERTED BY ITS SENTENCE. A generator's malformed run is arithmetic that went
 * wrong, and the failure it causes if it is admitted is a hole in a picture somebody has to trace
 * back; the checks below are what turn each of them into a named sentence at the handover. */
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Check.h"

#include "Document.h"
#include "Emit.h"
#include "Subject.h"

using outshine::Span;
using outshine::Gltf::Assembly;
using outshine::Gltf::Document;
using outshine::Gltf::Emission;
using outshine::Gltf::MaterialRef;
using outshine::Gltf::Piece;
using outshine::Gltf::Subject;
using outshine::Gltf::TangentSource;

namespace {

/* TWO PIECES THAT CARRY DIFFERENT ATTRIBUTE SETS, because that is what a real part looks like: a
 * bark tube states position and normal, a leaf states a uv as well, and the runs a subject holds
 * have to cover both without either piece reading the other's zeros. */
const float kBarkPosition[] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 2.f, 0.f};
const float kBarkNormal[] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f};
const uint32_t kBarkIndex[] = {0, 1, 2};
const float kLeafPosition[] = {2.f, 0.f, 0.f, 3.f, 0.f, 0.f, 2.f, 1.f, 0.f};
const float kLeafUv[] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f};
const uint32_t kLeafIndex[] = {0, 1, 2};

std::vector<Piece> TwoPieces() {
  std::vector<Piece> pieces(2);
  pieces[0].NodeName = "bark";
  pieces[0].Material = 0;
  pieces[0].PositionsM = Span<const float>(kBarkPosition);
  pieces[0].Normals = Span<const float>(kBarkNormal);
  pieces[0].Indices = Span<const uint32_t>(kBarkIndex);
  pieces[1].NodeName = "leaf";
  pieces[1].Material = -1;
  pieces[1].PositionsM = Span<const float>(kLeafPosition);
  pieces[1].Uv = Span<const float>(kLeafUv);
  pieces[1].Indices = Span<const uint32_t>(kLeafIndex);
  return pieces;
}

Assembly Over(const std::vector<Piece> &pieces) {
  Assembly what;
  what.Pieces = Span<const Piece>(pieces.data(), pieces.size());
  return what;
}

/* The sentence a refused assembly left behind, or an empty string where it was admitted. */
std::string Refusal(const std::vector<Piece> &pieces) {
  Subject subject;
  if (subject.Assemble(Over(pieces))) { return std::string(); }
  return subject.Error();
}

bool Mentions(const std::string &sentence, const char *fragment) {
  return sentence.find(fragment) != std::string::npos;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<Piece> pieces = TwoPieces();
  Subject made;
  CHECK(made.Assemble(Over(pieces)), "two produced pieces assemble into one subject");
  CHECK(made.Parts().size() == 2, "each piece is one part, in the order it was stated");
  CHECK(made.VertexCount() == 6, "the parts' vertices are one run");
  CHECK(made.TriangleCount() == 2, "the parts' triangles are one run");
  CHECK(made.Parts()[1].FirstVertex == 3 && made.Indices()[3] == 3,
        "a piece states its indices locally and the subject restates them over the shared run");
  CHECK(made.Parts()[0].HasNormal && !made.Parts()[1].HasNormal,
        "an attribute is carried per part and never per subject");
  CHECK(made.HasNormal() && made.HasUv() && !made.HasTangent(),
        "a run exists where any part carries it and is absent where none does");
  CHECK(made.Normals().size() == 18 && made.Uv().size() == 12,
        "a run that exists covers every vertex, including the vertices of a part that carried none");
  CHECK(made.Parts()[0].Tangent == TangentSource::None,
        "a producer that stated no basis is not recorded as having supplied one");
  CHECK(made.MinM()[1] == 0.0 && made.MaxM()[1] == 2.0, "the bounds are the produced positions'");

  /* THE FIXED POINT, ON A PRODUCED SUBJECT AND AT ZERO APPLICATIONS. */
  MaterialRef bark;
  bark.Name = "bark";
  Emission what;
  what.Geometry = &made;
  what.Materials = Span<const MaterialRef>(&bark, 1);
  what.Generator = "outshine test";
  std::vector<uint8_t> glb;
  std::string error;
  CHECK(outshine::Gltf::Emit(what, glb, error), "a produced subject is writable as it stands");
  if (!error.empty()) { std::printf("NOTE emit refused: %s\n", error.c_str()); }

  Document written;
  CHECK(written.Read(Span<const uint8_t>(glb.data(), glb.size()), "produced.glb"),
        "the file a produced subject wrote is a file this reader accepts");
  Subject again;
  CHECK(again.Build(written), "the written file flattens back into a subject");
  CHECK(again.PositionsM() == made.PositionsM(),
        "every produced position survives the round trip exactly, because a produced run is f32");
  CHECK(again.Normals() == made.Normals(), "every produced normal survives exactly");
  CHECK(again.Uv() == made.Uv(), "every produced uv survives exactly");
  CHECK(again.Indices() == made.Indices(), "the index run survives exactly");
  CHECK(again.Parts().size() == made.Parts().size(), "the part count survives");
  bool partsAgree = again.Parts().size() == made.Parts().size();
  for (size_t part = 0; partsAgree && part < made.Parts().size(); ++part) {
    partsAgree = again.Parts()[part].NodeName == made.Parts()[part].NodeName &&
                 again.Parts()[part].Material == made.Parts()[part].Material &&
                 again.Parts()[part].FirstVertex == made.Parts()[part].FirstVertex &&
                 again.Parts()[part].VertexCount == made.Parts()[part].VertexCount &&
                 again.Parts()[part].FirstIndex == made.Parts()[part].FirstIndex &&
                 again.Parts()[part].IndexCount == made.Parts()[part].IndexCount &&
                 again.Parts()[part].HasUv == made.Parts()[part].HasUv &&
                 again.Parts()[part].HasNormal == made.Parts()[part].HasNormal &&
                 again.Parts()[part].Tangent == made.Parts()[part].Tangent;
  }
  CHECK(partsAgree, "every part's name, material, boundaries and attribute set survive");

  /* WHAT A GENERATOR CANNOT HAND OVER, ONE NAMED SENTENCE EACH. */
  CHECK(Mentions(Refusal(std::vector<Piece>()), "no piece"),
        "an assembly of no piece is refused by name");

  std::vector<Piece> shortNormals = TwoPieces();
  shortNormals[0].Normals = Span<const float>(kBarkNormal, 6);
  CHECK(Mentions(Refusal(shortNormals), "normals over 3 vertices"),
        "a normal run that does not cover the vertices is refused with both counts");

  std::vector<Piece> pastTheEnd = TwoPieces();
  const uint32_t beyond[] = {0, 1, 3};
  pastTheEnd[1].Indices = Span<const uint32_t>(beyond);
  CHECK(Mentions(Refusal(pastTheEnd), "addresses vertex 3 of its own 3"),
        "an index past the piece's own vertices is refused and names the piece");

  std::vector<Piece> halfTriangle = TwoPieces();
  halfTriangle[0].Indices = Span<const uint32_t>(kBarkIndex, 2);
  CHECK(Mentions(Refusal(halfTriangle), "whole run of triangles"),
        "an index run that is not whole triangles is refused");

  const float notFinite[] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, NAN, 0.f};
  std::vector<Piece> nan = TwoPieces();
  nan[0].PositionsM = Span<const float>(notFinite);
  CHECK(Mentions(Refusal(nan), "not finite"),
        "arithmetic that went wrong is named at the handover rather than found in the picture");

  Subject refused;
  std::vector<Piece> empty;
  CHECK(!refused.Assemble(Over(empty)) && refused.Parts().empty() && refused.Indices().empty(),
        "a refused assembly carries no run at all");

  Note("produced subject vertices", (double)made.VertexCount(), "vertices");
  Note("produced subject bytes as glb", (double)glb.size(), "B");
  Covers("I.28 request -> Subject: a generator's product is the drawable, produced and not read");
  return Report();
}
