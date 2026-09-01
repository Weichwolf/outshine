#include "Document.h"

#include <cstdint>
#include <cstdlib>
#include <ios>
#include <iterator>
#include <cmath>
#include <numbers>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <string>
#include <vector>
#include <system_error>
#include <utility>

#include "Json.h"

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "glTF buffers are little-endian");

namespace outshine::Gltf {
namespace {

constexpr uint32_t kGlbMagic = 0x46546C67;
constexpr uint32_t kChunkJson = 0x4E4F534A;
constexpr uint32_t kChunkBinary = 0x004E4942;
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

bool KnownElement(std::string_view raw, ElementType &out) {
  if (raw == "SCALAR") {
    out = ElementType::Scalar;
    return true;
  }
  if (raw == "VEC2") {
    out = ElementType::Vec2;
    return true;
  }
  if (raw == "VEC3") {
    out = ElementType::Vec3;
    return true;
  }
  if (raw == "VEC4") {
    out = ElementType::Vec4;
    return true;
  }
  if (raw == "MAT2") {
    out = ElementType::Mat2;
    return true;
  }
  if (raw == "MAT3") {
    out = ElementType::Mat3;
    return true;
  }
  if (raw == "MAT4") {
    out = ElementType::Mat4;
    return true;
  }
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

std::string DirectoryOf(std::string_view path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash + 1));
}

std::string Number(size_t value) {
  return std::to_string(value);
}

[[nodiscard]] int SixBitsOf(char one) {
  if (one >= 'A' && one <= 'Z') { return one - 'A'; }
  if (one >= 'a' && one <= 'z') { return one - 'a' + 26; }
  if (one >= '0' && one <= '9') { return one - '0' + 52; }
  if (one == '+') { return 62; }
  if (one == '/') { return 63; }
  return -1;
}

[[nodiscard]] bool Base64Payload(std::string_view uri, std::string_view &payload) {
  const size_t comma = uri.find(',');
  if (comma == std::string_view::npos) { return false; }
  const std::string_view header = uri.substr(0, comma);
  if (header.size() < 7 || header.substr(header.size() - 7) != ";base64") { return false; }
  payload = uri.substr(comma + 1);
  return true;
}

[[nodiscard]] bool DecodeBase64(std::string_view payload, std::vector<uint8_t> &out) {
  size_t padding = 0;
  while (padding < 2 && payload.size() > padding && payload[payload.size() - 1 - padding] == '=') {
    ++padding;
  }
  const std::string_view body = payload.substr(0, payload.size() - padding);
  if (body.size() % 4 == 1) { return false; }
  out.clear();
  out.reserve(body.size() / 4 * 3 + 3);
  uint32_t held = 0;
  int bits = 0;
  for (const char one : body) {
    const int six = SixBitsOf(one);
    if (six < 0) { return false; }
    held = (held << 6u) | static_cast<uint32_t>(six);
    bits += 6;
    if (bits < 8) { continue; }
    bits -= 8;
    out.push_back(static_cast<uint8_t>((held >> static_cast<uint32_t>(bits)) & 0xFFu));
  }
  return true;
}

int HexDigit(char c) {
  if (c >= '0' && c <= '9') { return c - '0'; }
  if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
  if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
  return -1;
}

std::string PercentDecoded(std::string_view uri) {
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

[[nodiscard]] bool KnownMagFilter(int raw, Filter &out) {
  switch (raw) {
    case 9728: out = Filter::Nearest; return true;
    case 9729: out = Filter::Linear; return true;
    default: return false;
  }
}

[[nodiscard]] bool KnownMinFilter(int raw, Filter &base, MipFilter &mip) {
  switch (raw) {
    case 9728:
      base = Filter::Nearest;
      mip = MipFilter::None;
      return true;
    case 9729:
      base = Filter::Linear;
      mip = MipFilter::None;
      return true;
    case 9984:
      base = Filter::Nearest;
      mip = MipFilter::Nearest;
      return true;
    case 9985:
      base = Filter::Linear;
      mip = MipFilter::Nearest;
      return true;
    case 9986:
      base = Filter::Nearest;
      mip = MipFilter::Linear;
      return true;
    case 9987:
      base = Filter::Linear;
      mip = MipFilter::Linear;
      return true;
    default: return false;
  }
}

constexpr const char *const kHonouredExtensions[] = {"KHR_lights_punctual",
                                                     "KHR_materials_anisotropy",
                                                     "KHR_materials_iridescence",
                                                     "KHR_animation_pointer",
                                                     "EXT_mesh_gpu_instancing",
                                                     "EXT_texture_webp",
                                                     "KHR_materials_transmission",
                                                     "KHR_materials_volume",
                                                     "KHR_materials_clearcoat",
                                                     "KHR_materials_sheen",
                                                     "KHR_mesh_quantization",
                                                     "KHR_node_visibility",
                                                     "KHR_materials_emissive_strength",
                                                     "KHR_materials_ior",
                                                     "KHR_materials_specular",
                                                     "KHR_materials_unlit",
                                                     "KHR_materials_variants",
                                                     "KHR_texture_transform",
                                                     "KHR_xmp_json_ld",
                                                     nullptr};

constexpr const char *kLightsPunctual = "KHR_lights_punctual";
constexpr const char *kEmissiveStrength = "KHR_materials_emissive_strength";
constexpr const char *kIor = "KHR_materials_ior";
constexpr const char *kSpecular = "KHR_materials_specular";
constexpr const char *kSheen = "KHR_materials_sheen";
constexpr const char *kClearcoat = "KHR_materials_clearcoat";
constexpr const char *kAnisotropy = "KHR_materials_anisotropy";
constexpr const char *kIridescence = "KHR_materials_iridescence";
constexpr const char *kAnimationPointer = "KHR_animation_pointer";
constexpr const char *kMeshGpuInstancing = "EXT_mesh_gpu_instancing";
constexpr const char *kTextureWebp = "EXT_texture_webp";
constexpr const char *kTransmission = "KHR_materials_transmission";
constexpr const char *kVolume = "KHR_materials_volume";
constexpr const char *kUnlit = "KHR_materials_unlit";
constexpr const char *kMaterialsVariants = "KHR_materials_variants";
constexpr const char *kTextureTransform = "KHR_texture_transform";

bool ResolveMaterialPointer(std::string_view pointer,
                            AnimationChannel &channel,
                            UndrivenReason &why) {
  std::vector<std::string> segments;
  size_t at = 0;
  while (at <= pointer.size()) {
    const size_t slash = pointer.find('/', at);
    segments.emplace_back(
        pointer.substr(at, slash == std::string_view::npos ? std::string_view::npos : slash - at));
    if (slash == std::string::npos) { break; }
    at = slash + 1;
  }
  why = UndrivenReason::PointerUnparsed;
  if (segments.size() < 4 || !segments.front().empty() || segments[1] != "materials") {
    return false;
  }
  const std::string &index = segments[2];
  if (index.empty() || index.find_first_not_of("0123456789") != std::string::npos) { return false; }

  std::string tail = segments[3];
  for (size_t part = 4; part < segments.size(); ++part) { tail += "/" + segments[part]; }

  why = UndrivenReason::PointerUnheld;
  for (const AnimatablePointer &known : AnimatablePointers()) {
    if (tail != known.Tail) { continue; }
    channel.Path = AnimationPath::MaterialFactor;
    channel.Material = std::atoi(index.c_str());
    channel.Factor = known.Factor;
    return true;
  }
  return false;
}

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
  if (raw.empty() || raw == "OPAQUE") {
    out = AlphaMode::Opaque;
    return true;
  }
  if (raw == "MASK") {
    out = AlphaMode::Masked;
    return true;
  }
  if (raw == "BLEND") {
    out = AlphaMode::Blended;
    return true;
  }
  return false;
}

} // namespace

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

  return !semantic.empty() && semantic.front() == '_';
}

bool Document::Honours(std::string_view extension) {
  for (const char *const known : kHonouredExtensions) {
    if (known != nullptr && extension == known) { return true; }
  }
  return false;
}

bool Document::Refuse(std::string_view why) {
  Error_ = Path_.empty() ? std::string(why) : Path_ + ": " + std::string(why);
  return false;
}

bool Document::ReadFile(std::string_view path) {
  std::ifstream file(std::string(path), std::ios::binary);
  if (!file) {
    Path_ = path;
    return Refuse("cannot be opened");
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
  return Read({bytes.data(), bytes.size()}, path);
}

bool Document::Read(Span<const uint8_t> whole, std::string_view path) {
  *this = Document();
  Path_ = path;
  if (whole.Empty()) { return Refuse("is empty"); }
  const uint8_t *const bytes = whole.Data();
  const size_t length = whole.Size();

  if (length >= 12 && LittleWord(bytes) == kGlbMagic) {
    const uint32_t version = LittleWord(bytes + 4);
    const uint32_t declared = LittleWord(bytes + 8);
    if (version != kGlbVersion) {
      return Refuse("is a GLB of version " + Number(version) + ", and this reader is glTF 2.0");
    }
    if (declared > length) {
      return Refuse("declares " + Number(declared) + " bytes and " + Number(length) +
                    " are present");
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
        return Refuse("has a GLB chunk of " + Number(chunkLength) +
                      " bytes that runs past the file");
      }
      if (chunkType == kChunkJson) {
        if (jsonChunk != nullptr) {
          return Refuse("is a GLB carrying a second JSON chunk, and the container declares "
                        "exactly one -- a file with two structures has no structure");
        }
        if (at != 20) {
          return Refuse("is a GLB whose JSON chunk is not the first, and the container declares "
                        "the structure ahead of what it describes");
        }
        jsonChunk = bytes + at;
        jsonLength = chunkLength;
      } else if (chunkType == kChunkBinary) {
        if (binaryChunk != nullptr) {
          return Refuse("is a GLB carrying a second binary chunk, and the container declares at "
                        "most one -- a buffer view naming chunk 0 could mean either");
        }
        binaryChunk = bytes + at;
        binaryLength = chunkLength;
      }

      at += (chunkLength + 3) & ~size_t{3};
    }
    if (jsonChunk == nullptr) { return Refuse("is a GLB with no JSON chunk"); }
    return ReadJson(
        reinterpret_cast<const char *>(jsonChunk), jsonLength, binaryChunk, binaryLength);
  }

  return ReadJson(reinterpret_cast<const char *>(bytes), length, nullptr, 0);
}

constexpr double kMostDeclaredBytes = 4294967295.0;

[[nodiscard]] bool DeclaredSize(const Json::Ref &ref, size_t &out) {
  const double raw = ref.Num(0.0);
  if (!(raw >= 0.0) || raw != std::floor(raw) || raw > kMostDeclaredBytes) { return false; }
  out = static_cast<size_t>(raw);
  return true;
}

bool Document::ResolveBuffers(const Json &json, const uint8_t *binaryChunk, size_t binaryLength) {
  const Json::Ref buffers = json.Root()["buffers"];
  const std::string directory = DirectoryOf(Path_);
  for (size_t i = 0; i < buffers.Size(); ++i) {
    const Json::Ref buffer = buffers[i];
    size_t declared = 0;
    if (!DeclaredSize(buffer["byteLength"], declared)) {
      return Refuse("buffer " + Number(i) +
                    " declares a byteLength that is not a whole "
                    "non-negative count under the container's ceiling");
    }
    const std::string uri = PercentDecoded(buffer["uri"].Str(""));
    std::vector<uint8_t> bytes;
    if (uri.empty()) {
      if (binaryChunk == nullptr) {
        return Refuse("buffer " + Number(i) + " has no uri and the file carries no binary chunk");
      }
      bytes.assign(binaryChunk, binaryChunk + binaryLength);
    } else if (uri.rfind("data:", 0) == 0) {
      std::string_view payload;
      if (!Base64Payload(uri, payload)) {
        return Refuse("buffer " + Number(i) +
                      " is a data: URI that declares no ;base64 payload, and this reader "
                      "carries no other encoding");
      }
      if (!DecodeBase64(payload, bytes)) {
        return Refuse("buffer " + Number(i) +
                      " is a data: URI whose base64 payload holds a character the alphabet "
                      "does not, or a length no whole byte count can come from");
      }
      if (bytes.size() != declared) {
        return Refuse("buffer " + Number(i) + " declares " + Number(declared) +
                      " bytes and its data: URI decodes to " + Number(bytes.size()) +
                      " -- a declared length that disagrees with its payload is a refusal "
                      "rather than a resize");
      }
    } else {
      std::error_code stat;
      const auto measured = std::filesystem::file_size(directory + uri, stat);
      if (stat) {
        return Refuse("buffer " + Number(i) + " names " + uri + ", which cannot be opened");
      }
      const size_t reading = measured < static_cast<uintmax_t>(declared) + 1
                                 ? static_cast<size_t>(measured)
                                 : declared + 1;
      std::ifstream file(directory + uri, std::ios::binary);
      if (!file) {
        return Refuse("buffer " + Number(i) + " names " + uri + ", which cannot be opened");
      }
      bytes.resize(reading);
      file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      bytes.resize(static_cast<size_t>(file.gcount()));
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

bool Document::ReadJson(const char *text,
                        size_t length,
                        const uint8_t *binaryChunk,
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

  for (const char *named : {"accessors",
                            "animations",
                            "buffers",
                            "bufferViews",
                            "cameras",
                            "images",
                            "materials",
                            "meshes",
                            "nodes",
                            "samplers",
                            "scenes",
                            "skins",
                            "textures",
                            "extensionsUsed",
                            "extensionsRequired"}) {
    const Json::Ref held = root[named];
    if (held.Valid() && held.GetKind() == Json::Kind::Array && held.Size() == 0) {
      return Refuse(std::string(named) +
                    " is present and empty, and glTF 2.0 gives every one of "
                    "its arrays a minimum of one item -- an array that is there says something "
                    "stands in it");
    }
  }

  MinVersion_ = root["asset"]["minVersion"].Str("");
  if (!MinVersion_.empty() && MinVersion_.rfind("2.0", 0) != 0) {
    return Refuse("declares asset.minVersion '" + MinVersion_ +
                  "', which is above the glTF 2.0 this reader is");
  }
  const Json::Ref required = root["extensionsRequired"];
  for (size_t i = 0; i < required.Size(); ++i) { Required_.push_back(required[i].Str("")); }
  for (const std::string &extension : Required_) {
    if (extension == "KHR_mesh_quantization") { Quantised_ = true; }
    if (!Honours(extension)) {
      return Refuse("requires extension '" + extension + "', which this reader does not implement");
    }
  }

  {
    const Json::Ref carried = root["extensions"]["KHR_xmp_json_ld"]["packets"];
    for (size_t i = 0; i < carried.Size(); ++i) {
      const Json::Ref packet = carried[i];
      MetadataPacket held;
      held.Held.reserve(packet.Size());
      for (size_t at = 0; at < packet.Size(); ++at) {
        std::string key = packet.Key(at);
        if (key.empty()) { continue; }
        const Json::Ref said = packet[at];
        const bool spelled = said.GetKind() == Json::Kind::String ||
                             said.GetKind() == Json::Kind::Number ||
                             said.GetKind() == Json::Kind::Bool;
        held.Held.push_back(
            MetadataProperty{.Key = std::move(key),
                             .Value = spelled ? said.Str("") : std::string(said.Source()),
                             .Shape = spelled ? MetadataShape::Text : MetadataShape::Structure});
      }
      Metadata_.push_back(std::move(held));
    }

    struct Carrier {
      const char *Array;
      MetadataCarrier Kind;
    };

    static constexpr Carrier kCarriers[] = {
        {.Array = "scenes", .Kind = MetadataCarrier::Scene},
        {.Array = "nodes", .Kind = MetadataCarrier::Node},
        {.Array = "meshes", .Kind = MetadataCarrier::Mesh},
        {.Array = "materials", .Kind = MetadataCarrier::Material},
        {.Array = "images", .Kind = MetadataCarrier::Image},
        {.Array = "animations", .Kind = MetadataCarrier::Animation},
    };

    const auto Points =
        [&](const Json::Ref &at, MetadataCarrier carrier, size_t which, const char *what) -> bool {
      const Json::Ref names = at["extensions"]["KHR_xmp_json_ld"]["packet"];
      if (!names.Valid()) { return true; }
      const int packet = names.Int(-1);
      if (packet < 0 || static_cast<size_t>(packet) >= Metadata_.size()) {
        return Refuse(std::string(what) + " names metadata packet " +
                      Number(static_cast<size_t>(packet < 0 ? 0 : packet)) + " of " +
                      Number(Metadata_.size()) +
                      " -- a packet index outside the array it indexes is a refusal");
      }
      MetadataUses_.push_back(MetadataUse{.Carrier = carrier,
                                          .Which = static_cast<uint32_t>(which),
                                          .Packet = static_cast<uint32_t>(packet)});
      return true;
    };

    if (!Points(root["asset"], MetadataCarrier::Asset, 0, "asset")) { return false; }
    AssetMetadata_ = MetadataOf(MetadataCarrier::Asset, 0);
    for (const Carrier &carrier : kCarriers) {
      const Json::Ref held = root[carrier.Array];
      for (size_t which = 0; which < held.Size(); ++which) {
        const std::string what = std::string(carrier.Array) + " " + Number(which);
        if (!Points(held[which], carrier.Kind, which, what.c_str())) { return false; }
      }
    }
  }

  if (!ResolveBuffers(json, binaryChunk, binaryLength)) { return false; }

  const Json::Ref views = root["bufferViews"];
  for (size_t i = 0; i < views.Size(); ++i) {
    const Json::Ref declaration = views[i];
    BufferView view;
    if (!DeclaredSize(declaration["buffer"], view.Buffer) ||
        !DeclaredSize(declaration["byteOffset"], view.ByteOffset) ||
        !DeclaredSize(declaration["byteLength"], view.ByteLength) ||
        !DeclaredSize(declaration["byteStride"], view.ByteStride)) {
      return Refuse("bufferView " + Number(i) +
                    " declares a size that is not a whole "
                    "non-negative count under the container's ceiling");
    }
    if (declaration["byteStride"].Valid() &&
        (view.ByteStride < 4 || view.ByteStride > 252 || view.ByteStride % 4 != 0)) {
      return Refuse("bufferView " + Number(i) + " declares byteStride " + Number(view.ByteStride) +
                    ", and the spec holds a stride to a multiple of 4 in [4, 252]");
    }
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
    if (!DeclaredSize(declaration["byteOffset"], accessor.ByteOffset) ||
        !DeclaredSize(declaration["count"], accessor.Count)) {
      return Refuse("accessor " + Number(i) +
                    " declares a size that is not a whole "
                    "non-negative count under the container's ceiling");
    }
    accessor.Normalized = declaration["normalized"].Bool(false);
    const int rawComponent = declaration["componentType"].Int(0);
    if (!KnownComponent(rawComponent, accessor.Component)) {
      return Refuse("accessor " + Number(i) + " has componentType " +
                    Number(static_cast<size_t>(rawComponent)) + ", which glTF 2.0 does not define");
    }
    const std::string rawElement = declaration["type"].Str("");
    if (!KnownElement(rawElement, accessor.Element)) {
      return Refuse("accessor " + Number(i) + " has type '" + rawElement +
                    "', which glTF 2.0 does not define");
    }
    if (accessor.Normalized && accessor.Component == ComponentType::Float32) {
      return Refuse("accessor " + Number(i) + " is a normalized float, which glTF 2.0 forbids");
    }
    if (accessor.View >= 0 && static_cast<size_t>(accessor.View) >= Views_.size()) {
      return Refuse("accessor " + Number(i) + " names bufferView " +
                    Number(static_cast<size_t>(accessor.View)) + " of " + Number(Views_.size()));
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
      if (!DeclaredSize(sparse["count"], accessor.Sparse.Count)) {
        return Refuse("accessor " + Number(i) +
                      " declares a sparse count that is not a "
                      "whole non-negative count under the container's ceiling");
      }
      accessor.Sparse.IndicesBufferView = sparse["indices"]["bufferView"].Int(-1);
      if (!DeclaredSize(sparse["indices"]["byteOffset"], accessor.Sparse.IndicesByteOffset)) {
        return Refuse("accessor " + Number(i) +
                      " declares a sparse index offset that is "
                      "not a whole non-negative count under the container's ceiling");
      }
      if (!KnownComponent(sparse["indices"]["componentType"].Int(0),
                          accessor.Sparse.IndicesComponent)) {
        return Refuse("accessor " + Number(i) +
                      " has a sparse index componentType glTF 2.0 does not define");
      }
      accessor.Sparse.ValuesBufferView = sparse["values"]["bufferView"].Int(-1);
      if (!DeclaredSize(sparse["values"]["byteOffset"], accessor.Sparse.ValuesByteOffset)) {
        return Refuse("accessor " + Number(i) +
                      " declares a sparse value offset that is "
                      "not a whole non-negative count under the container's ceiling");
      }
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

  for (size_t a = 0; a < Accessors_.size(); ++a) {
    if (Accessors_[a].Min.empty() || Accessors_[a].Max.empty()) { continue; }
    std::string why;
    if (!BoundsHold(static_cast<int>(a), why)) {
      return Refuse("accessor " + Number(a) + " " + why);
    }
  }

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
        return Refuse("mesh " + Number(i) + " primitive " + Number(p) +
                      " has a mode glTF 2.0 does not define");
      }
      primitive.Indices = declared["indices"].Valid() ? declared["indices"].Int(-1) : -1;
      primitive.Material = declared["material"].Valid() ? declared["material"].Int(-1) : -1;
      const Json::Ref attributes = declared["attributes"];
      for (size_t a = 0; a < attributes.Size(); ++a) {
        Attribute attribute;
        attribute.Semantic = attributes.Key(a);
        attribute.Accessor = attributes[a].Int(-1);
        if (attribute.Accessor < 0 ||
            static_cast<size_t>(attribute.Accessor) >= Accessors_.size()) {
          return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " attribute " +
                        attribute.Semantic + " names an accessor the file does not carry");
        }
        const Accessor &carried = Accessors_[static_cast<size_t>(attribute.Accessor)];
        if (attribute.Semantic == "POSITION" &&
            (carried.Min.size() != ElementComponents(carried.Element) ||
             carried.Max.size() != ElementComponents(carried.Element))) {
          return Refuse("mesh " + Number(i) + " primitive " + Number(p) +
                        " has a POSITION accessor without the min/max bounds glTF 2.0 "
                        "requires");
        }
        const AttributeShape shape{.Element = carried.Element,
                                   .Component = carried.Component,
                                   .Normalized = carried.Normalized};
        if (!ShapeAllowed(attribute.Semantic, shape, Quantised_)) {
          return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " attribute " +
                        attribute.Semantic +
                        " is an accessor shape glTF 2.0 does not permit for it" +
                        (Quantised_ ? ", even with KHR_mesh_quantization"
                                    : " -- KHR_mesh_quantization widens this and the file does not "
                                      "require it"));
        }
        primitive.Attributes.push_back(std::move(attribute));
      }

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
          if (attribute.Accessor < 0 ||
              static_cast<size_t>(attribute.Accessor) >= Accessors_.size()) {
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
          if (Accessors_[static_cast<size_t>(base)].Count !=
              Accessors_[static_cast<size_t>(attribute.Accessor)].Count) {
            return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " morph target " +
                          Number(t) + " carries " +
                          Number(Accessors_[static_cast<size_t>(attribute.Accessor)].Count) + " " +
                          attribute.Semantic + " deltas over " +
                          Number(Accessors_[static_cast<size_t>(base)].Count) + " vertices");
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

      if (!mesh.Primitives.empty() &&
          mesh.Primitives[0].Targets.size() != primitive.Targets.size()) {
        return Refuse("mesh " + Number(i) + " primitive " + Number(p) + " declares " +
                      Number(primitive.Targets.size()) +
                      " morph targets and primitive 0 declares " +
                      Number(mesh.Primitives[0].Targets.size()) +
                      ", and one weight run drives every primitive of a mesh");
      }
      mesh.Primitives.push_back(std::move(primitive));
    }
    if (!mesh.Weights.empty() && !mesh.Primitives.empty() &&
        mesh.Weights.size() != mesh.Primitives[0].Targets.size()) {
      return Refuse("mesh " + Number(i) + " declares " + Number(mesh.Weights.size()) +
                    " weights over " + Number(mesh.Primitives[0].Targets.size()) +
                    " morph targets");
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
    if (!(camera.ZNearM > 0.0)) {
      return Refuse("camera " + Number(i) + " declares znear " + Number(camera.ZNearM) +
                    ", and a near plane at or behind the eye bounds nothing");
    }
    if (camera.ZFarM != 0.0 && !(camera.ZFarM > camera.ZNearM)) {
      return Refuse("camera " + Number(i) + " declares zfar " + Number(camera.ZFarM) +
                    " against a znear of " + Number(camera.ZNearM) +
                    ", and a far plane that does not lie beyond the near one encloses no volume");
    }
    if (camera.Kind == CameraKind::Orthographic && (camera.XMagM == 0.0 || camera.YMagM == 0.0)) {
      return Refuse("camera " + Number(i) + " declares an orthographic magnification of " +
                    Number(camera.XMagM) + " by " + Number(camera.YMagM) +
                    ", and a zero magnification collapses the picture to a line");
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
        return Refuse("node " + Number(i) +
                      " carries both a matrix and a TRS component, which glTF 2.0 forbids");
      }
      if (matrix.Size() != 16) {
        return Refuse("node " + Number(i) + " has a matrix of " + Number(matrix.Size()) +
                      " numbers");
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

    const Json::Ref instancing = declaration["extensions"][kMeshGpuInstancing]["attributes"];
    if (instancing.Valid()) {
      node.InstanceTranslation =
          instancing["TRANSLATION"].Valid() ? instancing["TRANSLATION"].Int(-1) : -1;
      node.InstanceRotation = instancing["ROTATION"].Valid() ? instancing["ROTATION"].Int(-1) : -1;
      node.InstanceScale = instancing["SCALE"].Valid() ? instancing["SCALE"].Int(-1) : -1;
      const int named[3] = {node.InstanceTranslation, node.InstanceRotation, node.InstanceScale};
      size_t count = 0;
      for (const int accessor : named) {
        if (accessor < 0) { continue; }
        if (static_cast<size_t>(accessor) >= Accessors_.size()) {
          return Refuse("node " + Number(i) + " instances on accessor " +
                        Number(static_cast<size_t>(accessor)) + " of " + Number(Accessors_.size()));
        }
        const size_t here = Accessors_[static_cast<size_t>(accessor)].Count;

        if (count != 0 && here != count) {
          return Refuse("node " + Number(i) +
                        " instances on accessors of different lengths, and the extension requires "
                        "one count");
        }
        count = here;
      }
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
      return Refuse("node " + Number(i) + " names camera " +
                    Number(static_cast<size_t>(node.Camera)) + " of " + Number(Cameras_.size()));
    }
    if (node.Light >= 0 && static_cast<size_t>(node.Light) >= Lights_.size()) {
      return Refuse("node " + Number(i) + " names " + kLightsPunctual + " light " +
                    Number(static_cast<size_t>(node.Light)) + " of " + Number(Lights_.size()));
    }
    for (const int child : node.Children) {
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

  {
    std::vector<uint8_t> rooted(Nodes_.size(), 0);
    std::vector<int> walked;
    for (size_t i = 0; i < Nodes_.size(); ++i) {
      walked.clear();
      size_t steps = 0;
      int at = static_cast<int>(i);
      while (at >= 0 && rooted[static_cast<size_t>(at)] == 0) {
        if (++steps > Nodes_.size()) {
          return Refuse("node " + Number(i) +
                        " never reaches a root, and a glTF hierarchy is a forest -- the "
                        "chain of parents is a cycle");
        }
        walked.push_back(at);
        at = Parent_[static_cast<size_t>(at)];
      }
      for (const int seen : walked) { rooted[static_cast<size_t>(seen)] = 1; }
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
      if (Parent_[static_cast<size_t>(node)] >= 0) {
        return Refuse("scene " + Number(i) + " names node " + Number(static_cast<size_t>(node)) +
                      " as a root while node " +
                      Number(static_cast<size_t>(Parent_[static_cast<size_t>(node)])) +
                      " carries it as a child -- the spec's scene nodes are root nodes");
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

  MorphAt_.assign(Nodes_.size() + 1, 0);
  for (size_t node = 0; node < Nodes_.size(); ++node) {
    size_t count = 0;
    const int mesh = Nodes_[node].Mesh;
    if (mesh >= 0 && static_cast<size_t>(mesh) < Meshes_.size() &&
        !Meshes_[static_cast<size_t>(mesh)].Primitives.empty()) {
      count = Meshes_[static_cast<size_t>(mesh)].Primitives[0].Targets.size();
    }
    MorphAt_[node + 1] = MorphAt_[node] + count;
  }
  if (!ReadSkins(root)) { return false; }
  if (!ReadAnimations(json)) { return false; }
  return true;
}

bool Document::ReadSkins(const Json::Ref &root) {
  const Json::Ref skins = root["skins"];
  for (size_t i = 0; i < skins.Size(); ++i) {
    const Json::Ref declaration = skins[i];
    Skin skin;
    skin.Name = declaration["name"].Str("");
    skin.Skeleton = declaration["skeleton"].Valid() ? declaration["skeleton"].Int(-1) : -1;
    const Json::Ref joints = declaration["joints"];
    if (joints.Size() == 0) {
      return Refuse("skin " + Number(i) +
                    " names no joint, and a skin with no joint deforms nothing");
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
      return Refuse("node " + Number(i) +
                    " names a skin and carries no mesh, and glTF states a "
                    "skin is only meaningful on the node that instantiates the geometry");
    }
  }
  return true;
}

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
      } else if (path == "pointer") {
        const std::string pointer = target["extensions"][kAnimationPointer]["pointer"].Str("");
        UndrivenReason why = UndrivenReason::PointerUnparsed;
        if (!ResolveMaterialPointer(pointer, channel, why)) {
          animation.Undriven.push_back(UndrivenChannel{.Pointer = pointer, .Why = why});
          continue;
        }
      } else {
        return Refuse("animation " + Number(i) + " channel " + Number(c) + " drives '" + path +
                      "', which is none of translation, rotation, scale, weights or pointer");
      }
      const size_t components = PathComponents(channel.Path);
      const Accessor &values = Accessors_[static_cast<size_t>(
          animation.Samplers[static_cast<size_t>(channel.Sampler)].Output)];
      if (components > 0 && ElementComponents(values.Element) != components) {
        return Refuse("animation " + Number(i) + " channel " + Number(c) + " drives '" + path +
                      "', which is " + Number(components) + " components, from an output of " +
                      Number(ElementComponents(values.Element)));
      }
      if (channel.Path != AnimationPath::Weights) {
        const AnimationSampler &driving = animation.Samplers[static_cast<size_t>(channel.Sampler)];
        const Accessor &grid = Accessors_[static_cast<size_t>(driving.Input)];
        const size_t perKeyframe = driving.How == Interpolation::CubicSpline ? 3u : 1u;
        if (values.Count != grid.Count * perKeyframe) {
          return Refuse("animation " + Number(i) + " channel " + Number(c) + " drives '" + path +
                        "' with " + Number(values.Count) + " outputs over " + Number(grid.Count) +
                        " keyframes, and the spec demands exactly " +
                        Number(grid.Count * perKeyframe));
        }
      }
      animation.Channels.push_back(channel);
    }
    Animations_.push_back(std::move(animation));
  }
  return true;
}

bool Document::ReadLights(const Json &json) {
  const Json::Ref declared = json.Root()["extensions"][kLightsPunctual]["lights"];
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
          static_cast<float>(cone["outerConeAngle"].Num(0.25 * std::numbers::pi));
      if (!(light.Light.InnerConeRad >= 0.0f) ||
          !(light.Light.InnerConeRad < light.Light.OuterConeRad) ||
          !(light.Light.OuterConeRad <= 0.5f * std::numbers::pi_v<float>)) {
        return Refuse(std::string(kLightsPunctual) + " spot light " + Number(i) +
                      " declares an inner cone that is not below its outer cone inside [0, pi/2]");
      }
    }
    Lights_.push_back(std::move(light));
  }
  return true;
}

bool Document::ReadVariants(const Json &json) {
  const Json::Ref declared = json.Root()["extensions"][kMaterialsVariants];
  if (!declared.Valid()) { return true; }
  if (declared.GetKind() != Json::Kind::Object) {
    return Refuse(std::string(kMaterialsVariants) +
                  " is declared as something other than an object");
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

bool Document::ReadVariantMappings(const Json::Ref &declared,
                                   size_t mesh,
                                   size_t primitive,
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
    if (wears < 0 || static_cast<size_t>(wears) >= Materials_.size()) {
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
      if (variant < 0 || static_cast<size_t>(variant) >= Variants_.size()) {
        return Refuse(where + " mapping " + Number(i) + " names variant " +
                      std::to_string(variant) + " of " + Number(Variants_.size()));
      }
      if (into.VariantMaterials[static_cast<size_t>(variant)] >= 0) {
        return Refuse(where + " maps variant '" + Variants_[static_cast<size_t>(variant)] +
                      "' to material " +
                      std::to_string(into.VariantMaterials[static_cast<size_t>(variant)]) +
                      " and to material " + std::to_string(wears) +
                      ", and the extension states each variant index no more than once");
      }
      into.VariantMaterials[static_cast<size_t>(variant)] = wears;
    }
  }
  return true;
}

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
      std::string_view payload;
      if (!Base64Payload(image.Uri, payload)) {
        return Refuse("image " + Number(i) +
                      " is a data: URI that declares no ;base64 payload, and this reader "
                      "carries no other encoding");
      }
      std::vector<uint8_t> bytes;
      if (!DecodeBase64(payload, bytes)) {
        return Refuse("image " + Number(i) +
                      " is a data: URI whose base64 payload holds a character the alphabet "
                      "does not, or a length no whole byte count can come from");
      }
      if (bytes.empty()) {
        return Refuse("image " + Number(i) + " is a data: URI that decodes to no bytes");
      }
      BufferView held;
      held.Buffer = Buffers_.size();
      held.ByteOffset = 0;
      held.ByteLength = bytes.size();
      Buffers_.push_back(std::move(bytes));
      image.View = static_cast<int>(Views_.size());
      Views_.push_back(held);
      image.Uri.clear();
    }
    Images_.push_back(std::move(image));
  }

  const Json::Ref textures = root["textures"];
  for (size_t i = 0; i < textures.Size(); ++i) {
    const Json::Ref declaration = textures[i];
    Texture texture;
    texture.Name = declaration["name"].Str("");
    texture.Source = declaration["source"].Valid() ? declaration["source"].Int(-1) : -1;

    const Json::Ref webp = declaration["extensions"][kTextureWebp]["source"];
    if (webp.Valid()) { texture.Source = webp.Int(-1); }
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
    for (float &channel : material.Surface.BaseColour) { channel = 1.0f; }
  }
  material.Surface.Metalness = static_cast<float>(pbr["metallicFactor"].Num(1.0));
  material.Surface.Roughness = static_cast<float>(pbr["roughnessFactor"].Num(1.0));

  const Json::Ref emissive = declaration["emissiveFactor"];
  for (size_t k = 0; k < 3 && k < emissive.Size(); ++k) {
    material.Surface.Emission[k] = static_cast<float>(emissive[k].Num(0.0));
  }

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

  const Json::Ref transmission = declaration["extensions"][kTransmission];
  if (transmission.Valid()) {
    const Json::Ref factor = transmission["transmissionFactor"];
    if (factor.Valid()) {
      if (factor.GetKind() != Json::Kind::Number || !(factor.Num() >= 0.0) || factor.Num() > 1.0) {
        return Refuse("material " + Number(index) +
                      " declares a transmissionFactor outside [0, 1]");
      }
      material.Surface.Transmission = static_cast<float>(factor.Num());
    }
  }

  const Json::Ref volume = declaration["extensions"][kVolume];
  if (volume.Valid()) {
    const Json::Ref thickness = volume["thicknessFactor"];
    if (thickness.Valid()) {
      if (thickness.GetKind() != Json::Kind::Number || !(thickness.Num() >= 0.0)) {
        return Refuse("material " + Number(index) + " declares a thicknessFactor below 0");
      }
      material.Surface.Thickness = static_cast<float>(thickness.Num());
    }
    const Json::Ref distance = volume["attenuationDistance"];
    if (distance.Valid()) {
      if (distance.GetKind() != Json::Kind::Number || !(distance.Num() > 0.0)) {
        return Refuse("material " + Number(index) +
                      " declares an attenuationDistance that is not above 0");
      }
      material.Surface.AttenuationDistance = static_cast<float>(distance.Num());
    }
    const Json::Ref colour = volume["attenuationColor"];
    if (colour.Valid()) {
      if (colour.GetKind() != Json::Kind::Array || colour.Size() != 3) {
        return Refuse("material " + Number(index) +
                      " declares an attenuationColor that is not three numbers");
      }
      for (size_t channel = 0; channel < 3; ++channel) {
        const Json::Ref component = colour[channel];
        if (component.GetKind() != Json::Kind::Number || !(component.Num() >= 0.0)) {
          return Refuse("material " + Number(index) +
                        " declares an attenuationColor component below 0");
        }
        material.Surface.AttenuationColour[channel] = static_cast<float>(component.Num());
      }
    }
  }

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

  const Json::Ref clearcoat = declaration["extensions"][kClearcoat];
  if (clearcoat.Valid()) {
    const Json::Ref factor = clearcoat["clearcoatFactor"];
    if (factor.Valid()) {
      if (factor.GetKind() != Json::Kind::Number || !(factor.Num() >= 0.0) || factor.Num() > 1.0) {
        return Refuse("material " + Number(index) + " declares a clearcoatFactor outside [0, 1]");
      }
      material.Surface.Clearcoat = static_cast<float>(factor.Num());
    }
    const Json::Ref rough = clearcoat["clearcoatRoughnessFactor"];
    if (rough.Valid()) {
      if (rough.GetKind() != Json::Kind::Number || !(rough.Num() >= 0.0) || rough.Num() > 1.0) {
        return Refuse("material " + Number(index) +
                      " declares a clearcoatRoughnessFactor outside [0, 1]");
      }
      material.Surface.ClearcoatRoughness = static_cast<float>(rough.Num());
    }
  }

  const Json::Ref anisotropy = declaration["extensions"][kAnisotropy];
  if (anisotropy.Valid()) {
    const Json::Ref strengthOf = anisotropy["anisotropyStrength"];
    if (strengthOf.Valid()) {
      if (strengthOf.GetKind() != Json::Kind::Number || !(strengthOf.Num() >= 0.0) ||
          strengthOf.Num() > 1.0) {
        return Refuse("material " + Number(index) +
                      " declares an anisotropyStrength outside [0, 1]");
      }
      material.Surface.Anisotropy = static_cast<float>(strengthOf.Num());
    }
    const Json::Ref turn = anisotropy["anisotropyRotation"];
    if (turn.Valid()) {
      if (turn.GetKind() != Json::Kind::Number) {
        return Refuse("material " + Number(index) +
                      " declares an anisotropyRotation that is not a number");
      }
      material.Surface.AnisotropyRotationRad = static_cast<float>(turn.Num());
    }
  }

  const Json::Ref iridescence = declaration["extensions"][kIridescence];
  if (iridescence.Valid()) {
    const Json::Ref factor = iridescence["iridescenceFactor"];
    if (factor.Valid()) {
      if (factor.GetKind() != Json::Kind::Number || !(factor.Num() >= 0.0) || factor.Num() > 1.0) {
        return Refuse("material " + Number(index) +
                      " declares an iridescenceFactor outside [0, 1]");
      }
      material.Surface.Iridescence = static_cast<float>(factor.Num());
    }
    const Json::Ref filmIor = iridescence["iridescenceIor"];
    if (filmIor.Valid()) {
      if (filmIor.GetKind() != Json::Kind::Number || !(filmIor.Num() >= 1.0)) {
        return Refuse("material " + Number(index) + " declares an iridescenceIor below 1");
      }
      material.Surface.IridescenceIor = static_cast<float>(filmIor.Num());
    }
    const Json::Ref least = iridescence["iridescenceThicknessMinimum"];
    if (least.Valid()) {
      if (least.GetKind() != Json::Kind::Number || !(least.Num() >= 0.0)) {
        return Refuse("material " + Number(index) +
                      " declares an iridescenceThicknessMinimum below 0");
      }
      material.Surface.IridescenceThicknessMinNm = static_cast<float>(least.Num());
    }
    const Json::Ref most = iridescence["iridescenceThicknessMaximum"];
    if (most.Valid()) {
      if (most.GetKind() != Json::Kind::Number || !(most.Num() >= 0.0)) {
        return Refuse("material " + Number(index) +
                      " declares an iridescenceThicknessMaximum below 0");
      }
      material.Surface.IridescenceThicknessMaxNm = static_cast<float>(most.Num());
    }
  }
  const Json::Ref strength = declaration["extensions"][kEmissiveStrength]["emissiveStrength"];
  if (strength.Valid()) {
    if (strength.GetKind() != Json::Kind::Number) {
      return Refuse("material " + Number(index) +
                    " declares an emissiveStrength that is not a "
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

  const Json::Ref unlit = declaration["extensions"][kUnlit];
  if (unlit.Valid()) {
    if (unlit.GetKind() != Json::Kind::Object) {
      return Refuse("material " + Number(index) +
                    " declares KHR_materials_unlit as something "
                    "other than an object, and the extension defines an empty object");
    }
    material.Surface.Unlit = true;
  }

  const std::string mode = declaration["alphaMode"].Str("");
  if (!KnownAlphaMode(mode, material.Surface.Alpha)) {
    return Refuse("material " + Number(index) + " has alphaMode '" + mode +
                  "', and glTF 2.0 has "
                  "OPAQUE, MASK and BLEND");
  }
  material.Surface.CoverageCut = static_cast<float>(declaration["alphaCutoff"].Num(0.5));

  const struct {
    Json::Ref Under;
    const char *Slot;
    TextureRef *Into;
  } slots[] = {
      {.Under = pbr, .Slot = "baseColorTexture", .Into = &material.BaseColour},
      {.Under = pbr, .Slot = "metallicRoughnessTexture", .Into = &material.MetallicRoughness},
      {.Under = declaration, .Slot = "normalTexture", .Into = &material.Normal},
      {.Under = declaration, .Slot = "occlusionTexture", .Into = &material.Occlusion},
      {.Under = declaration, .Slot = "emissiveTexture", .Into = &material.Emissive},
      {.Under = specular, .Slot = "specularTexture", .Into = &material.SpecularStrength},
      {.Under = specular, .Slot = "specularColorTexture", .Into = &material.SpecularTint},
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

bool Document::BoundsHold(int accessorIndex, std::string &why) const {
  const Accessor &accessor = Accessors_[static_cast<size_t>(accessorIndex)];
  const size_t components = ElementComponents(accessor.Element);
  if (accessor.Min.size() != components || accessor.Max.size() != components) { return true; }
  std::vector<double> held;
  if (!ReadElements(accessorIndex, held)) { return true; }
  if (held.size() != accessor.Count * components) { return true; }

  const bool single = accessor.Component == ComponentType::Float32 && !accessor.Normalized;
  const auto narrow = [single](double value) {
    return single ? static_cast<double>(static_cast<float>(value)) : value;
  };
  const auto declaredAs = [&accessor](double value) {
    return accessor.Normalized ? Normalise(value, accessor.Component) : value;
  };
  for (size_t component = 0; component < components; ++component) {
    double least = 0.0;
    double most = 0.0;
    for (size_t element = 0; element < accessor.Count; ++element) {
      const double value = narrow(held[element * components + component]);
      if (element == 0 || value < least) { least = value; }
      if (element == 0 || value > most) { most = value; }
    }
    const double declaredLeast = narrow(declaredAs(accessor.Min[component]));
    const double declaredMost = narrow(declaredAs(accessor.Max[component]));
    if (accessor.Count > 0 && (least < declaredLeast || most > declaredMost)) {
      why = "carries an element outside the bounds it declares: component " + Number(component) +
            " runs " + Number(least) + " to " + Number(most) + " over a declared " +
            Number(declaredLeast) + " to " + Number(declaredMost);
      return false;
    }
    if (accessor.Count > 0 && (least != declaredLeast || most != declaredMost)) {
      why = "declares bounds its data does not meet: component " + Number(component) + " runs " +
            Number(least) + " to " + Number(most) + " and the accessor declares " +
            Number(declaredLeast) + " to " + Number(declaredMost) +
            " -- glTF 2.0 asks for the actual componentwise extremes, not a box around them";
      return false;
    }
  }
  return true;
}

bool Document::ReadElements(int accessorIndex, std::vector<double> &out) const {
  out.clear();
  if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= Accessors_.size()) {
    return false;
  }
  const Accessor &accessor = Accessors_[static_cast<size_t>(accessorIndex)];
  const size_t components = ElementComponents(accessor.Element);
  const size_t rows = ElementRows(accessor.Element);
  const size_t columns = ElementColumns(accessor.Element);
  const size_t componentBytes = ComponentBytes(accessor.Component);
  size_t stride = 0;
  size_t element = 0;
  if (!ElementBytes(accessor, stride, element)) { return false; }
  const size_t columnBytes = (columns > 1) ? element / columns : 0;

  Span<const uint8_t> span;
  if (accessor.View >= 0) {
    if (!ViewSpan(accessor.View, span)) { return false; }
    if (accessor.Count > 0) {
      if (accessor.ByteOffset > span.Size() || element > span.Size() - accessor.ByteOffset ||
          (accessor.Count - 1) > (span.Size() - accessor.ByteOffset - element) / stride) {
        return false;
      }
    }
  }

  if (accessor.View < 0) {
    constexpr size_t kFillOverCarried = 1024;
    size_t carriedBytes = 0;
    for (const std::vector<uint8_t> &buffer : Buffers_) { carriedBytes += buffer.size(); }
    if (accessor.Count > 0 &&
        (components == 0 ||
         accessor.Count > (carriedBytes * kFillOverCarried) / (components * sizeof(double)))) {
      return false;
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

  const size_t element = TightElementBytes(accessor.Element, accessor.Component);
  const size_t columnBytes = (columns > 1) ? element / columns : 0;
  const size_t indexBytes = ComponentBytes(sparse.IndicesComponent);
  if (sparse.IndicesComponent == ComponentType::Int8 ||
      sparse.IndicesComponent == ComponentType::Int16 ||
      sparse.IndicesComponent == ComponentType::Float32) {
    return false;
  }

  Span<const uint8_t> indices;
  Span<const uint8_t> values;
  if (!ViewSpan(sparse.IndicesBufferView, indices)) { return false; }
  if (!ViewSpan(sparse.ValuesBufferView, values)) { return false; }
  if (sparse.IndicesByteOffset > indices.Size() ||
      sparse.Count > (indices.Size() - sparse.IndicesByteOffset) / indexBytes) {
    return false;
  }
  if (sparse.ValuesByteOffset > values.Size() ||
      (element > 0 && sparse.Count > (values.Size() - sparse.ValuesByteOffset) / element)) {
    return false;
  }

  for (size_t k = 0; k < sparse.Count; ++k) {
    const double index = Component(indices.Data() + sparse.IndicesByteOffset + k * indexBytes,
                                   sparse.IndicesComponent);
    if (index < 0 || static_cast<size_t>(index) >= accessor.Count) { return false; }
    const uint8_t *at = values.Data() + sparse.ValuesByteOffset + k * element;
    for (size_t column = 0; column < columns; ++column) {
      for (size_t row = 0; row < rows; ++row) {
        const double raw =
            Component(at + column * columnBytes + row * componentBytes, accessor.Component);
        out[static_cast<size_t>(index) * components + column * rows + row] =
            accessor.Normalized ? Normalise(raw, accessor.Component) : raw;
      }
    }
  }
  return true;
}

bool Document::ReadIndices(int accessorIndex, std::vector<uint32_t> &out) const {
  out.clear();
  if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= Accessors_.size()) {
    return false;
  }
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
    const auto index = static_cast<size_t>(chain[i - 1]);
    const Node &step = Nodes_[index];
    const Transform local =
        (posed != nullptr)
            ? posed[index]
            : (step.HasMatrix ? Transform::FromColumnMajor(step.Matrix)
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
