/* THE glTF 2.0 VOCABULARY, one enumeration per place the format uses an integer, and the records the
 * reader fills. The numbers are the format's own (they are what a file carries), so they are stated
 * once here and never appear again as a literal.
 *
 * AN ATTRIBUTE IS A NAME, NOT A SLOT. A primitive keeps whatever semantics its file carries --
 * JOINTS_0, TEXCOORD_1, COLOR_0 included -- because deciding which of them a vertex layout holds is
 * a question about the renderer and not about the file. The reader answers "what is in it"; nothing
 * here answers "what shape does it become". */
#ifndef GLTF_TYPES_H
#define GLTF_TYPES_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "Keyframes.h"

#include "Material.h"
#include "PunctualLight.h"
#include "UvTransform.h"

namespace outshine::Gltf {

enum class ComponentType : uint16_t {
  Int8 = 5120,
  UInt8 = 5121,
  Int16 = 5122,
  UInt16 = 5123,
  UInt32 = 5125,
  Float32 = 5126,
};

enum class ElementType : uint8_t { Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4 };

enum class PrimitiveMode : uint8_t {
  Points = 0,
  Lines = 1,
  LineLoop = 2,
  LineStrip = 3,
  Triangles = 4,
  TriangleStrip = 5,
  TriangleFan = 6,
};

enum class CameraKind : uint8_t { Perspective, Orthographic };

/* Rows and columns of an element, and the byte width of one component. The format's own alignment
 * rule -- every column of a matrix starts on a 4-byte boundary -- is applied where the stride is
 * computed, because it is a property of the pair and not of either half. */
size_t ComponentBytes(ComponentType component);
size_t ElementRows(ElementType element);
size_t ElementColumns(ElementType element);
inline size_t ElementComponents(ElementType element) {
  return ElementRows(element) * ElementColumns(element);
}
size_t TightElementBytes(ElementType element, ComponentType component);

struct BufferView {
  size_t Buffer = 0;
  size_t ByteOffset = 0;
  size_t ByteLength = 0;
  /* 0 means "the format did not say", which is tight packing -- not "stride zero". */
  size_t ByteStride = 0;
};

/* The compaction: a base run of elements, some of which are overridden by index. A sparse accessor
 * with no bufferView reads as zeros before the overrides are applied, which is what makes a mostly
 * empty morph target cost only what it changes. */
struct SparseOverride {
  size_t Count = 0;
  int IndicesBufferView = -1;
  size_t IndicesByteOffset = 0;
  ComponentType IndicesComponent = ComponentType::UInt32;
  int ValuesBufferView = -1;
  size_t ValuesByteOffset = 0;
};

struct Accessor {
  int View = -1;
  size_t ByteOffset = 0;
  ComponentType Component = ComponentType::Float32;
  ElementType Element = ElementType::Scalar;
  size_t Count = 0;
  bool Normalized = false;
  bool HasSparse = false;
  SparseOverride Sparse;
  std::vector<double> Min, Max;
};

struct Attribute {
  std::string Semantic;
  int Accessor = -1;
};

/* ONE MORPH TARGET: the same shape as a primitive's attribute list, and deliberately so. glTF states
 * a target carries DELTAS against the base attribute of the same name, over the same vertex run, so
 * the record that answers *which accessor holds this semantic* is the record that answers it here
 * too -- `Find` is the same question and gets the same spelling. */
struct MorphTarget {
  std::vector<Attribute> Attributes;

  /* -1 for "this target does not displace that semantic", which is legal: a target may move positions
   * and leave normals alone. */
  int Find(const char *semantic) const;
};

struct Primitive {
  std::vector<Attribute> Attributes;
  /* THE MORPH TARGETS, IN THE FILE'S OWN ORDER, because the order IS the join: a weights keyframe
   * carries one value per target and pairs with them by position, and `mesh.weights` does too. */
  std::vector<MorphTarget> Targets;
  int Indices = -1;
  int Material = -1;
  PrimitiveMode Mode = PrimitiveMode::Triangles;

  /* `KHR_materials_variants` READ INTO THE QUESTION IT ANSWERS (board:1188): one entry per variant
   * the document declares, holding the material that variant puts on this primitive, and -1 where
   * this variant does not remap it. Empty where the file declares no variants at all.
   *
   * IT IS DENSE RATHER THAN THE FILE'S LIST OF MAPPINGS, and that is what makes the format's own
   * "each variant index must be used no more than one time" a shape instead of a rule: a second
   * mapping of one variant has nowhere to be written, and the reader refuses at the file rather
   * than leaving two answers for a consumer to pick between. */
  std::vector<int> VariantMaterials;

  /* -1 for "this file does not carry it". A caller that needs it refuses by name; nothing here
   * invents one (board:0073). */
  int Find(const char *semantic) const;

  /* WHICH MATERIAL THIS PRIMITIVE WEARS UNDER THE ACTIVE VARIANT (board:1188). `variant` is an
   * index into the document's variant table and -1 is "no active variant", which is the ordinary
   * state of every file in this tree.
   *
   * NO ACTIVE VARIANT AND A VARIANT THAT DOES NOT MAP THIS PRIMITIVE ARE ONE PATH and both answer
   * `Material`, because the extension says so in one sentence: "when no mapping contains the active
   * variant, or there is no active variant, a compliant viewer will fall back on vanilla glTF
   * behaviour". The default is therefore not a special case here either. */
  int MaterialUnder(int variant) const;
};

struct Mesh {
  std::string Name;
  std::vector<Primitive> Primitives;
  /* THE REST WEIGHTS, one per morph target, or empty where the file declares none -- in which case
   * the format states every weight is zero. Empty is left empty for the reason an absent
   * `inverseBindMatrices` is: a reader that materialised the zeros would erase the difference
   * between a file that declared them and one that did not. IT IS THE MESH'S AND NOT THE
   * PRIMITIVE'S, because glTF puts it there: every primitive of a mesh morphs together. */
  std::vector<double> Weights;
};

/* ONE ENTRY OF `KHR_lights_punctual`'s document-level table. The shading half is
 * `outshine::PunctualLight`, which is the engine's one vocabulary for a light the way
 * `outshine::Material` is for a surface; what is added here is only what is about the FILE. The
 * light carries no place of its own -- a node does, and `Position`/`Direction` stay at their
 * defaults until a consumer resolves the node that references it. */
struct LightRef {
  std::string Name;
  outshine::PunctualLight Light;
};

struct Node {
  std::string Name;
  std::vector<int> Children;
  int Mesh = -1;
  int Camera = -1;
  /* Into `Document::Lights()`, or -1. The extension puts the reference on the node and the beam on
   * the node's -Z, so a light's direction is the hierarchy's answer and never the table's. */
  int Light = -1;
  /* A node carries a matrix or a TRS triple, never both -- the format says so and the reader
   * refuses the file that does. */
  bool HasMatrix = false;
  double Matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  double Translation[3] = {0, 0, 0};
  double Rotation[4] = {0, 0, 0, 1}; /* xyzw, the format's order */
  double Scale[3] = {1, 1, 1};
  /* `EXT_mesh_gpu_instancing`: accessor indices for one transform per instance, or -1 (board:1416).
   * The three are independent -- a file may give translations and no rotations -- and the extension
   * requires every one it gives to have the same count, which is the instance count.
   *
   * A NODE WITH INSTANCES DRAWS ITS MESH ONCE PER INSTANCE and not once. The extension is optional in
   * the sense that a client may ignore it, and ignoring it draws ONE body where the file says there
   * are many -- which is a wrong picture rather than a plainer one, so it is read. */
  int InstanceTranslation = -1;
  int InstanceRotation = -1;
  int InstanceScale = -1;
  /* `KHR_node_visibility`. The extension's own rule: *a node is visible if and only if its own
   * visible property is true and all its parents are visible*, so THIS FIELD IS THE NODE'S OWN
   * ANSWER and the inherited one is the walk's. Default true, which is the format's, so a file
   * without the extension costs no branch anywhere. */
  bool Visible = true;
  /* Into `Document::Skins()`, or -1. glTF puts the reference on the NODE that carries the mesh, so a
   * skin is a property of an instance and not of the geometry -- two nodes may share one mesh and
   * name different skins, and the flatten must therefore skin per node rather than per primitive. */
  int Skin = -1;
};

/* A SKIN IS A JOINT LIST PLUS ONE MATRIX PER JOINT, and both are positional: `Joints[i]` is a node
 * index and `InverseBind[i]` is the matrix that takes a vertex out of that joint's bind pose. The
 * pairing is the whole record, which is why they are two vectors of one length rather than a table
 * with a key.
 *
 * `Skeleton` is the common root the format lets a file name and which nothing here needs: glTF states
 * that a joint's transform is read from the scene graph, so the skinning matrix is already absolute
 * and the skeleton node adds nothing to it. It is carried because a file may declare it and a reader
 * that dropped it could not round-trip. */
struct Skin {
  std::string Name;
  std::vector<int> Joints;
  int Skeleton = -1;
  /* 16 doubles per joint, column-major as the format states, or empty where the file declares none --
   * in which case every inverse bind matrix is the identity, which the format says explicitly. */
  std::vector<double> InverseBind;
};

struct Scene {
  std::string Name;
  std::vector<int> Roots;
};

/* WHAT A SAMPLER DOES AT THE EDGE OF THE IMAGE. glTF's own numbers, so the file's integers become
 * this enumeration once and never travel as integers (`Enum.2`). `TextureSettingsTest` renders one
 * cell per mode and a viewer that collapses two of them shows the wrong picture in exactly two of
 * its cells. */
enum class Wrap : uint16_t { ClampToEdge = 33071, MirroredRepeat = 33648, Repeat = 10497 };

/* Nearest or linear WITHIN a level. `TextureLinearInterpolationTest` is about WHERE the sRGB
 * decode happens relative to this filter, not about which filter it is. */
enum class Filter : uint8_t { Nearest, Linear };

/* AND WHICH LEVELS THE MINIFICATION FILTER MAY READ, which is a SECOND question the format packs into
 * the same integer (board:1134). `minFilter` names both halves at once -- 9986 is
 * `NEAREST_MIPMAP_LINEAR`, nearest inside a level and linear between levels -- so a reader that
 * answers only the first has silently answered the second as well. It travels as its own enumeration
 * because the alternative is a caller re-deriving it from a raw integer that this layer was supposed to
 * have consumed. */
enum class MipFilter : uint8_t { None, Nearest, Linear };

struct Sampler {
  Wrap WrapS = Wrap::Repeat;   /* the format's default when the field is absent */
  Wrap WrapT = Wrap::Repeat;
  Filter Mag = Filter::Linear;
  Filter Min = Filter::Linear;
  MipFilter Mip = MipFilter::Linear; /* the format's default `minFilter` is unspecified; see the reader */
};

/* AN IMAGE IS BYTES AND A DECLARED TYPE, never a decoded raster: this layer has no decoder and
 * naming one here would put an SDL dependency inside the format reader. `Uri` is already
 * PERCENT-DECODED -- the format requires reserved characters to be encoded on the way out
 * (`Specification.adoc:550`), so decoding them is this reader's obligation on the way in. */
struct Image {
  std::string Name;
  std::string Uri;        /* empty when the image lives in a bufferView */
  std::string MimeType;
  int View = -1;
};

struct Texture {
  std::string Name;
  int Source = -1;    /* into Images */
  int Sampler = -1;   /* -1 is the format's "use repeat and auto filtering" */
};

/* WHICH TEXTURE FEEDS WHICH SLOT, and which UV SET it reads. `TexCoord` is the number after
 * TEXCOORD_, so `MultiUVTest`'s logo declaring `texCoord: 1` is carried rather than collapsed.
 *
 * `KHR_texture_transform` IS PER TEXTURE REFERENCE AND THEREFORE LIVES HERE (board:1177), inside each
 * `textureInfo` and never on the material: `TextureTransformMultiTest` puts its only transform on the
 * normal map of one material and on the occlusion map of another, so an engine carrying ONE transform
 * per material has nowhere to put the second and renders the first over both.
 *
 * IT IS THE COMPOSED MATRIX AND NOT THE THREE PROPERTIES, and the identity where the reference
 * declares no extension -- so presence is signalled by the numbers themselves and no `HasTransform`
 * flag exists to disagree with them. The extension's own `texCoord` OVERRIDES the `textureInfo`'s when
 * supplied, so `TexCoord` above is the answer after that override and there is no second field
 * carrying the one it replaced. */
struct TextureRef {
  int Texture = -1;
  int TexCoord = 0;
  outshine::UvTransform Transform;
  [[nodiscard]] bool Declared() const { return Texture >= 0; }
};

/* THE FILE'S MATERIAL. The shading half is `outshine::Material`, which is the engine's one
 * vocabulary for a surface and is glTF's own metal-rough parameterisation; what is added here is
 * only what is about the FILE -- which texture, which UV set, and whether the file says the surface
 * has two sides. */
struct MaterialRef {
  std::string Name;
  outshine::Material Surface;
  TextureRef BaseColour;
  TextureRef MetallicRoughness;
  TextureRef Normal;
  TextureRef Occlusion;
  TextureRef Emissive;
  /* `KHR_materials_specular`'s two images (board:1205). `SpecularStrength` carries a scalar in ALPHA
   * and multiplies `SpecularFactor`; `SpecularTint` is sRGB and multiplies `SpecularColour`. */
  TextureRef SpecularStrength;
  TextureRef SpecularTint;
  double NormalScale = 1.0;
  double OcclusionStrength = 1.0;
};

/* HOW A SAMPLER GETS FROM ONE KEYFRAME TO THE NEXT IS `outshine::Keyframes::Interpolation` AND NOT A
 * SECOND ENUMERATION HERE. The evaluator in core already carries glTF's three words and glTF's own
 * triple layout for the spline, so a copy in this layer would be a second spelling of one meaning
 * that nothing keeps in step. What this layer owns is the REFUSAL: a fourth word is named and
 * rejected by the reader rather than falling back to `Linear`, which is what `InterpolationTest`
 * exists to catch a viewer doing.
 *
 * `CubicSpline` CHANGES THE SHAPE OF THE OUTPUT and not only the arithmetic: its output accessor
 * carries THREE elements per keyframe -- in-tangent, value, out-tangent -- so a reader that treated
 * the word as a hint would read a third of the animation and see two thirds of it as keyframes. */
using Interpolation = outshine::Keyframes::Interpolation;

/* WHAT A CHANNEL DRIVES. `Weights` is carried because the format has it and a file that uses it must
 * not read as something else; nothing in this tree consumes it yet, and that is a fact about the
 * consumer rather than about the reader. */
enum class AnimationPath : uint8_t { Translation, Rotation, Scale, Weights, MaterialFactor };

/* WHICH NUMBER OF A MATERIAL A POINTER CHANNEL DRIVES (board:1392).
 *
 * `KHR_animation_pointer` TARGETS A JSON POINTER INTO THE WHOLE ASSET, and the asset object model is
 * every mutable property the format has. **This enumeration is the subset this reader RESOLVES, and a
 * pointer outside it is a refusal that quotes the pointer** -- which is the only honest shape: a
 * grammar that silently accepted an unknown path would read an animation and drive nothing.
 *
 * THESE FOUR ARE ONE PARSE AND NOT A CHOICE. `/materials/<i>/pbrMetallicRoughness/baseColorFactor`
 * is what a corpus asset needs; `metallicFactor`, `roughnessFactor` and `/emissiveFactor` sit on the
 * same two-segment walk and cost nothing beyond naming them. */
enum class MaterialFactor : uint8_t { BaseColour, Metalness, Roughness, Emissive };

/* WHAT A CHANNEL THIS ENGINE CANNOT DRIVE IS, and it is NOT a refusal (board:1392).
 *
 * glTF 2.0 says outright that a client may ignore an animation -- *client implementations may select
 * an animation entry and pause it on the first frame, play it automatically, or IGNORE ALL ANIMATIONS
 * until further user requests* -- so refusing a whole file over one channel is stricter than the
 * format and costs a subject that would otherwise draw. **Degrade on detail; refuse on existence.**
 *
 * IT IS COUNTED AND NOT DROPPED, which is the difference between a shortfall and a silence. The same
 * shape `Subject::Undrawn` already carries for a primitive mode this rasteriser has no pass for. */
enum class UndrivenReason : uint8_t {
  /* The pointer is a shape this reader does not parse at all. */
  PointerUnparsed,
  /* The pointer parses and names a property this reader holds no field for. */
  PointerUnheld,
};

/* How many numbers one keyframe of a material factor carries. */
size_t FactorComponents(MaterialFactor factor);

/* THE COMPONENTS ONE KEYFRAME OF A PATH CARRIES: 3 for a translation or a scale, 4 for a rotation
 * quaternion. `Weights` is as many as the mesh has morph targets and is therefore not answerable
 * from the path alone -- 0 says so, rather than a number that would be wrong. */
size_t PathComponents(AnimationPath path);

/* ONE SAMPLER: a time grid, the values on it, and how to get between them. `Input` and `Output` are
 * accessor indices, so the decode is `Document::ReadElements` like every other run in the file and
 * this record holds no bytes of its own. */
struct AnimationSampler {
  int Input = -1;
  int Output = -1;
  Interpolation How = Interpolation::Linear;
};

/* ONE CHANNEL: which sampler drives which property of which node. `Node` may be -1, which the format
 * allows and defines as a channel to be IGNORED rather than an error. */
struct AnimationChannel {
  int Sampler = -1;
  int Node = -1;
  AnimationPath Path = AnimationPath::Translation;
  /* SET ONLY WHERE `Path` IS `MaterialFactor`, and then both are: which material and which of its
   * numbers. A pointer channel names no node -- the format says the channel's `node` **MUST NOT** be
   * set -- so `Node` stays -1 and these two carry the target instead. */
  int Material = -1;
  MaterialFactor Factor = MaterialFactor::BaseColour;
};

struct Animation {
  std::string Name;
  std::vector<AnimationSampler> Samplers;
  std::vector<AnimationChannel> Channels;
  /* WHAT THIS ANIMATION ASKED FOR AND DID NOT GET, counted per reason (board:1392). A consumer that
   * wants to know whether it is showing the whole animation asks here; one that does not is unchanged.
   * The pointers themselves are kept because a shortfall a reader cannot NAME is one nobody can act
   * on -- and they are bounded by the channel count, which the file declares. */
  size_t Undriven = 0;
  std::vector<std::string> UndrivenPointers;
};

/* glTF's OWN DEFAULT MATERIAL, WHICH A PRIMITIVE THAT NAMES NONE WEARS (board:1193). The format
 * states it in one sentence -- "the default material ... all default values" -- so it is
 * `baseColorFactor [1,1,1,1]`, `metallicFactor 1`, `roughnessFactor 1`, `OPAQUE`, single-sided.
 *
 * IT IS NOT `outshine::Material{}` AND THE DIFFERENCE IS THE PICTURE. This engine's own default is a
 * mid-grey dielectric, because a surface nobody described is not a metal; the format's is white and
 * fully metallic. `BoxVertexColors` declares no material at all, so under the engine's default its
 * `COLOR_0` would be multiplied by 0.5 and the whole body would come out at half the radiance the
 * file asks for -- a picture that looks authored rather than wrong.
 *
 * IT LIVES IN THIS LAYER BECAUSE IT IS A FACT ABOUT THE FORMAT, not about whoever met a -1. A
 * consumer substituting its own default is how a file's declared appearance quietly becomes an
 * engine preference nobody wrote down. */
outshine::Material DefaultMaterial();

/* HOW MANY COMPONENTS A `COLOR_0` ACCESSOR CARRIES, AND THE REFUSAL THAT IS THE OTHER HALF OF THE
 * QUESTION (board:1193). The specification permits SIX cells and no others -- `VEC3` or `VEC4`,
 * times float, unsigned byte normalized or unsigned short normalized (`Specification.adoc:1339`) --
 * so this answers 3 or 4 and names what it refused.
 *
 * THE READER ALREADY TURNS ALL SIX INTO ONE THING: `Document::ReadElements` divides a normalized
 * integer by its own maximum, so an unsigned byte 255 and a float 1.0 arrive as the same double and
 * every consumer downstream sees one alphabet. What is left for this to decide is which spellings
 * are legal at all, and a signed or unsigned INTEGER that is not normalized is the trap it exists
 * for: those bytes decode to 0..255 rather than 0..1, so a viewer that let them through would
 * multiply base colour by 255 and publish it as a colour.
 *
 * `VEC3` IS ALPHA 1.0 AND THAT IS THE FORMAT'S SENTENCE, not a convenience: "When a `COLOR_n`
 * attribute uses an accessor of `"VEC3"` type, its alpha component MUST be assumed to have a value
 * of `1.0`" (`Specification.adoc:1354`). It is applied where the run is filled, so nothing
 * downstream carries two widths of vertex colour. */
[[nodiscard]] bool VertexColourComponents(const Accessor &accessor, size_t &components,
                                          std::string &why);

/* WHAT A SUBJECT DOES NOT CARRY, NAMED. The empty string means it carries all of them; anything else
 * is the refusal a case prints and stops on. There is no arm that derives a missing semantic --
 * board:0073 forbids it, and the Khronos `Triangle` having no NORMAL is a property of
 * the subject, recorded, not repaired. */
std::string MissingSemantics(const Primitive &primitive,
                             std::initializer_list<const char *> required);

/* WHICH UV SETS A SUBJECT'S VERTICES CARRY (board:1182). It is its own enumeration and not a
 * `UvSet` value, because "the sets that exist" and "the set this reference reads" are two different
 * questions with the same answer type -- passed positionally they would be swappable and the
 * compiler would have nothing to say (`I.24`, `Enum.2`). */
enum class CarriedUvSets { FirstOnly, Both };

/* WHICH UV SET A TEXTURE REFERENCE READS, ANSWERED AGAINST WHAT THE SUBJECT CARRIES (board:1182),
 * and this is the NARROWING of board:1177's refusal rather than its removal:
 *
 *   the reference names TEXCOORD_0                                  -> the first set
 *   the reference names TEXCOORD_1 and the subject carries it       -> the second set
 *   the reference names TEXCOORD_1 and the subject carries one set  -> a NAMED REFUSAL
 *   the reference names TEXCOORD_2 or beyond                        -> a NAMED REFUSAL
 *
 * THERE IS NO FOURTH ARM AND IN PARTICULAR NO FALL-BACK. Answering `First` for a reference that
 * named the second set puts the image somewhere the file did not ask for, and an emissive image on
 * the wrong uv set is still an image on a surface: it reads as an authored appearance rather than as
 * a defect, which is why this is a refusal and not a substitution. `MultiUVTest` writes "Multiple
 * UVs not supported in this viewer" into exactly the place the first set addresses, because that is
 * what the fall-back draws.
 *
 * IT REFUSES AGAINST THE SUBJECT AND NOT AGAINST THE ENGINE'S CAPABILITY. The second set reaching
 * the sampler is what this engine now does; a file that names a set its own geometry does not carry
 * is a file that cannot be drawn as declared, whatever a renderer can bind.
 *
 * `socket` NAMES THE glTF FIELD FOR THE SENTENCE and nothing else; the caller adds which material it
 * was. `false` leaves `why` holding the whole refusal and `out` untouched. */
[[nodiscard]] bool UvSetOf(const TextureRef &reference, CarriedUvSets carried, const char *socket,
                           UvSet &out, std::string &why);

} // namespace outshine::Gltf
#endif
