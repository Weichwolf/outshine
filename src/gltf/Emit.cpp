#include "Emit.h"

#include <cmath>
#include <cstring>

namespace outshine::Gltf {

namespace {

constexpr uint32_t kGlbMagic = 0x46546C67;
constexpr uint32_t kGlbVersion = 2;
constexpr uint32_t kChunkJson = 0x4E4F534A;
constexpr uint32_t kChunkBinary = 0x004E4942;
constexpr int kFloat32 = 5126;
constexpr int kUInt32 = 5125;

template <class Scalar> void Append(std::vector<uint8_t> &out, Scalar value) {
  const auto *at = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), at, at + sizeof value);
}

void PadTo4(std::vector<uint8_t> &out, uint8_t filler) {
  while ((out.size() % 4) != 0) { out.push_back(filler); }
}

std::string Number(double value) {
  char text[40];
  std::snprintf(text, sizeof text, "%.9g", value);
  return text;
}

std::string Integer(size_t value) { return std::to_string(value); }

std::string Quoted(const std::string &text) {
  std::string out = "\"";
  for (const char raw : text) {
    const unsigned char code = static_cast<unsigned char>(raw);
    switch (raw) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (code < 0x20) {
          char escape[8];
          std::snprintf(escape, sizeof escape, "\\u%04x", code);
          out += escape;
        } else {
          out += raw;
        }
    }
  }
  return out + "\"";
}

const char *AlphaSpelling(AlphaMode mode) {
  switch (mode) {
    case AlphaMode::Opaque: return "OPAQUE";
    case AlphaMode::Masked: return "MASK";
    case AlphaMode::Blended: return "BLEND";
  }
  return "OPAQUE";
}

struct Attribute {
  const char *Semantic;
  size_t Components;
  const std::vector<double> *From;
};

class Writer {
public:
  Writer(const Emission &what, std::string &error)
      : What_(what), Subject_(*what.Geometry), Error_(error) {}

  [[nodiscard]] bool Run(std::vector<uint8_t> &glb) {
    if (!Admissible()) { return false; }
    std::string json = "{\"asset\":{\"version\":\"2.0\"";
    if (!What_.Generator.empty()) { json += ",\"generator\":" + Quoted(What_.Generator); }
    json += "}";
    json += Materials();
    json += Parts();
    json += ",\"buffers\":[{\"byteLength\":" + Integer(Binary_.size()) + "}]}";
    Container(json, glb);
    return true;
  }

private:
  [[nodiscard]] bool Refuse(const std::string &why) {
    Error_ = why;
    return false;
  }

  [[nodiscard]] bool Admissible() {
    if (Subject_.Parts().empty()) {
      return Refuse("the subject draws no part, and a file with no triangle is not a subject");
    }
    if (!Subject_.Lights().empty()) {
      return Refuse("the subject places " + Integer(Subject_.Lights().size()) +
                    " punctual lights, and this writer emits geometry and material only -- a "
                    "generator's product carries its light nowhere");
    }
    for (size_t part = 0; part < Subject_.Parts().size(); ++part) {
      const Part &drawn = Subject_.Parts()[part];
      if (drawn.Tangent == TangentSource::Generated) {
        return Refuse("part " + Integer(part) + " carries a GENERATED tangent basis, and writing it "
                      "as a supplied one would tell a consumer the author stated it -- which is "
                      "exactly what NormalTangentMirrorTest fails an engine for. The basis the "
                      "reader would regenerate needs the normal texture that produced it, and this "
                      "writer has no image bytes");
      }
      if (drawn.Material >= 0 && (size_t)drawn.Material >= What_.Materials.Size()) {
        return Refuse("part " + Integer(part) + " names material " + Integer((size_t)drawn.Material) +
                      " over a table of " + Integer(What_.Materials.Size()));
      }
    }
    for (size_t index = 0; index < What_.Materials.Size(); ++index) {
      if (!Statable(What_.Materials[index], index)) { return false; }
    }
    return true;
  }

  [[nodiscard]] bool Statable(const MaterialRef &material, size_t index) {
    const struct {
      const TextureRef &Slot;
      const char *Name;
    } images[] = {{material.BaseColour, "baseColorTexture"},
                  {material.MetallicRoughness, "metallicRoughnessTexture"},
                  {material.Normal, "normalTexture"},
                  {material.Occlusion, "occlusionTexture"},
                  {material.Emissive, "emissiveTexture"}};
    for (const auto &image : images) {
      if (image.Slot.Declared()) {
        return Refuse("material " + Integer(index) + " declares a " + image.Name +
                      ", and this writer emits no image: a generated surface's texture is produced "
                      "by a generator in this tree and has no bytes here to point at");
      }
    }
    if (material.Surface.Transmission > 0.0f) {
      return Refuse("material " + Integer(index) + " declares transmission " +
                    Number(material.Surface.Transmission) +
                    ", which glTF states only through KHR_materials_transmission");
    }
    for (const float channel : material.Surface.Emission) {
      if (channel > 1.0f) {
        return Refuse("material " + Integer(index) + " emits " + Number(channel) +
                      ", and the core format caps emissiveFactor at 1 -- the extension that lifts "
                      "it multiplies two numbers whose product does not round-trip exactly");
      }
    }
    return true;
  }

  std::string Materials() {
    if (What_.Materials.Empty()) { return ""; }
    std::string json = ",\"materials\":[";
    bool unlit = false;
    for (size_t index = 0; index < What_.Materials.Size(); ++index) {
      const MaterialRef &material = What_.Materials[index];
      const Material &surface = material.Surface;
      if (index > 0) { json += ","; }
      json += "{";
      if (!material.Name.empty()) { json += "\"name\":" + Quoted(material.Name) + ","; }
      json += "\"pbrMetallicRoughness\":{\"baseColorFactor\":[";
      for (int channel = 0; channel < 4; ++channel) {
        json += (channel > 0 ? "," : "") + Number(surface.BaseColour[channel]);
      }
      json += "],\"metallicFactor\":" + Number(surface.Metalness);
      json += ",\"roughnessFactor\":" + Number(surface.Roughness) + "}";
      json += ",\"emissiveFactor\":[" + Number(surface.Emission[0]) + "," +
              Number(surface.Emission[1]) + "," + Number(surface.Emission[2]) + "]";
      json += ",\"alphaMode\":\"" + std::string(AlphaSpelling(surface.Alpha)) + "\"";
      json += ",\"alphaCutoff\":" + Number(surface.CoverageCut);
      json += ",\"doubleSided\":" + std::string(surface.DoubleSided ? "true" : "false");
      if (surface.Unlit) {
        json += ",\"extensions\":{\"KHR_materials_unlit\":{}}";
        unlit = true;
      }
      json += "}";
    }
    json += "]";

    if (unlit) { json += ",\"extensionsUsed\":[\"KHR_materials_unlit\"]"; }
    return json;
  }

  std::string Parts() {
    std::string nodes = ",\"nodes\":[";
    std::string meshes = ",\"meshes\":[";
    std::string roots;
    for (size_t part = 0; part < Subject_.Parts().size(); ++part) {
      const Part &drawn = Subject_.Parts()[part];
      if (part > 0) {
        nodes += ",";
        meshes += ",";
        roots += ",";
      }
      roots += Integer(part);
      nodes += "{\"mesh\":" + Integer(part);
      if (!drawn.NodeName.empty()) { nodes += ",\"name\":" + Quoted(drawn.NodeName); }
      nodes += "}";
      meshes += "{\"primitives\":[{\"attributes\":{" + Streams(drawn) + "},\"indices\":" +
                Integer(Indices(drawn));
      if (drawn.Material >= 0) { meshes += ",\"material\":" + Integer((size_t)drawn.Material); }
      meshes += "}]}";
    }
    return nodes + "]" + meshes + "]" + ",\"scene\":0,\"scenes\":[{\"nodes\":[" + roots + "]}]" +
           ",\"accessors\":[" + Accessors_ + "],\"bufferViews\":[" + Views_ + "]";
  }

  std::string Streams(const Part &drawn) {
    const Attribute declared[] = {
        {"POSITION", 3, &Subject_.PositionsM()},
        {"NORMAL", 3, drawn.HasNormal ? &Subject_.Normals() : nullptr},
        {"TEXCOORD_0", 2, drawn.HasUv ? &Subject_.Uv() : nullptr},
        {"TEXCOORD_1", 2, drawn.HasUv1 ? &Subject_.Uv1() : nullptr},
        {"TANGENT", 4, drawn.Tangent == TangentSource::Supplied ? &Subject_.Tangents() : nullptr},

        {"COLOR_0", 4, drawn.HasColour ? &Subject_.Colours() : nullptr}};
    std::string json;
    for (const Attribute &attribute : declared) {
      if (attribute.From == nullptr) { continue; }
      if (!json.empty()) { json += ","; }
      json += std::string("\"") + attribute.Semantic + "\":" + Integer(Vertices(drawn, attribute));
    }
    return json;
  }

  size_t Vertices(const Part &drawn, const Attribute &attribute) {
    const size_t at = Binary_.size();
    std::vector<double> lowest(attribute.Components, 0.0);
    std::vector<double> highest(attribute.Components, 0.0);
    for (size_t vertex = 0; vertex < drawn.VertexCount; ++vertex) {
      for (size_t component = 0; component < attribute.Components; ++component) {
        const float value = static_cast<float>(
            (*attribute.From)[(drawn.FirstVertex + vertex) * attribute.Components + component]);
        Append(Binary_, value);
        if (vertex == 0 || (double)value < lowest[component]) { lowest[component] = value; }
        if (vertex == 0 || (double)value > highest[component]) { highest[component] = value; }
      }
    }
    const bool bounded = std::strcmp(attribute.Semantic, "POSITION") == 0;
    return Accessor(View(at, Binary_.size() - at), kFloat32,
                    attribute.Components == 2 ? "VEC2"
                                              : (attribute.Components == 4 ? "VEC4" : "VEC3"),
                    drawn.VertexCount, bounded ? &lowest : nullptr,
                    bounded ? &highest : nullptr);
  }

  size_t Indices(const Part &drawn) {
    const size_t at = Binary_.size();
    for (size_t corner = 0; corner < drawn.IndexCount; ++corner) {
      Append(Binary_,
             static_cast<uint32_t>(Subject_.Indices()[drawn.FirstIndex + corner] - drawn.FirstVertex));
    }
    return Accessor(View(at, Binary_.size() - at), kUInt32, "SCALAR", drawn.IndexCount, nullptr,
                    nullptr);
  }

  size_t View(size_t at, size_t length) {
    if (!Views_.empty()) { Views_ += ","; }
    Views_ += "{\"buffer\":0,\"byteOffset\":" + Integer(at) + ",\"byteLength\":" + Integer(length) +
              "}";
    return ViewCount_++;
  }

  size_t Accessor(size_t view, int component, const char *element, size_t count,
                  const std::vector<double> *lowest, const std::vector<double> *highest) {
    if (!Accessors_.empty()) { Accessors_ += ","; }
    Accessors_ += "{\"bufferView\":" + Integer(view) +
                  ",\"componentType\":" + Integer((size_t)component) + ",\"count\":" +
                  Integer(count) + ",\"type\":\"" + element + "\"";
    if (lowest != nullptr && highest != nullptr) {
      Accessors_ += ",\"min\":[";
      for (size_t axis = 0; axis < lowest->size(); ++axis) {
        Accessors_ += (axis > 0 ? "," : "") + Number((*lowest)[axis]);
      }
      Accessors_ += "],\"max\":[";
      for (size_t axis = 0; axis < highest->size(); ++axis) {
        Accessors_ += (axis > 0 ? "," : "") + Number((*highest)[axis]);
      }
      Accessors_ += "]";
    }
    Accessors_ += "}";
    return AccessorCount_++;
  }

  void Container(const std::string &json, std::vector<uint8_t> &glb) {
    std::vector<uint8_t> text(json.begin(), json.end());
    PadTo4(text, ' ');
    PadTo4(Binary_, 0);
    glb.clear();
    Append(glb, kGlbMagic);
    Append(glb, kGlbVersion);
    Append(glb, static_cast<uint32_t>(12 + 8 + text.size() + 8 + Binary_.size()));
    Append(glb, static_cast<uint32_t>(text.size()));
    Append(glb, kChunkJson);
    glb.insert(glb.end(), text.begin(), text.end());
    Append(glb, static_cast<uint32_t>(Binary_.size()));
    Append(glb, kChunkBinary);
    glb.insert(glb.end(), Binary_.begin(), Binary_.end());
  }

  const Emission &What_;
  const Subject &Subject_;
  std::string &Error_;
  std::vector<uint8_t> Binary_;
  std::string Views_, Accessors_;
  size_t ViewCount_ = 0, AccessorCount_ = 0;
};

}

bool Emit(const Emission &what, std::vector<uint8_t> &glb, std::string &error) {
  error.clear();
  glb.clear();
  if (what.Geometry == nullptr) {
    error = "the emission names no subject";
    return false;
  }
  return Writer(what, error).Run(glb);
}

}
