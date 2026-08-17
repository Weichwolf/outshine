/* glTF's `COLOR_0`, AND THE SIX CELLS OF WHICH ONE ASSET COVERS ONE (board:1193).
 *
 * THE SPECIFICATION PERMITS SIX SPELLINGS AND `BoxVertexColors` CARRIES ONE OF THEM: `VEC3` or
 * `VEC4`, times float, unsigned byte normalized or unsigned short normalized
 * (`Specification.adoc:1339`). The corpus's only vertex-coloured asset is float `VEC3`, so the other
 * five reach no picture at all, and the shape of that gap is board:1179's and board:1186's -- a path
 * that exists, is claimed, and is exercised by nothing. They are exercised HERE, as six accessors
 * over one set of colours, because a synthetic file is what makes a cell that no upstream asset
 * spells decidable at all.
 *
 * THE ONE CLAIM THE SIX SHARE: they are the SAME colours, and WHERE THEY CANNOT BE, THAT IS
 * MEASURED RATHER THAN TOLERATED. The reader divides a normalized integer by its own maximum, so an
 * unsigned byte 255, an unsigned short 65535 and a float 1.0 arrive as one double, and a `VEC3`
 * element is widened with the alpha 1.0 the format states -- so nothing downstream carries six
 * shapes of vertex colour, or two widths of it. At 0 and at 1 the six agree BIT FOR BIT. At an
 * intermediate value they cannot: `128/255` is not a dyadic rational, so a float accessor stores the
 * nearest f32 and a normalized byte accessor divides in double, and the two lattices meet only at 0
 * and 1 for these denominators. The residual is a REPRESENTATION and is asserted against half an f32
 * ulp -- an equality here would have been a claim about arithmetic nobody can satisfy.
 *
 * AND THE ONE THEY ARE MOST LIKELY TO BE READ WRONG BY: `COLOR_0` IS LINEAR IN ALL SIX. An unsigned
 * byte colour is what an sRGB-encoded value looks like -- every base-colour texel in the corpus is
 * one -- and the format says this one is not: "this value acts as an additional LINEAR multiplier to
 * base color" (`Specification.adoc:2088`). A reader that decoded it would produce a picture that is
 * plausible and wrong, which is the silent-success class board:1182 was filed against, so the two
 * readings are stated side by side below and the wrong one is named.
 *
 * THE CLAMP IS A REQUIREMENT ON THE ASSET AND THIS ENGINE REFUSES RATHER THAN REPAIRS. "All
 * components of each `COLOR_0` accessor element MUST be clamped to `[0.0, 1.0]` range"
 * (`Specification.adoc:1356`) sits in a paragraph of statements about what a FILE must contain, so a
 * file outside the range is malformed. Three answers were available and all three are defensible:
 * clamp, refuse, trust. THIS ENGINE REFUSES AND NAMES THE VERTEX, THE CHANNEL AND THE VALUE, because
 * clamping repairs somebody else's asset inside a comparison whose subject IS that asset (board:0073
 * on deriving what a file does not carry), and trusting multiplies base colour past one and publishes
 * a brighter body that reads as authored. It is spellable on TWO of the six cells: a normalized
 * unsigned byte or short cannot leave [0, 1], so on the other four the range is carried by the type
 * and the refusal has nowhere to fire.
 *
 * AND THE OTHER OPERAND IS glTF's DEFAULT MATERIAL, which is why it is stated here too: the asset
 * that carries the picture declares NO material, so `COLOR_0` multiplies the format's own
 * `baseColorFactor` of [1,1,1,1] and not this engine's mid-grey default. Those are the render case's
 * two candidate causes for one residual, and this is where they are separated -- the case cannot.
 *
 * NOTHING HERE RENDERS. Which numbers a file's vertex colours are is a computation over the file. */
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"
#include "Material.h"
#include "Subject.h"
#include "Types.h"

namespace {

using outshine::Gltf::Accessor;
using outshine::Gltf::Assembly;
using outshine::Gltf::ComponentType;
using outshine::Gltf::Document;
using outshine::Gltf::ElementType;
using outshine::Gltf::Piece;
using outshine::Gltf::Subject;
using outshine::Gltf::VertexColourComponents;
using outshine::Test::Append;
using outshine::Test::Glb;

/* THE THREE COLOURS THE SIX CELLS ALL CARRY. 0 and 1 are exact in every one of the six, which is
 * what the bit-for-bit claim below is asserted on; `128/255` is the intermediate value, and it is
 * the one an sRGB decode would move -- by more than a factor of two -- which is what makes the
 * linearity claim decidable rather than asserted. */
constexpr double kZero = 0.0;
constexpr double kHalf = 128.0 / 255.0;
constexpr double kOne = 1.0;
constexpr double kAlpha = 64.0 / 255.0;

/* The same value read as though it were sRGB-encoded, which is what this reader must NOT do. It is
 * the format's own transfer, spelled here once so the number in the message is derived. */
double LinearFromSrgb(double encoded) {
  return encoded < 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

struct Cell {
  const char *Name;
  int ComponentCode;
  bool Normalized;
  const char *Element;
};

/* THE SIX CELLS, AS THE FILE SPELLS THEM. */
constexpr Cell kCells[6] = {
    {"float VEC3", 5126, false, "VEC3"},        {"float VEC4", 5126, false, "VEC4"},
    {"ubyte normalized VEC3", 5121, true, "VEC3"},
    {"ubyte normalized VEC4", 5121, true, "VEC4"},
    {"ushort normalized VEC3", 5123, true, "VEC3"},
    {"ushort normalized VEC4", 5123, true, "VEC4"}};

/* One triangle whose three vertices carry the three colours above, in whichever of the six
 * spellings the cell names. The colour run is the LAST view so that its width may vary without
 * moving anything else. */
std::string Text(const Cell &cell, size_t colourBytes) {
  const std::string normalized = cell.Normalized ? ",\"normalized\":true" : "";
  return std::string(
             "{\"asset\":{\"version\":\"2.0\"},"
             "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"COLOR_0\":1},"
             "\"indices\":2}]}],"
             "\"nodes\":[{\"mesh\":0}],\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
             "\"accessors\":["
             "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
             "\"min\":[0,0,0],\"max\":[1,1,0]},"
             "{\"bufferView\":2,\"componentType\":") +
         std::to_string(cell.ComponentCode) + ",\"count\":3,\"type\":\"" + cell.Element + "\"" +
         normalized +
         "},"
         "{\"bufferView\":1,\"componentType\":5125,\"count\":3,\"type\":\"SCALAR\"}],"
         "\"bufferViews\":["
         "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
         "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":12},"
         "{\"buffer\":0,\"byteOffset\":48,\"byteLength\":" +
         std::to_string(colourBytes) +
         "}],"
         "\"buffers\":[{\"byteLength\":" +
         std::to_string(48 + colourBytes) + "}]}";
}

/* The colour run in the cell's own component type. `components` is 3 or 4; the fourth value is
 * `kAlpha`, so a reader that widened a VEC3 with something other than 1.0 and a reader that dropped
 * a VEC4's fourth component are two different failures below. */
std::vector<uint8_t> Binary(const Cell &cell, size_t components) {
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const uint32_t indices[3] = {0, 1, 2};
  const double colours[3][4] = {{kZero, kHalf, kOne, kAlpha},
                                {kHalf, kOne, kZero, kAlpha},
                                {kOne, kZero, kHalf, kAlpha}};
  std::vector<uint8_t> out;
  for (const float value : positions) { Append(out, value); }
  for (const uint32_t value : indices) { Append(out, value); }
  for (const auto &colour : colours) {
    for (size_t channel = 0; channel < components; ++channel) {
      const double value = colour[channel];
      if (cell.ComponentCode == 5126) {
        Append(out, (float)value);
      } else if (cell.ComponentCode == 5121) {
        Append(out, (uint8_t)std::lround(value * 255.0));
      } else {
        Append(out, (uint16_t)std::lround(value * 65535.0));
      }
    }
    /* The format's own alignment rule: every accessor element starts on a multiple of its component
     * size, and a three-wide unsigned byte element is three bytes. Padding it here would state a
     * stride the accessor does not declare, so the run stays tight and the reader reads it tight. */
  }
  return out;
}

size_t ComponentBytesOf(const Cell &cell) {
  return cell.ComponentCode == 5126 ? 4u : (cell.ComponentCode == 5121 ? 1u : 2u);
}

bool Reads(const Cell &cell, Document &into, Subject &out, std::string &why) {
  const size_t components = std::string(cell.Element) == "VEC4" ? 4u : 3u;
  const std::vector<uint8_t> binary = Binary(cell, components);
  const std::vector<uint8_t> glb =
      Glb(Text(cell, 3u * components * ComponentBytesOf(cell)), binary);
  if (!into.Read({glb.data(), glb.size()}, "colour.glb")) {
    why = into.Error();
    return false;
  }
  if (!out.Build(into)) {
    why = out.Error();
    return false;
  }
  return true;
}

/* A FILE WHOSE COLOUR IS OUT OF RANGE, which only a float accessor can spell. */
std::vector<uint8_t> OutOfRangeBinary(float value) {
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const uint32_t indices[3] = {0, 1, 2};
  std::vector<uint8_t> out;
  for (const float axis : positions) { Append(out, axis); }
  for (const uint32_t index : indices) { Append(out, index); }
  for (int vertex = 0; vertex < 3; ++vertex) {
    for (int channel = 0; channel < 3; ++channel) {
      Append(out, (vertex == 2 && channel == 1) ? value : 0.5f);
    }
  }
  return out;
}

Accessor Spelled(ComponentType component, ElementType element, bool normalized) {
  Accessor accessor;
  accessor.Component = component;
  accessor.Element = element;
  accessor.Normalized = normalized;
  return accessor;
}

} // namespace

int main() {
  using namespace outshine::Test;
  Covers("board:1193");

  /* THE SIX CELLS, AND THE CLAIM IS THAT THEY ARE ONE ANSWER. */
  for (const Cell &cell : kCells) {
    const size_t components = std::string(cell.Element) == "VEC4" ? 4u : 3u;
    Document file;
    Subject subject;
    std::string why;
    const bool read = Reads(cell, file, subject, why);
    CHECK(read, "a COLOR_0 accessor the format permits is read");
    if (!read) {
      Note(why.c_str());
      continue;
    }
    CHECK(subject.HasColour(), "the subject carries a vertex colour run");
    CHECK(subject.Parts().size() == 1 && subject.Parts()[0].HasColour,
          "and the part says so, per primitive rather than per subject");
    CHECK(subject.Colours().size() == subject.VertexCount() * 4,
          "the run is four wide whatever the accessor was, because the alpha multiplies base "
          "colour's alpha and therefore reaches alphaMode");
    if (subject.Colours().size() != subject.VertexCount() * 4) { continue; }
    /* THE CELL'S OWN LATTICE. A float accessor carries the nearest f32 to 128/255 and a normalized
     * integer one carries the rational divided in double; both are the same colour and neither is
     * the other's bits. */
    const double half = cell.ComponentCode == 5126 ? (double)(float)kHalf : kHalf;
    const double alpha = cell.ComponentCode == 5126 ? (double)(float)kAlpha : kAlpha;
    const double expected[3][4] = {{kZero, half, kOne, components == 4 ? alpha : 1.0},
                                   {half, kOne, kZero, components == 4 ? alpha : 1.0},
                                   {kOne, kZero, half, components == 4 ? alpha : 1.0}};
    bool exact = true;
    bool endpoints = true;
    for (size_t vertex = 0; vertex < 3; ++vertex) {
      for (size_t channel = 0; channel < 4; ++channel) {
        const double read = subject.Colours()[vertex * 4 + channel];
        exact = exact && read == expected[vertex][channel];
        if (expected[vertex][channel] == 0.0 || expected[vertex][channel] == 1.0) {
          endpoints = endpoints && read == expected[vertex][channel];
        }
      }
    }
    CHECK(exact, "every cell decodes to its own lattice's exact value, with no transfer applied");
    CHECK(endpoints, "and 0 and 1 come out of all six bit for bit, so the six agree wherever the "
                     "two lattices can");
    CHECK(std::fabs(subject.Colours()[1] - kHalf) <= 0.5 * 5.9604644775390625e-08,
          "and where they cannot, the six differ by at most half an f32 ulp -- a representation, "
          "not a decode");
    Note(cell.Name);
    Note("  the middle colour, as read", subject.Colours()[1], "linear");
    Note("  its distance from 128/255", std::fabs(subject.Colours()[1] - kHalf), "linear");
  }

  /* THE VALUE IS LINEAR, AND THE READING THAT WOULD BE WRONG IS NAMED. */
  {
    Document file;
    Subject subject;
    std::string why;
    CHECK(Reads(kCells[2], file, subject, why),
          "the unsigned byte cell -- the one an sRGB-encoded colour looks like -- is read");
    const double decoded = LinearFromSrgb(kHalf);
    CHECK(subject.Colours().size() > 1 && subject.Colours()[1] == kHalf,
          /* The unsigned byte cell divides in double, so this one IS exact. */
          "an unsigned byte 128 is 128/255 LINEAR and never the sRGB decode of it: COLOR_0 is an "
          "additional linear multiplier, and decoding it here is a plausible wrong picture");
    Note("unsigned byte 128 as read", kHalf, "linear");
    Note("what an sRGB decode would have made of it", decoded, "linear");
    Note("what that would cost the picture, at this one value",
         255.0 * (1.055 * std::pow(kHalf, 1.0 / 2.4) - 0.055) -
             255.0 * (1.055 * std::pow(decoded, 1.0 / 2.4) - 0.055),
         "display codes");
  }

  /* THE SIX ARE THE WHOLE SET, and a seventh spelling is a refusal that names what it refused. */
  {
    size_t components = 0;
    std::string why;
    CHECK(VertexColourComponents(Spelled(ComponentType::Float32, ElementType::Vec3, false),
                                 components, why) &&
              components == 3,
          "float VEC3 is three components");
    CHECK(VertexColourComponents(Spelled(ComponentType::UInt16, ElementType::Vec4, true),
                                 components, why) &&
              components == 4,
          "unsigned short normalized VEC4 is four");
    CHECK(!VertexColourComponents(Spelled(ComponentType::UInt8, ElementType::Vec4, false),
                                  components, why),
          "an unnormalized unsigned byte is refused: those bytes decode to 0..255 and would "
          "multiply base colour by two orders of magnitude");
    Note(why.c_str());
    CHECK(!VertexColourComponents(Spelled(ComponentType::UInt32, ElementType::Vec4, true),
                                  components, why),
          "an unsigned int is refused whatever it says about being normalized");
    CHECK(!VertexColourComponents(Spelled(ComponentType::Float32, ElementType::Vec2, false),
                                  components, why),
          "a VEC2 is refused: a colour is three or four components");
    Note(why.c_str());
    CHECK(!VertexColourComponents(Spelled(ComponentType::Float32, ElementType::Scalar, false),
                                  components, why),
          "and a scalar is refused for the same reason");
  }

  /* OUT OF RANGE IS A NAMED REFUSAL AND NOT A CLAMP. */
  for (const float value : {1.5f, -0.25f}) {
    const std::vector<uint8_t> glb =
        Glb(Text(kCells[0], 36), OutOfRangeBinary(value));
    Document file;
    Subject subject;
    CHECK(file.Read({glb.data(), glb.size()}, "out-of-range.glb"),
          "a file whose COLOR_0 leaves [0, 1] is still a readable document");
    CHECK(!subject.Build(file),
          "and the flatten refuses it rather than clamping: the format requires the ASSET to be in "
          "range, so a file outside it is malformed and repairing it here would hide the defect "
          "inside a picture that looks authored");
    CHECK(subject.Error().find("COLOR_0 of vertex 2") != std::string::npos &&
              subject.Error().find("channel 1") != std::string::npos,
          "and the refusal names the vertex and the channel it refused");
    CHECK(subject.Colours().empty() && subject.PositionsM().empty(),
          "a refused subject carries no run at all");
    Note(subject.Error().c_str());
  }

  /* A PRODUCED RUN IS HELD TO THE SAME RANGE, because [0, 1] is a property of the quantity and not
   * of where it came from. */
  {
    const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const float good[12] = {0, 0, 0, 1, 1, 1, 1, 1, 0.5f, 0.5f, 0.25f, 1};
    const float bad[12] = {0, 0, 0, 1, 1, 1, 1, 1, 0.5f, 0.5f, 1.5f, 1};
    const uint32_t indices[3] = {0, 1, 2};
    for (const float *run : {good, bad}) {
      Piece piece;
      piece.PositionsM = {positions, 9};
      piece.Colours = {run, 12};
      piece.Indices = {indices, 3};
      Assembly what;
      what.Pieces = {&piece, 1};
      Subject subject;
      const bool assembled = subject.Assemble(what);
      CHECK(assembled == (run == good),
            "a produced vertex colour run is accepted in range and refused out of it");
      if (!assembled) { Note(subject.Error().c_str()); }
      if (assembled) {
        CHECK(subject.Colours().size() == 12 && subject.Parts()[0].HasColour,
              "and an accepted one reaches the subject as its own run");
      }
    }
  }

  /* AND THE OTHER OPERAND: glTF's DEFAULT MATERIAL IS NOT THIS ENGINE'S DEFAULT SURFACE. */
  {
    const outshine::Material format = outshine::Gltf::DefaultMaterial();
    const outshine::Material engine;
    CHECK(format.BaseColour[0] == 1.0f && format.BaseColour[1] == 1.0f &&
              format.BaseColour[2] == 1.0f && format.BaseColour[3] == 1.0f,
          "glTF's default material is baseColorFactor [1,1,1,1], which is the multiplicative "
          "identity COLOR_0 is multiplied into when a primitive names no material");
    CHECK(format.Metalness == 1.0f && format.Roughness == 1.0f,
          "and metallicFactor 1, roughnessFactor 1, which are the format's defaults and not this "
          "engine's");
    CHECK(format.BaseColour[0] != engine.BaseColour[0],
          "the engine's own default surface is a different colour, so a consumer that substituted "
          "it for a file's missing material would halve every channel of the body and read as the "
          "vertex colour being wrong");
    Note("the engine's default base colour", (double)engine.BaseColour[0], "linear");
  }

  return Report();
}
