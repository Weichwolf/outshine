#ifndef OUTSHINE_IMPORT_DOCUMENT_H
#define OUTSHINE_IMPORT_DOCUMENT_H

#include <limits>
#include <span>
#include <string_view>
#include <cstdint>
#include <string>
#include <vector>

#include "Viewport.h"
#include "Transform.h"
#include "Types.h"

#include "Json.h"

namespace outshine::Gltf {

class Document {
public:
  [[nodiscard]] bool Read(std::span<const uint8_t> whole, std::string_view path);
  [[nodiscard]] bool ReadFile(std::string_view path);

  [[nodiscard]] const std::string &Error() const { return Error_; }

  [[nodiscard]] const std::string &Path() const { return Path_; }

  [[nodiscard]] const std::string &Version() const { return Version_; }

  [[nodiscard]] const std::string &MinVersion() const { return MinVersion_; }

  [[nodiscard]] const std::vector<std::string> &ExtensionsRequired() const { return Required_; }

  [[nodiscard]] static bool Honours(std::string_view extension);

  [[nodiscard]] const std::vector<Accessor> &Accessors() const { return Accessors_; }

  [[nodiscard]] const std::vector<BufferView> &BufferViews() const { return Views_; }

  [[nodiscard]] const std::vector<MetadataPacket> &Metadata() const { return Metadata_; }

  [[nodiscard]] int MetadataOfAsset() const { return AssetMetadata_; }

  [[nodiscard]] const std::vector<MetadataUse> &MetadataUses() const { return MetadataUses_; }

  [[nodiscard]] int MetadataOf(MetadataCarrier carrier, size_t which) const {
    for (const MetadataUse &one : MetadataUses_) {
      if (one.Carrier == carrier && one.Which == which) { return static_cast<int>(one.Packet); }
    }
    return -1;
  }

  [[nodiscard]] const std::vector<Mesh> &Meshes() const { return Meshes_; }

  [[nodiscard]] const std::vector<Node> &Nodes() const { return Nodes_; }

  [[nodiscard]] const std::vector<Camera> &Cameras() const { return Cameras_; }

  [[nodiscard]] const std::vector<Scene> &Scenes() const { return Scenes_; }

  [[nodiscard]] const std::vector<Skin> &Skins() const { return Skins_; }

  [[nodiscard]] size_t MorphWeightsFirst(size_t node) const { return MorphAt_[node]; }

  [[nodiscard]] size_t MorphWeightsCount(size_t node) const {
    return MorphAt_[node + 1] - MorphAt_[node];
  }

  [[nodiscard]] size_t MorphWeightsTotal() const { return MorphAt_.empty() ? 0 : MorphAt_.back(); }

  [[nodiscard]] const std::vector<MaterialRef> &Materials() const { return Materials_; }

  [[nodiscard]] const std::vector<Texture> &Textures() const { return Textures_; }

  [[nodiscard]] const std::vector<Image> &Images() const { return Images_; }

  [[nodiscard]] const std::vector<Sampler> &Samplers() const { return Samplers_; }

  [[nodiscard]] const std::vector<LightRef> &Lights() const { return Lights_; }

  [[nodiscard]] const std::vector<std::string> &Variants() const { return Variants_; }

  [[nodiscard]] const std::vector<Animation> &Animations() const { return Animations_; }

  [[nodiscard]] int DefaultScene() const { return DefaultScene_; }

  [[nodiscard]] bool ImageBytes(int image, std::vector<uint8_t> &out) const;

  struct ElementSpan {
    size_t Stride = 0;
    size_t Element = 0;
  };

  static void DecodeElements(const Accessor &accessor,
                             std::span<const uint8_t> span,
                             ElementSpan over,
                             std::vector<double> &out);
  [[nodiscard]] bool ReadElements(int accessor, std::vector<double> &out) const;
  [[nodiscard]] static bool ComponentBoundHolds(const Accessor &accessor,
                                                std::span<const double> held,
                                                size_t component,
                                                std::string &why);
  [[nodiscard]] bool BoundsHold(int accessor, std::string &why) const;

  [[nodiscard]] bool ReadIndices(int accessor, std::vector<uint32_t> &out) const;

  [[nodiscard]] bool WorldTransform(int node, Transform &out) const;

  [[nodiscard]] bool
  WorldTransform(int node, std::span<const Transform> locals, Transform &out) const;

  [[nodiscard]] bool ViewTransform(int cameraNode, Transform &out) const;

private:
  struct Ranged {
    double Low = 0.0;
    double High = std::numeric_limits<double>::infinity();
    bool LowOpen = false;
  };

  [[nodiscard]] bool
  Factor(const Json::Ref &at, const char *named, size_t material, Ranged within, float &into);

  [[nodiscard]] bool
  ReadMaterialColours(const Json::Ref &declaration, size_t index, Material &into);
  [[nodiscard]] bool
  ReadMaterialTextures(const Json::Ref &declaration, size_t index, MaterialRef &material);

  struct MeshAt {
    size_t Mesh = 0;
    size_t Primitive = 0;
  };

  [[nodiscard]] static std::string Where(MeshAt at);
  [[nodiscard]] bool ReadPrimitive(const Json::Ref &declared, MeshAt at, Primitive &into);
  [[nodiscard]] bool
  ReadPrimitiveAttributes(const Json::Ref &attributes, MeshAt at, Primitive &into);
  [[nodiscard]] bool ReadMorphTargets(const Json::Ref &targets, MeshAt at, Primitive &into);

  [[nodiscard]] bool Refuse(std::string_view why);

  [[nodiscard]] bool Chain(int node, const Transform *posed, Transform &out) const;
  [[nodiscard]] bool
  ReadJson(const char *text, size_t length, const uint8_t *binaryChunk, size_t binaryLength);
  [[nodiscard]] bool
  ResolveBuffers(const Json &json, const uint8_t *binaryChunk, size_t binaryLength);

  struct CarriedBuffer {
    std::string Uri;
    std::string Directory;
    const uint8_t *Chunk = nullptr;
    size_t ChunkBytes = 0;
    size_t Declared = 0;
    size_t Index = 0;
  };

  struct GlbChunks {
    const uint8_t *Json = nullptr;
    size_t JsonBytes = 0;
    const uint8_t *Binary = nullptr;
    size_t BinaryBytes = 0;
  };

  [[nodiscard]] bool ReadBufferPayload(const CarriedBuffer &carried, std::vector<uint8_t> &bytes);
  [[nodiscard]] bool WalkGlbChunks(const uint8_t *bytes, size_t declared, GlbChunks &into);
  [[nodiscard]] bool ReadSamplers(const Json::Ref &root);
  [[nodiscard]] bool ReadImages(const Json::Ref &root);
  [[nodiscard]] bool ReadTextures(const Json::Ref &root);
  [[nodiscard]] bool ReadAppearance(const Json &json);
  [[nodiscard]] bool ReadLights(const Json &json);
  [[nodiscard]] bool ReadVariants(const Json &json);

  [[nodiscard]] bool
  ReadVariantMappings(const Json::Ref &declared, size_t mesh, size_t primitive, Primitive &into);
  [[nodiscard]] bool ReadAsset(const Json::Ref &root);
  [[nodiscard]] bool ReadMetadataPackets(const Json::Ref &root);
  [[nodiscard]] bool ReadMetadata(const Json::Ref &root);
  [[nodiscard]] bool ReadViews(const Json::Ref &root);
  [[nodiscard]] bool ReadSparseAccessor(const Json::Ref &sparse, size_t index, Accessor &accessor);
  [[nodiscard]] bool ReadAccessors(const Json::Ref &root);
  [[nodiscard]] bool HoldAccessorBounds();
  [[nodiscard]] bool ReadMeshes(const Json::Ref &root);
  [[nodiscard]] bool ReadCameras(const Json::Ref &root);
  [[nodiscard]] bool ReadNodeTransform(const Json::Ref &declaration, size_t index, Node &into);
  [[nodiscard]] bool ReadNodeInstancing(const Json::Ref &declaration, size_t index, Node &into);
  [[nodiscard]] bool HoldNodeReferences();
  [[nodiscard]] bool HoldNodeHierarchy();
  [[nodiscard]] bool ReadNodes(const Json::Ref &root);
  [[nodiscard]] bool ReadScenes(const Json::Ref &root);
  [[nodiscard]] bool ReadSkin(const Json::Ref &declaration, size_t index, Skin &skin);
  [[nodiscard]] bool HoldSkinReferences();
  [[nodiscard]] bool ReadSkins(const Json::Ref &root);

  struct TrackAt {
    size_t Animation = 0;
    size_t Sampler = 0;
  };

  struct ChannelAt {
    size_t Animation = 0;
    size_t Channel = 0;
  };

  [[nodiscard]] static std::string Where(TrackAt at);
  [[nodiscard]] static std::string Where(ChannelAt at);
  [[nodiscard]] bool
  ReadAnimationSampler(const Json::Ref &declared, TrackAt at, AnimationSampler &into);
  [[nodiscard]] bool ReadAnimationChannel(const Json::Ref &declared, ChannelAt at, Animation &into);
  [[nodiscard]] bool ReadAnimations(const Json &json);
  [[nodiscard]] bool ReadMaterial(const Json::Ref &declaration, size_t index);
  [[nodiscard]] bool ElementBytes(const Accessor &accessor, size_t &stride, size_t &element) const;
  [[nodiscard]] bool ViewSpan(int view, std::span<const uint8_t> &out) const;
  [[nodiscard]] bool ApplySparse(const Accessor &accessor, std::vector<double> &out) const;

  std::string Path_, Error_, Version_, MinVersion_;
  std::vector<std::string> Required_;

  bool Quantised_ = false;
  std::vector<std::vector<uint8_t>> Buffers_;
  std::vector<BufferView> Views_;
  std::vector<MetadataPacket> Metadata_;
  int AssetMetadata_ = -1;
  std::vector<MetadataUse> MetadataUses_;
  std::vector<Accessor> Accessors_;
  std::vector<Mesh> Meshes_;
  std::vector<Node> Nodes_;
  std::vector<Camera> Cameras_;
  std::vector<Scene> Scenes_;
  std::vector<Skin> Skins_;

  std::vector<size_t> MorphAt_;
  std::vector<MaterialRef> Materials_;
  std::vector<Texture> Textures_;
  std::vector<Image> Images_;
  std::vector<Sampler> Samplers_;
  std::vector<LightRef> Lights_;
  std::vector<std::string> Variants_;
  std::vector<Animation> Animations_;
  std::vector<int> Parent_;
  int DefaultScene_ = -1;
};

} // namespace outshine::Gltf
#endif
