/* `KHR_materials_variants` IS A SELECTION AND IT IS SPENT IN THE FLATTEN (board:1188).
 *
 * THE CLAIM: a declared variant decides which material index a PART wears, and nothing below the
 * flatten can tell that the file carried the extension at all. `Part::Material` is what the surface
 * table and then the draw list's material slot are built from, so a selection that lands here has
 * landed everywhere -- and a consumer cannot forget to apply it, because there is no second place
 * the material of a part is decided.
 *
 * WHY IT IS CHECKED AS NUMBERS AND NOT ONLY AS A PICTURE: a selection that silently did nothing
 * renders the primitive's own material, which for `MaterialsVariantsShoe` is exactly what its
 * `midnight` variant asks for. That reads as a correct picture. The render pair
 * (the two `MaterialsVariantsShoe` cases) is what catches it in pixels; this is what
 * catches it in the one field it is written into.
 *
 * AND THE THREE REFUSALS, EACH THE SAME INVARIANT AT A DIFFERENT LEVEL: a name the file does not
 * declare, a primitive mapped twice by one variant, and two variants of one name. None of the three
 * has a defensible answer, so none of them gets one. */
#include <cstdint>
#include <string>
#include <vector>

#include "Check.h"
#include "Document.h"
#include "Glb.h"
#include "Span.h"
#include "Subject.h"
#include "Variant.h"

namespace {

using outshine::Gltf::Document;
using outshine::Gltf::Subject;
using outshine::Gltf::VariantSelection;

/* One mesh of two triangles over three materials, in `MaterialsVariantsShoe`'s own shape: the FIRST
 * primitive is remapped by both variants and the SECOND by neither, which is the extension's
 * per-primitive fall-back written into the fixture rather than asserted about it. The mesh's own
 * materials are 0 and 2, so no variant's answer is the file's answer and no assertion below can
 * hold by coincidence. */
std::string Text(const std::string &variants, const std::string &mappings) {
  return std::string("{\"asset\":{\"version\":\"2.0\"},") + variants +
         "\"materials\":[{\"name\":\"zero\"},{\"name\":\"one\"},{\"name\":\"two\"}],"
         "\"meshes\":[{\"primitives\":["
         "{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0" + mappings + "},"
         "{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":2}]}],"
         "\"nodes\":[{\"mesh\":0}],\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
         "\"accessors\":["
         "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
         "\"min\":[0,0,0],\"max\":[1,1,0]},"
         "{\"bufferView\":1,\"componentType\":5125,\"count\":3,\"type\":\"SCALAR\"}],"
         "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
         "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":12}],"
         "\"buffers\":[{\"byteLength\":48}]}";
}

const char *const kTwoVariants =
    "\"extensions\":{\"KHR_materials_variants\":{\"variants\":[{\"name\":\"beach\"},"
    "{\"name\":\"street\"}]}},";

/* `beach` puts material 1 on the first primitive and `street` puts material 2 on it. */
const char *const kMapped =
    ",\"extensions\":{\"KHR_materials_variants\":{\"mappings\":["
    "{\"material\":1,\"variants\":[0]},{\"material\":2,\"variants\":[1]}]}}";

std::vector<uint8_t> Binary() {
  const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const uint32_t indices[3] = {0, 1, 2};
  std::vector<uint8_t> out;
  for (const float value : positions) { outshine::Test::Append(out, value); }
  for (const uint32_t value : indices) { outshine::Test::Append(out, value); }
  return out;
}

bool Reads(const std::string &variants, const std::string &mappings, Document &into) {
  const std::vector<uint8_t> glb = outshine::Test::Glb(Text(variants, mappings), Binary());
  return into.Read(outshine::Span<const uint8_t>(glb.data(), glb.size()), "");
}

/* The two parts' materials under one declaration, as the pair they are compared as. */
struct Worn {
  int First = -2;
  int Second = -2;
};

Worn WornUnder(const Document &file, const VariantSelection &variant, Subject &into) {
  if (!into.Build(file, variant)) { return Worn(); }
  if (into.Parts().size() != 2) { return Worn(); }
  return Worn{into.Parts()[0].Material, into.Parts()[1].Material};
}

} // namespace

int main() {
  using namespace outshine::Test;
  Covers("board:1188");

  Document file;
  CHECK(Reads(kTwoVariants, kMapped, file),
        "a file declaring two material variants and a primitive that maps both is read");
  CHECK(file.Variants().size() == 2 && file.Variants()[0] == "beach" &&
            file.Variants()[1] == "street",
        "and the document's variant table is the file's names in the file's order");
  CHECK(Document::Honours("KHR_materials_variants"),
        "and the reader claims the extension, so a file that REQUIRES it loads");

  Subject subject;
  const Worn vanilla = WornUnder(file, VariantSelection(), subject);
  CHECK(vanilla.First == 0 && vanilla.Second == 2,
        "with no variant selected every part wears the material its primitive names -- the "
        "extension's own rule, and the same path as a file that carries no variants at all");

  const Worn beach = WornUnder(file, VariantSelection("beach"), subject);
  CHECK(beach.First == 1,
        "the declared variant decides which material index the part wears, and that is the whole "
        "of the extension: the mapping is gone by the time anything downstream sees the part");
  CHECK(beach.Second == 2,
        "and a primitive no mapping of that variant names keeps its own material -- per primitive, "
        "which is what the format says and not per subject");

  const Worn street = WornUnder(file, VariantSelection("street"), subject);
  CHECK(street.First == 2 && street.Second == 2,
        "a second variant of the same file selects a second material, so the selection is a "
        "function of the declaration and not of the file's order");
  CHECK(beach.First != vanilla.First && street.First != vanilla.First,
        "and NEITHER selection is the file's own material, so a selection that silently did "
        "nothing fails here rather than passing on a coincidence");

  /* THE FIRST REFUSAL: a name the file does not declare. */
  Subject refused;
  CHECK(!refused.Build(file, VariantSelection("midnight")),
        "a variant name the file does not declare is a REFUSAL and never the default, never the "
        "first: the wrong material is a picture nobody can tell from an authored one");
  CHECK(refused.Error().find("midnight") != std::string::npos,
        "and the sentence names what was asked for");
  CHECK(refused.Error().find("beach") != std::string::npos &&
            refused.Error().find("street") != std::string::npos,
        "and what the file carries instead, so both sides of the disagreement are in it");
  CHECK(refused.Parts().empty(),
        "and nothing is drawn in its place");

  /* THE SECOND: one variant mapped twice by one primitive, which the extension forbids in as many
   * words -- "across the entire mappings array, each variant index must be used no more than one
   * time" -- and which has no answer because the file states two. */
  Document twice;
  CHECK(!Reads(kTwoVariants,
               ",\"extensions\":{\"KHR_materials_variants\":{\"mappings\":["
               "{\"material\":1,\"variants\":[0]},{\"material\":2,\"variants\":[0]}]}}",
               twice),
        "a primitive that maps one variant to two materials is refused AT READ TIME, because the "
        "file has stated two answers to one question and neither is more correct");
  CHECK(twice.Error().find("'beach' to material 1 and to material 2") != std::string::npos,
        "and the sentence names the variant and both materials it was given");

  /* THE THIRD: two variants of one name, which is the same defect one level up -- a declaration
   * naming that variant would select two of them. */
  Document ambiguous;
  CHECK(!Reads("\"extensions\":{\"KHR_materials_variants\":{\"variants\":[{\"name\":\"beach\"},"
               "{\"name\":\"beach\"}]}},",
               kMapped, ambiguous),
        "two variants of one name are refused, because a selection is BY NAME and that name would "
        "select two");

  /* AND THE ORDINARY FILE IS UNTOUCHED: no variants, no mappings, no selection, one path. */
  Document plain;
  CHECK(Reads("", "", plain), "a file carrying no variants at all is read");
  CHECK(plain.Variants().empty(), "and declares none");
  Subject unselected;
  const Worn own = WornUnder(plain, VariantSelection(), unselected);
  CHECK(own.First == 0 && own.Second == 2,
        "and flattens exactly as it did before the extension existed, which is what makes the "
        "default not a special case");
  Subject named;
  CHECK(!named.Build(plain, VariantSelection("beach")),
        "while a declaration that selects a variant of a file carrying none is refused too");
  CHECK(named.Error().find("no KHR_materials_variants at all") != std::string::npos,
        "and the sentence says which side is empty");

  return Report();
}
