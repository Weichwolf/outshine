#ifndef GLTF_DOCUMENT_H
#define GLTF_DOCUMENT_H

#include <cstdint>
#include <string>
#include <vector>

#include "Span.h"

#include "Camera.h"
#include "Transform.h"
#include "Types.h"

#include "Json.h"

namespace outshine::Gltf {

class Document {
public:

  [[nodiscard]] bool Read(Span<const uint8_t> bytes, const std::string &path);
  [[nodiscard]] bool ReadFile(const std::string &path);

  const std::string &Error() const { return Error_; }
  const std::string &Path() const { return Path_; }
  const std::string &Version() const { return Version_; }

  const std::string &MinVersion() const { return MinVersion_; }
  const std::vector<std::string> &ExtensionsRequired() const { return Required_; }

  [[nodiscard]] static bool Honours(const std::string &extension);

  const std::vector<Accessor> &Accessors() const { return Accessors_; }
  const std::vector<BufferView> &BufferViews() const { return Views_; }
  const std::vector<Mesh> &Meshes() const { return Meshes_; }
  const std::vector<Node> &Nodes() const { return Nodes_; }
  const std::vector<Camera> &Cameras() const { return Cameras_; }
  const std::vector<Scene> &Scenes() const { return Scenes_; }
  const std::vector<Skin> &Skins() const { return Skins_; }

  size_t MorphWeightsFirst(size_t node) const { return MorphAt_[node]; }
  size_t MorphWeightsCount(size_t node) const { return MorphAt_[node + 1] - MorphAt_[node]; }
  size_t MorphWeightsTotal() const { return MorphAt_.empty() ? 0 : MorphAt_.back(); }
  const std::vector<MaterialRef> &Materials() const { return Materials_; }
  const std::vector<Texture> &Textures() const { return Textures_; }
  const std::vector<Image> &Images() const { return Images_; }
  const std::vector<Sampler> &Samplers() const { return Samplers_; }

  const std::vector<LightRef> &Lights() const { return Lights_; }

  const std::vector<std::string> &Variants() const { return Variants_; }

  const std::vector<Animation> &Animations() const { return Animations_; }
  int DefaultScene() const { return DefaultScene_; }

  [[nodiscard]] bool ImageBytes(int image, std::vector<uint8_t> &out) const;

  [[nodiscard]] bool ReadElements(int accessor, std::vector<double> &out) const;

  [[nodiscard]] bool ReadIndices(int accessor, std::vector<uint32_t> &out) const;

  [[nodiscard]] bool WorldTransform(int node, Transform &out) const;

  [[nodiscard]] bool WorldTransform(int node, Span<const Transform> locals, Transform &out) const;

  [[nodiscard]] bool ViewTransform(int cameraNode, Transform &out) const;

private:
  [[nodiscard]] bool Refuse(const std::string &why);

  [[nodiscard]] bool Chain(int node, const Transform *posed, Transform &out) const;
  [[nodiscard]] bool ReadJson(const char *text, size_t length, const uint8_t *binaryChunk,
                              size_t binaryLength);
  [[nodiscard]] bool ResolveBuffers(const Json &json, const uint8_t *binaryChunk,
                                    size_t binaryLength);
  [[nodiscard]] bool ReadAppearance(const Json &json);
  [[nodiscard]] bool ReadLights(const Json &json);
  [[nodiscard]] bool ReadVariants(const Json &json);

  [[nodiscard]] bool ReadVariantMappings(const Json::Ref &declared, size_t mesh, size_t primitive,
                                         Primitive &into);
  [[nodiscard]] bool ReadSkins(const Json::Ref &root);
  [[nodiscard]] bool ReadAnimations(const Json &json);
  [[nodiscard]] bool ReadMaterial(const Json::Ref &declaration, size_t index);
  [[nodiscard]] bool ElementBytes(const Accessor &accessor, size_t &stride, size_t &element) const;
  [[nodiscard]] bool ViewSpan(int view, Span<const uint8_t> &out) const;
  [[nodiscard]] bool ApplySparse(const Accessor &accessor, std::vector<double> &out) const;

  std::string Path_, Error_, Version_, MinVersion_;
  std::vector<std::string> Required_;

  bool Quantised_ = false;
  std::vector<std::vector<uint8_t>> Buffers_;
  std::vector<BufferView> Views_;
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

}
#endif
