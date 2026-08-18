#include "Document.h"

#include <cstring>
#include <fstream>

#include "Json.h"

/* THE FORMAT IS LITTLE-ENDIAN AND SO IS EVERY TARGET THIS ENGINE HAS. Said as a compile error rather
 * than as a comment, because a big-endian port that silently byte-swapped every accessor would be
 * discovered by looking at a picture. */
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "glTF buffers are little-endian");

namespace outshine::Gltf {
namespace {

constexpr uint32_t kGlbMagic = 0x46546C67;      /* 'glTF' */
constexpr uint32_t kChunkJson = 0x4E4F534A;     /* 'JSON' */
constexpr uint32_t kChunkBinary = 0x004E4942;   /* 'BIN\0' */
constexpr uint32_t kGlbVersion = 2;

uint32_t LittleWord(const uint8_t *at) {
  uint32_t word = 0;
  std::memcpy(&word, at, sizeof word);
  return word;
}

bool KnownComponent(int raw, ComponentType &out) {
  switch (raw) {
  case 5120: out = ComponentType::Int8; return true;
  case 5121: out = ComponentType::UInt8; return true;
  case 5122: out = ComponentType::Int16; return true;
  case 5123: out = ComponentType::UInt16; return true;
  case 5125: out = ComponentType::UInt32; return true;
  case 5126: out = ComponentType::Float32; return true;
  default: return false;
  }
}

bool KnownElement(const std::string &raw, ElementType &out) {
  if (raw == "SCALAR") { out = ElementType::Scalar; return true; }
  if (raw == "VEC2") { out = ElementType::Vec2; return true; }
  if (raw == "VEC3") { out = ElementType::Vec3; return true; }
  if (raw == "VEC4") { out = ElementType::Vec4; return true; }
  if (raw == "MAT2") { out = ElementType::Mat2; return true; }
  if (raw == "MAT3") { out = ElementType::Mat3; return true; }
  if (raw == "MAT4") { out = ElementType::Mat4; return true; }
  return false;
}

bool KnownMode(int raw, PrimitiveMode &out) {
  if (raw < 0 || raw > 6) { return false; }
  out = static_cast<PrimitiveMode>(raw);
  return true;
}

double Component(const uint8_t *at, ComponentType component) {
  switch (component) {
  case ComponentType::Int8: {
    int8_t v = 0;
    std::memcpy(&v, at, sizeof v);
    return v;
  }
  case ComponentType::UInt8: return *at;
  case ComponentType::Int16: {
    int16_t v = 0;
    std::memcpy(&v, at, sizeof v);
    return v;
  }
  case ComponentType::UInt16: {
    uint16_t v = 0;
    std::memcpy(&v, at, sizeof v);
    return v;
  }
  case ComponentType::UInt32: {
    uint32_t v = 0;
    std::memcpy(&v, at, sizeof v);
    return v;
  }
  case ComponentType::Float32: {
    float v = 0;
    std::memcpy(&v, at, sizeof v);
    return v;
  }
  }
  return 0;
}

/* The format's own normalisation, and the asymmetry is the format's too: a signed component divides
 * by its positive maximum and clamps at -1, so -128 and -127 both mean -1. */
double Normalise(double raw, ComponentType component) {
  switch (component) {
  case ComponentType::Int8: return raw < -127.0 ? -1.0 : raw / 127.0;
  case ComponentType::UInt8: return raw / 255.0;
  case ComponentType::Int16: return raw < -32767.0 ? -1.0 : raw / 32767.0;
  case ComponentType::UInt16: return raw / 65535.0;
  case ComponentType::UInt32: return raw / 4294967295.0;
  case ComponentType::Float32: return raw;
  }
  return raw;
}

std::string DirectoryOf(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  return (slash == std::string::npos) ? std::string() : path.substr(0, slash + 1);
}

std::string Number(size_t value) { return std::to_string(value); }

/* A URI IS PERCENT-DECODED BEFORE IT REACHES THE FILESYSTEM. glTF requires the writer to
 * percent-encode reserved characters (`Specification.adoc:550`), which makes decoding them a
 * reader's obligation and not a convenience: `Box With Spaces` names its images `Box%20With%20Spaces
 * .png` and its buffer with literal spaces, so a reader that concatenates opens the buffer and
 * fails at the first texture. An incomplete or non-hex escape at the end is left verbatim -- the
 * alternative is reading past the string. */
int HexDigit(char c) {
  if (c >= '0' && c <= '9') { return c - '0'; }
  if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
  if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
  return -1;
}

std::string PercentDecoded(const std::string &uri) {
  std::string out;
  out.reserve(uri.size());
  for (size_t i = 0; i < uri.size(); ++i) {
    const int high = (uri[i] == '%' && i + 2 < uri.size()) ? HexDigit(uri[i + 1]) : -1;
    const int low = (high >= 0) ? HexDigit(uri[i + 2]) : -1;
    if (low < 0) {
      out.push_back(uri[i]);
      continue;
    }
    out.push_back(static_cast<char>(high * 16 + low));
    i += 2;
  }
  return out;
}

bool KnownWrap(int raw, Wrap &out) {
  switch (raw) {
  case 33071: out = Wrap::ClampToEdge; return true;
  case 33648: out = Wrap::MirroredRepeat; return true;
  case 10497: out = Wrap::Repeat; return true;
  default: return false;
  }
}

/* `magFilter` HAS TWO LEGAL VALUES AND NO MIP HALF, because magnification reads one level by
 * definition. An undeclared one is the client's choice and this reader takes LINEAR. */
[[nodiscard]] bool KnownMagFilter(int raw, Filter &out) {
  switch (raw) {
  case 9728: out = Filter::Nearest; return true;
  case 9729: out = Filter::Linear; return true;
  default: return false;
  }
}

/* `minFilter` PACKS TWO ANSWERS INTO ONE INTEGER and this reader used to give one of them
 * (board:1134). It read `9728 is NEAREST and everything else in the format's set is a LINEAR base`,
 * which is false for the two values that are a NEAREST base WITH mipmapping -- 9984 and 9986 -- and
 * those are not exotic: `NormalTangentTest` and `TextureSettingsTest` both declare 9986, so two
 * corpus subjects were being filtered linearly under minification where the file asks for point
 * sampling. The mip half was lost entirely, decided instead by a constant at the sampler.
 *
 * AN UNDECLARED `minFilter` IS THE CLIENT'S CHOICE and this reader takes a linear base with linear
 * levels -- what the sampler did unconditionally before, so absence keeps its behaviour and only a
 * DECLARATION changes it. AN UNKNOWN ONE IS REFUSED, like a wrap mode: the previous silent mapping to
 * LINEAR is how a file could ask for something this engine does not implement and be told nothing. */
[[nodiscard]] bool KnownMinFilter(int raw, Filter &base, MipFilter &mip) {
  switch (raw) {
  case 9728: base = Filter::Nearest; mip = MipFilter::None; return true;
  case 9729: base = Filter::Linear; mip = MipFilter::None; return true;
  case 9984: base = Filter::Nearest; mip = MipFilter::Nearest; return true;
  case 9985: base = Filter::Linear; mip = MipFilter::Nearest; return true;
  case 9986: base = Filter::Nearest; mip = MipFilter::Linear; return true;
  case 9987: base = Filter::Linear; mip = MipFilter::Linear; return true;
  default: return false;
  }
}

/* WHAT THIS READER IMPLEMENTS WELL ENOUGH TO CLAIM. An extension is added here in the round its
 * behaviour is built, so `extensionsRequired` naming anything else is a refusal -- a list seeded
 * with names nobody implemented would be the silent-acceptance defect wearing a table.
 *
 * A DATA EXTENSION -- one that adds numbers to a row rather than an arm to the renderer -- IS BUILT
 * THE WAY `KHR_texture_transform` WAS (board:1177), and the three decisions are stated where they are
 * used rather than restated here: its values are composed into their final form AT THE READER
 * (`core/UvTransform.h`), its defaults are the IDENTITY of whatever the consumer does with them, so
 * absence and presence-with-defaults are one computation with no branch and no pipeline permutation
 * (`board:1156`), and PRESENCE IS THE VALUES THEMSELVES -- there is no `Has...` flag anywhere in this
 * reader that can disagree with the numbers beside it.
 *
 * A SELECTION EXTENSION IS THE OTHER SHAPE AND IT IS NOT BUILT THAT WAY (board:1188).
 * `KHR_materials_variants` carries no number for a material row -- it is a MAPPING from a variant to
 * a material, per primitive -- so putting it in the row would put a table inside the thing it
 * selects. It is read into the primitive, resolved into the material a part wears while the subject
 * is flattened, and is therefore gone before a draw list exists: the render path never learns that
 * variants are a thing, and no fragment arm, pipeline or interpolant is added by it. */
constexpr const char *const kHonouredExtensions[] = {"KHR_lights_punctual",
                                                     "KHR_materials_sheen",
                                                     "KHR_mesh_quantization",
                                                     "KHR_node_visibility",
                                                     "KHR_materials_emissive_strength",
                                                     "KHR_materials_ior",
                                                     "KHR_materials_specular",
                                                     "KHR_materials_unlit",
                                                     "KHR_materials_variants",
                                                     "KHR_texture_transform", nullptr};

constexpr const char *kLightsPunctual = "KHR_lights_punctual";
constexpr const char *kEmissiveStrength = "KHR_materials_emissive_strength";
constexpr const char *kIor = "KHR_materials_ior";
constexpr const char *kSpecular = "KHR_materials_specular";
constexpr const char *kSheen = "KHR_materials_sheen";
constexpr const char *kUnlit = "KHR_materials_unlit";
constexpr const char *kMaterialsVariants = "KHR_materials_variants";
constexpr const char *kTextureTransform = "KHR_texture_transform";

/* A DECLARED PAIR OF NUMBERS, OR A REFUSAL NAMING WHICH PROPERTY WAS NOT ONE. `Num(def)` answers the
 * default for a string, an object and a null alike, so a present-but-wrong value would otherwise be
 * read as the extension's default and ship as a picture nobody could trace back to the file -- the
 * same silent success `emissiveStrength` refuses one function above. */
bool ReadUvPair(const Json::Ref &declared, const char *property, double out[2], std::string &why) {
  if (!declared.Valid()) { return true; }
  if (declared.GetKind() != Json::Kind::Array || declared.Size() != 2) {
    why = std::string("declares a KHR_texture_transform ") + property +
          " that is not an array of two numbers";
    return false;
  }
  for (size_t axis = 0; axis < 2; ++axis) {
    if (declared[axis].GetKind() != Json::Kind::Number) {
      why = std::string("declares a KHR_texture_transform ") + property + " whose component " +
            Number(axis) + " is not a number";
      return false;
    }
    out[axis] = declared[axis].Num();
  }
  return true;
}

/* ONE `textureInfo`'s `KHR_texture_transform`, COMPOSED (board:1177). The three properties leave this
 * function as a matrix and never as themselves, so translation x rotation x scale is stated in
 * `core/UvTransform.h` once and has no second spelling; what is here is only the reading and the
 * refusals.
 *
 * `texCoord` OVERWRITES the reference's own set rather than sitting beside it, which is the
 * extension's own rule -- "Overrides the textureInfo texCoord value if supplied" -- so a consumer
 * cannot read the superseded one by mistake. */
bool ReadTextureTransform(const Json::Ref &info, TextureRef &into, std::string &why) {
  const Json::Ref declared = info["extensions"][kTextureTransform];
  if (!declared.Valid()) { return true; }
  if (declared.GetKind() != Json::Kind::Object) {
    why = "declares KHR_texture_transform as something other than an object";
    return false;
  }
  outshine::UvTransformProperties properties;
  if (!ReadUvPair(declared["offset"], "offset", properties.OffsetUv, why)) { return false; }
  if (!ReadUvPair(declared["scale"], "scale", properties.ScaleUv, why)) { return false; }
  const Json::Ref rotation = declared["rotation"];
  if (rotation.Valid()) {
    if (rotation.GetKind() != Json::Kind::Number) {
      why = "declares a KHR_texture_transform rotation that is not a number";
      return false;
    }
    properties.RotationRad = rotation.Num();
  }
  const Json::Ref set = declared["texCoord"];
  if (set.Valid()) {
    if (set.GetKind() != Json::Kind::Number) {
      why = "declares a KHR_texture_transform texCoord that is not a number";
      return false;
    }
    into.TexCoord = set.Int(0);
  }
  into.Transform = outshine::UvTransformOf(properties);
  return true;
}

bool KnownAlphaMode(const std::string &raw, AlphaMode &out) {
  if (raw.empty() || raw == "OPAQUE") { out = AlphaMode::Opaque; return true; }
  if (raw == "MASK") { out = AlphaMode::Masked; return true; }
  if (raw == "BLEND") { out = AlphaMode::Blended; return true; }
  return false;
}

} // namespace

/* WHICH ACCESSOR AN ATTRIBUTE MAY BE, and it is the format's own table rather than this reader's
 * taste (`Specification.adoc`, mesh primitive attribute semantics). Base glTF 2.0 permits:
 *
 *   POSITION   VEC3  float
 *   NORMAL     VEC3  float
 *   TANGENT    VEC4  float
 *   TEXCOORD_n VEC2  float | unsigned byte normalized | unsigned short normalized
 *   COLOR_n    VEC3 or VEC4  float | unsigned byte normalized | unsigned short normalized
 *   JOINTS_n   VEC4  unsigned byte | unsigned short
 *   WEIGHTS_n  VEC4  float | unsigned byte normalized | unsigned short normalized
 *
 * `KHR_mesh_quantization` WIDENS EXACTLY FOUR OF THOSE ROWS and nothing else. Its own words: *because
 * the extension does not provide a way to specify both FLOAT and quantized versions of the data,
 * files that use the extension must specify it in extensionsRequired* -- so it is never optional and
 * the widened set is gated on the file declaring it.
 *
 * WHY THE TABLE IS ENFORCED AT ALL. Until now every combination decoded, which made honouring the
 * extension mean nothing: a reader that already accepted a quantised POSITION was not implementing
 * the extension, it was failing to check. The check IS the behaviour here (board:1384), and
 * [MEASURED] over the 148 models at the pin it refuses none of them -- one declares the extension and
 * uses it, and no model uses a non-base combination without declaring it. */
struct AttributeShape {
  ElementType Element;
  ComponentType Component;
  bool Normalized;
};

bool ShapeAllowed(const std::string &semantic, const AttributeShape &shape, bool quantised) {
  const bool f32 = shape.Component == ComponentType::Float32;
  const bool u8 = shape.Component == ComponentType::UInt8;
  const bool u16 = shape.Component == ComponentType::UInt16;
  const bool i8 = shape.Component == ComponentType::Int8;
  const bool i16 = shape.Component == ComponentType::Int16;
  const bool vec2 = shape.Element == ElementType::Vec2;
  const bool vec3 = shape.Element == ElementType::Vec3;
  const bool vec4 = shape.Element == ElementType::Vec4;
  const bool norm = shape.Normalized;

  if (semantic == "POSITION") {
    if (vec3 && f32 && !norm) { return true; }
    return quantised && vec3 && (i8 || u8 || i16 || u16);
  }
  if (semantic == "NORMAL") {
    if (vec3 && f32 && !norm) { return true; }
    return quantised && vec3 && norm && (i8 || i16);
  }
  if (semantic == "TANGENT") {
    if (vec4 && f32 && !norm) { return true; }
    return quantised && vec4 && norm && (i8 || i16);
  }
  if (semantic.rfind("TEXCOORD_", 0) == 0) {
    if (vec2 && ((f32 && !norm) || ((u8 || u16) && norm))) { return true; }
    return quantised && vec2 && (i8 || (u8 && !norm) || i16 || (u16 && !norm));
  }
  if (semantic.rfind("COLOR_", 0) == 0) {
    return (vec3 || vec4) && ((f32 && !norm) || ((u8 || u16) && norm));
  }
  if (semantic.rfind("JOINTS_", 0) == 0) { return vec4 && (u8 || u16) && !norm; }
  if (semantic.rfind("WEIGHTS_", 0) == 0) {
    return vec4 && ((f32 && !norm) || ((u8 || u16) && norm));
  }
  /* AN APPLICATION-SPECIFIC SEMANTIC IS THE FILE'S BUSINESS AND NOT THIS TABLE'S: the format reserves
   * every name it defines and leaves `_`-prefixed ones to the writer. */
  return true;
}

bool Document::Honours(const std::string &extension) {
  for (const char *const known : kHonouredExtensions) {
    if (known != nullptr && extension == known) { return true; }
  }
  return false;
}

bool Document::Refuse(const std::string &why) {
  Error_ = Path_.empty() ? why : Path_ + ": " + why;
  return false;
}

bool Document::ReadFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    Path_ = path;
    return Refuse("cannot be opened");
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
  return Read({bytes.data(), bytes.size()}, path);
}

bool Document::Read(Span<const uint8_t> whole, const std::string &path) {
  *this = Document();
  Path_ = path;
  if (whole.Empty()) { return Refuse("is empty"); }
  const uint8_t *const bytes = whole.Data();
  const size_t length = whole.Size();

  /* THE CONTAINER IS DECIDED BY THE BYTES. A .glb named .gltf is still a .glb, and a reader that
   * trusted the extension would report a JSON parse failure at byte 0 for a file that is fine. */
  if (length >= 12 && LittleWord(bytes) == kGlbMagic) {
    const uint32_t version = LittleWord(bytes + 4);
    const uint32_t declared = LittleWord(bytes + 8);
    if (version != kGlbVersion) {
      return Refuse("is a GLB of version " + Number(version) + ", and this reader is glTF 2.0");
    }
    if (declared > length) {
      return Refuse("declares " + Number(declared) + " bytes and " + Number(length) + " are present");
    }
    const uint8_t *jsonChunk = nullptr;
    size_t jsonLength = 0;
    const uint8_t *binaryChunk = nullptr;
    size_t binaryLength = 0;
    size_t at = 12;
    while (at + 8 <= declared) {
      const uint32_t chunkLength = LittleWord(bytes + at);
      const uint32_t chunkType = LittleWord(bytes + at + 4);
      at += 8;
      if (chunkLength > declared - at) {
        return Refuse("has a GLB chunk of " + Number(chunkLength) + " bytes that runs past the file");
      }
      if (chunkType == kChunkJson && jsonChunk == nullptr) {
        jsonChunk = bytes + at;
        jsonLength = chunkLength;
      } else if (chunkType == kChunkBinary && binaryChunk == nullptr) {
        binaryChunk = bytes + at;
        binaryLength = chunkLength;
      }
      /* Chunks are 4-byte aligned; an unknown type is skipped, which is what the format asks of a
       * reader so a future chunk does not make an otherwise readable file unreadable. */
      at += (chunkLength + 3) & ~size_t{3};
    }
    if (jsonChunk == nullptr) { return Refuse("is a GLB with no JSON chunk"); }
    return ReadJson(reinterpret_cast<const char *>(jsonChunk), jsonLength, binaryChunk,
                    binaryLength);
  }

  return ReadJson(reinterpret_cast<const char *>(bytes), length, nullptr, 0);
}

bool Document::ResolveBuffers(const Json &json, const uint8_t *binaryChunk, size_t binaryLength) {
  const Json::Ref buffers = json.Root()["buffers"];
  const std::string directory = DirectoryOf(Path_);
  for (size_t i = 0; i < buffers.Size(); ++i) {
    const Json::Ref buffer = buffers[i];
    const size_t declared = static_cast<size_t>(buffer["byteLength"].Num(0.0));
    const std::string uri = PercentDecoded(buffer["uri"].Str(""));
    std::vector<uint8_t> bytes;
    if (uri.empty()) {
      if (binaryChunk == nullptr) {
        return Refuse("buffer " + Number(i) + " has no uri and the file carries no binary chunk");
      }
      bytes.assign(binaryChunk, binaryChunk + binaryLength);
    } else if (uri.rfind("data:", 0) == 0) {
      /* A named refusal, not a gap: an embedded base64 buffer is a container this round did not
       * build, and a file that carries one must say so rather than read as empty. */
      return Refuse("buffer " + Number(i) + " is a data: URI, which this reader does not decode");
    } else {
      std::ifstream file(directory + uri, std::ios::binary);
      if (!file) { return Refuse("buffer " + Number(i) + " names " + uri + ", which cannot be opened"); }
      bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    if (bytes.size() < declared) {
      return Refuse("buffer " + Number(i) + " declares " + Number(declared) + " bytes and " +
                    Number(bytes.size()) + " are present");
    }
    bytes.resize(declared);
    Buffers_.push_back(std::move(bytes));
  }
  return true;
}

bool Document::ReadJson(const char *text, size_t length, const uint8_t *binaryChunk,
                        size_t binaryLength) {
  Json json;
  if (!json.Parse(text, length)) {
    return Refuse("is not valid JSON, stopping at byte " + Number(json.StoppedAt()));
  }
  const Json::Ref root = json.Root();
  if (root.GetKind() != Json::Kind::Object) { return Refuse("is not a glTF object"); }

  Version_ = root["asset"]["version"].Str("");
  if (Version_.rfind("2.", 0) != 0) {
    return Refuse("declares asset.version '" + Version_ + "', and this reader is glTF 2.0");
  }

  /* THE TWO DOORS A FILE STATES ITS OWN UNREADABILITY THROUGH, and both were open. `minVersion` is
   * the writer saying "below this the file cannot be read at all", and `extensionsRequired` the
   * same statement per extension -- a quantized or Draco-compressed file whose extension is ignored
   * reads as plausible numbers in the wrong units rather than as a refusal. Neither is a warning:
   * the format says a client that cannot honour them must not load the asset. */
  MinVersion_ = root["asset"]["minVersion"].Str("");
  if (!MinVersion_.empty() && MinVersion_.rfind("2.0", 0) != 0) {
    return Refuse("declares asset.minVersion '" + MinVersion_ +
                  "', which is above the glTF 2.0 this reader is");
  }
  const Json::Ref required = root["extensionsRequired"];
  for (size_t i = 0; i < required.Size(); ++i) {
    Required_.push_back(required[i].Str(""));
  }
  for (const std::string &extension : Required_) {
    if (extension == "KHR_mesh_quantization") { Quantised_ = true; }
    if (!Honours(extension)) {
      return Refuse("requires extension '" + extension + "', which this reader does not implement");
    }
  }

  if (!ResolveBuffers(json, binaryChunk, binaryLength)) { return false; }

  const Json::Ref views = root["bufferViews"];
  for (size_t i = 0; i < views.Size(); ++i) {
    const Json::Ref declaration = views[i];
    BufferView view;
    view.Buffer = static_cast<size_t>(declaration["buffer"].Num(0.0));
    view.ByteOffset = static_cast<size_t>(declaration["byteOffset"].Num(0.0));
    view.ByteLength = static_cast<size_t>(declaration["byteLength"].Num(0.0));
    view.ByteStride = static_cast<size_t>(declaration["byteStride"].Num(0.0));
    if (view.Buffer >= Buffers_.size()) {
      return Refuse("bufferView " + Number(i) + " names buffer " + Number(view.Buffer) + " of " +
                    Number(Buffers_.size()));
    }
    const std::vector<uint8_t> &buffer = Buffers_[view.Buffer];
    if (view.ByteOffset > buffer.size() || view.ByteLength > buffer.size() - view.ByteOffset) {
      return Refuse("bufferView " + Number(i) + " spans [" + Number(view.ByteOffset) + ", " +
                    Number(view.ByteOffset + view.ByteLength) + ") of a buffer of " +
                    Number(buffer.size()) + " bytes");
    }
    Views_.push_back(view);
  }

  const Json::Ref accessors = root["accessors"];
  for (size_t i = 0; i < accessors.Size(); ++i) {
    const Json::Ref declaration = accessors[i];
    Accessor accessor;
    const Json::Ref view = declaration["bufferView"];
    accessor.View = view.Valid() ? view.Int(-1) : -1;
    accessor.ByteOffset = static_cast<size_t>(declaration["byteOffset"].Num(0.0));
    accessor.Count = static_cast<size_t>(declaration["count"].Num(0.0));
    accessor.Normalized = declaration["normalized"].Bool(false);
    const int rawComponent = declaration["componentType"].Int(0);
    if (!KnownComponent(rawComponent, accessor.Component)) {
      return Refuse("accessor " + Number(i) + " has componentType " + Number(static_cast<size_t>(rawComponent)) +
                    ", which glTF 2.0 does not define");
    }
    const std::string rawElement = declaration["type"].Str("");
    if (!KnownElement(rawElement, accessor.Element)) {
      return Refuse("accessor " + Number(i) + " has type '" + rawElement + "', which glTF 2.0 does not define");
    }
    if (accessor.Normalized && accessor.Component == ComponentType::Float32) {
      return Refuse("accessor " + Number(i) + " is a normalized float, which glTF 2.0 forbids");
    }
    if (accessor.View >= 0 && static_cast<size_t>(accessor.View) >= Views_.size()) {
      return Refuse("accessor " + Number(i) + " names bufferView " + Number(static_cast<size_t>(accessor.View)) +
                    " of " + Number(Views_.size()));
    }
    for (size_t k = 0; k < declaration["min"].Size(); ++k) {
      accessor.Min.push_back(declaration["min"][k].Num(0.0));
    }
    for (size_t k = 0; k < declaration["max"].Size(); ++k) {
      accessor.Max.push_back(declaration["max"][k].Num(0.0));
    }
    const Json::Ref sparse = declaration["sparse"];
    if (sparse.Valid()) {
      accessor.HasSparse = true;
      accessor.Sparse.Count = static_cast<size_t>(sparse["count"].Num(0.0));
      accessor.Sparse.IndicesBufferView = sparse["indices"]["bufferView"].Int(-1);
      accessor.Sparse.IndicesByteOffset =
          static_cast<size_t>(sparse["indices"]["byteOffset"].Num(0.0));
      if (!KnownComponent(sparse["indices"]["componentType"].Int(0),
                          accessor.Sparse.IndicesComponent)) {
        return Refuse("accessor " + Number(i) + " has a sparse index componentType glTF 2.0 does not define");
      }
      accessor.Sparse.ValuesBufferView = sparse["values"]["bufferView"].Int(-1);
      accessor.Sparse.ValuesByteOffset =
          static_cast<size_t>(sparse["values"]["byteOffset"].Num(0.0));
      if (accessor.Sparse.Count > accessor.Count) {
        return Refuse("accessor " + Number(i) + " overrides " + Number(accessor.Sparse.Count) +
                      " of " + Number(accessor.Count) + " elements");
      }
    }
    Accessors_.push_back(std::move(accessor));
  }

  if (!ReadAppearance(json)) { return false; }
  if (!ReadLights(json)) { return false; }
  if (!ReadVariants(json)) { return false; }

  const Json::Ref meshes = root["meshes"];
  for (size_t i = 0; i < meshes.Size(); ++i) {
    const Json::Ref declaration = meshes[i];
    Mesh mesh;
    mesh.Name = declaration["name"].Str("");
    const Json::Ref weights = declaration["weights"];
    for (size_t w = 0; w < weights.Size(); ++w) { mesh.Weights.push_back(weights[w].Num(0.0)); }
    const Json::Ref primitives = declaration["primitives"];
    for (size_t p = 0; p < primitives.Size(); ++p) {
      const Json::Ref declared = primitives[p];
      Primitive primitive;
      if (!KnownMode(declared["mode"].Valid() ? declared["mode"].Int(4) : 4, primitive.Mode)) {
        return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " has a mode glTF 2.0 does not define");
      }
      primitive.Indices = declared["indices"].Valid() ? declared["indices"].Int(-1) : -1;
      primitive.Material = declared["material"].Valid() ? declared["material"].Int(-1) : -1;
      const Json::Ref attributes = declared["attributes"];
      for (size_t a = 0; a < attributes.Size(); ++a) {
        Attribute attribute;
        attribute.Semantic = attributes.Key(a);
        attribute.Accessor = attributes[a].Int(-1);
        if (attribute.Accessor < 0 || static_cast<size_t>(attribute.Accessor) >= Accessors_.size()) {
          return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " attribute " +
                        attribute.Semantic + " names an accessor the file does not carry");
        }
        const Accessor &carried = Accessors_[static_cast<size_t>(attribute.Accessor)];
        const AttributeShape shape{carried.Element, carried.Component, carried.Normalized};
        if (!ShapeAllowed(attribute.Semantic, shape, Quantised_)) {
          return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " attribute " +
                        attribute.Semantic + " is an accessor shape glTF 2.0 does not permit for it" +
                        (Quantised_ ? ", even with KHR_mesh_quantization"
                                    : " -- KHR_mesh_quantization widens this and the file does not "
                                      "require it"));
        }
        primitive.Attributes.push_back(std::move(attribute));
      }
      /* THE MORPH TARGETS, and every semantic in one is checked against the base attribute it
       * displaces. glTF allows POSITION, NORMAL and TANGENT in a target and nothing else, and a
       * target that names a semantic the primitive does not carry displaces nothing -- both are
       * refused here rather than left for a flatten to skip silently. */
      const Json::Ref targets = declared["targets"];
      for (size_t t = 0; t < targets.Size(); ++t) {
        MorphTarget target;
        const Json::Ref declaredTarget = targets[t];
        for (size_t a = 0; a < declaredTarget.Size(); ++a) {
          Attribute attribute;
          attribute.Semantic = declaredTarget.Key(a);
          attribute.Accessor = declaredTarget[a].Int(-1);
          if (attribute.Semantic != "POSITION" && attribute.Semantic != "NORMAL" &&
              attribute.Semantic != "TANGENT") {
            return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " morph target " +
                          Number(t) + " displaces " + attribute.Semantic +
                          ", and glTF 2.0 states a target carries POSITION, NORMAL or TANGENT");
          }
          if (attribute.Accessor < 0 || static_cast<size_t>(attribute.Accessor) >= Accessors_.size()) {
            return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " morph target " +
                          Number(t) + " names an accessor the file does not carry for " +
                          attribute.Semantic);
          }
          const int base = primitive.Find(attribute.Semantic.c_str());
          if (base < 0) {
            return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " morph target " +
                          Number(t) + " displaces " + attribute.Semantic +
                          " and the primitive carries none, so the delta has nothing to displace");
          }
          if (Accessors_[(size_t)base].Count != Accessors_[(size_t)attribute.Accessor].Count) {
            return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " morph target " +
                          Number(t) + " carries " +
                          Number(Accessors_[(size_t)attribute.Accessor].Count) + " " +
                          attribute.Semantic + " deltas over " +
                          Number(Accessors_[(size_t)base].Count) + " vertices");
          }
          target.Attributes.push_back(std::move(attribute));
        }
        primitive.Targets.push_back(std::move(target));
      }
      if (primitive.Indices >= 0 && static_cast<size_t>(primitive.Indices) >= Accessors_.size()) {
        return Refuse("mesh " + Number(i) + " primitive " + Number(p) +
                      " names an index accessor the file does not carry");
      }
      if (primitive.Material >= 0 && static_cast<size_t>(primitive.Material) >= Materials_.size()) {
        return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " names material " +
                      Number(static_cast<size_t>(primitive.Material)) + " of " +
                      Number(Materials_.size()));
      }
      const Json::Ref variants = declared["extensions"][kMaterialsVariants];
      if (variants.Valid() && !ReadVariantMappings(variants, i, p, primitive)) { return false; }
      /* EVERY PRIMITIVE OF A MESH MORPHS TOGETHER, which is why glTF puts the weights on the mesh
       * and not on the primitive -- so two primitives declaring different target counts have no
       * single weight run to drive them and the file cannot mean anything. */
      if (!mesh.Primitives.empty() &&
          mesh.Primitives[0].Targets.size() != primitive.Targets.size()) {
        return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " declares " +
                      Number(primitive.Targets.size()) + " morph targets and primitive 0 declares " +
                      Number(mesh.Primitives[0].Targets.size()) +
                      ", and one weight run drives every primitive of a mesh");
      }
      mesh.Primitives.push_back(std::move(primitive));
    }
    if (!mesh.Weights.empty() && !mesh.Primitives.empty() &&
        mesh.Weights.size() != mesh.Primitives[0].Targets.size()) {
      return Refuse("mesh " + Number(i) + " declares " + Number(mesh.Weights.size()) +
                    " weights over " + Number(mesh.Primitives[0].Targets.size()) + " morph targets");
    }
    Meshes_.push_back(std::move(mesh));
  }

  const Json::Ref cameras = root["cameras"];
  for (size_t i = 0; i < cameras.Size(); ++i) {
    const Json::Ref declaration = cameras[i];
    Camera camera;
    camera.Name = declaration["name"].Str("");
    const std::string kind = declaration["type"].Str("");
    if (kind == "perspective") {
      camera.Kind = CameraKind::Perspective;
      const Json::Ref lens = declaration["perspective"];
      camera.YfovRad = lens["yfov"].Num(0.0);
      camera.AspectRatio = lens["aspectRatio"].Num(0.0);
      camera.ZNearM = lens["znear"].Num(0.0);
      camera.ZFarM = lens["zfar"].Num(0.0);
    } else if (kind == "orthographic") {
      camera.Kind = CameraKind::Orthographic;
      const Json::Ref lens = declaration["orthographic"];
      camera.XMagM = lens["xmag"].Num(0.0);
      camera.YMagM = lens["ymag"].Num(0.0);
      camera.ZNearM = lens["znear"].Num(0.0);
      camera.ZFarM = lens["zfar"].Num(0.0);
    } else {
      return Refuse("camera " + Number(i) + " has type '" + kind + "', and glTF 2.0 has two");
    }
    Cameras_.push_back(std::move(camera));
  }

  const Json::Ref nodes = root["nodes"];
  for (size_t i = 0; i < nodes.Size(); ++i) {
    const Json::Ref declaration = nodes[i];
    Node node;
    node.Name = declaration["name"].Str("");
    node.Mesh = declaration["mesh"].Valid() ? declaration["mesh"].Int(-1) : -1;
    node.Camera = declaration["camera"].Valid() ? declaration["camera"].Int(-1) : -1;
    /* `KHR_node_visibility`, and it is READ HERE AND RESOLVED IN THE FLATTEN. The extension carries
     * one boolean and its default is `true`, so absence and presence-with-true are the same value
     * and there is no `Has...` flag beside it that could disagree -- the same shape
     * `KHR_texture_transform` set (board:1177). What is NOT decided here is inheritance: a node's own
     * answer is a property of the node, and *visible if and only if its own is true and all its
     * parents are* is a property of the WALK, which is where the hierarchy exists. */
    node.Visible = declaration["extensions"]["KHR_node_visibility"]["visible"].Bool(true);
    node.Skin = declaration["skin"].Valid() ? declaration["skin"].Int(-1) : -1;
    const Json::Ref lit = declaration["extensions"][kLightsPunctual]["light"];
    node.Light = lit.Valid() ? lit.Int(-1) : -1;
    const Json::Ref matrix = declaration["matrix"];
    const Json::Ref translation = declaration["translation"];
    const Json::Ref rotation = declaration["rotation"];
    const Json::Ref scale = declaration["scale"];
    if (matrix.Valid()) {
      if (translation.Valid() || rotation.Valid() || scale.Valid()) {
        return Refuse("node " + Number(i) + " carries both a matrix and a TRS component, which glTF 2.0 forbids");
      }
      if (matrix.Size() != 16) {
        return Refuse("node " + Number(i) + " has a matrix of " + Number(matrix.Size()) + " numbers");
      }
      node.HasMatrix = true;
      for (size_t k = 0; k < 16; ++k) { node.Matrix[k] = matrix[k].Num(0.0); }
    } else {
      for (size_t k = 0; k < 3 && k < translation.Size(); ++k) {
        node.Translation[k] = translation[k].Num(0.0);
      }
      for (size_t k = 0; k < 4 && k < rotation.Size(); ++k) {
        node.Rotation[k] = rotation[k].Num(0.0);
      }
      for (size_t k = 0; k < 3 && k < scale.Size(); ++k) { node.Scale[k] = scale[k].Num(1.0); }
    }
    const Json::Ref children = declaration["children"];
    for (size_t k = 0; k < children.Size(); ++k) { node.Children.push_back(children[k].Int(-1)); }
    Nodes_.push_back(std::move(node));
  }

  Parent_.assign(Nodes_.size(), -1);
  for (size_t i = 0; i < Nodes_.size(); ++i) {
    const Node &node = Nodes_[i];
    if (node.Mesh >= 0 && static_cast<size_t>(node.Mesh) >= Meshes_.size()) {
      return Refuse("node " + Number(i) + " names mesh " + Number(static_cast<size_t>(node.Mesh)) +
                    " of " + Number(Meshes_.size()));
    }
    if (node.Camera >= 0 && static_cast<size_t>(node.Camera) >= Cameras_.size()) {
      return Refuse("node " + Number(i) + " names camera " + Number(static_cast<size_t>(node.Camera)) +
                    " of " + Number(Cameras_.size()));
    }
    if (node.Light >= 0 && static_cast<size_t>(node.Light) >= Lights_.size()) {
      return Refuse("node " + Number(i) + " names " + kLightsPunctual + " light " +
                    Number(static_cast<size_t>(node.Light)) + " of " + Number(Lights_.size()));
    }
    for (int child : node.Children) {
      if (child < 0 || static_cast<size_t>(child) >= Nodes_.size()) {
        return Refuse("node " + Number(i) + " names a child the file does not carry");
      }
      if (Parent_[static_cast<size_t>(child)] >= 0) {
        return Refuse("node " + Number(static_cast<size_t>(child)) +
                      " is a child of two nodes, and a glTF hierarchy is a forest");
      }
      Parent_[static_cast<size_t>(child)] = static_cast<int>(i);
    }
  }

  const Json::Ref scenes = root["scenes"];
  for (size_t i = 0; i < scenes.Size(); ++i) {
    Scene scene;
    scene.Name = scenes[i]["name"].Str("");
    const Json::Ref roots = scenes[i]["nodes"];
    for (size_t k = 0; k < roots.Size(); ++k) {
      const int node = roots[k].Int(-1);
      if (node < 0 || static_cast<size_t>(node) >= Nodes_.size()) {
        return Refuse("scene " + Number(i) + " names a root node the file does not carry");
      }
      scene.Roots.push_back(node);
    }
    Scenes_.push_back(std::move(scene));
  }
  DefaultScene_ = root["scene"].Valid() ? root["scene"].Int(-1) : (Scenes_.empty() ? -1 : 0);
  if (DefaultScene_ >= 0 && static_cast<size_t>(DefaultScene_) >= Scenes_.size()) {
    return Refuse("names default scene " + Number(static_cast<size_t>(DefaultScene_)) + " of " +
                  Number(Scenes_.size()));
  }
  /* THE MORPH LAYOUT, DERIVED ONCE AND HERE, after the meshes and the nodes are both complete. */
  MorphAt_.assign(Nodes_.size() + 1, 0);
  for (size_t node = 0; node < Nodes_.size(); ++node) {
    size_t count = 0;
    const int mesh = Nodes_[node].Mesh;
    if (mesh >= 0 && (size_t)mesh < Meshes_.size() && !Meshes_[(size_t)mesh].Primitives.empty()) {
      count = Meshes_[(size_t)mesh].Primitives[0].Targets.size();
    }
    MorphAt_[node + 1] = MorphAt_[node] + count;
  }
  if (!ReadSkins(root)) { return false; }
  if (!ReadAnimations(json)) { return false; }
  return true;
}

/* THE SKINS, READ AFTER THE NODES because every joint is a node index and the table has to be
 * complete before one can be refused for naming a node the file does not carry.
 *
 * AN ABSENT `inverseBindMatrices` IS NOT AN OMISSION, it is the format stating that every matrix is
 * the identity -- so the vector is left empty and the consumer reads the absence, rather than this
 * reader materialising N identities that would then have to be told apart from declared ones. */
bool Document::ReadSkins(const Json::Ref &root) {
  const Json::Ref skins = root["skins"];
  for (size_t i = 0; i < skins.Size(); ++i) {
    const Json::Ref declaration = skins[i];
    Skin skin;
    skin.Name = declaration["name"].Str("");
    skin.Skeleton = declaration["skeleton"].Valid() ? declaration["skeleton"].Int(-1) : -1;
    const Json::Ref joints = declaration["joints"];
    if (joints.Size() == 0) {
      return Refuse("skin " + Number(i) + " names no joint, and a skin with no joint deforms nothing");
    }
    for (size_t k = 0; k < joints.Size(); ++k) {
      const int node = joints[k].Int(-1);
      if (node < 0 || static_cast<size_t>(node) >= Nodes_.size()) {
        return Refuse("skin " + Number(i) + " names joint node " + Number(k) +
                      " which the file does not carry");
      }
      skin.Joints.push_back(node);
    }
    if (skin.Skeleton >= 0 && static_cast<size_t>(skin.Skeleton) >= Nodes_.size()) {
      return Refuse("skin " + Number(i) + " names a skeleton node the file does not carry");
    }
    const Json::Ref bind = declaration["inverseBindMatrices"];
    if (bind.Valid()) {
      if (!ReadElements(bind.Int(-1), skin.InverseBind)) { return false; }
      if (skin.InverseBind.size() != skin.Joints.size() * 16) {
        return Refuse("skin " + Number(i) + " names " + Number(skin.Joints.size()) +
                      " joints and an inverseBindMatrices accessor holding " +
                      Number(skin.InverseBind.size() / 16) + " matrices");
      }
    }
    Skins_.push_back(std::move(skin));
  }
  for (size_t i = 0; i < Nodes_.size(); ++i) {
    const int skin = Nodes_[i].Skin;
    if (skin >= 0 && static_cast<size_t>(skin) >= Skins_.size()) {
      return Refuse("node " + Number(i) + " names skin " + Number(static_cast<size_t>(skin)) +
                    " and the file declares " + Number(Skins_.size()));
    }
    if (skin >= 0 && Nodes_[i].Mesh < 0) {
      return Refuse("node " + Number(i) + " names a skin and carries no mesh, and glTF states a "
                    "skin is only meaningful on the node that instantiates the geometry");
    }
  }
  return true;
}

/* THE ANIMATIONS, READ LAST because every reference in them points backwards: a sampler names two
 * accessors and a channel names a node, and both tables are complete by the time this runs.
 *
 * THREE WORDS AND NO FOURTH. `interpolation` and `target.path` are both strings with a closed set of
 * values, and an unrecognised one is refused by name rather than falling back to `LINEAR` or to
 * `translation` -- a viewer that quietly substitutes one is exactly what `InterpolationTest` was
 * built to expose, and a reader that did it would make the case unable to see it.
 *
 * A CHANNEL WITH NO TARGET NODE IS NOT AN ERROR: the format defines it as a channel to be ignored,
 * so it is carried at -1 and the consumer's loop skips it. Refusing it here would refuse a
 * conforming file. */
bool Document::ReadAnimations(const Json &json) {
  const Json::Ref animations = json.Root()["animations"];
  for (size_t i = 0; i < animations.Size(); ++i) {
    const Json::Ref declaration = animations[i];
    Animation animation;
    animation.Name = declaration["name"].Str("");

    const Json::Ref samplers = declaration["samplers"];
    for (size_t s = 0; s < samplers.Size(); ++s) {
      const Json::Ref declared = samplers[s];
      AnimationSampler sampler;
      sampler.Input = declared["input"].Int(-1);
      sampler.Output = declared["output"].Int(-1);
      const std::string how = declared["interpolation"].Str("LINEAR");
      if (how == "LINEAR") {
        sampler.How = Interpolation::Linear;
      } else if (how == "STEP") {
        sampler.How = Interpolation::Step;
      } else if (how == "CUBICSPLINE") {
        sampler.How = Interpolation::CubicSpline;
      } else {
        return Refuse("animation " + Number(i) + " sampler " + Number(s) + " interpolates by '" +
                      how + "', which is none of LINEAR, STEP or CUBICSPLINE");
      }
      for (const int accessor : {sampler.Input, sampler.Output}) {
        if (accessor < 0 || static_cast<size_t>(accessor) >= Accessors_.size()) {
          return Refuse("animation " + Number(i) + " sampler " + Number(s) +
                        " names an accessor the file does not carry");
        }
      }
      const Accessor &times = Accessors_[static_cast<size_t>(sampler.Input)];
      if (times.Element != ElementType::Scalar) {
        return Refuse("animation " + Number(i) + " sampler " + Number(s) +
                      " has a time grid that is not SCALAR");
      }
      /* THE SHAPE RULE THE FORMAT PUTS ON A SAMPLER, and it is what makes `CUBICSPLINE` a different
       * DECODE and not a different formula: three elements per keyframe against one. A file that got
       * this wrong would otherwise be read as an animation a third as long.
       *
       * IT IS A MULTIPLE AND NOT AN EQUALITY, BECAUSE A SAMPLER DOES NOT KNOW ITS PATH (board:1203).
       * A `weights` output is SCALAR and carries one value PER TARGET per keyframe -- 127 keyframes
       * and two targets is 254 -- so an equality here refuses every morph animation in the corpus.
       * Which multiple is right is a question about the mesh the CHANNEL names, and `Pose::Build`
       * refuses a width that disagrees with it, where the mesh is in scope. What is checked here is
       * what a sampler alone can decide: that the run divides. */
      const size_t perKeyframe = (sampler.How == Interpolation::CubicSpline) ? 3u : 1u;
      const Accessor &values = Accessors_[static_cast<size_t>(sampler.Output)];
      const size_t wanted = times.Count * perKeyframe;
      if (wanted == 0 || values.Count == 0 || values.Count % wanted != 0) {
        return Refuse("animation " + Number(i) + " sampler " + Number(s) + " states " +
                      Number(times.Count) + " keyframes and " + Number(values.Count) +
                      " output elements, which is not a whole number of values per keyframe of " +
                      Number(wanted));
      }
      if (sampler.How == Interpolation::CubicSpline && times.Count < 2) {
        return Refuse("animation " + Number(i) + " sampler " + Number(s) +
                      " is CUBICSPLINE over fewer than two keyframes, which has no tangent span");
      }
      animation.Samplers.push_back(sampler);
    }

    const Json::Ref channels = declaration["channels"];
    for (size_t c = 0; c < channels.Size(); ++c) {
      const Json::Ref declared = channels[c];
      AnimationChannel channel;
      channel.Sampler = declared["sampler"].Int(-1);
      if (channel.Sampler < 0 ||
          static_cast<size_t>(channel.Sampler) >= animation.Samplers.size()) {
        return Refuse("animation " + Number(i) + " channel " + Number(c) + " names sampler " +
                      Number(static_cast<size_t>(channel.Sampler < 0 ? 0 : channel.Sampler)) +
                      " of " + Number(animation.Samplers.size()));
      }
      const Json::Ref target = declared["target"];
      channel.Node = target["node"].Valid() ? target["node"].Int(-1) : -1;
      if (channel.Node >= 0 && static_cast<size_t>(channel.Node) >= Nodes_.size()) {
        return Refuse("animation " + Number(i) + " channel " + Number(c) +
                      " targets a node the file does not carry");
      }
      const std::string path = target["path"].Str("");
      if (path == "translation") {
        channel.Path = AnimationPath::Translation;
      } else if (path == "rotation") {
        channel.Path = AnimationPath::Rotation;
      } else if (path == "scale") {
        channel.Path = AnimationPath::Scale;
      } else if (path == "weights") {
        channel.Path = AnimationPath::Weights;
      } else {
        return Refuse("animation " + Number(i) + " channel " + Number(c) + " drives '" + path +
                      "', which is none of translation, rotation, scale or weights");
      }
      const size_t components = PathComponents(channel.Path);
      const Accessor &values =
          Accessors_[static_cast<size_t>(animation.Samplers[static_cast<size_t>(channel.Sampler)]
                                             .Output)];
      if (components > 0 && ElementComponents(values.Element) != components) {
        return Refuse("animation " + Number(i) + " channel " + Number(c) + " drives '" + path +
                      "', which is " + Number(components) + " components, from an output of " +
                      Number(ElementComponents(values.Element)));
      }
      animation.Channels.push_back(channel);
    }
    Animations_.push_back(std::move(animation));
  }
  return true;
}

/* `KHR_lights_punctual`'s DOCUMENT-LEVEL TABLE. A light here has no place: the extension puts the
 * reference on a node and the beam on that node's -Z, so where and which way are the hierarchy's
 * answer and are resolved by whoever walks it.
 *
 * `type` IS THE ONE FIELD WITH NO DEFAULT AND AN UNKNOWN SPELLING IS A REFUSAL, because the three
 * arms differ in their UNITS -- `intensity` is lux under `directional` and candela under the other
 * two -- so a light whose type was guessed is a number in the wrong unit rather than a light in the
 * wrong shape. The extension's own defaults are used where a field is absent: white, intensity 1,
 * and a spot cone of 0 to pi/4. */
bool Document::ReadLights(const Json &json) {
  const Json::Ref declared =
      json.Root()["extensions"][kLightsPunctual]["lights"];
  for (size_t i = 0; i < declared.Size(); ++i) {
    const Json::Ref entry = declared[i];
    LightRef light;
    light.Name = entry["name"].Str("");
    const std::string kind = entry["type"].Str("");
    if (kind == "directional") {
      light.Light.Kind = LightKind::Directional;
    } else if (kind == "point") {
      light.Light.Kind = LightKind::Point;
    } else if (kind == "spot") {
      light.Light.Kind = LightKind::Spot;
    } else {
      return Refuse(std::string(kLightsPunctual) + " light " + Number(i) + " has type '" + kind +
                    "', and the extension defines directional, point and spot");
    }
    const Json::Ref colour = entry["color"];
    for (size_t k = 0; k < 3 && k < colour.Size(); ++k) {
      light.Light.Colour[k] = static_cast<float>(colour[k].Num(1.0));
    }
    light.Light.Intensity = static_cast<float>(entry["intensity"].Num(1.0));
    light.Light.RangeM = static_cast<float>(entry["range"].Num(0.0));
    if (light.Light.Kind == LightKind::Spot) {
      const Json::Ref cone = entry["spot"];
      light.Light.InnerConeRad = static_cast<float>(cone["innerConeAngle"].Num(0.0));
      light.Light.OuterConeRad =
          static_cast<float>(cone["outerConeAngle"].Num(0.7853981633974483));
      if (!(light.Light.InnerConeRad >= 0.0f) ||
          !(light.Light.InnerConeRad < light.Light.OuterConeRad) ||
          !(light.Light.OuterConeRad <= 1.5707963267948966f)) {
        return Refuse(std::string(kLightsPunctual) + " spot light " + Number(i) +
                      " declares an inner cone that is not below its outer cone inside [0, pi/2]");
      }
    }
    Lights_.push_back(std::move(light));
  }
  return true;
}

/* `KHR_materials_variants`' DOCUMENT-LEVEL TABLE (board:1188), which is a list of NAMES and nothing
 * else. It is read before the meshes because a primitive's mappings are checked against it, and a
 * mapping validated against a table that did not exist yet would be validated against nothing.
 *
 * A NAME IS REQUIRED BY THE EXTENSION'S OWN SCHEMA AND A REPEATED ONE IS REFUSED HERE. The second is
 * this reader's addition and it is what makes selection BY NAME answerable: a declaration names the
 * variant it renders, so two variants called `beach` are a file stating two answers to one question
 * -- the same defect as a primitive mapped twice, one level up. An index would sidestep it and put a
 * position in a list into every declaration that selects. */
bool Document::ReadVariants(const Json &json) {
  const Json::Ref declared = json.Root()["extensions"][kMaterialsVariants];
  if (!declared.Valid()) { return true; }
  if (declared.GetKind() != Json::Kind::Object) {
    return Refuse(std::string(kMaterialsVariants) + " is declared as something other than an object");
  }
  const Json::Ref variants = declared["variants"];
  if (variants.GetKind() != Json::Kind::Array || variants.Size() == 0) {
    return Refuse(std::string(kMaterialsVariants) +
                  " declares no non-empty variants array, and the extension is that array");
  }
  for (size_t i = 0; i < variants.Size(); ++i) {
    const Json::Ref name = variants[i]["name"];
    if (name.GetKind() != Json::Kind::String) {
      return Refuse(std::string(kMaterialsVariants) + " variant " + Number(i) +
                    " states no name, and a name is what a declaration selects it by");
    }
    const std::string spelling = name.Str("");
    for (size_t earlier = 0; earlier < Variants_.size(); ++earlier) {
      if (Variants_[earlier] == spelling) {
        return Refuse(std::string(kMaterialsVariants) + " variants " + Number(earlier) + " and " +
                      Number(i) + " are both named '" + spelling +
                      "', so the name selects two of them and neither is more correct");
      }
    }
    Variants_.push_back(spelling);
  }
  return true;
}

/* ONE PRIMITIVE'S MAPPINGS, INTO THE DENSE RUN `Primitive::VariantMaterials` IS (board:1188).
 *
 * "ACROSS THE ENTIRE MAPPINGS ARRAY, EACH VARIANT INDEX MUST BE USED NO MORE THAN ONE TIME" is the
 * extension's own sentence, and here it is the slot already being written -- the file has stated two
 * materials for one variant, and picking either is picking a picture the file did not decide. */
bool Document::ReadVariantMappings(const Json::Ref &declared, size_t mesh, size_t primitive,
                                   Primitive &into) {
  const std::string where =
      "mesh " + Number(mesh) + " primitive " + Number(primitive) + " " + kMaterialsVariants;
  if (declared.GetKind() != Json::Kind::Object) {
    return Refuse(where + " is declared as something other than an object");
  }
  if (Variants_.empty()) {
    return Refuse(where + " states mappings and the file's root declares no variants for them to "
                          "name");
  }
  const Json::Ref mappings = declared["mappings"];
  if (mappings.GetKind() != Json::Kind::Array || mappings.Size() == 0) {
    return Refuse(where + " declares no non-empty mappings array");
  }
  into.VariantMaterials.assign(Variants_.size(), -1);
  for (size_t i = 0; i < mappings.Size(); ++i) {
    const Json::Ref mapping = mappings[i];
    const Json::Ref material = mapping["material"];
    const int wears = material.GetKind() == Json::Kind::Number ? material.Int(-1) : -1;
    if (wears < 0 || (size_t)wears >= Materials_.size()) {
      return Refuse(where + " mapping " + Number(i) + " names material " + std::to_string(wears) +
                    " of " + Number(Materials_.size()) +
                    ", and the extension requires one on every mapping");
    }
    const Json::Ref named = mapping["variants"];
    if (named.GetKind() != Json::Kind::Array || named.Size() == 0) {
      return Refuse(where + " mapping " + Number(i) + " declares no non-empty variants array");
    }
    for (size_t k = 0; k < named.Size(); ++k) {
      const int variant = named[k].GetKind() == Json::Kind::Number ? named[k].Int(-1) : -1;
      if (variant < 0 || (size_t)variant >= Variants_.size()) {
        return Refuse(where + " mapping " + Number(i) + " names variant " + std::to_string(variant) +
                      " of " + Number(Variants_.size()));
      }
      if (into.VariantMaterials[(size_t)variant] >= 0) {
        return Refuse(where + " maps variant '" + Variants_[(size_t)variant] + "' to material " +
                      std::to_string(into.VariantMaterials[(size_t)variant]) + " and to material " +
                      std::to_string(wears) +
                      ", and the extension states each variant index no more than once");
      }
      into.VariantMaterials[(size_t)variant] = wears;
    }
  }
  return true;
}

/* THE APPEARANCE TABLES, in the order their references run: an image is bytes, a texture names an
 * image and a sampler, a material names textures. Read as one operation because each of the three
 * checks its references against the one before it, and splitting them would put the same index
 * bound in three places. */
bool Document::ReadAppearance(const Json &json) {
  const Json::Ref root = json.Root();

  const Json::Ref samplers = root["samplers"];
  for (size_t i = 0; i < samplers.Size(); ++i) {
    const Json::Ref declaration = samplers[i];
    Sampler sampler;
    for (const char *axis : {"wrapS", "wrapT"}) {
      const Json::Ref declared = declaration[axis];
      if (!declared.Valid()) { continue; }
      Wrap wrap = Wrap::Repeat;
      if (!KnownWrap(declared.Int(0), wrap)) {
        return Refuse("sampler " + Number(i) + " has a " + axis + " glTF 2.0 does not define");
      }
      (axis[4] == 'S' ? sampler.WrapS : sampler.WrapT) = wrap;
    }
    if (!KnownMagFilter(declaration["magFilter"].Int(9729), sampler.Mag)) {
      return Refuse("sampler " + Number(i) + " has a magFilter glTF 2.0 does not define");
    }
    if (!KnownMinFilter(declaration["minFilter"].Int(9987), sampler.Min, sampler.Mip)) {
      return Refuse("sampler " + Number(i) + " has a minFilter glTF 2.0 does not define");
    }
    Samplers_.push_back(sampler);
  }

  const Json::Ref images = root["images"];
  for (size_t i = 0; i < images.Size(); ++i) {
    const Json::Ref declaration = images[i];
    Image image;
    image.Name = declaration["name"].Str("");
    image.Uri = PercentDecoded(declaration["uri"].Str(""));
    image.MimeType = declaration["mimeType"].Str("");
    image.View = declaration["bufferView"].Valid() ? declaration["bufferView"].Int(-1) : -1;
    if (image.Uri.empty() && image.View < 0) {
      return Refuse("image " + Number(i) + " has neither a uri nor a bufferView");
    }
    if (image.View >= 0 && static_cast<size_t>(image.View) >= Views_.size()) {
      return Refuse("image " + Number(i) + " names bufferView " +
                    Number(static_cast<size_t>(image.View)) + " of " + Number(Views_.size()));
    }
    if (image.Uri.rfind("data:", 0) == 0) {
      return Refuse("image " + Number(i) + " is a data: URI, which this reader does not decode");
    }
    Images_.push_back(std::move(image));
  }

  const Json::Ref textures = root["textures"];
  for (size_t i = 0; i < textures.Size(); ++i) {
    const Json::Ref declaration = textures[i];
    Texture texture;
    texture.Name = declaration["name"].Str("");
    texture.Source = declaration["source"].Valid() ? declaration["source"].Int(-1) : -1;
    texture.Sampler = declaration["sampler"].Valid() ? declaration["sampler"].Int(-1) : -1;
    if (texture.Source >= 0 && static_cast<size_t>(texture.Source) >= Images_.size()) {
      return Refuse("texture " + Number(i) + " names image " +
                    Number(static_cast<size_t>(texture.Source)) + " of " + Number(Images_.size()));
    }
    if (texture.Sampler >= 0 && static_cast<size_t>(texture.Sampler) >= Samplers_.size()) {
      return Refuse("texture " + Number(i) + " names sampler " +
                    Number(static_cast<size_t>(texture.Sampler)) + " of " +
                    Number(Samplers_.size()));
    }
    Textures_.push_back(std::move(texture));
  }

  const Json::Ref materials = root["materials"];
  for (size_t i = 0; i < materials.Size(); ++i) {
    if (!ReadMaterial(materials[i], i)) { return false; }
  }
  return true;
}

bool Document::ReadMaterial(const Json::Ref &declaration, size_t index) {
  MaterialRef material;
  material.Name = declaration["name"].Str("");
  material.Surface.DoubleSided = declaration["doubleSided"].Bool(false);

  const Json::Ref pbr = declaration["pbrMetallicRoughness"];
  const Json::Ref baseColour = pbr["baseColorFactor"];
  for (size_t k = 0; k < 4 && k < baseColour.Size(); ++k) {
    material.Surface.BaseColour[k] = static_cast<float>(baseColour[k].Num(1.0));
  }
  if (!baseColour.Valid()) {
    /* glTF's own default is white, and the engine's default `Material` is a mid grey, so a file that
     * declares no factor must overwrite it rather than inherit ours. */
    for (float &channel : material.Surface.BaseColour) { channel = 1.0f; }
  }
  material.Surface.Metalness = static_cast<float>(pbr["metallicFactor"].Num(1.0));
  material.Surface.Roughness = static_cast<float>(pbr["roughnessFactor"].Num(1.0));

  const Json::Ref emissive = declaration["emissiveFactor"];
  for (size_t k = 0; k < 3 && k < emissive.Size(); ++k) {
    material.Surface.Emission[k] = static_cast<float>(emissive[k].Num(0.0));
  }
  /* KHR_materials_emissive_strength MULTIPLIES the factor rather than replacing it, and the product
   * is what leaves this reader: `emissiveFactor` is capped at 1 by the core format, so the extension
   * is the only way a glTF says "brighter than white" and a consumer that kept the two apart would
   * have to remember to multiply them at every site that reads emission. */
  /* KHR_materials_ior AND KHR_materials_specular SET ONE QUANTITY BETWEEN THEM, and `DielectricF0`
   * in `core/Material.h` is where they meet -- so both are read into the row here and neither is
   * combined with the other until the one place that does it.
   *
   * `ior = 0` IS LEGAL AND IS NOT "ABSENT". The extension states it means a surface with no Fresnel,
   * so the default 1.5 may not stand in for it and the check below is on validity rather than on
   * being non-zero. */
  const Json::Ref ior = declaration["extensions"][kIor]["ior"];
  if (ior.Valid()) {
    if (ior.GetKind() != Json::Kind::Number) {
      return Refuse("material " + Number(index) + " declares an ior that is not a number");
    }
    if (!(ior.Num() >= 0.0)) {
      return Refuse("material " + Number(index) + " declares a negative ior");
    }
    material.Surface.Ior = static_cast<float>(ior.Num());
  }
  const Json::Ref specular = declaration["extensions"][kSpecular];
  if (specular.Valid()) {
    const Json::Ref factor = specular["specularFactor"];
    if (factor.Valid()) {
      if (factor.GetKind() != Json::Kind::Number || !(factor.Num() >= 0.0)) {
        return Refuse("material " + Number(index) +
                      " declares a specularFactor that is not a number at or above zero");
      }
      material.Surface.SpecularFactor = static_cast<float>(factor.Num());
    }
    const Json::Ref tint = specular["specularColorFactor"];
    for (size_t k = 0; k < 3 && k < tint.Size(); ++k) {
      material.Surface.SpecularColour[k] = static_cast<float>(tint[k].Num(1.0));
    }
  }
  /* `KHR_materials_transmission` AND `KHR_materials_volume` ARE READ TOGETHER BECAUSE THEY ARE ONE
   * PHYSICAL THING (board:1386). Transmission says what fraction of light passes through the
   * surface; the volume says whether there is a medium behind it and what that medium does to the
   * light on its way. [MEASURED] over the 148 models at the pin, 100 of the 136 transmissive
   * materials carry a volume -- so a reader that took one without the other would draw the majority
   * of the corpus's glass as a thin sheet, which is a wrong picture rather than a plainer one. */
  /* `KHR_materials_transmission` AND `KHR_materials_volume` ARE NOT READ, AND THAT IS A WITHDRAWAL
   * RATHER THAN AN OVERSIGHT (board:1386). The renderer's half is written and compiled -- the
   * material row, the surface routing on thickness, three fragment arms, a second pass over the same
   * draw list and its composite -- and the picture it draws is not right yet: with these two read,
   * 26 of the corpus's models went red and the driver's own validation aborted 30 arms.
   *
   * **Half-built is worse than not built**, so the reader that switches the capability on is not
   * here until the round that finishes it. Neither extension is in `kHonouredExtensions`, so a file
   * that REQUIRES one is refused by name and nothing is drawn wrongly in silence. */
  /* `KHR_materials_sheen`, read into the row the way every DATA extension is (board:1177): the
   * values are composed here and the defaults are the identity of what the consumer does with them,
   * so absence and presence-with-defaults are one computation with no branch. The extension's own
   * defaults are a BLACK colour and a zero roughness, and black is what turns the layer off. */
  /* THE TWO SHEEN TEXTURES ARE NOT READ AND A MATERIAL THAT DECLARES ONE IS DRAWN WITHOUT IT
   * (board:1385). `sheenColorTexture` multiplies the colour and `sheenRoughnessTexture` the roughness
   * in its alpha; both are absent here, so such a material reaches the shader with its FACTORS alone
   * -- which is exactly what the extension says a material with no texture looks like, so the picture
   * is a plainer one rather than a wrong one. **It is a shortfall and it is named**: [MEASURED] 10 of
   * the 42 sheen materials at the pin declare one, and one model, `SheenCloth`, is entirely made of
   * them. */
  const Json::Ref sheen = declaration["extensions"][kSheen];
  if (sheen.Valid()) {
    const Json::Ref colour = sheen["sheenColorFactor"];
    for (size_t k = 0; k < 3 && k < colour.Size(); ++k) {
      const Json::Ref channel = colour[k];
      if (channel.GetKind() != Json::Kind::Number || !(channel.Num() >= 0.0)) {
        return Refuse("material " + Number(index) +
                      " declares a sheenColorFactor channel that is not a number at or above zero");
      }
      material.Surface.SheenColour[k] = static_cast<float>(channel.Num());
    }
    const Json::Ref roughness = sheen["sheenRoughnessFactor"];
    if (roughness.Valid()) {
      if (roughness.GetKind() != Json::Kind::Number || !(roughness.Num() >= 0.0) ||
          roughness.Num() > 1.0) {
        return Refuse("material " + Number(index) +
                      " declares a sheenRoughnessFactor outside [0, 1]");
      }
      material.Surface.SheenRoughness = static_cast<float>(roughness.Num());
    }
  }
  const Json::Ref strength =
      declaration["extensions"][kEmissiveStrength]["emissiveStrength"];
  if (strength.Valid()) {
    /* A PRESENT VALUE THAT IS NOT A NUMBER IS REFUSED AND NEVER DEFAULTED. `Num(def)` answers the
     * default for a string, an object and a null alike, so a declared brightness of `"2"` would have
     * been read as 1 and shipped as a picture nobody could trace back to the file -- a silent
     * success inside a reader that refuses everything else by name. */
    if (strength.GetKind() != Json::Kind::Number) {
      return Refuse("material " + Number(index) + " declares an emissiveStrength that is not a "
                    "number");
    }
    const double scale = strength.Num();
    if (!(scale >= 0.0)) {
      return Refuse("material " + Number(index) + " declares an emissiveStrength of " +
                    std::to_string(scale) + ", and the extension's minimum is 0");
    }
    for (float &channel : material.Surface.Emission) {
      channel = static_cast<float>(channel * scale);
    }
  }

  /* KHR_materials_unlit IS DECLARED BY THE PRESENCE OF ITS OBJECT and carries no property of its
   * own, so the object's KIND is the whole of what is read: a `true` or a number under that key is a
   * file saying something the extension does not define, and defaulting it to "unlit" would accept a
   * spelling nobody wrote. */
  const Json::Ref unlit = declaration["extensions"][kUnlit];
  if (unlit.Valid()) {
    if (unlit.GetKind() != Json::Kind::Object) {
      return Refuse("material " + Number(index) + " declares KHR_materials_unlit as something "
                    "other than an object, and the extension defines an empty object");
    }
    material.Surface.Unlit = true;
  }

  const std::string mode = declaration["alphaMode"].Str("");
  if (!KnownAlphaMode(mode, material.Surface.Alpha)) {
    return Refuse("material " + Number(index) + " has alphaMode '" + mode + "', and glTF 2.0 has "
                  "OPAQUE, MASK and BLEND");
  }
  material.Surface.CoverageCut = static_cast<float>(declaration["alphaCutoff"].Num(0.5));

  /* WHICH OBJECT A SOCKET SITS IN, CARRIED AS THE OBJECT AND NOT AS A FLAG (board:1205, `Enum.2`).
   * It was a `bool UnderPbr` while there were two places a texture could be declared; there are now
   * three -- glTF puts two under `pbrMetallicRoughness`, three at the material's root and two inside
   * `KHR_materials_specular` -- and a boolean cannot name a third. */
  const struct {
    Json::Ref Under;
    const char *Slot;
    TextureRef *Into;
  } slots[] = {
      {pbr, "baseColorTexture", &material.BaseColour},
      {pbr, "metallicRoughnessTexture", &material.MetallicRoughness},
      {declaration, "normalTexture", &material.Normal},
      {declaration, "occlusionTexture", &material.Occlusion},
      {declaration, "emissiveTexture", &material.Emissive},
      {specular, "specularTexture", &material.SpecularStrength},
      {specular, "specularColorTexture", &material.SpecularTint},
  };
  for (const auto &slot : slots) {
    const Json::Ref declared = slot.Under[slot.Slot];
    if (!declared.Valid()) { continue; }
    slot.Into->Texture = declared["index"].Int(-1);
    slot.Into->TexCoord = declared["texCoord"].Int(0);
    if (slot.Into->Texture < 0 || static_cast<size_t>(slot.Into->Texture) >= Textures_.size()) {
      return Refuse("material " + Number(index) + " " + slot.Slot + " names texture " +
                    Number(static_cast<size_t>(slot.Into->Texture)) + " of " +
                    Number(Textures_.size()));
    }
    /* READ AFTER the reference's own `texCoord` and never before it, because the extension's set
     * OVERRIDES that one and the two are written into the same field. */
    std::string why;
    if (!ReadTextureTransform(declared, *slot.Into, why)) {
      return Refuse("material " + Number(index) + " " + slot.Slot + " " + why);
    }
    if (slot.Into->TexCoord < 0) {
      return Refuse("material " + Number(index) + " " + slot.Slot + " names a negative UV set");
    }
  }
  material.NormalScale = declaration["normalTexture"]["scale"].Num(1.0);
  material.OcclusionStrength = declaration["occlusionTexture"]["strength"].Num(1.0);
  Materials_.push_back(std::move(material));
  return true;
}

bool Document::ImageBytes(int index, std::vector<uint8_t> &out) const {
  out.clear();
  if (index < 0 || static_cast<size_t>(index) >= Images_.size()) { return false; }
  const Image &image = Images_[static_cast<size_t>(index)];
  if (image.View >= 0) {
    Span<const uint8_t> span;
    if (!ViewSpan(image.View, span)) { return false; }
    out.assign(span.Data(), span.Data() + span.Size());
    return true;
  }
  std::ifstream file(DirectoryOf(Path_) + image.Uri, std::ios::binary);
  if (!file) { return false; }
  out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return !out.empty();
}

bool Document::ViewSpan(int view, Span<const uint8_t> &out) const {
  if (view < 0 || static_cast<size_t>(view) >= Views_.size()) { return false; }
  const BufferView &span = Views_[static_cast<size_t>(view)];
  out = Span<const uint8_t>(Buffers_[span.Buffer].data() + span.ByteOffset, span.ByteLength);
  return true;
}

bool Document::ElementBytes(const Accessor &accessor, size_t &stride, size_t &element) const {
  element = TightElementBytes(accessor.Element, accessor.Component);
  if (element == 0) { return false; }
  stride = element;
  if (accessor.View >= 0) {
    const size_t declared = Views_[static_cast<size_t>(accessor.View)].ByteStride;
    if (declared > 0) { stride = declared; }
  }
  return stride >= element;
}

bool Document::ReadElements(int accessorIndex, std::vector<double> &out) const {
  out.clear();
  if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= Accessors_.size()) { return false; }
  const Accessor &accessor = Accessors_[static_cast<size_t>(accessorIndex)];
  const size_t components = ElementComponents(accessor.Element);
  const size_t rows = ElementRows(accessor.Element);
  const size_t columns = ElementColumns(accessor.Element);
  const size_t componentBytes = ComponentBytes(accessor.Component);
  size_t stride = 0, element = 0;
  if (!ElementBytes(accessor, stride, element)) { return false; }
  const size_t columnBytes = (columns > 1) ? element / columns : 0;

  /* THE SPAN IS CHECKED BEFORE ANYTHING IS WRITTEN. A refused read that had already filled `out`
   * with zeros hands its caller a mesh at the origin, which is data a bug can be mistaken for. */
  Span<const uint8_t> span;
  if (accessor.View >= 0) {
    if (!ViewSpan(accessor.View, span)) { return false; }
    if (accessor.Count > 0) {
      const size_t last = accessor.ByteOffset + (accessor.Count - 1) * stride + element;
      if (accessor.ByteOffset > span.Size() || last > span.Size()) { return false; }
    }
  }

  out.assign(accessor.Count * components, 0.0);
  if (accessor.View >= 0) {
    for (size_t i = 0; i < accessor.Count; ++i) {
      const uint8_t *at = span.Data() + accessor.ByteOffset + i * stride;
      for (size_t column = 0; column < columns; ++column) {
        const uint8_t *columnAt = at + column * columnBytes;
        for (size_t row = 0; row < rows; ++row) {
          const double raw = Component(columnAt + row * componentBytes, accessor.Component);
          out[i * components + column * rows + row] =
              accessor.Normalized ? Normalise(raw, accessor.Component) : raw;
        }
      }
    }
  }
  if (accessor.HasSparse && !ApplySparse(accessor, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool Document::ApplySparse(const Accessor &accessor, std::vector<double> &out) const {
  const SparseOverride &sparse = accessor.Sparse;
  const size_t components = ElementComponents(accessor.Element);
  const size_t rows = ElementRows(accessor.Element);
  const size_t columns = ElementColumns(accessor.Element);
  const size_t componentBytes = ComponentBytes(accessor.Component);
  /* Sparse values are tightly packed: the overriding run carries no byteStride of its own. */
  const size_t element = TightElementBytes(accessor.Element, accessor.Component);
  const size_t columnBytes = (columns > 1) ? element / columns : 0;
  const size_t indexBytes = ComponentBytes(sparse.IndicesComponent);
  if (sparse.IndicesComponent == ComponentType::Int8 ||
      sparse.IndicesComponent == ComponentType::Int16 ||
      sparse.IndicesComponent == ComponentType::Float32) {
    return false;
  }

  Span<const uint8_t> indices, values;
  if (!ViewSpan(sparse.IndicesBufferView, indices)) { return false; }
  if (!ViewSpan(sparse.ValuesBufferView, values)) { return false; }
  if (sparse.IndicesByteOffset + sparse.Count * indexBytes > indices.Size()) { return false; }
  if (sparse.ValuesByteOffset + sparse.Count * element > values.Size()) { return false; }

  for (size_t k = 0; k < sparse.Count; ++k) {
    const double index =
        Component(indices.Data() + sparse.IndicesByteOffset + k * indexBytes, sparse.IndicesComponent);
    if (index < 0 || static_cast<size_t>(index) >= accessor.Count) { return false; }
    const uint8_t *at = values.Data() + sparse.ValuesByteOffset + k * element;
    for (size_t column = 0; column < columns; ++column) {
      for (size_t row = 0; row < rows; ++row) {
        const double raw = Component(at + column * columnBytes + row * componentBytes,
                                     accessor.Component);
        out[static_cast<size_t>(index) * components + column * rows + row] =
            accessor.Normalized ? Normalise(raw, accessor.Component) : raw;
      }
    }
  }
  return true;
}

bool Document::ReadIndices(int accessorIndex, std::vector<uint32_t> &out) const {
  out.clear();
  if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= Accessors_.size()) { return false; }
  const Accessor &accessor = Accessors_[static_cast<size_t>(accessorIndex)];
  if (accessor.Element != ElementType::Scalar) { return false; }
  if (accessor.Component != ComponentType::UInt8 && accessor.Component != ComponentType::UInt16 &&
      accessor.Component != ComponentType::UInt32) {
    return false;
  }
  std::vector<double> elements;
  if (!ReadElements(accessorIndex, elements)) { return false; }
  out.reserve(elements.size());
  for (double value : elements) { out.push_back(static_cast<uint32_t>(value)); }
  return true;
}

bool Document::WorldTransform(int node, Transform &out) const {
  return Chain(node, nullptr, out);
}

bool Document::WorldTransform(int node, Span<const Transform> locals, Transform &out) const {
  if (locals.Size() != Nodes_.size()) { return false; }
  return Chain(node, locals.Data(), out);
}

bool Document::Chain(int node, const Transform *posed, Transform &out) const {
  if (node < 0 || static_cast<size_t>(node) >= Nodes_.size()) { return false; }
  out = Transform::Identity();
  std::vector<int> chain;
  for (int at = node, steps = 0; at >= 0; at = Parent_[static_cast<size_t>(at)], ++steps) {
    if (static_cast<size_t>(steps) > Nodes_.size()) { return false; }
    chain.push_back(at);
  }
  for (size_t i = chain.size(); i > 0; --i) {
    const size_t index = static_cast<size_t>(chain[i - 1]);
    const Node &step = Nodes_[index];
    const Transform local =
        posed ? posed[index]
              : (step.HasMatrix
                     ? Transform::FromColumnMajor(step.Matrix)
                     : Transform::FromTrs(step.Translation, step.Rotation, step.Scale));
    out = out * local;
  }
  return true;
}

bool Document::ViewTransform(int cameraNode, Transform &out) const {
  Transform world;
  if (!WorldTransform(cameraNode, world)) { return false; }
  return world.Inverse(out);
}

} // namespace outshine::Gltf
