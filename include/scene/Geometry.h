#ifndef OUTSHINE_GEOMETRY_H
#define OUTSHINE_GEOMETRY_H

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>

#include "math/Mat4.h"
#include "Material.h"
#include "Texture.h"
#include "PunctualLight.h"

namespace outshine {

/// Failure when adding or replacing an owned material; stored values remain unchanged.
enum class MaterialError {
  CapacityExceeded, ///< The owner-local material count or storage capacity is exhausted.
  MissingMaterial,  ///< The owner-local material index is absent.
  InvalidMaterial   ///< Factors, texture parameters or owner-local image bindings are invalid.
};

/// Failure when creating an empty mesh part; existing parts remain unchanged.
enum class GeometryPartError {
  CapacityExceeded ///< The owner-local part count or storage capacity is exhausted.
};

/// Failure when replacing a mesh part's local-to-model transform; no state changes.
enum class PlacementError {
  MissingPart,     ///< The owner-local part index is absent.
  InvalidTransform ///< Components are nonfinite or the last row is not (0, 0, 0, 1).
};

/// Failure when adding or replacing a native light; stored values remain unchanged.
enum class LightMutationError {
  MissingLight,    ///< The owner-local light index is absent.
  CapacityExceeded ///< No further owner-local light index can be allocated.
};

/// Failure when binding a part to a material; stored values remain unchanged.
enum class MaterialBindingError {
  MissingPart,    ///< The owner-local part index is absent.
  MissingMaterial ///< The owner-local material index is absent or unbound.
};

/// Failure when replacing a mesh attribute; the previous attribute remains unchanged.
enum class GeometryAttributeError {
  MissingPart,      ///< The owner-local part index is absent.
  IncompleteTuple,  ///< The scalar count cannot form the attribute's required tuples.
  NonFiniteValue,   ///< A floating-point attribute contains NaN or infinity.
  NonUnitNormal,    ///< A normal's length differs from one beyond the documented tolerance.
  InvalidTextureSet ///< The requested texture coordinate set is not zero or one.
};

/// Failure while combining independently owned native geometry snapshots.
enum class GeometryAppendError {
  MalformedSource, ///< Source has no complete native geometry product.
  CapacityExceeded ///< Combined owner-local indices cannot be represented.
};

/// Failure while adding an owned RGBA8 image; existing images remain unchanged.
enum class GeometryImageError {
  InvalidDimensions, ///< Width or height cannot describe a positive RGBA8 image.
  ByteCountMismatch, ///< Supplied bytes do not exactly cover the declared image.
  CapacityExceeded   ///< Another owner-local image index cannot be represented.
};

/// Stable diagnostic text for a native geometry mutation error.
/// @param error Typed mutation failure to describe.
/// @return Static text; no allocation or owner access.
[[nodiscard]] constexpr std::string_view Describe(MaterialError error) noexcept {
  switch (error) {
    case MaterialError::CapacityExceeded: return "material capacity exceeded";
    case MaterialError::MissingMaterial: return "material is absent";
    case MaterialError::InvalidMaterial: return "material values or image bindings are invalid";
  }
  return "unknown material error";
}

/// Stable diagnostic text for a native RGBA8 image mutation error.
/// @param error Typed image failure to describe.
/// @return Static text; no allocation or owner access.
[[nodiscard]] constexpr std::string_view Describe(GeometryImageError error) noexcept {
  switch (error) {
    case GeometryImageError::InvalidDimensions: return "image dimensions are invalid";
    case GeometryImageError::ByteCountMismatch: return "image byte count does not match dimensions";
    case GeometryImageError::CapacityExceeded: return "image capacity exceeded";
  }
  return "unknown image error";
}

/// Move-only owner of CPU mesh attributes, materials, images, lights and part placements.
/// Vertex positions are local metres in a right-handed, Y-up frame; triangles use CCW
/// front faces. Part placements map local coordinates into model space. No import-format
/// objects or GPU resources are required to build this content.
///
/// Returned spans, references and string_views are borrowed. Treat them
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
/// Systemwide allocator exhaustion is fatal. Typed mutation errors report invalid input or
/// unrepresentable storage/index requirements; they do not report allocator exhaustion.
class Geometry {
public:
  /// Create an empty owner; allocates its private storage.
  Geometry();
  /// Release owned CPU data and invalidate every borrowed view.
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

  /// Copy active parts, materials, images and lights into an independent native owner.
  /// Owner-local indices and local-to-model placements are preserved; spare part capacity is not.
  /// Requires a non-moved-from source without concurrent mutation. Source views remain valid.
  /// Cost and allocations scale with active owned data. Systemwide allocator exhaustion is fatal.
  /// @return Independent geometry; later mutation or destruction of either owner is isolated.
  [[nodiscard]] Geometry clone() const;

  /// Append a complete native snapshot transactionally, remapping its owner-local image and
  /// material indices. The source remains unchanged; success invalidates every borrowed view of
  /// this geometry because its complete owned storage is replaced. Failure preserves both
  /// geometries. Requires exclusive access to this owner and no concurrent mutation of source.
  /// @param source Complete native geometry whose parts, materials, images and lights are copied.
  /// @return Success, or a typed source/capacity error. Systemwide allocator exhaustion is fatal.
  /// Cost and allocation scale with the combined owned data.
  [[nodiscard]] std::expected<void, GeometryAppendError> append(const Geometry &source);

  /// Append an empty part with identity placement; fill attributes before publication.
  /// @param named Name copied into this owner.
  /// @param material Owner-local reference, or unbound for the default material.
  /// @return New zero-based part index, or CapacityExceeded without mutation.
  /// The material reference is stored without validation.
  /// May grow part storage; cost includes copying the name and relocating part records.
  [[nodiscard]] std::expected<int, GeometryPartError> addPart(std::string_view named,
                                                              MaterialInstance material);
  /// Remove all active parts, materials, images and lights; retain reusable part capacity.
  /// Invalidates all borrowed views and part/material/image indices. Requires exclusive
  /// access to this non-moved-from object; no concurrent readers or writers are allowed.
  void clear();

  /// Replace a part's local-to-model placement without modifying its vertex attributes.
  /// @param part Active owner-local part index.
  /// @param model Finite affine matrix with translations in metres and last row exactly (0, 0, 0,
  /// 1).
  /// @return Success, MissingPart or InvalidTransform. Errors preserve placement and attributes.
  /// Scale (including zero), reflection and shear are accepted; no inverse is required.
  /// Vertex data and borrowed attribute views remain unchanged on both success and failure.
  /// O(1), no allocation; requires exclusive access.
  [[nodiscard]] std::expected<void, PlacementError> setPlacement(int part,
                                                                 const Mat4 &model) noexcept;
  /// Replace local light data while preserving its name and placement.
  /// @param lamp Active owner-local light index.
  /// @param light Values copied as supplied; validity and units follow PunctualLight.
  /// @return Success, or MissingLight without mutation.
  /// O(1), no allocation; requires exclusive access. Numeric values are not validated.
  [[nodiscard]] std::expected<void, LightMutationError>
  setLight(int lamp, const PunctualLight &light) noexcept;
  /// Replace a part's material reference without modifying either material.
  /// @param part Active owner-local part index.
  /// @param surface Bound material reference belonging to this owner; foreign owners
  /// cannot be detected because MaterialInstance stores only an index.
  /// @return Success, or MissingPart/MissingMaterial without mutation.
  /// O(1), no allocation; requires exclusive access.
  [[nodiscard]] std::expected<void, MaterialBindingError>
  setMaterial(int part, MaterialInstance surface) noexcept;

  /// Append a material after validating its numeric, sampler and UV values.
  /// Image references may point to images added later; wellFormed() requires them to resolve.
  /// InvalidMaterial or CapacityExceeded leaves values, names and slot numbering unchanged.
  /// Requires exclusive access to this non-moved-from owner. Validation is constant time;
  /// success copies the name and may allocate or relocate material records.
  /// @param named Name copied into this owner.
  /// @param surface Metallic-roughness material with owner-local image references.
  /// @return New owner-local material reference, or the validation/capacity error.
  [[nodiscard]] std::expected<MaterialInstance, MaterialError> addSurface(std::string_view named,
                                                                          const Material &surface);
  /// Append copied light data and placement without numeric validation; may allocate.
  /// @param named Name copied into this owner.
  /// @param light Local light data with PunctualLight's units and conventions.
  /// @param placed Local-to-model affine placement; translations in metres.
  /// @return New zero-based owner-local light index, or CapacityExceeded without mutation.
  [[nodiscard]] std::expected<int, LightMutationError>
  addLamp(std::string_view named, const PunctualLight &light, const Mat4 &placed);

  /// Copy finite XYZ positions in local metres; the source is borrowed only during the call.
  /// Return a typed error without mutation for an invalid part, incomplete XYZ tuple or nonfinite
  /// value. Cross-attribute lengths and index bounds are checked by wellFormed().
  /// Successful replacement invalidates positionsOf(part); may allocate, O(metres.size()).
  /// Requires exclusive access to this non-moved-from object.
  /// @param part Active part index.
  /// @param metres Complete XYZ tuples, or empty to clear positions.
  /// @return Success, or a typed validation failure without mutation.
  [[nodiscard]] std::expected<void, GeometryAttributeError>
  setPositions(int part, std::span<const float> metres);
  /// Copy XYZ normals; each length must differ from one by at most 0.001.
  /// @param part Active part index.
  /// @param unit Finite tuples of 3 floats; empty removes the optional attribute.
  /// @return Success, or a typed validation failure without mutation.
  /// O(unit.size()), may allocate; cross-attribute counts are checked by wellFormed().
  [[nodiscard]] std::expected<void, GeometryAttributeError> setNormals(int part,
                                                                       std::span<const float> unit);
  /// Copy finite UV pairs without clamping or transforming them; O(uv.size()), may allocate.
  /// @param part Active part index.
  /// @param uv Finite UV pairs; empty removes the selected optional attribute.
  /// @param set Coordinate set, exactly 0 or 1.
  /// @return Success, or a typed validation failure without mutation.
  [[nodiscard]] std::expected<void, GeometryAttributeError>
  setTexture(int part, std::span<const float> uv, int set = 0);
  /// Copy XYZ tangent directions and handedness W; unit length and W sign are not validated.
  /// @param part Active part index.
  /// @param xyzw Finite tuples of 4 floats; empty removes the optional attribute.
  /// @return Success, or a typed validation failure without mutation.
  /// O(xyzw.size()), may allocate; cross-attribute counts are checked by wellFormed().
  [[nodiscard]] std::expected<void, GeometryAttributeError>
  setTangents(int part, std::span<const float> xyzw);
  /// Copy linear RGBA vertex factors; no range clamping or colour conversion.
  /// @param part Active part index.
  /// @param rgba Finite tuples of 4 floats; empty removes the optional attribute.
  /// @return Success, or a typed validation failure without mutation.
  /// O(rgba.size()), may allocate; cross-attribute counts are checked by wellFormed().
  [[nodiscard]] std::expected<void, GeometryAttributeError> setColours(int part,
                                                                       std::span<const float> rgba);
  /// Copy triangle indices; O(indices.size()), may allocate.
  /// @param part Active part index.
  /// @param indices Complete CCW triplets of part-local vertex indices; empty clears them.
  /// @return Success, or MissingPart/IncompleteTuple without mutation.
  /// Index bounds are deferred to wellFormed(), allowing attributes to be built in any order.
  [[nodiscard]] std::expected<void, GeometryAttributeError>
  setTriangles(int part, std::span<const uint32_t> indices);

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
  /// @param rgba Exactly widthPx * heightPx * 4 bytes, in row order, without padding.
  /// Texture usage determines sRGB versus linear interpretation.
  /// @return New owner-local image index, or a typed input/capacity error without mutation.
  [[nodiscard]] std::expected<int, GeometryImageError>
  addImage(int widthPx, int heightPx, std::span<const uint8_t> rgba);
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

  /// Validate and replace one material without allocation; serialize with owner access.
  /// @param surface Owner-local material reference.
  /// @param row Replacement data, including owner-local image references.
  /// @return Success or a typed failure preserving all previous values, names and indices.
  /// Image bindings must already resolve in this owner; no forward references on replacement.
  [[nodiscard]] std::expected<void, MaterialError> setSurface(MaterialInstance surface,
                                                              const Material &row) noexcept;

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
  /// attribute counts and in-range indices. Validate material factors, texture bindings and
  /// assigned material indices; unbound material indices select the default material.
  /// Part placements are validated on assignment. Light transforms and renderer feature
  /// support are not checked here.
  /// No allocation or mutation; O(materials + active parts + indices). No concurrent mutation.
  /// The object must not have been moved from.
  /// @return True if every active part meets these structural conditions.
  [[nodiscard]] bool wellFormed() const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
