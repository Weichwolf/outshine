/* THE glTF 2.0 BOUNDARY. Two containers -- a `.gltf` with its buffers beside it, and a `.glb` --
 * decided by the bytes and never by the file name, because a container that lies about its extension
 * is exactly the file a reader must not guess at.
 *
 * EVERY FAILURE IS A REFUSAL THAT NAMES WHAT WAS MISSING, and there is no partial document: a file
 * that reads nine of its ten accessors would hand its caller a subject that is silently not the
 * declared one (the same rule Scenario::Mod already carries). `Error()` is the sentence.
 *
 * WHAT THIS READER DOES NOT DO IS ALSO A DECISION. It derives nothing -- a primitive without NORMAL
 * stays without one, and the caller refuses by name (board:0073) -- and it chooses no
 * vertex layout: attributes come out as the semantics the file carries, in a decoded run of doubles,
 * so the question of which of them a GPU buffer holds is still open where it belongs. */
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
  /* `path` names the file in every refusal and supplies the directory external buffers are resolved
   * against. Bytes, so a caller that already has them -- a content store, a test -- does not have to
   * put them on a disk first. */
  [[nodiscard]] bool Read(Span<const uint8_t> bytes, const std::string &path);
  [[nodiscard]] bool ReadFile(const std::string &path);

  const std::string &Error() const { return Error_; }
  const std::string &Path() const { return Path_; }
  const std::string &Version() const { return Version_; }
  /* Empty unless the file states one. A file that states a minVersion above 2.0 does not read at
   * all, so anything this returns is a version this reader accepted. */
  const std::string &MinVersion() const { return MinVersion_; }
  const std::vector<std::string> &ExtensionsRequired() const { return Required_; }
  /* Whether this reader implements an extension well enough to load a file that requires it. */
  [[nodiscard]] static bool Honours(const std::string &extension);

  const std::vector<Accessor> &Accessors() const { return Accessors_; }
  const std::vector<BufferView> &BufferViews() const { return Views_; }
  const std::vector<Mesh> &Meshes() const { return Meshes_; }
  const std::vector<Node> &Nodes() const { return Nodes_; }
  const std::vector<Camera> &Cameras() const { return Cameras_; }
  const std::vector<Scene> &Scenes() const { return Scenes_; }
  const std::vector<MaterialRef> &Materials() const { return Materials_; }
  const std::vector<Texture> &Textures() const { return Textures_; }
  const std::vector<Image> &Images() const { return Images_; }
  const std::vector<Sampler> &Samplers() const { return Samplers_; }
  /* `KHR_lights_punctual`'s document-level table, unplaced. A node is what places one. */
  const std::vector<LightRef> &Lights() const { return Lights_; }
  /* `KHR_materials_variants`' document-level table (board:1188): the variant NAMES, in the file's
   * own order, so an index into this run is what `Primitive::VariantMaterials` is keyed by. Empty
   * where the file declares none, which is every asset in this tree but one.
   *
   * IT IS THE NAMES AND NOT A TABLE OF ANYTHING ELSE, because a name is the whole of what a variant
   * is: the extension puts nothing else on one, and the mapping from a variant to a material lives
   * on the primitive, which is what makes this a SELECTION and not material data (board:1177 is the
   * other shape). */
  const std::vector<std::string> &Variants() const { return Variants_; }
  /* Every animation the file declares, samplers and channels resolved against the tables above. A
   * file with none yields an empty run, which is what almost every asset in this tree is. */
  const std::vector<Animation> &Animations() const { return Animations_; }
  int DefaultScene() const { return DefaultScene_; }

  /* THE ENCODED BYTES OF ONE IMAGE, from its file or from its bufferView -- never decoded, because
   * a decoder is an SDL dependency and this layer has none. The caller decodes. */
  [[nodiscard]] bool ImageBytes(int image, std::vector<uint8_t> &out) const;

  /* THE DECODED ELEMENTS OF ONE ACCESSOR, `Count * components` doubles in element order, with the
   * byte stride, the normalisation and the sparse overrides already applied. `out` is the caller's
   * so a loop over a mesh reuses one buffer -- the exception F.20 states for itself. */
  [[nodiscard]] bool ReadElements(int accessor, std::vector<double> &out) const;
  /* The same run as unsigned integers, which is what an index accessor is. Refuses a float or a
   * signed component: an index that had to be rounded is a file this reader will not interpret. */
  [[nodiscard]] bool ReadIndices(int accessor, std::vector<uint32_t> &out) const;

  /* The node's transform in the coordinates of the scene root, TRS or matrix as the file states it.
   * Refuses a node index the file does not carry, and a hierarchy that reaches itself. */
  [[nodiscard]] bool WorldTransform(int node, Transform &out) const;
  /* THE SAME CHAIN WITH THE HIERARCHY POSED (board:1169): `locals` carries one local transform per
   * node of this document -- `Gltf::Pose::At` is what writes one -- and the walk composes those
   * instead of the file's own placements. A run of any other length is refused, so a pose with a
   * gap in it has no spelling and the parent chain has exactly one implementation. */
  [[nodiscard]] bool WorldTransform(int node, Span<const Transform> locals, Transform &out) const;
  /* The inverse of a camera node's world transform: glTF's camera looks down -Z in node space, so
   * this is the view transform and no sign is chosen here. */
  [[nodiscard]] bool ViewTransform(int cameraNode, Transform &out) const;

private:
  [[nodiscard]] bool Refuse(const std::string &why);
  /* The one parent-chain walk both overloads above are: `posed` is null for the file's own
   * placements and otherwise points at one local transform per node. */
  [[nodiscard]] bool Chain(int node, const Transform *posed, Transform &out) const;
  [[nodiscard]] bool ReadJson(const char *text, size_t length, const uint8_t *binaryChunk,
                              size_t binaryLength);
  [[nodiscard]] bool ResolveBuffers(const Json &json, const uint8_t *binaryChunk,
                                    size_t binaryLength);
  [[nodiscard]] bool ReadAppearance(const Json &json);
  [[nodiscard]] bool ReadLights(const Json &json);
  [[nodiscard]] bool ReadVariants(const Json &json);
  /* One primitive's `KHR_materials_variants` mappings, against the table `ReadVariants` filled. */
  [[nodiscard]] bool ReadVariantMappings(const Json::Ref &declared, size_t mesh, size_t primitive,
                                         Primitive &into);
  [[nodiscard]] bool ReadAnimations(const Json &json);
  [[nodiscard]] bool ReadMaterial(const Json::Ref &declaration, size_t index);
  [[nodiscard]] bool ElementBytes(const Accessor &accessor, size_t &stride, size_t &element) const;
  [[nodiscard]] bool ViewSpan(int view, Span<const uint8_t> &out) const;
  [[nodiscard]] bool ApplySparse(const Accessor &accessor, std::vector<double> &out) const;

  std::string Path_, Error_, Version_, MinVersion_;
  std::vector<std::string> Required_;
  std::vector<std::vector<uint8_t>> Buffers_;
  std::vector<BufferView> Views_;
  std::vector<Accessor> Accessors_;
  std::vector<Mesh> Meshes_;
  std::vector<Node> Nodes_;
  std::vector<Camera> Cameras_;
  std::vector<Scene> Scenes_;
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
