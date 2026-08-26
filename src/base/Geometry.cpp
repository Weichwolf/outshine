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
  std::vector<Piece> Parts;
  std::vector<Named> Surfaces;
  std::vector<Placed> Lamps;

  [[nodiscard]] const Piece *At(int part) const {
    return part >= 0 && part < (int)Parts.size() ? &Parts[(size_t)part] : nullptr;
  }
};


}

namespace outshine {

Geometry::Geometry() : Held_(std::make_unique<Held>()) {}
Geometry::~Geometry() = default;
Geometry::Geometry(Geometry &&) noexcept = default;
Geometry &Geometry::operator=(Geometry &&) noexcept = default;

int Geometry::Part(std::string_view named, int material) {
  Geometry::Held::Piece piece;
  piece.Named = std::string(named);
  piece.Material = material;
  const double still[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (size_t at = 0; at < 16u; ++at) { piece.PlacedM[at] = still[at]; }
  Held_->Parts.push_back(std::move(piece));
  return (int)Held_->Parts.size() - 1;
}

namespace {

[[nodiscard]] bool Into(std::vector<float> &slot, std::span<const float> from) {
  slot.assign(from.begin(), from.end());
  return true;
}

}

bool Geometry::Positions(int part, std::span<const float> metres) {
  if (part < 0 || part >= (int)Held_->Parts.size() || metres.size() % 3 != 0) { return false; }
  return Into(Held_->Parts[(size_t)part].PositionsM, metres);
}

bool Geometry::Normals(int part, std::span<const float> unit) {
  if (part < 0 || part >= (int)Held_->Parts.size() || unit.size() % 3 != 0) { return false; }
  return Into(Held_->Parts[(size_t)part].Normals, unit);
}

bool Geometry::Texture(int part, std::span<const float> uv, int set) {
  if (part < 0 || part >= (int)Held_->Parts.size() || uv.size() % 2 != 0) { return false; }
  if (set != 0 && set != 1) { return false; }
  Geometry::Held::Piece &piece = Held_->Parts[(size_t)part];
  return Into(set == 0 ? piece.Uv : piece.Uv1, uv);
}

bool Geometry::Tangents(int part, std::span<const float> xyzw) {
  if (part < 0 || part >= (int)Held_->Parts.size() || xyzw.size() % 4 != 0) { return false; }
  return Into(Held_->Parts[(size_t)part].Tangents, xyzw);
}

bool Geometry::Colours(int part, std::span<const float> rgba) {
  if (part < 0 || part >= (int)Held_->Parts.size() || rgba.size() % 4 != 0) { return false; }
  return Into(Held_->Parts[(size_t)part].Colours, rgba);
}

bool Geometry::Triangles(int part, std::span<const uint32_t> indices) {
  if (part < 0 || part >= (int)Held_->Parts.size() || indices.size() % 3 != 0) { return false; }
  Held_->Parts[(size_t)part].Indices.assign(indices.begin(), indices.end());
  return true;
}

void Geometry::Place(int part, const double modelM16[16]) {
  if (part < 0 || part >= (int)Held_->Parts.size()) { return; }
  for (size_t at = 0; at < 16u; ++at) { Held_->Parts[(size_t)part].PlacedM[at] = modelM16[at]; }
}

int Geometry::Surface(std::string_view named, const Material &surface) {
  Held_->Surfaces.push_back(Geometry::Held::Named{std::string(named), surface});
  return (int)Held_->Surfaces.size() - 1;
}

int Geometry::Lamp(std::string_view named, const PunctualLight &light, const double placedM16[16]) {
  Geometry::Held::Placed placed;
  placed.Named = std::string(named);
  placed.Light = light;
  for (size_t at = 0; at < 16u; ++at) { placed.PlacedM[at] = placedM16[at]; }
  Held_->Lamps.push_back(std::move(placed));
  return (int)Held_->Lamps.size() - 1;
}

int Geometry::Surfaces() const { return (int)Held_->Surfaces.size(); }

std::string_view Geometry::SurfaceNameOf(int surface) const {
  return surface >= 0 && surface < (int)Held_->Surfaces.size()
             ? std::string_view(Held_->Surfaces[(size_t)surface].Named)
             : std::string_view();
}

const Material &Geometry::SurfaceAt(int surface) const {
  static const Material plain;
  return surface >= 0 && surface < (int)Held_->Surfaces.size()
             ? Held_->Surfaces[(size_t)surface].Surface
             : plain;
}

int Geometry::Lamps() const { return (int)Held_->Lamps.size(); }

std::string_view Geometry::LampNameOf(int lamp) const {
  return lamp >= 0 && lamp < (int)Held_->Lamps.size()
             ? std::string_view(Held_->Lamps[(size_t)lamp].Named)
             : std::string_view();
}

const PunctualLight &Geometry::LampAt(int lamp) const {
  static const PunctualLight dark;
  return lamp >= 0 && lamp < (int)Held_->Lamps.size() ? Held_->Lamps[(size_t)lamp].Light : dark;
}

const double *Geometry::LampPlacementOf(int lamp) const {
  static const double still[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  return lamp >= 0 && lamp < (int)Held_->Lamps.size() ? Held_->Lamps[(size_t)lamp].PlacedM : still;
}

const double *Geometry::PlacementOf(int part) const {
  static const double still[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? piece->PlacedM : still;
}

int Geometry::Parts() const { return (int)Held_->Parts.size(); }

std::string_view Geometry::NameOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::string_view(piece->Named) : std::string_view();
}

int Geometry::MaterialOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? piece->Material : -1;
}

std::span<const float> Geometry::PositionsOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const float>(piece->PositionsM) : std::span<const float>();
}

std::span<const float> Geometry::NormalsOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const float>(piece->Normals) : std::span<const float>();
}

std::span<const float> Geometry::TextureOf(int part, int set) const {
  const Held::Piece *piece = Held_->At(part);
  if (piece == nullptr || (set != 0 && set != 1)) { return std::span<const float>(); }
  return std::span<const float>(set == 0 ? piece->Uv : piece->Uv1);
}

std::span<const float> Geometry::TangentsOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const float>(piece->Tangents) : std::span<const float>();
}

std::span<const float> Geometry::ColoursOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const float>(piece->Colours) : std::span<const float>();
}

std::span<const uint32_t> Geometry::TrianglesOf(int part) const {
  const Held::Piece *piece = Held_->At(part);
  return piece != nullptr ? std::span<const uint32_t>(piece->Indices)
                          : std::span<const uint32_t>();
}

bool Geometry::Whole() const {
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
