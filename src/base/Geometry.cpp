#include "math/Mat4.h"
#include "MaterialValidation.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <scene/Geometry.h>

namespace outshine {

struct Geometry::Held {
  struct Piece {
    std::string Named;
    int Material;
    std::vector<float> PositionsM;
    std::vector<float> Normals;
    std::vector<float> Uv;
    std::vector<float> Uv1;
    std::vector<float> Tangents;
    std::vector<float> Colours;
    std::vector<uint32_t> Indices;
    Mat4 PlacedM;

    [[nodiscard]] bool Valid(size_t materials) const {
      if (Material >= 0 && std::cmp_greater_equal(Material, materials)) { return false; }
      if (PositionsM.empty() || Indices.empty()) { return false; }
      const size_t vertices = PositionsM.size() / 3;
      if (!Normals.empty() && Normals.size() / 3 != vertices) { return false; }
      if (!Uv.empty() && Uv.size() / 2 != vertices) { return false; }
      if (!Uv1.empty() && Uv1.size() / 2 != vertices) { return false; }
      if (!Tangents.empty() && Tangents.size() / 4 != vertices) { return false; }
      if (!Colours.empty() && Colours.size() / 4 != vertices) { return false; }
      return std::ranges::all_of(Indices, [vertices](uint32_t index) { return index < vertices; });
    }
  };

  struct Named {
    std::string Named;
    Material Surface;
  };

  struct Placed {
    std::string Named;
    PunctualLight Light;
    Mat4 PlacedM;
  };

  struct Picture {
    int WidthPx = 0;
    int HeightPx = 0;
    std::vector<uint8_t> Rgba;
  };

  std::vector<Piece> Parts;
  size_t Live = 0;
  std::vector<Named> Surfaces;
  std::vector<Picture> Images;
  std::vector<Placed> Lamps;

  [[nodiscard]] const Piece *At(int part) const {
    return part >= 0 && std::cmp_less(part, Live) ? &Parts[static_cast<size_t>(part)] : nullptr;
  }
};

}

namespace outshine {

Geometry::Geometry() : Held_(std::make_unique<Held>()) {}

Geometry::~Geometry() = default;
Geometry::Geometry(Geometry &&) noexcept = default;
Geometry &Geometry::operator=(Geometry &&) noexcept = default;

Geometry Geometry::clone() const {
  Geometry copy;
  copy.Held_->Parts.assign(Held_->Parts.begin(),
                           Held_->Parts.begin() + static_cast<std::ptrdiff_t>(Held_->Live));
  copy.Held_->Live = Held_->Live;
  copy.Held_->Surfaces = Held_->Surfaces;
  copy.Held_->Images = Held_->Images;
  copy.Held_->Lamps = Held_->Lamps;
  return copy;
}

void Geometry::clear() {
  for (size_t at = 0; at < Held_->Live && at < Held_->Parts.size(); ++at) {
    Geometry::Held::Piece &piece = Held_->Parts[at];
    piece.Named.clear();
    piece.PositionsM.clear();
    piece.Normals.clear();
    piece.Uv.clear();
    piece.Uv1.clear();
    piece.Tangents.clear();
    piece.Colours.clear();
    piece.Indices.clear();
  }
  Held_->Live = 0;
  Held_->Surfaces.clear();
  Held_->Images.clear();
  Held_->Lamps.clear();
}

int Geometry::addPart(std::string_view named, MaterialInstance material) {
  if (Held_->Live == Held_->Parts.size()) { Held_->Parts.emplace_back(); }
  Geometry::Held::Piece &piece = Held_->Parts[Held_->Live];
  piece.Named.assign(named.begin(), named.end());
  piece.Material = material.index();
  const Mat4 still;
  for (size_t at = 0; at < 16u; ++at) { piece.PlacedM[at] = still[at]; }
  return static_cast<int>(Held_->Live++);
}

namespace {

constexpr double kUnitWithin = 1.0e-3;
constexpr double kNoAreaM4 = 1.0e-12;

[[nodiscard]] bool Into(std::vector<float> &slot, std::span<const float> from) {
  for (const float value : from) {
    if (!std::isfinite(value)) { return false; }
  }
  slot.assign(from.begin(), from.end());
  return true;
}

}

bool Geometry::setPositions(int part, std::span<const float> metres) {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live) || metres.size() % 3 != 0) {
    return false;
  }
  return Into(Held_->Parts[static_cast<size_t>(part)].PositionsM, metres);
}

bool Geometry::setNormals(int part, std::span<const float> unit) {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live) || unit.size() % 3 != 0) {
    return false;
  }
  for (size_t at = 0; at + 2 < unit.size(); at += 3) {
    const double length = std::sqrt(static_cast<double>(unit[at]) * unit[at] +
                                    static_cast<double>(unit[at + 1]) * unit[at + 1] +
                                    static_cast<double>(unit[at + 2]) * unit[at + 2]);
    if (std::fabs(length - 1.0) > kUnitWithin) { return false; }
  }
  return Into(Held_->Parts[static_cast<size_t>(part)].Normals, unit);
}

int Geometry::windingAgainstNormals(int part) const {
  const Held::Piece *piece = Held_->At(part);
  if (piece == nullptr) { return 0; }
  const std::vector<float> &p = piece->PositionsM;
  const std::vector<float> &n = piece->Normals;
  int against = 0;
  for (size_t at = 0; at + 2 < piece->Indices.size(); at += 3) {
    const std::array<size_t, 3> corner = {
        piece->Indices[at], piece->Indices[at + 1], piece->Indices[at + 2]};
    if (corner[2] * 3 + 2 >= p.size() || corner[2] * 3 + 2 >= n.size() ||
        corner[0] * 3 + 2 >= p.size() || corner[1] * 3 + 2 >= p.size() ||
        corner[0] * 3 + 2 >= n.size() || corner[1] * 3 + 2 >= n.size()) {
      continue;
    }
    std::array<double, 3> u{};
    std::array<double, 3> v{};
    std::array<double, 3> sum{};
    for (size_t axis = 0; axis < 3; ++axis) {
      u[axis] = static_cast<double>(p[corner[1] * 3 + axis]) - p[corner[0] * 3 + axis];
      v[axis] = static_cast<double>(p[corner[2] * 3 + axis]) - p[corner[0] * 3 + axis];
      sum[axis] = static_cast<double>(n[corner[0] * 3 + axis]) + n[corner[1] * 3 + axis] +
                  n[corner[2] * 3 + axis];
    }
    const std::array<double, 3> face = {
        u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    if (face[0] * face[0] + face[1] * face[1] + face[2] * face[2] < kNoAreaM4) { continue; }
    if (face[0] * sum[0] + face[1] * sum[1] + face[2] * sum[2] < 0.0) { ++against; }
  }
  return against;
}

bool Geometry::setTexture(int part, std::span<const float> uv, int set) {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live) || uv.size() % 2 != 0) { return false; }
  if (set != 0 && set != 1) { return false; }
  Geometry::Held::Piece &piece = Held_->Parts[static_cast<size_t>(part)];
  return Into(set == 0 ? piece.Uv : piece.Uv1, uv);
}

bool Geometry::setTangents(int part, std::span<const float> xyzw) {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live) || xyzw.size() % 4 != 0) {
    return false;
  }
  return Into(Held_->Parts[static_cast<size_t>(part)].Tangents, xyzw);
}

bool Geometry::setColours(int part, std::span<const float> rgba) {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live) || rgba.size() % 4 != 0) {
    return false;
  }
  return Into(Held_->Parts[static_cast<size_t>(part)].Colours, rgba);
}

bool Geometry::setTriangles(int part, std::span<const uint32_t> indices) {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live) || indices.size() % 3 != 0) {
    return false;
  }
  Held_->Parts[static_cast<size_t>(part)].Indices.assign(indices.begin(), indices.end());
  return true;
}

bool Geometry::setLight(int lamp, const PunctualLight &light) noexcept {
  if (lamp < 0 || std::cmp_greater_equal(lamp, Held_->Lamps.size())) { return false; }
  Held_->Lamps[static_cast<size_t>(lamp)].Light = light;
  return true;
}

bool Geometry::setMaterial(int part, MaterialInstance surface) noexcept {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live) || !surface.bound() ||
      std::cmp_greater_equal(surface.index(), Held_->Surfaces.size())) {
    return false;
  }
  Held_->Parts[static_cast<size_t>(part)].Material = surface.index();
  return true;
}

bool Geometry::setPlacement(int part, const Mat4 &model) noexcept {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live)) { return false; }
  Held_->Parts[static_cast<size_t>(part)].PlacedM = model;
  return true;
}

MaterialInstance Geometry::addSurface(std::string_view named, const Material &surface) {
  Held_->Surfaces.push_back(Geometry::Held::Named{.Named = std::string(named), .Surface = surface});
  return MaterialInstance(static_cast<int>(Held_->Surfaces.size()) - 1);
}

int Geometry::addLamp(std::string_view named, const PunctualLight &light, const Mat4 &placed) {
  Geometry::Held::Placed lamp;
  lamp.Named = std::string(named);
  lamp.Light = light;
  lamp.PlacedM = placed;
  Held_->Lamps.push_back(std::move(lamp));
  return static_cast<int>(Held_->Lamps.size()) - 1;
}

int Geometry::addImage(int widthPx, int heightPx, std::span<const uint8_t> rgba) {
  if (widthPx <= 0 || heightPx <= 0 ||
      std::cmp_greater_equal(Held_->Images.size(), std::numeric_limits<int>::max())) {
    return -1;
  }
  constexpr size_t bytesPerPixel = 4;
  const auto width = static_cast<size_t>(widthPx);
  const auto height = static_cast<size_t>(heightPx);
  if (width > std::numeric_limits<size_t>::max() / bytesPerPixel / height) { return -1; }
  const size_t bytes = width * height * bytesPerPixel;
  if (rgba.size() != bytes) { return -1; }
  Held::Picture made;
  made.WidthPx = widthPx;
  made.HeightPx = heightPx;
  made.Rgba.assign(rgba.begin(), rgba.end());
  Held_->Images.push_back(std::move(made));
  return static_cast<int>(Held_->Images.size()) - 1;
}

int Geometry::images() const {
  return static_cast<int>(Held_->Images.size());
}

std::expected<void, MaterialUpdateError> Geometry::setSurface(MaterialInstance surface,
                                                              const Material &row) noexcept {
  const int at = surface.index();
  if (at < 0 || static_cast<size_t>(at) >= Held_->Surfaces.size()) {
    return std::unexpected(MaterialUpdateError::MissingMaterial);
  }
  if (!MaterialIsValid(row, Held_->Images.size())) {
    return std::unexpected(MaterialUpdateError::InvalidMaterial);
  }
  Held_->Surfaces[static_cast<size_t>(at)].Surface = row;
  return {};
}

ImageView Geometry::imageAt(int image) const {
  if (image < 0 || static_cast<size_t>(image) >= Held_->Images.size()) { return ImageView{}; }
  const Held::Picture &held = Held_->Images[static_cast<size_t>(image)];
  return ImageView{.WidthPx = held.WidthPx,
                   .HeightPx = held.HeightPx,
                   .Rgba = std::span<const uint8_t>(held.Rgba.data(), held.Rgba.size())};
}

int Geometry::surfaces() const {
  return static_cast<int>(Held_->Surfaces.size());
}

std::string_view Geometry::surfaceNameOf(int surface) const {
  return surface >= 0 && std::cmp_less(surface, Held_->Surfaces.size())
             ? std::string_view(Held_->Surfaces[static_cast<size_t>(surface)].Named)
             : std::string_view();
}

const Material &Geometry::surfaceAt(MaterialInstance surface) const {
  static const Material plain;
  const int at = surface.index();
  return at >= 0 && std::cmp_less(at, Held_->Surfaces.size())
             ? Held_->Surfaces[static_cast<size_t>(at)].Surface
             : plain;
}

int Geometry::lamps() const {
  return static_cast<int>(Held_->Lamps.size());
}

std::string_view Geometry::lampNameOf(int lamp) const {
  return lamp >= 0 && std::cmp_less(lamp, Held_->Lamps.size())
             ? std::string_view(Held_->Lamps[static_cast<size_t>(lamp)].Named)
             : std::string_view();
}

const PunctualLight &Geometry::lampAt(int lamp) const {
  static const PunctualLight dark;
  return lamp >= 0 && std::cmp_less(lamp, Held_->Lamps.size())
             ? Held_->Lamps[static_cast<size_t>(lamp)].Light
             : dark;
}

const Mat4 &Geometry::lampPlacementOf(int lamp) const {
  static const Mat4 still;
  return lamp >= 0 && std::cmp_less(lamp, Held_->Lamps.size())
             ? Held_->Lamps[static_cast<size_t>(lamp)].PlacedM
             : still;
}

const Mat4 &Geometry::placementOf(int part) const {
  static const Mat4 still;
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? piece->PlacedM : still;
}

int Geometry::parts() const {
  return static_cast<int>(Held_->Live);
}

std::string_view Geometry::nameOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::string_view(piece->Named) : std::string_view();
}

MaterialInstance Geometry::materialOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return MaterialInstance(piece != nullptr ? piece->Material : -1);
}

std::span<const float> Geometry::positionsOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const float>(piece->PositionsM) : std::span<const float>();
}

std::span<const float> Geometry::normalsOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const float>(piece->Normals) : std::span<const float>();
}

std::span<const float> Geometry::textureOf(int part, UvSet set) const {
  const Held::Piece *piece = Held_->At(part);
  if (piece == nullptr) { return {}; }
  return {set == UvSet::Uv0 ? piece->Uv : piece->Uv1};
}

std::span<const float> Geometry::tangentsOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const float>(piece->Tangents) : std::span<const float>();
}

std::span<const float> Geometry::coloursOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const float>(piece->Colours) : std::span<const float>();
}

std::span<const uint32_t> Geometry::trianglesOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const uint32_t>(piece->Indices) : std::span<const uint32_t>();
}

bool Geometry::wellFormed() const {
  if (Held_->Live == 0) { return false; }
  if (!std::ranges::all_of(Held_->Surfaces, [this](const Held::Named &surface) {
        return MaterialIsValid(surface.Surface, Held_->Images.size());
      })) {
    return false;
  }
  const auto parts = std::span(Held_->Parts).first(Held_->Live);
  return std::ranges::all_of(
      parts, [this](const Held::Piece &piece) { return piece.Valid(Held_->Surfaces.size()); });
}

}
