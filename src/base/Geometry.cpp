#include "GeometryHeld.h"

namespace outshine {

Geometry::Geometry() : Held_(std::make_unique<Held>()) {}
Geometry::~Geometry() = default;
Geometry::Geometry(Geometry &&) noexcept = default;
Geometry &Geometry::operator=(Geometry &&) noexcept = default;

const Geometry::Held &Geometry::Inside() const { return *Held_; }

int Geometry::Part(std::string_view named, int material) {
  Held_->Parts.push_back(Geometry::Held::Piece{std::string(named), material, {}, {}, {}, {}, {}, {}, {}});
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

int Geometry::Parts() const { return (int)Held_->Parts.size(); }

namespace {

[[nodiscard]] const Geometry::Held::Piece *At(const Geometry::Held &held, int part) {
  return part >= 0 && part < (int)held.Parts.size() ? &held.Parts[(size_t)part] : nullptr;
}

}

std::string_view Geometry::NameOf(int part) const {
  const Held::Piece *piece = At(*Held_, part);
  return piece != nullptr ? std::string_view(piece->Named) : std::string_view();
}

int Geometry::MaterialOf(int part) const {
  const Held::Piece *piece = At(*Held_, part);
  return piece != nullptr ? piece->Material : -1;
}

std::span<const float> Geometry::PositionsOf(int part) const {
  const Held::Piece *piece = At(*Held_, part);
  return piece != nullptr ? std::span<const float>(piece->PositionsM) : std::span<const float>();
}

std::span<const float> Geometry::NormalsOf(int part) const {
  const Held::Piece *piece = At(*Held_, part);
  return piece != nullptr ? std::span<const float>(piece->Normals) : std::span<const float>();
}

std::span<const float> Geometry::TextureOf(int part, int set) const {
  const Held::Piece *piece = At(*Held_, part);
  if (piece == nullptr || (set != 0 && set != 1)) { return std::span<const float>(); }
  return std::span<const float>(set == 0 ? piece->Uv : piece->Uv1);
}

std::span<const float> Geometry::TangentsOf(int part) const {
  const Held::Piece *piece = At(*Held_, part);
  return piece != nullptr ? std::span<const float>(piece->Tangents) : std::span<const float>();
}

std::span<const float> Geometry::ColoursOf(int part) const {
  const Held::Piece *piece = At(*Held_, part);
  return piece != nullptr ? std::span<const float>(piece->Colours) : std::span<const float>();
}

std::span<const uint32_t> Geometry::TrianglesOf(int part) const {
  const Held::Piece *piece = At(*Held_, part);
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
