#include <string>
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
    double PlacedM[16];
  };

  struct Named {
    std::string Named;
    Material Surface;
  };

  struct Placed {
    std::string Named;
    PunctualLight Light;
    double PlacedM[16];
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
    return part >= 0 && part < (int)Live ? &Parts[(size_t)part] : nullptr;
  }
};

}

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
  const double still[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (size_t at = 0; at < 16u; ++at) { piece.PlacedM[at] = still[at]; }
  return (int)Held_->Live++;
}

namespace {

[[nodiscard]] bool Into(std::vector<float> &slot, std::span<const float> from) {
  slot.assign(from.begin(), from.end());
  return true;
}

}

bool Geometry::setPositions(int part, std::span<const float> metres) {
  if (part < 0 || part >= (int)Held_->Live || metres.size() % 3 != 0) { return false; }
  return Into(Held_->Parts[(size_t)part].PositionsM, metres);
}

bool Geometry::setNormals(int part, std::span<const float> unit) {
  if (part < 0 || part >= (int)Held_->Live || unit.size() % 3 != 0) { return false; }
  return Into(Held_->Parts[(size_t)part].Normals, unit);
}

bool Geometry::setTexture(int part, std::span<const float> uv, int set) {
  if (part < 0 || part >= (int)Held_->Live || uv.size() % 2 != 0) { return false; }
  if (set != 0 && set != 1) { return false; }
  Geometry::Held::Piece &piece = Held_->Parts[(size_t)part];
  return Into(set == 0 ? piece.Uv : piece.Uv1, uv);
}

bool Geometry::setTangents(int part, std::span<const float> xyzw) {
  if (part < 0 || part >= (int)Held_->Live || xyzw.size() % 4 != 0) { return false; }
  return Into(Held_->Parts[(size_t)part].Tangents, xyzw);
}

bool Geometry::setColours(int part, std::span<const float> rgba) {
  if (part < 0 || part >= (int)Held_->Live || rgba.size() % 4 != 0) { return false; }
  return Into(Held_->Parts[(size_t)part].Colours, rgba);
}

bool Geometry::setTriangles(int part, std::span<const uint32_t> indices) {
  if (part < 0 || part >= (int)Held_->Live || indices.size() % 3 != 0) { return false; }
  Held_->Parts[(size_t)part].Indices.assign(indices.begin(), indices.end());
  return true;
}

bool TransformManager::setTransform(int part, const double modelM16[16]) {
  if (part < 0 || part >= Of_->parts()) { return false; }
  Of_->place(part, modelM16);
  return true;
}

const double *TransformManager::getTransform(int part) const {
  return Of_->placementOf(part);
}

TransformManager Geometry::transforms(void) {
  return TransformManager(*this);
}

LightManager Geometry::lights(void) {
  return LightManager(*this);
}

RenderableManager Geometry::renderables(void) {
  return RenderableManager(*this);
}

int LightManager::count(void) const {
  return Of_->lamps();
}

const PunctualLight &LightManager::getLight(int lamp) const {
  return Of_->lampAt(lamp);
}

std::string_view LightManager::nameOf(int lamp) const {
  return Of_->lampNameOf(lamp);
}

const double *LightManager::getTransform(int lamp) const {
  return Of_->lampPlacementOf(lamp);
}

bool LightManager::setLight(int lamp, const PunctualLight &light) {
  if (lamp < 0 || lamp >= Of_->lamps()) { return false; }
  Of_->relight(lamp, light);
  return true;
}

int RenderableManager::count(void) const {
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
  if (lamp < 0 || lamp >= (int)Held_->Lamps.size()) { return; }
  Held_->Lamps[(size_t)lamp].Light = light;
}

void Geometry::resurface(int part, MaterialInstance surface) {
  if (part < 0 || part >= (int)Held_->Live) { return; }
  Held_->Parts[(size_t)part].Material = surface.index();
}

void Geometry::place(int part, const double modelM16[16]) {
  if (part < 0 || part >= (int)Held_->Live) { return; }
  for (size_t at = 0; at < 16u; ++at) { Held_->Parts[(size_t)part].PlacedM[at] = modelM16[at]; }
}

MaterialInstance Geometry::addSurface(std::string_view named, const Material &surface) {
  Held_->Surfaces.push_back(Geometry::Held::Named{std::string(named), surface});
  return MaterialInstance((int)Held_->Surfaces.size() - 1);
}

int Geometry::addLamp(std::string_view named,
                      const PunctualLight &light,
                      const double placedM16[16]) {
  Geometry::Held::Placed placed;
  placed.Named = std::string(named);
  placed.Light = light;
  for (size_t at = 0; at < 16u; ++at) { placed.PlacedM[at] = placedM16[at]; }
  Held_->Lamps.push_back(std::move(placed));
  return (int)Held_->Lamps.size() - 1;
}

int Geometry::addImage(int widthPx, int heightPx, std::span<const uint8_t> rgba) {
  if (widthPx <= 0 || heightPx <= 0 || rgba.size() < (size_t)widthPx * (size_t)heightPx * 4u) {
    return -1;
  }
  Held::Picture made;
  made.WidthPx = widthPx;
  made.HeightPx = heightPx;
  made.Rgba.assign(rgba.begin(), rgba.end());
  Held_->Images.push_back(std::move(made));
  return (int)Held_->Images.size() - 1;
}

int Geometry::images() const {
  return (int)Held_->Images.size();
}

bool Geometry::setSurface(MaterialInstance surface, const Material &row) {
  const int at = surface.index();
  if (at < 0 || (size_t)at >= Held_->Surfaces.size()) { return false; }
  Held_->Surfaces[(size_t)at].Surface = row;
  return true;
}

ImageView Geometry::imageAt(int image) const {
  if (image < 0 || (size_t)image >= Held_->Images.size()) { return ImageView{}; }
  const Held::Picture &held = Held_->Images[(size_t)image];
  return ImageView{
      held.WidthPx, held.HeightPx, std::span<const uint8_t>(held.Rgba.data(), held.Rgba.size())};
}

int Geometry::surfaces() const {
  return (int)Held_->Surfaces.size();
}

std::string_view Geometry::surfaceNameOf(int surface) const {
  return surface >= 0 && surface < (int)Held_->Surfaces.size()
             ? std::string_view(Held_->Surfaces[(size_t)surface].Named)
             : std::string_view();
}

const Material &Geometry::surfaceAt(MaterialInstance surface) const {
  static const Material plain;
  const int at = surface.index();
  return at >= 0 && at < (int)Held_->Surfaces.size() ? Held_->Surfaces[(size_t)at].Surface : plain;
}

int Geometry::lamps() const {
  return (int)Held_->Lamps.size();
}

std::string_view Geometry::lampNameOf(int lamp) const {
  return lamp >= 0 && lamp < (int)Held_->Lamps.size()
             ? std::string_view(Held_->Lamps[(size_t)lamp].Named)
             : std::string_view();
}

const PunctualLight &Geometry::lampAt(int lamp) const {
  static const PunctualLight dark;
  return lamp >= 0 && lamp < (int)Held_->Lamps.size() ? Held_->Lamps[(size_t)lamp].Light : dark;
}

const double *Geometry::lampPlacementOf(int lamp) const {
  static const double still[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  return lamp >= 0 && lamp < (int)Held_->Lamps.size() ? Held_->Lamps[(size_t)lamp].PlacedM : still;
}

const double *Geometry::placementOf(int part) const {
  static const double still[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? piece->PlacedM : still;
}

int Geometry::parts() const {
  return (int)Held_->Live;
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
  if (piece == nullptr || (set != 0 && set != 1)) { return std::span<const float>(); }
  return std::span<const float>(set == 0 ? piece->Uv : piece->Uv1);
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
    for (uint32_t index : piece.Indices) {
      if ((size_t)index >= vertices) { return false; }
    }
  }
  return true;
}

}
