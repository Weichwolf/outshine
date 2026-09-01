#include "math/Mat4.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Geometry.h>

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

} // namespace outshine

namespace outshine {

Geometry::Geometry() : Held_(std::make_unique<Held>()) {}

Geometry::~Geometry() = default;
Geometry::Geometry(Geometry &&) noexcept = default;
Geometry &Geometry::operator=(Geometry &&) noexcept = default;

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

[[nodiscard]] bool Into(std::vector<float> &slot, std::span<const float> from) {
  slot.assign(from.begin(), from.end());
  return true;
}

} // namespace

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
  return Into(Held_->Parts[static_cast<size_t>(part)].Normals, unit);
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

bool TransformManager::setTransform(int part, const Mat4 &model) {
  if (part < 0 || part >= Of_->parts()) { return false; }
  Of_->place(part, model);
  return true;
}

const Mat4 &TransformManager::getTransform(int part) const {
  return Of_->placementOf(part);
}

TransformManager Geometry::transforms() {
  return TransformManager(*this);
}

LightManager Geometry::lights() {
  return LightManager(*this);
}

RenderableManager Geometry::renderables() {
  return RenderableManager(*this);
}

int LightManager::count() const {
  return Of_->lamps();
}

const PunctualLight &LightManager::getLight(int lamp) const {
  return Of_->lampAt(lamp);
}

std::string_view LightManager::nameOf(int lamp) const {
  return Of_->lampNameOf(lamp);
}

const Mat4 &LightManager::getTransform(int lamp) const {
  return Of_->lampPlacementOf(lamp);
}

bool LightManager::setLight(int lamp, const PunctualLight &light) {
  if (lamp < 0 || lamp >= Of_->lamps()) { return false; }
  Of_->relight(lamp, light);
  return true;
}

int RenderableManager::count() const {
  return Of_->parts();
}

std::string_view RenderableManager::nameOf(int part) const {
  return Of_->nameOf(part);
}

MaterialInstance RenderableManager::getMaterial(int part) const {
  return Of_->materialOf(part);
}

size_t RenderableManager::vertexCount(int part) const {
  return Of_->positionsOf(part).size() / 3;
}

size_t RenderableManager::triangleCount(int part) const {
  return Of_->trianglesOf(part).size() / 3;
}

bool RenderableManager::setMaterial(int part, MaterialInstance surface) {
  if (part < 0 || part >= Of_->parts() || !surface.bound() || surface.index() >= Of_->surfaces()) {
    return false;
  }
  Of_->resurface(part, surface);
  return true;
}

void Geometry::relight(int lamp, const PunctualLight &light) {
  if (lamp < 0 || std::cmp_greater_equal(lamp, Held_->Lamps.size())) { return; }
  Held_->Lamps[static_cast<size_t>(lamp)].Light = light;
}

void Geometry::resurface(int part, MaterialInstance surface) {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live)) { return; }
  Held_->Parts[static_cast<size_t>(part)].Material = surface.index();
}

void Geometry::place(int part, const Mat4 &model) {
  if (part < 0 || std::cmp_greater_equal(part, Held_->Live)) { return; }
  Held_->Parts[static_cast<size_t>(part)].PlacedM = model;
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
      rgba.size() < static_cast<size_t>(widthPx) * static_cast<size_t>(heightPx) * 4u) {
    return -1;
  }
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

bool Geometry::setSurface(MaterialInstance surface, const Material &row) {
  const int at = surface.index();
  if (at < 0 || static_cast<size_t>(at) >= Held_->Surfaces.size()) { return false; }
  Held_->Surfaces[static_cast<size_t>(at)].Surface = row;
  return true;
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

std::span<const float> Geometry::textureOf(int part, int set) const {
  const Held::Piece *piece = Held_->At(part);
  if (piece == nullptr || (set != 0 && set != 1)) { return {}; }
  return {set == 0 ? piece->Uv : piece->Uv1};
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
  if (Held_->Parts.empty()) { return false; }
  for (const Geometry::Held::Piece &piece : Held_->Parts) {
    if (piece.PositionsM.empty() || piece.Indices.empty()) { return false; }
    const size_t vertices = piece.PositionsM.size() / 3;
    if (!piece.Normals.empty() && piece.Normals.size() / 3 != vertices) { return false; }
    if (!piece.Uv.empty() && piece.Uv.size() / 2 != vertices) { return false; }
    if (!piece.Uv1.empty() && piece.Uv1.size() / 2 != vertices) { return false; }
    if (!piece.Tangents.empty() && piece.Tangents.size() / 4 != vertices) { return false; }
    if (!piece.Colours.empty() && piece.Colours.size() / 4 != vertices) { return false; }
    for (const uint32_t index : piece.Indices) {
      if (static_cast<size_t>(index) >= vertices) { return false; }
    }
  }
  return true;
}

} // namespace outshine
