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

} // namespace

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
    const std::string uri = buffer["uri"].Str("");
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

  const Json::Ref meshes = root["meshes"];
  for (size_t i = 0; i < meshes.Size(); ++i) {
    const Json::Ref declaration = meshes[i];
    Mesh mesh;
    mesh.Name = declaration["name"].Str("");
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
        primitive.Attributes.push_back(std::move(attribute));
      }
      if (primitive.Indices >= 0 && static_cast<size_t>(primitive.Indices) >= Accessors_.size()) {
        return Refuse("mesh " + Number(i) + " primitive " + Number(p) +
                      " names an index accessor the file does not carry");
      }
      mesh.Primitives.push_back(std::move(primitive));
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
  return true;
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
  if (node < 0 || static_cast<size_t>(node) >= Nodes_.size()) { return false; }
  out = Transform::Identity();
  std::vector<int> chain;
  for (int at = node, steps = 0; at >= 0; at = Parent_[static_cast<size_t>(at)], ++steps) {
    if (static_cast<size_t>(steps) > Nodes_.size()) { return false; }
    chain.push_back(at);
  }
  for (size_t i = chain.size(); i > 0; --i) {
    const Node &step = Nodes_[static_cast<size_t>(chain[i - 1])];
    const Transform local = step.HasMatrix
                                ? Transform::FromColumnMajor(step.Matrix)
                                : Transform::FromTrs(step.Translation, step.Rotation, step.Scale);
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
