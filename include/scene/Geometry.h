#ifndef OUTSHINE_GEOMETRY_H
#define OUTSHINE_GEOMETRY_H

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "math/Mat4.h"
#include "Material.h"
#include "Texture.h"
#include "PunctualLight.h"

namespace outshine {

class Geometry;

/// Borrowed access to one Geometry's part transforms; does not own the geometry.
/// The owner must outlive this manager and must not be moved. Read operations require
/// no concurrent mutation; writes require exclusive access. Operations are O(1),
/// allocate nothing, and use Geometry's borrowed-reference invalidation contract.
class TransformManager {
public:
  /// Copy a local-to-model matrix without baking it into vertex attributes.
  /// @param part Active part index in the owner.
  /// @param model Column-major affine matrix; translations are metres. The caller
  /// must supply a finite, usable transform; this setter does not validate it.
  /// @return False for an absent part, without mutation; true after replacement.
  [[nodiscard]] bool setTransform(int part, const Mat4 &model);
  /// @param part Active part index in the owner.
  /// @return Borrowed local-to-model matrix, or a static identity for an absent part.
  [[nodiscard]] const Mat4 &getTransform(int part) const;

private:
  friend class Geometry;

  explicit TransformManager(Geometry &of) : Of_(&of) {}

  Geometry *Of_ = nullptr;
};

/// Borrowed light access with the ownership, threading and O(1) costs of TransformManager.
/// Light indices belong to this Geometry; light data and placement are stored separately.
class LightManager {
public:
  /// @return Number of active entries in the owner, without allocation.
  [[nodiscard]] int count() const;
  /// @param lamp Owner-local light index.
  /// @return Borrowed light data, or a static default PunctualLight when absent.
  [[nodiscard]] const PunctualLight &getLight(int lamp) const;
  /// @param lamp Owner-local light index.
  /// @param light Data copied as supplied; units and validity follow PunctualLight.
  /// @return False without mutation when absent; true after replacing data. Placement
  /// is unchanged. Photometric ranges and finite values are not validated here.
  [[nodiscard]] bool setLight(int lamp, const PunctualLight &light);
  /// @param lamp Owner-local light index.
  /// @return Borrowed name, or an empty view when absent.
  [[nodiscard]] std::string_view nameOf(int lamp) const;
  /// @param lamp Owner-local light index.
  /// @return Borrowed local-to-model matrix in metres, or static identity when absent.
  [[nodiscard]] const Mat4 &getTransform(int lamp) const;

private:
  friend class Geometry;

  explicit LightManager(Geometry &of) : Of_(&of) {}

  Geometry *Of_ = nullptr;
};

/// Borrowed part access with the ownership, threading and O(1) costs of TransformManager.
/// This manager edits CPU content; it neither owns GPU resources nor publishes updates.
class RenderableManager {
public:
  /// @return Number of active entries in the owner, without allocation.
  [[nodiscard]] int count() const;
  /// @param part Active owner-local part index.
  /// @return Borrowed name, or an empty view when absent.
  [[nodiscard]] std::string_view nameOf(int part) const;
  /// @param part Active owner-local part index.
  /// @return Owner-local material reference, unbound when the part is absent.
  [[nodiscard]] MaterialInstance getMaterial(int part) const;
  /// @param part Active owner-local part index.
  /// @param surface Bound material reference from the same Geometry.
  /// @return False without mutation for an absent part or material; true after assignment.
  [[nodiscard]] bool setMaterial(int part, MaterialInstance surface);
  /// @param part Active owner-local part index.
  /// @return Stored XYZ tuple count, or zero when absent; not a validity check.
  [[nodiscard]] size_t vertexCount(int part) const;
  /// @param part Active owner-local part index.
  /// @return Stored index-triplet count, or zero when absent; not a validity check.
  [[nodiscard]] size_t triangleCount(int part) const;

private:
  friend class Geometry;

  explicit RenderableManager(Geometry &of) : Of_(&of) {}

  Geometry *Of_ = nullptr;
};

/// Move-only owner of CPU mesh attributes, materials, images, lights and part placements.
/// Vertex positions are local metres in a right-handed, Y-up frame; triangles use CCW
/// front faces. Part placements map local coordinates into model space. No import-format
/// objects or GPU resources are required to build this content.
///
/// Managers and returned spans, references and string_views are borrowed. Treat them
/// as invalid after mutation of their owner, clear(), move or destruction; reacquire
/// them before use. Input strings and spans are copied during the call and must not
/// alias storage modified by that call. Indices are owner-local and are not persistent
/// handles: clear() invalidates them and subsequent additions can reuse their values.
///
/// There is no thread affinity or internal synchronization. Concurrent const reads are
/// allowed on a stable owner; all mutations require exclusive access. After move, the
/// source supports only destruction or move assignment. Query operations are O(1) and
/// allocate nothing unless documented otherwise. Additions and attribute replacements
/// may allocate and are preparation operations, not bounded realtime operations.
/// Allocation failure currently follows the allocator's exception contract; boolean
/// and integer failure results describe input validation, not allocation failure.
class Geometry {
public:
  /// Create an empty owner; allocates its private storage.
  Geometry();
  /// Release owned CPU data and invalidate every borrowed manager and view.
  ~Geometry();
  /// Transfer the complete owner in O(1), without allocation.
  /// @param other Source, left usable only for destruction or move assignment.
  Geometry(Geometry &&other) noexcept;
  /// Release previous contents and transfer ownership without allocation.
  /// Cost includes destruction of previous contents; invalidates borrowed views.
  /// @param other Source, left usable only for destruction or move assignment.
  /// @return This owner.
  Geometry &operator=(Geometry &&other) noexcept;
  /// Copying an owning geometry implicitly is prohibited.
  Geometry(const Geometry &) = delete;
  /// Implicit replacement by copying an owning geometry is prohibited.
  Geometry &operator=(const Geometry &) = delete;

  /// Append an empty part with identity placement; fill attributes before publication.
  /// @param named Name copied into this owner.
  /// @param material Owner-local reference, or unbound for the default material.
  /// @return New zero-based part index. The reference is stored without validation.
  /// May grow part storage; cost includes copying the name and relocating part records.
  int addPart(std::string_view named, MaterialInstance material);
  /// Remove all active parts, materials, images and lights; retain reusable part capacity.
  /// Invalidates all borrowed views and part/material/image indices. Requires exclusive
  /// access to this non-moved-from object; no concurrent readers or writers are allowed.
  void clear();

  /// @return A non-owning manager bound to this object; no allocation.
  [[nodiscard]] TransformManager transforms();
  /// @return A non-owning manager bound to this object; no allocation.
  [[nodiscard]] LightManager lights();
  /// @return A non-owning manager bound to this object; no allocation.
  [[nodiscard]] RenderableManager renderables();

  /// Append a copied material; numeric and texture-reference validity is the caller's duty.
  /// @param named Name copied into this owner.
  /// @param surface Metallic-roughness material with owner-local image references.
  /// @return New owner-local material reference. May allocate and relocate material records.
  [[nodiscard]] MaterialInstance addSurface(std::string_view named, const Material &surface);
  /// Append copied light data and placement without numeric validation; may allocate.
  /// @param named Name copied into this owner.
  /// @param light Local light data with PunctualLight's units and conventions.
  /// @param placed Local-to-model affine placement; translations in metres.
  /// @return New zero-based owner-local light index.
  int addLamp(std::string_view named, const PunctualLight &light, const Mat4 &placed);

  /// Copy finite XYZ positions in local metres; the source is borrowed only during the call.
  /// Return false without mutation for an invalid part, incomplete XYZ tuple or nonfinite
  /// value. Cross-attribute lengths and index bounds are checked by wellFormed().
  /// Successful replacement invalidates positionsOf(part); may allocate, O(metres.size()).
  /// Requires exclusive access to this non-moved-from object.
  /// @param part Active part index.
  /// @param metres Complete XYZ tuples, or empty to clear positions.
  /// @return True after copying; false for the validation failures described above.
  [[nodiscard]] bool setPositions(int part, std::span<const float> metres);
  /// Copy XYZ normals; each length must differ from one by at most 0.001.
  /// @param part Active part index.
  /// @param unit Finite tuples of 3 floats; empty removes the optional attribute.
  /// @return False without mutation for invalid part, tuple size or values; true after copy.
  /// O(unit.size()), may allocate; cross-attribute counts are checked by wellFormed().
  bool setNormals(int part, std::span<const float> unit);
  /// Copy finite UV pairs without clamping or transforming them; O(uv.size()), may allocate.
  /// @param part Active part index.
  /// @param uv Finite UV pairs; empty removes the selected optional attribute.
  /// @param set Coordinate set, exactly 0 or 1.
  /// @return False without mutation for invalid part, set, tuple size or values; true after copy.
  bool setTexture(int part, std::span<const float> uv, int set = 0);
  /// Copy XYZ tangent directions and handedness W; unit length and W sign are not validated.
  /// @param part Active part index.
  /// @param xyzw Finite tuples of 4 floats; empty removes the optional attribute.
  /// @return False without mutation for invalid part, tuple size or values; true after copy.
  /// O(xyzw.size()), may allocate; cross-attribute counts are checked by wellFormed().
  bool setTangents(int part, std::span<const float> xyzw);
  /// Copy linear RGBA vertex factors; no range clamping or colour conversion.
  /// @param part Active part index.
  /// @param rgba Finite tuples of 4 floats; empty removes the optional attribute.
  /// @return False without mutation for invalid part, tuple size or values; true after copy.
  /// O(rgba.size()), may allocate; cross-attribute counts are checked by wellFormed().
  bool setColours(int part, std::span<const float> rgba);
  /// Copy triangle indices; O(indices.size()), may allocate.
  /// @param part Active part index.
  /// @param indices Complete CCW triplets of part-local vertex indices; empty clears them.
  /// @return False without mutation for an absent part or incomplete triplet; true after copy.
  /// Index bounds are deferred to wellFormed(), allowing attributes to be built in any order.
  bool setTriangles(int part, std::span<const uint32_t> indices);

  /// How many of a part's triangles are wound against their own vertex normals: a triangle
  /// whose counter-clockwise face normal opposes the sum of its three vertex normals. A mesh
  /// built to the standard reads 0; a flipped face or a normal pointing into a body reads here
  /// before it reads as a black pixel. Zero-area triangles and triangles lacking
  /// any referenced position or normal are not counted; incomplete input is safe.
  /// @param part the part to count, as addPart returned it
  /// @return the count, or 0 for a part that does not exist
  [[nodiscard]] int windingAgainstNormals(int part) const;

  /// @return Number of active parts, excluding retained capacity.
  [[nodiscard]] int parts() const;
  /// @param part Active owner-local part index.
  /// @return Borrowed name, or an empty view when absent.
  [[nodiscard]] std::string_view nameOf(int part) const;
  /// @param part Active part index.
  /// @return Stored owner-local reference, unbound if the part is absent; not a validity check.
  [[nodiscard]] MaterialInstance materialOf(int part) const;
  /// @param part Active part index.
  /// @return Borrowed column-major local-to-model matrix, or static identity when absent.
  /// Translation components are metres; vertex attributes remain in local coordinates.
  [[nodiscard]] const Mat4 &placementOf(int part) const;

  /// Copy tightly packed RGBA8 image bytes without colour conversion; O(rgba.size()), allocates.
  /// @param widthPx Positive width in pixels.
  /// @param heightPx Positive height in pixels.
  /// @param rgba At least widthPx * heightPx * 4 bytes, in row order. Trailing bytes are
  /// currently retained too. Texture usage determines sRGB versus linear interpretation.
  /// @return New zero-based image index, or -1 without mutation for invalid dimensions
  /// or insufficient input. The dimension product must be representable by size_t.
  int addImage(int widthPx, int heightPx, std::span<const uint8_t> rgba);
  /// @return Number of owned images.
  [[nodiscard]] int images() const;
  /// @param image Owner-local image index.
  /// @return Dimensions and borrowed RGBA bytes, or an empty ImageView when absent.
  [[nodiscard]] ImageView imageAt(int image) const;

  /// @return Number of owned materials.
  [[nodiscard]] int surfaces() const;
  /// @param surface Owner-local index.
  /// @return Borrowed name, or an empty view when absent.
  [[nodiscard]] std::string_view surfaceNameOf(int surface) const;
  /// @param surface Owner-local material reference.
  /// @return Borrowed stored material, or a static default Material for an absent reference.
  [[nodiscard]] const Material &surfaceAt(MaterialInstance surface) const;

  /// Replace a material by copying without numeric or texture-reference validation.
  /// @param surface Owner-local material reference.
  /// @param row Replacement data, including owner-local image references.
  /// @return False without mutation for an absent reference; true after replacement.
  bool setSurface(MaterialInstance surface, const Material &row);

  /// @return Number of owned lights.
  [[nodiscard]] int lamps() const;
  /// @param lamp Owner-local index.
  /// @return Borrowed name, or an empty view when absent.
  [[nodiscard]] std::string_view lampNameOf(int lamp) const;
  /// @param lamp Owner-local light index.
  /// @return Borrowed local light data, or a static default PunctualLight when absent.
  [[nodiscard]] const PunctualLight &lampAt(int lamp) const;
  /// @param lamp Owner-local light index.
  /// @return Borrowed local-to-model affine matrix in metres, or static identity when absent.
  [[nodiscard]] const Mat4 &lampPlacementOf(int lamp) const;
  /// @param part Active part index.
  /// @return Borrowed local XYZ positions in metres, empty if the part or attribute is absent.
  [[nodiscard]] std::span<const float> positionsOf(int part) const;
  /// @param part Active part index.
  /// @return Borrowed local XYZ unit normals, empty if the part or attribute is absent.
  [[nodiscard]] std::span<const float> normalsOf(int part) const;
  /// Select one of the two supported texture-coordinate channels.
  using UvSet = outshine::UvSet;

  /// A part's texture coordinates for @p set, or an empty span when it carries none.
  /// @param part Which part of the geometry.
  /// @param set Which coordinate set, defaulting to the first.
  /// @return Borrowed UV pairs, empty for an absent part or channel. Requires a valid UvSet.
  [[nodiscard]] std::span<const float> textureOf(int part, UvSet set = UvSet::Uv0) const;
  /// @param part Active part index.
  /// @return Borrowed local tangent XYZ and handedness W, empty if the part or attribute is absent.
  [[nodiscard]] std::span<const float> tangentsOf(int part) const;
  /// @param part Active part index.
  /// @return Borrowed linear RGBA vertex factors, empty if the part or attribute is absent.
  [[nodiscard]] std::span<const float> coloursOf(int part) const;
  /// @param part Active part index.
  /// @return Borrowed CCW triplets of part-local vertex indices, empty if the part or attribute is
  /// absent.
  [[nodiscard]] std::span<const uint32_t> trianglesOf(int part) const;
  /// Validate active parts, ignoring retained capacity from earlier builds.
  /// Require at least one part, nonempty positions and triangles, matching optional
  /// attribute counts and in-range indices. Does not validate materials or transforms.
  /// No allocation or mutation; O(active parts + indices). No concurrent mutation.
  /// The object must not have been moved from.
  /// @return True if every active part meets these structural conditions.
  [[nodiscard]] bool wellFormed() const;

private:
  friend class TransformManager;
  friend class LightManager;
  friend class RenderableManager;
  void place(int part, const Mat4 &model);
  void relight(int lamp, const PunctualLight &light);
  void resurface(int part, MaterialInstance surface);

  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
