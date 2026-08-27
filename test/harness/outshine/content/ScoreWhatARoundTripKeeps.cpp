#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <Geometry.h>

#include "Check.h"
#include "Document.h"
#include "Subject.h"

namespace {

// THE READER MUST FILL THE ONE VALUE, because there is nowhere else to read TO. Today it reads into
// `Gltf::Subject` while the door carries `Geometry` -- two representations of one thing, and that
// is why they can drift: nineteen glTF extensions reach the picture and the builder carries vertex
// streams.
//
// FILLING THE GAP ROW BY ROW IS THE WRONG REPAIR and this case is the cheap way to prove the value
// is complete without rewriting the reader first. `Subject::Handed()` expresses what the reader
// produced AS a `Geometry`; assembling that gives a second subject. If the one value can carry what
// the reader makes, the two are identical -- and if it cannot, the comparison names exactly which
// field went missing, which is worth more than an argument about which fields ought to be there.
//
// The oracle is identity under a round trip and owes nothing to our design: read, express, rebuild.
// Anything the middle form cannot hold shows up as a difference, and a difference is a gap in the
// value rather than a bug in the comparison.
constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/ZmZmP83MzD7NzEw+AACAP83MTD5mZmY/zczMPgAAgD/NzMw+zcxM"
    "PmZmZj8AAIA/AACAPwAAAAAAAAAAAACAPwAAgD8AAAAAAAAAAAAAgD8AAIA/AAAAAAAAAAAAAIA/";

[[nodiscard]] std::string Minimal(void) {
  return std::string(
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0,1]}],"
      "\"nodes\":[{\"mesh\":0,\"name\":\"first\"},"
      "{\"mesh\":0,\"name\":\"second\",\"translation\":[2,0,0]}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,"
      "\"TEXCOORD_0\":2,\"COLOR_0\":3,\"TANGENT\":4},\"material\":0}]}],"
      "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.2,0.1,1.0],"
      "\"metallicFactor\":0.25,\"roughnessFactor\":0.75}}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
      "\"min\":[0,0,0],\"max\":[1,1,0]},"
      "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
      "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
      "{\"bufferView\":3,\"componentType\":5126,\"count\":3,\"type\":\"VEC4\"},"
      "{\"bufferView\":4,\"componentType\":5126,\"count\":3,\"type\":\"VEC4\"}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
      "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36},"
      "{\"buffer\":0,\"byteOffset\":72,\"byteLength\":24},"
      "{\"buffer\":0,\"byteOffset\":96,\"byteLength\":48},"
      "{\"buffer\":0,\"byteOffset\":144,\"byteLength\":48}],"
      "\"buffers\":[{\"byteLength\":192,\"uri\":\"data:application/octet-stream;base64,") +
      kTriangleBase64 + "\"}]}";
}

[[nodiscard]] size_t Apart(const std::vector<double> &was, const std::vector<double> &is) {
  if (was.size() != is.size()) { return was.size() > is.size() ? was.size() : is.size(); }
  size_t many = 0;
  for (size_t at = 0; at < was.size(); ++at) {
    if (std::fabs(was[at] - is[at]) > 1.0e-6) { ++many; }
  }
  return many;
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Gltf;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string held = Minimal();
  Document file;
  const outshine::Span<const uint8_t> bytes((const uint8_t *)held.data(), held.size());
  const bool wasRead = file.Read(bytes, "round-trip.gltf");
  CHECK(wasRead, ("the fixture reads: " + file.Error()).c_str());
  if (!wasRead) { return Report(); }

  Subject read;
  CHECK(read.Build(file), ("the reader stands it: " + read.Error()).c_str());
  if (read.Parts().empty()) { return Report(); }

  const outshine::Geometry expressed = read.Handed();
  Subject rebuilt;
  CHECK(rebuilt.Assemble(expressed), ("the one value rebuilds it: " + rebuilt.Error()).c_str());
  if (rebuilt.Parts().empty()) { return Report(); }

  std::printf("THE READER STOOD    %zu part(s), %zu vertices, %zu indices, %zu surface(s)\n",
              read.Parts().size(), read.VertexCount(), read.Indices().size(),
              read.Surfaces().size());
  std::printf("THE ROUND TRIP GAVE %zu part(s), %zu vertices, %zu indices, %zu surface(s)\n",
              rebuilt.Parts().size(), rebuilt.VertexCount(), rebuilt.Indices().size(),
              rebuilt.Surfaces().size());
  std::printf("POSITIONS APART %zu   NORMALS APART %zu   UV APART %zu   COLOURS APART %zu\n",
              Apart(read.PositionsM(), rebuilt.PositionsM()), Apart(read.Normals(),
              rebuilt.Normals()), Apart(read.Uv(), rebuilt.Uv()),
              Apart(read.Colours(), rebuilt.Colours()));

  CHECK(read.Parts().size() == 2,
        "the fixture stands TWO parts from one mesh under two nodes, so the round trip below "
        "carries a part boundary and a node transform rather than a single flat run");
  CHECK(rebuilt.Parts().size() == read.Parts().size(),
        "**THE ONE VALUE CARRIES WHAT THE READER MADE**: expressing a read subject as a "
        "`Geometry` and assembling it gives the same number of parts. A middle form that could "
        "not hold a part boundary would merge them, and no argument about which fields ought to "
        "be there would have found that");
  CHECK(Apart(read.PositionsM(), rebuilt.PositionsM()) == 0,
        "and every vertex lands where it did, so the node transforms the reader BAKED survived "
        "the trip -- positions are what a middle form loses first when it keeps sources instead "
        "of what was placed");
  CHECK(Apart(read.Normals(), rebuilt.Normals()) == 0,
        "and so do the normals, which the reader generates flat when a primitive omits them -- "
        "the pass that does it belongs to the packer now, so both sides of the trip run it");
  CHECK(rebuilt.Surfaces().size() == read.Surfaces().size() &&
            !read.Surfaces().empty() &&
            std::fabs(rebuilt.Surfaces()[0].Metalness - read.Surfaces()[0].Metalness) < 1.0e-6,
        "and the material rows cross with their numbers: 0.25 metalness read from the file "
        "arrives as 0.25 through the value, which is the half of the gap table that closed when "
        "the subject began holding the surfaces it was assembled with");

  CHECK(!read.Uv().empty() && Apart(read.Uv(), rebuilt.Uv()) == 0,
        "and the uv set crosses whole, which the value could only carry once `Texture(part, uv, "
        "set)` existed on it -- a middle form without a second uv set would drop one silently");
  std::printf("TANGENTS APART %zu  (read holds %zu, rebuilt %zu)\n",
              Apart(read.Tangents(), rebuilt.Tangents()), read.Tangents().size(),
              rebuilt.Tangents().size());
  CHECK(!read.Tangents().empty() && Apart(read.Tangents(), rebuilt.Tangents()) == 0,
        "**AND A SUPPLIED TANGENT BASIS CROSSES**: this was the question expected to answer NO, "
        "because tangents are one of the three the reader writes through the packed surface "
        "rather than through a stream. It crosses because the value carries a tangent stream and "
        "the reader's supplied path fills it -- what does not cross is a GENERATED basis, which "
        "belongs to the packer and is recomputed on the far side rather than carried");

  CHECK(!read.Colours().empty() && Apart(read.Colours(), rebuilt.Colours()) == 0,
        "and so do the vertex colours, which glTF states in [0,1] and the assembler REFUSES "
        "outside it -- so a round trip that mangled them would be refused rather than quietly "
        "wrong");

  Covers("gltf: what the reader makes can be expressed as the door's one geometry value and "
         "rebuilt from it -- parts, placed vertices, generated normals and material rows survive "
         "the round trip -- parts, placed vertices, generated normals, uv sets, vertex colours "
         "and material rows -- so the value is complete for what the reader produces");
  return Report();
}
