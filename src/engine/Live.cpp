#include <expected>
#include <span>
#include <array>
#include <chrono>
#include "math/Units.h"
#include "math/Mat4.h"
#include "Live.h"
#include "AzimuthElevation.h"

#include "Shaped.h"
#include "Surfaces.h"

#include <cstdint>
#include <cstddef>
#include <limits>

#include <algorithm>

#include <memory>
#include <numbers>
#include <cmath>
#include <string>
#include <string_view>
#include <optional>
#include <utility>
#include <ratio>
#include <vector>

#include "Heap.h"
#include "Image.h"

#include "Framing.h"
#include "SubjectProxy.h"
#include "Wgs84.h"

namespace outshine::Core {

namespace Says {
constexpr auto InvalidInitialGeometry = "initial native geometry is not well formed";
constexpr auto NoGeometrySurface = "native geometry requires a declared surface policy";
}

constexpr double kExposureCalibration = 1.2;
constexpr double kMeteredMiddleGrey = 2.5;

constexpr double kLuminanceRed = 0.2126;
constexpr double kLuminanceGreen = 0.7152;
constexpr double kLuminanceBlue = 0.0722;

namespace {

struct Listed {
  const std::vector<std::string> &Stages;
  const std::vector<std::string> &Outputs;
};

bool DeclarePlan(const std::vector<Render::SubjectMaterial> &surfaces,
                 bool sky,
                 bool shadows,
                 bool presents,
                 const Listed &lists,
                 Render::PlanSpec &declaration,
                 std::string &error) {
  declaration.Outputs = {Render::Resource::FrameTex};
  if (presents) { declaration.Outputs.push_back(Render::Resource::Surface); }
  for (const std::string &named : lists.Outputs) {
    const std::optional<Render::Resource> row = Render::Compiled::ResourceByName(named);
    if (!row) {
      error = "the declaration asks the frame to keep '" + named +
              "', and the catalogue holds no picture by that name -- an output list is checked "
              "against the catalogue because a typo that silently drops a buffer leaves a client "
              "reading zeros and calling them a measurement";
      return false;
    }
    bool already = false;
    for (const Render::Resource held : declaration.Outputs) { already = already || held == *row; }
    if (!already) { declaration.Outputs.push_back(*row); }
  }

  if (!lists.Stages.empty()) {

    declaration.Content.clear();
    for (const std::string &named : lists.Stages) {
      const std::optional<Render::Stage> row = Render::Compiled::StageByName(named);
      if (!row) {
        error = "the declaration names render stage '" + named +
                "', and the catalogue holds no row by that name -- a stage list is checked "
                "against the catalogue because a typo that silently drops a pass is a picture "
                "nobody can explain";
        return false;
      }
      declaration.Content.push_back(*row);
    }
    declaration.Display = Render::Declared<Render::Transfer>(Render::Transfer::Filmic);
    declaration.Exposure = Render::Declared<float>(1.0f);
    return true;
  }
  declaration.Outputs.push_back(Render::Resource::SceneVelocity);
  declaration.Content = {Render::Stage::Subjects, Render::Stage::Overlay};
  if (sky) {
    declaration.Content.push_back(Render::Stage::Sky);
    declaration.Content.push_back(Render::Stage::AerialPerspective);
  }
  if (shadows) { declaration.Content.push_back(Render::Stage::LightVisibility); }
  bool carriesGlass = false;
  for (const Render::SubjectMaterial &surface : surfaces) {
    const SurfaceKind kind = surface.State().Kind();
    carriesGlass =
        carriesGlass || kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive;
  }
  if (carriesGlass) {
    declaration.Content.push_back(Render::Stage::SubjectsTransmissive);
    declaration.Content.push_back(Render::Stage::CompositeTransmission);
  }

  declaration.Display = Render::Declared<Render::Transfer>(Render::Transfer::Filmic);
  declaration.Exposure = Render::Declared<float>(1.0f);
  return true;
}

}

Live::Live(Render::SceneRenderer &renderer, Declaration declaration, const Ui::Font *font)
    : Renderer_(&renderer), Declared_(std::move(declaration)) {
  if (Declared_.InitialGeometry != nullptr) {
    Held_.Carries(Declared_.InitialGeometry->clone());
    Declared_.InitialGeometry = nullptr;
  }
  Over_.Faces(font);
}

Live::~Live() {
  if (Renderer_ == nullptr) { return; }

  std::string ignored;
  (void)Renderer_->SetSubjectMesh(Render::SubjectMesh{}, ignored);
  (void)Renderer_->SetOverlay(nullptr, 0, ignored);
  Renderer_->SetPictureRegion({});
}

bool Live::Open(Render::SceneRenderer &renderer,
                Declaration declaration,
                const Ui::Font *font,
                std::unique_ptr<Live> &out,
                std::string &error) {
  const bool framesSubject = out && !out->Camera_.Prepared().HasExplicitCamera;
  if (!renderer.BeginsWorldCandidate(error)) { return false; }
  std::unique_ptr<Live> candidate;
  if (!Prepare(renderer, std::move(declaration), font, candidate, error)) {
    renderer.AbandonsWorldCandidate();
    return false;
  }
  if (framesSubject) { candidate->FrameItself(); }
  if (!renderer.PublishesWorldCandidate(error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  HandOffRenderer(out);
  out = std::move(candidate);
  return true;
}

bool Live::Prepare(Render::SceneRenderer &renderer,
                   Declaration declaration,
                   const Ui::Font *font,
                   std::unique_ptr<Live> &out,
                   std::string &error) {
  if (declaration.InitialGeometry != nullptr && !declaration.InitialGeometry->wellFormed()) {
    error = Says::InvalidInitialGeometry;
    return false;
  }
  std::unique_ptr<Live> live(new Live(renderer, std::move(declaration), font));
  if (!live->Build(error)) { return false; }
  out = std::move(live);
  return true;
}

bool Live::ReplacesGeometry(Render::SceneRenderer &renderer,
                            const Live &previous,
                            Geometry replacement,
                            const Ui::Font *font,
                            std::unique_ptr<Live> &out,
                            std::string &error) {
  std::unique_ptr<Live> candidate;
  if (!PreparesGeometryReplacement(
          renderer, previous, std::move(replacement), font, candidate, error)) {
    return false;
  }
  return PublishesPreparedWorld(renderer, out, candidate, error);
}

bool Live::PreparesGeometryReplacement(Render::SceneRenderer &renderer,
                                       const Live &previous,
                                       Geometry replacement,
                                       const Ui::Font *font,
                                       std::unique_ptr<Live> &candidate,
                                       std::string &error) {
  if (!renderer.BeginsWorldCandidate(error)) { return false; }
  if (!Prepare(renderer, previous.Declared_, font, candidate, error)) {
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->GroundAlbedo_ = previous.GroundAlbedo_;
  candidate->GroundSurface_ = previous.GroundSurface_;
  candidate->Scratch_.Digests = previous.Scratch_.Digests;
  if (!candidate->SetGeometry(std::move(replacement), previous.Carrying_, error) ||
      !candidate->RestoresPieceResources(error) || !candidate->RestoresGroundResources(error) ||
      !candidate->Scrolled(previous.Over_.Scrolled(), error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->Camera_ = previous.Camera_;
  return true;
}

bool Live::PreparesWorldReplacement(Render::SceneRenderer &renderer,
                                    const Live &previous,
                                    const Ui::Font *font,
                                    std::unique_ptr<Live> &candidate,
                                    std::string &error) {
  if (previous.Held_.HoldsBuilt()) {
    return PreparesGeometryReplacement(
        renderer, previous, previous.Held_.Built().clone(), font, candidate, error);
  }
  if (!renderer.BeginsWorldCandidate(error)) { return false; }
  if (!Prepare(renderer, previous.Declared_, font, candidate, error)) {
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->GroundAlbedo_ = previous.GroundAlbedo_;
  candidate->GroundSurface_ = previous.GroundSurface_;
  candidate->Scratch_.Digests = previous.Scratch_.Digests;
  if (!candidate->RestoresPieceResources(error) || !candidate->RestoresGroundResources(error) ||
      !candidate->Scrolled(previous.Over_.Scrolled(), error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->Camera_ = previous.Camera_;
  return true;
}

bool Live::PublishesPreparedWorld(Render::SceneRenderer &renderer,
                                  std::unique_ptr<Live> &out,
                                  std::unique_ptr<Live> &candidate,
                                  std::string &error) {
  if (!renderer.PublishesWorldCandidate(error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  HandOffRenderer(out);
  out = std::move(candidate);
  return true;
}

double Live::Framing() const {
  return Declared_.Fill > 0.0 ? Declared_.Fill : Render::kFramingFill;
}

bool Live::Reshape(std::string &error) {
  if (EverShaped_ && ShapedAt_ == Held_.Changed()) { return true; }
  EverShaped_ = false;
  Shaped_ = {};
  const bool alsoStands = Held_.Stands() && !Held_.Assembled().Parts().empty();
  const auto prepare = [this, alsoStands] {
    if (!Held_.HoldsBuilt()) { return Gltf::Shaped(Held_.Assembled(), ShapeParts_); }
    if (alsoStands) { return Gltf::Shaped(Held_.Assembled(), Held_.Built(), ShapeParts_); }
    return Render::PrepareShape(Held_.Built(), ShapeParts_);
  };
  const auto shaped = prepare();
  if (!shaped) {
    error = Describe(shaped.error());
    return false;
  }
  Shaped_ = *shaped;
  ShapedAt_ = Held_.Changed();
  EverShaped_ = true;
  return true;
}

namespace {

double Photopic(const Vec3f &triple) {
  return kLuminanceRed * static_cast<double>(triple[0]) +
         kLuminanceGreen * static_cast<double>(triple[1]) +
         kLuminanceBlue * static_cast<double>(triple[2]);
}

}

Medium Live::DeclaredAir() const {
  return Hazed(kEarthAir, Declared_.Haze);
}

double Live::MeteredLux() const {
  if (!Declared_.KeyFromClock) { return Declared_.KeyLux; }
  const double cosSun = std::sin(Declared_.KeyElevationDeg * kDeg2Rad);
  const GroundLight reach = GroundAir_.Evaluate(DeclaredAir(), cosSun);
  const double straightDown = cosSun > 0.0 ? cosSun : 0.0;
  return kSolarIlluminanceLx *
         (straightDown * Photopic(reach.SunTransmittance) + Photopic(reach.SkyIrradiance));
}

void Live::PaintsPart(Wearing what,
                      const Scenario::SurfaceOverride &said,
                      std::vector<uint32_t> &wearers) {
  Render::SubjectMaterial made =
      said.KeepsMaps ? Table_.Slots[what.Slot] : Render::SubjectMaterial{};
  made.Row = said.Row;
  if (what.Slot < wearers.size() && wearers[what.Slot] == 1u) {
    Table_.Slots[what.Slot] = made;
    return;
  }
  if (what.Slot < wearers.size()) { wearers[what.Slot] -= 1u; }
  const int carried = what.Slot < Table_.Material.size() ? Table_.Material[what.Slot] : -1;
  const int native =
      what.Slot < Table_.NativeMaterial.size() ? Table_.NativeMaterial[what.Slot] : -1;
  Table_.Slots.push_back(made);
  Table_.Material.push_back(carried);
  Table_.NativeMaterial.push_back(native);
  Table_.Decoded.emplace_back();
  Table_.PartSlot[what.Part] = static_cast<uint32_t>(Table_.Slots.size() - 1u);
}

size_t Live::WornByNodeOrPart() {
  size_t took = 0;
  const std::vector<Gltf::Part> &standing = Held_.Assembled().Parts();
  const size_t many =
      standing.size() < Table_.PartSlot.size() ? standing.size() : Table_.PartSlot.size();
  std::vector<uint32_t> wearers(Table_.Slots.size(), 0u);
  for (const uint32_t worn : Table_.PartSlot) {
    if (worn < wearers.size()) { wearers[worn] += 1u; }
  }
  Table_.Slots.reserve(Table_.Slots.size() + many);
  Table_.Material.reserve(Table_.Material.size() + many);
  Table_.NativeMaterial.reserve(Table_.NativeMaterial.size() + many);
  Table_.Decoded.reserve(Table_.Decoded.size() + many);
  for (size_t part = 0; part < many; ++part) {
    const uint32_t slot = Table_.PartSlot[part];
    if (slot >= Table_.Slots.size()) { continue; }
    for (const Scenario::SurfaceOverride &said : Declared_.Overriding) {
      const bool byNode = !said.Node.empty() && said.Node == standing[part].NodeName;
      const bool byPart = said.Part >= 0 && std::cmp_equal(said.Part, part);
      if (!byNode && !byPart) { continue; }
      PaintsPart({.Part = part, .Slot = slot}, said, wearers);
      ++took;
      break;
    }
  }
  return took;
}

bool Live::WearsOverrides([[maybe_unused]] std::string &error) {
  for (size_t slot = 0; slot < Table_.Slots.size(); ++slot) {
    const int index = Table_.Material[slot];
    if (index < 0 || static_cast<size_t>(index) >= Held_.File().Materials().size()) { continue; }
    const std::string &named = Held_.File().Materials()[static_cast<size_t>(index)].Name;
    for (const Scenario::SurfaceOverride &said : Declared_.Overriding) {
      if (said.Named != named) { continue; }
      if (!said.KeepsMaps) { Table_.Slots[slot] = Render::SubjectMaterial{}; }
      Table_.Slots[slot].Row = said.Row;
      ++OverridesWorn_;
      break;
    }
  }
  OverridesWorn_ += WornByNodeOrPart();
  return true;
}

size_t Live::WornByNativeSurfaceAndPart(const Geometry &native, size_t firstPart) {
  return WornByNativeSurface(native) + WornByNativeParts(native, firstPart);
}

size_t Live::WornByNativeSurface(const Geometry &native) {
  size_t took = 0;
  for (size_t slot = 0; slot < Table_.Slots.size(); ++slot) {
    const int surface = Table_.NativeMaterial[slot];
    if (surface < 0 || surface >= native.surfaces()) { continue; }
    const std::string_view named = native.surfaceNameOf(surface);
    for (const Scenario::SurfaceOverride &said : Declared_.Overriding) {
      if (said.Named != named) { continue; }
      if (!said.KeepsMaps) { Table_.Slots[slot] = Render::SubjectMaterial{}; }
      Table_.Slots[slot].Row = said.Row;
      ++took;
      break;
    }
  }
  return took;
}

size_t Live::WornByNativeParts(const Geometry &native, size_t firstPart) {
  size_t took = 0;
  std::vector<uint32_t> wearers(Table_.Slots.size(), 0u);
  for (const uint32_t worn : Table_.PartSlot) {
    if (worn < wearers.size()) { wearers[worn] += 1u; }
  }
  const auto nativeParts = static_cast<size_t>(native.parts());
  const size_t available =
      Table_.PartSlot.size() > firstPart ? Table_.PartSlot.size() - firstPart : 0u;
  const size_t many = std::min(nativeParts, available);
  Table_.Slots.reserve(Table_.Slots.size() + many);
  Table_.Material.reserve(Table_.Material.size() + many);
  Table_.NativeMaterial.reserve(Table_.NativeMaterial.size() + many);
  Table_.Decoded.reserve(Table_.Decoded.size() + many);
  for (size_t local = 0; local < many; ++local) {
    const size_t part = firstPart + local;
    const uint32_t slot = Table_.PartSlot[part];
    if (slot >= Table_.Slots.size() || part >= Shaped_.Parts.size()) { continue; }
    for (const Scenario::SurfaceOverride &said : Declared_.Overriding) {
      const bool byNode = !said.Node.empty() && said.Node == Shaped_.Parts[part].Name;
      const bool byPart = said.Part >= 0 && std::cmp_equal(said.Part, part);
      if (!byNode && !byPart) { continue; }
      PaintsPart({.Part = part, .Slot = slot}, said, wearers);
      ++took;
      break;
    }
  }
  return took;
}

bool Live::RejectsUnwornOverrides(std::string &error) const {
  if (Declared_.Overriding.empty() || OverridesWorn_ > 0) { return true; }
  error = "this declaration names " + std::to_string(Declared_.Overriding.size()) +
          " surface(s) of '" + Declared_.Stands +
          "' and the subject carries neither those material names, those part names nor those "
          "part indices -- a surface declared onto nothing changes no pixel and says it did";
  return false;
}

bool Live::JoinsSubjects(std::string &error) {
  for (const std::string &joining : Declared_.Joins) {
    Core::Posed arriving;
    if (!arriving.Reads({.Path = joining, .Variant = ""},
                        Declared_.Animation,
                        {.Clip = Declared_.Clip, .Fps = Declared_.Fps},
                        error)) {
      return false;
    }
    if (!arriving.Poses(0.0, error)) { return false; }
    AssetReads_ += 1;
    if (!Held_.Appends(arriving.Assembled())) {
      error = "the subject '" + joining + "' would not append onto the one before it";
      return false;
    }
  }
  return true;
}

bool Live::StandsSubjects(std::string &error) {
  if (!Held_.Stands()) {
    if (!Held_.Reads({.Path = Declared_.Stands, .Variant = Declared_.Variant},
                     Declared_.Animation,
                     {.Clip = Declared_.Clip, .Fps = Declared_.Fps},
                     error)) {
      return false;
    }
    AssetReads_ += 1;
  }
  if (!Pose(0.0, error)) { return false; }
  if (!JoinsSubjects(error)) { return false; }
  Gltf::ResolveSurfaceTable(Held_.File(), Held_.Assembled(), true, true, Table_);
  if (!Gltf::ResolveFileSurface(Held_.File(),
                                Held_.Assembled(),
                                Render::ColourFrom::Row,
                                Render::ColourCarrier::Texture,
                                Table_,
                                error)) {
    return false;
  }
  OverridesWorn_ = 0;
  if (!Declared_.Overriding.empty() && !WearsOverrides(error)) { return false; }
  if (Held_.HoldsBuilt() && !AppendNativeSurfaceTable(Held_.Built(), error)) { return false; }
  return RejectsUnwornOverrides(error);
}

bool Live::AppendNativeSurfaceTable(const Geometry &native, std::string &error) {
  const auto base = static_cast<uint32_t>(Table_.Slots.size());
  const Geometry &also = native;
  for (int surface = 0; surface < also.surfaces(); ++surface) {
    Render::SubjectMaterial made;
    made.Row = also.surfaceAt(MaterialInstance(surface));
    Table_.Slots.push_back(made);
    Table_.Material.push_back(-1);
    Table_.NativeMaterial.push_back(surface);
    Table_.Decoded.emplace_back();
  }
  const size_t before = Table_.PartSlot.size();
  Table_.PartSlot.resize(before + static_cast<size_t>(also.parts()), base);
  std::optional<uint32_t> defaultSlot;
  for (int part = 0; part < also.parts(); ++part) {
    const int material = also.materialOf(part).index();
    uint32_t slot = base;
    if (material >= 0 && material < also.surfaces()) {
      slot += static_cast<uint32_t>(material);
    } else {
      if (!defaultSlot) {
        if (Declared_.Surfacing.empty()) {
          error = Says::NoGeometrySurface;
          return false;
        }
        defaultSlot = static_cast<uint32_t>(Table_.Slots.size());
        Render::SubjectMaterial surface;
        surface.Row = Declared_.Surfacing.front();
        Table_.Slots.push_back(surface);
        Table_.Material.push_back(-1);
        Table_.NativeMaterial.push_back(-1);
        Table_.Decoded.emplace_back();
      }
      slot = *defaultSlot;
    }
    Table_.PartSlot[before + static_cast<size_t>(part)] = slot;
  }
  if (!Render::ResolveNativeTextures(
          also, std::span<Render::SubjectMaterial>(Table_.Slots).subspan(base), error)) {
    return false;
  }
  OverridesWorn_ += WornByNativeSurfaceAndPart(native, before);
  Joined_ = before;
  Carrying_ = before;
  return true;
}

void Live::ClearsSubject() {
  Held_.Clears();
  Table_ = Render::SurfaceTable();
  ShadowRadiusStoodM_ = 0.0;
  Joined_ = 0;
  Carrying_ = 0;
  Stoodup_ = false;
  PartBounds_.clear();
  if (Renderer_ != nullptr) {
    std::string ignored;
    (void)Renderer_->SetSubjectMesh(Render::SubjectMesh{}, ignored);
    (void)Renderer_->SetSubjectPlacements(nullptr, 0, ignored);
  }
}

bool Live::CarriesBuilt(std::string &error) {
  if (Declared_.Surfacing.empty()) {
    error = "the declaration carries a built subject and no surface -- a body without a "
            "material cannot be resolved, and an empty list is a refusal, not a "
            "dereference";
    return false;
  }
  const auto tookFrom = std::chrono::steady_clock::now();
  CarryMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tookFrom)
                 .count();
  const auto reshapedFrom = std::chrono::steady_clock::now();
  if (!Reshape(error)) { return false; }
  ReshapeMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - reshapedFrom)
          .count();
  const auto resolvedFrom = std::chrono::steady_clock::now();
  Render::ResolveDeclaredSurface(Shaped_, Declared_.Surfacing.front(), Table_);
  Table_.NativeMaterial = Table_.Material;
  const bool textured =
      Held_.HoldsBuilt()
          ? Render::ResolveNativeTextures(Held_.Built(), Table_.Slots, error)
          : Render::ResolveNativeTextures(Held_.Assembled().Images(), Table_.Slots, error);
  if (!textured) { return false; }
  OverridesWorn_ = 0;
  OverridesWorn_ += WornByNativeSurfaceAndPart(Held_.Built(), 0);
  if (!RejectsUnwornOverrides(error)) { return false; }
  if (GroundSurface_ >= 0) {
    for (size_t slot = 0; slot < Table_.Slots.size(); ++slot) {
      if (Table_.Material[slot] == GroundSurface_) {
        Table_.Slots[slot].Domain = Render::SurfaceDomain::Ground;
      }
    }
  }
  ResolveMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - resolvedFrom)
          .count();
  return true;
}

bool Live::RestoresPieceResources(std::string &error) {
  return Renderer_->RestorePieces(error);
}

bool Live::RestoresGroundResources(std::string &error) {
  return Renderer_->RestoreGroundResources(error);
}

void Live::WearsPieces() {
  if (Renderer_ == nullptr) { return; }
  const size_t surfaces =
      Held_.HoldsBuilt() ? static_cast<size_t>(Held_.Built().surfaces()) : Shaped_.Surfaces.size();
  std::vector<uint32_t> slotOf(surfaces, Render::kNoSlot);
  for (size_t slot = 0; slot < Table_.NativeMaterial.size(); ++slot) {
    const int surface = Table_.NativeMaterial[slot];
    if (surface >= 0 && static_cast<size_t>(surface) < slotOf.size()) {
      slotOf[static_cast<size_t>(surface)] = static_cast<uint32_t>(slot);
    }
  }
  Renderer_->SetNativePieceSurfaces(slotOf);
}

void Live::StandsShadowRadius() {
  ShadowRadiusStoodM_ = Declared_.ShadowRadiusM;
  if (ShadowRadiusStoodM_ > 0.0 || Shaped_.TriangleCount() == 0) { return; }
  const auto boundedFrom = std::chrono::steady_clock::now();
  const Box bounded = Shaped_.BoundsOf(Joined_);
  BoundsMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - boundedFrom)
          .count();
  double across = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double span = (bounded.Max[axis] - bounded.Min[axis]) * Declared_.MetresPerUnit;
    across += span * span;
  }
  ShadowRadiusStoodM_ = 0.5 * std::sqrt(across);
}

PunctualLight Live::KeyLight() const {
  const Vec3f toSun = TowardTheKey();
  PunctualLight key;
  key.Kind = LightKind::Directional;
  key.Intensity = static_cast<float>(Declared_.KeyLux);
  if (Declared_.KeyFromClock) {
    const GroundLight reach = GroundAir_.Evaluate(DeclaredAir(), static_cast<double>(toSun[1]));
    key.Intensity = static_cast<float>(kSolarIlluminanceLx);
    for (int channel = 0; channel < 3; ++channel) {
      key.Colour[channel] = reach.SunTransmittance[channel];
    }
  }
  for (int axis = 0; axis < 3; ++axis) { key.Direction[axis] = -toSun[axis]; }
  return key;
}

Vec3f Live::TowardTheKey() const {
  const Vec3 direction = EastUpSouthDirection(Declared_.KeyBearingDeg * kDeg2Rad,
                                              Declared_.KeyElevationDeg * kDeg2Rad);
  Vec3f into;
  for (int axis = 0; axis < 3; ++axis) { into[axis] = static_cast<float>(direction[axis]); }
  return into;
}

void Live::StandsKeyLight() {
  if (Declared_.DrawsSky) { Renderer_->SetMedium(DeclaredAir()); }

  const Vec3f toSun = TowardTheKey();
  const Vec3f up = {{0.0f, 1.0f, 0.0f}};

  Renderer_->SetSky(
      toSun,
      up,
      static_cast<float>(Declared_.KeyFromClock ? kSolarIlluminanceLx : Declared_.KeyLux),
      0.0f);
  if (ShadowRadiusStoodM_ > 0.0) { Renderer_->SetShadowFrame(toSun, up, ShadowRadiusStoodM_); }
}

bool Live::StandsPlan(std::string &error) {
  Render::PlanSpec declaration;
  if (!DeclarePlan(Table_.Slots,
                   Declared_.DrawsSky,
                   ShadowRadiusStoodM_ > 0.0,
                   Renderer_ != nullptr && Renderer_->Presents(),
                   {.Stages = Declared_.Stages, .Outputs = Declared_.Outputs},
                   declaration,
                   error)) {
    return false;
  }
  if (!Declared_.Transfer.empty()) {
    const std::optional<Render::Transfer> meant =
        Render::Spells(Render::kTransfers, Declared_.Transfer);
    if (!meant) {
      error = "the declaration transfers the frame as '" + Declared_.Transfer +
              "', and this engine spells " + Render::Spellings(Render::kTransfers);
      return false;
    }
    declaration.Display = Render::Declared<Render::Transfer>(*meant);
  }
  if (!Declared_.Precision.empty()) {
    const std::optional<Render::ScenePrecision> meant =
        Render::Spells(Render::kPrecisions, Declared_.Precision);
    if (!meant) {
      error = "the declaration carries the scene at '" + Declared_.Precision +
              "' precision, and this engine spells " + Render::Spellings(Render::kPrecisions);
      return false;
    }
    declaration.Precision = Render::Declared<Render::ScenePrecision>(*meant);
  }
  if (Declared_.Exposure > 0.0) {
    declaration.Exposure = Render::Declared<float>(static_cast<float>(Declared_.Exposure));
  } else {
    const double metered = MeteredLux();
    if (metered > 0.0) {
      const double ev100 = std::log2(metered / kMeteredMiddleGrey);
      declaration.Exposure = Render::Declared<float>(
          static_cast<float>(1.0 / (kExposureCalibration * std::pow(2.0, ev100))));
    }
  }
  if (Plan_ != nullptr && !(PlanDeclared_ == declaration)) { Plan_ = nullptr; }
  if (Plan_ == nullptr) {
    auto made = Render::Compiled::Compile(declaration);
    if (!made) {
      error = std::move(made).error();
      return false;
    }
    Plan_ = *std::move(made);
    PlanDeclared_ = std::move(declaration);
    PlanInits_ += 1;
    const auto stood = Renderer_->Init(
        {.WidthPx = Declared_.SurfaceWidthPx, .HeightPx = Declared_.SurfaceHeightPx}, Plan_);
    if (!stood) {
      error = std::move(stood).error();
      return false;
    }
  }
  return true;
}

bool Live::Build(std::string &error) {
  if (!Held_.HoldsBuilt() && Declared_.Stands.empty()) { ClearsSubject(); }
  if (Held_.HoldsBuilt() && Declared_.Stands.empty() && !CarriesBuilt(error)) { return false; }
  if (!Declared_.Stands.empty() && !StandsSubjects(error)) { return false; }

  if (!Reshape(error)) { return false; }
  Joined_ = Shaped_.Parts.size();
  if (Carrying_ > 0) { Joined_ = Carrying_; }
  StandsShadowRadius();

  if (!StandsPlan(error)) { return false; }
  WearsPieces();
  if (DeclaresKeyLight()) { StandsKeyLight(); }

  if (auto bound = BindSubject(); !bound) {
    error = std::move(bound.error());
    return false;
  }
  if (!Renderer_->RestorePieceMaterials(error)) { return false; }
  const auto composedFrom = std::chrono::steady_clock::now();
  const bool composed = Compose(error);
  ComposeMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - composedFrom)
          .count();
  return composed;
}

std::expected<void, std::string> Live::BindSubject() {
  std::string error;
  if (Shaped_.TriangleCount() > 0) {
    Renderer_->SetPictureRegion({.X = Declared_.PictureLeftFrac,
                                 .Y = Declared_.PictureTopFrac,
                                 .Width = Declared_.PictureWidthFrac,
                                 .Height = Declared_.PictureHeightFrac});
    auto insideFrom = std::chrono::steady_clock::now();
    const auto sinceInside = [&insideFrom] {
      const double ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - insideFrom)
              .count();
      insideFrom = std::chrono::steady_clock::now();
      return ms;
    };
    const auto wholeFrom = std::chrono::steady_clock::now();

    Renderer_->CastsBelow(static_cast<uint32_t>(Joined_));
    if (!Stand(error)) { return std::unexpected(std::move(error)); }
    StandMs_ = sinceInside();
    if (!Render::Surface(*Renderer_, Stood_, Camera_.Prepared(), Scratch_, error)) {
      return std::unexpected(std::move(error));
    }
    SurfaceMs_ = sinceInside();
    if (!Submit(error)) { return std::unexpected(std::move(error)); }
    if (Camera_.IsUnbound()) { Camera_.MarkBound(); }
    SubmitMs_ = sinceInside();
    InsideMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wholeFrom)
            .count();
  } else {
    if (!Camera_.NeedsBinding()) { Camera_.Unbind(); }
    if (!Renderer_->SetSubjectMaterials(Table_.Slots, error)) {
      return std::unexpected(std::move(error));
    }
    Renderer_->SetPictureRegion({});
  }
  return {};
}

bool Live::Pose(double seconds, std::string &error) {
  if (!Held_.Poses(seconds, error)) { return false; }
  if (!Reshape(error)) { return false; }
  return true;
}

bool Live::Measure(double seconds, std::string &error) {
  if (!Held_.Measures(seconds, error)) { return false; }
  if (!Reshape(error)) { return false; }
  return true;
}

void Live::Eye(const Render::Viewpoint &from) noexcept {
  Camera_.Override(from);
}

void Live::CapturesRenderedPositions() {
  RenderedPositionsM_.clear();
  RenderedPositionsM_.reserve(Shaped_.VertexCount() * 3u);
  for (const Render::ShapePart &part : Shaped_.Parts) {
    RenderedPositionsM_.insert(
        RenderedPositionsM_.end(), part.PositionsM.begin(), part.PositionsM.end());
  }
}

void Live::CoverShapedParts() {
  for (size_t part = 0; part < PartBounds_.size() && part < Shaped_.Parts.size(); ++part) {
    const Render::ShapePart &one = Shaped_.Parts[part];
    Box &held = PartBounds_[part];
    for (size_t vertex = 0; vertex < one.VertexCount && (vertex + 1) * 3 <= one.PositionsM.size();
         ++vertex) {
      const float *const from = one.PositionsM.data() + vertex * 3;
      held.Cover(Vec3{{from[0], from[1], from[2]}});
    }
  }
}

bool Live::PartVolumes(std::string &error) {
  if (!PartBounds_.empty()) { return true; }
  const size_t parts = Shaped_.Parts.size();
  if (parts == 0) { return true; }
  PartBounds_.assign(parts, Box{});
  CoverShapedParts();
  for (int sample = 0; sample < Sweeps(); ++sample) {
    if (Seconds(sample) == Held_.AtS()) { continue; }
    if (!Measure(Seconds(sample), error)) { return false; }
    CoverShapedParts();
  }
  return Held_.Frames() <= 1 || Measure(Held_.AtS(), error);
}

bool Live::PlacedBounds(Extents &into, std::string &error) {
  if (!PartVolumes(error)) { return false; }
  const size_t framed = Joined_ > 0 && Joined_ < PartBounds_.size() ? Joined_ : PartBounds_.size();
  Box grown;
  for (size_t part = 0; part < framed; ++part) {
    grown.Cover(PartBounds_[part].Through(part < Stood_.Parts() ? Stood_.Placement(part) : Mat4{}));
  }
  const Vec3 leastM = grown.Empty() ? Vec3{} : grown.Min;
  const Vec3 mostM = grown.Empty() ? Vec3{} : grown.Max;
  for (int axis = 0; axis < 3; ++axis) {
    into.LeastM[axis] = leastM[axis];
    into.MostM[axis] = mostM[axis];
  }
  return true;
}

bool Live::Look(std::string &error) {
  if (Camera_.HasOverride()) {
    Camera_.Prepared().Eye = Camera_.Override();
    Camera_.Prepared().HasExplicitCamera = true;
    return Render::Aim(*Renderer_, Shaped_, Camera_.Prepared(), Stood_.Anchor(), error);
  }
  Extents placed;
  if (!PlacedBounds(placed, error)) { return false; }
  const Vec3 &least = placed.LeastM;
  const Vec3 &most = placed.MostM;
  const auto framing = Render::FrameBounds(
      {.Min = least, .Max = most},
      {.Fill = Framing(), .Aspect = Renderer_->PictureW() / Renderer_->PictureH()});
  if (!framing) {
    error = framing.error();
    return false;
  }
  Render::Viewpoint framed = *framing;
  const Vec3 centre = {
      {(least[0] + most[0]) * 0.5, (least[1] + most[1]) * 0.5, (least[2] + most[2]) * 0.5}};
  const double turn = Camera_.OrbitDegrees() * kDeg2Rad;
  const double cosine = std::cos(turn);
  const double sine = std::sin(turn);
  const auto spun = [cosine, sine](const Vec3 &from) {
    return Vec3{{from[0] * cosine + from[2] * sine, from[1], -from[0] * sine + from[2] * cosine}};
  };
  framed.EyeM = centre + spun(framed.EyeM - centre);
  framed.Forward = spun(framed.Forward);
  framed.Right = spun(framed.Right);
  framed.Up = spun(framed.Up);
  Camera_.Prepare(framed, false, Joined_);
  return Render::Aim(*Renderer_, Shaped_, Camera_.Prepared(), Stood_.Anchor(), error);
}

void Live::StandsEnvironment() {
  Render::SubjectEnvironment environment;
  for (int channel = 0; channel < 3; ++channel) {
    environment.RadianceLinear[channel] = static_cast<float>(Declared_.IndirectLight[channel]);
    environment.GroundLinear[channel] = environment.RadianceLinear[channel];
  }
  if (Declared_.DrawsSky && DeclaresKeyLight()) { LightsFromTheSky(environment); }
  for (int channel = 0; channel < 3; ++channel) {
    AmbientStood_[channel] = environment.RadianceLinear[channel];
    GroundStood_[channel] = environment.GroundLinear[channel];
  }
  Stood_.Around(environment);
}

void Live::ReadIrradiance(std::span<const float, Render::kIrradianceFloats> irradiance) {
  const auto &environment = Stood_.IndirectLight();
  const double scale = environment.SkyLux / std::numbers::pi;
  for (size_t channel = 0; channel < 3; ++channel) {
    const double sky = irradiance[channel] * scale;
    const double sun = irradiance[3 + channel] * std::max(environment.CosSunZenith, 0.0) * scale;
    AmbientStood_[channel] = environment.RadianceLinear[channel] + sky;
    GroundStood_[channel] =
        environment.GroundLinear[channel] + environment.GroundAlbedo[channel] * (sky + sun);
  }
}

void Live::LightsFromTheSky(Render::SubjectEnvironment &environment) const {
  environment.SkyLux = Declared_.KeyFromClock ? kSolarIlluminanceLx : Declared_.KeyLux;
  environment.CosSunZenith = std::sin(Declared_.KeyElevationDeg * kDeg2Rad);
  for (int channel = 0; channel < 3; ++channel) {
    environment.GroundAlbedo[channel] = GroundAlbedo_[channel];
  }
}

void Live::EmitsPerPart() {
  for (size_t part = 0; part < Table_.PartSlot.size(); ++part) {
    const uint32_t slot = Table_.PartSlot[part];
    if (slot >= Table_.Slots.size()) { continue; }
    const Material &row = Table_.Slots[slot].Row;
    const bool emits = row.Emission[0] > 0.0f || row.Emission[1] > 0.0f || row.Emission[2] > 0.0f;
    std::array<float, 3> radiance{};
    for (int channel = 0; channel < 3; ++channel) {
      float value = row.BaseColour[channel];
      if (!row.Unlit) {
        value = emits ? row.Emission[channel]
                      : value * static_cast<float>(Declared_.IndirectLight[channel]);
      }
      radiance[static_cast<size_t>(channel)] = value;
    }
    (void)Stood_.Emits(part, radiance);
  }
}

bool Live::Stand(std::string &error) {
  auto standFrom = std::chrono::steady_clock::now();
  const auto sinceStand = [&standFrom] {
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - standFrom)
            .count();
    standFrom = std::chrono::steady_clock::now();
    return ms;
  };
  Stood_ = Render::SubjectProxy{};
  const Vec3 anchorEcefM = {{kWgs84A, 0.0, 0.0}};
  if (!Reshape(error)) { return false; }
  ReshapeAgainMs_ = sinceStand();
  Stood_.Stands(Shaped_, anchorEcefM);
  ProxyStandsMs_ = sinceStand();
  const Mat4 unmoved;
  for (size_t part = 0; part < Stood_.Parts(); ++part) {
    if (!Stood_.Places(part, unmoved)) { return false; }
  }
  Camera_.Prepare(Camera_.HasOverride() ? Camera_.Override() : Render::Viewpoint{},
                  Camera_.HasOverride(),
                  Joined_);
  SubmittedPose_.Reset();
  if (Held_.Moves() && RenderedPositionsM_.size() == Shaped_.VertexCount() * 3u) {
    Stood_.Posed(RenderedPositionsM_);
  }
  PlacesMs_ = sinceStand();
  if (!Stood_.Wears(Table_.PartSlot, Table_.Slots, error)) { return false; }
  WearsMs_ = sinceStand();
  EmitsPerPart();

  LampsMs_ = sinceStand();
  for (const PunctualLight &placed : Shaped_.Lamps) { Stood_.Lit(placed); }
  if (DeclaresKeyLight()) { Stood_.Lit(KeyLight()); }
  LitMs_ = sinceStand();
  StandsEnvironment();
  MediumMs_ = sinceStand();

  Render::Viewpoint eye = Camera_.Prepared().Eye;
  const bool declared = Held_.Camera().has_value();
  if (declared) { eye = *Held_.Camera(); }
  Camera_.Prepared().Eye = eye;
  if (!Camera_.HasOverride() && (Declared_.Fill > 0.0 || !declared)) {
    const auto boundedFrom = std::chrono::steady_clock::now();
    Box bounded = Shaped_.BoundsOf(Joined_);
    BoundsMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - boundedFrom)
            .count();
    for (int sample = 1; sample < Sweeps(); ++sample) {
      if (!Measure(Seconds(sample), error)) { return false; }
      bounded.Cover(Shaped_.BoundsOf(Joined_));
    }
    if (Held_.Frames() > 1 && !Measure(0.0, error)) { return false; }
    const auto fitted = Render::FrameBounds(
        bounded, {.Fill = Framing(), .Aspect = Renderer_->PictureW() / Renderer_->PictureH()});
    if (!fitted) {
      error = fitted.error();
      return false;
    }
    eye = *fitted;
    Camera_.Prepared().Eye = eye;
  }
  FramingMs_ = sinceStand();
  return true;
}

bool Live::Submit(std::string &error) {
  if (!Stoodup_) {
    Stoodup_ = Render::Place(*Renderer_, Stood_, Camera_.Prepared(), Scratch_, error);
    return Stoodup_;
  }
  return Render::Move(*Renderer_, Stood_, Camera_.Prepared(), Scratch_, error);
}

const std::string &Live::ProgrammeOf(size_t surface) const {
  static const std::string kNone;
  return surface < Declared_.Surfaces.size() ? Declared_.Surfaces[surface].Programme : kNone;
}

bool Live::Redeclare(std::vector<Shows> surfaces, std::string &error) {
  if (!Compose(surfaces, error)) { return false; }
  Declared_.Surfaces = std::move(surfaces);
  return true;
}

void Live::SkyEye(double aboveGroundM) {
  if (Renderer_ == nullptr) { return; }

  constexpr double kSkyEyeStepM = 2.0;
  const double quantisedM =
      std::floor(std::fmax(0.0, aboveGroundM) / kSkyEyeStepM + 0.5) * kSkyEyeStepM;
  Renderer_->SetSkyEye(static_cast<float>(quantisedM));
}

bool Live::Carries(size_t bodies, std::string &error) {
  if (bodies == 0) {
    error = "a picture was asked to carry no bodies at all, and that is a different statement "
            "from carrying one that has not moved";
    return false;
  }
  const size_t stood = Stood_.Instances();
  if (!Stood_.Carries(bodies)) {
    error = "the subject proxy stands over nothing, so it cannot carry " + std::to_string(bodies) +
            " bodies";
    return false;
  }
  if (stood != bodies && Stoodup_) {
    Stoodup_ = false;
    if (!Submit(error)) { return false; }
  }
  if (SubmittedPose_.Bodies() != bodies) { SubmittedPose_.Resize(bodies); }
  return true;
}

bool Live::Carry(const Bearing &held, std::string &error) {
  return Carry(0, held, error);
}

Mat4 Live::InMetres(const Mat4 &placed) const {
  const double perUnit = Declared_.MetresPerUnit > 0.0 ? Declared_.MetresPerUnit : 1.0;
  Mat4 out = placed;
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 4; ++row) { out[column * 4 + row] *= perUnit; }
  }
  return out;
}

bool Live::Carry(size_t body, const Bearing &held, std::string &error) {
  const Mat4 bodyM = InMetres(held.WorldFromBodyM);
  if (Joined_ == 0) {
    error = "nothing joined this picture from a file, so there is no body to carry -- every part "
            "stands where the world put it";
    return false;
  }
  const size_t parts = Shaped_.Parts.size();
  if (Stood_.Parts() != parts) {
    error = "the subject proxy stands over " + std::to_string(Stood_.Parts()) +
            " parts and the geometry carries " + std::to_string(parts) +
            ", so nothing standing was held.AsBuilt from what is being carried";
    return false;
  }
  SubmittedPose_.EnsureOne();
  if (body >= SubmittedPose_.Bodies() || body >= Stood_.Instances()) {
    error = "a body numbered " + std::to_string(body) + " was carried into a picture standing " +
            std::to_string(Stood_.Instances()) +
            " deep -- a picture carries the bodies it was told to carry and no others";
    return false;
  }
  const size_t instances = Stood_.Instances();
  const size_t rows = parts * instances;
  const bool bodyMoved = SubmittedPose_.BodyChanged(body, bodyM);
  const bool builtMoved = SubmittedPose_.BuiltChanged(held.AsBuilt);
  if (!bodyMoved && !builtMoved) { return true; }

  const size_t joined = Joined_ < parts ? Joined_ : parts;
  if (bodyMoved) {
    for (size_t part = 0; part < joined; ++part) {
      if (!Stood_.Places(part, body, bodyM)) { return false; }
    }
  }
  if (builtMoved) {
    for (size_t part = joined; part < parts; ++part) {
      if (!Stood_.Places(part, body, held.AsBuilt)) { return false; }
    }
  }
  PartBounds_.clear();
  Renderer_->CastsBelow(static_cast<uint32_t>(Joined_));

  if ((bodyMoved && joined > 0) &&
      (!Render::Moved(*Renderer_,
                      {.Rows = rows, .Many = instances, .Which = body, .ToPart = joined},
                      bodyM,
                      error))) {
    return false;
  }

  if ((builtMoved && joined < parts) &&
      (!Render::Moved(
          *Renderer_,
          {.Rows = rows, .Many = instances, .Which = body, .FromPart = joined, .ToPart = parts},
          held.AsBuilt,
          error))) {
    return false;
  }

  SubmittedPose_.Commit(body, bodyM, held.AsBuilt);
  return true;
}

bool Live::SetGeometry(outshine::Geometry &&built, size_t carried, std::string &error) {
  if (Declared_.Surfacing.empty()) {
    error = Says::NoGeometrySurface;
    return false;
  }
  return SetGeometry(std::move(built), carried, Declared_.Surfacing.front(), error);
}

bool Live::SetGeometry(outshine::Geometry &&built,
                       size_t carried,
                       const Material &wearing,
                       std::string &error) {
  Camera_.Invalidate();
  const std::vector<Material> wore = std::move(Declared_.Surfacing);
  Declared_.Surfacing.assign(1u, wearing);
  Held_.Carries(std::move(built));
  Stoodup_ = false;
  Carrying_ = carried;
  const auto phaseAt = std::chrono::steady_clock::now();
  const bool stood = Build(error);
  BuildMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  Carrying_ = 0;
  Declared_.Surfacing = wore;
  return stood;
}

size_t Live::TookPosing_ = 0, Live::TookSubmitting_ = 0, Live::TookAiming_ = 0,
       Live::TookDrawing_ = 0;
size_t Live::AssetReads_ = 0;
size_t Live::PlanInits_ = 0;

bool Live::Advance(std::string &error) {
  static const Heap::Tag kAdvancingTag("live-advance");
  const Heap::Tagged advancing(kAdvancingTag);
  const auto took = [](const char *tag, size_t before) { return Heap::TakenUnder(tag) - before; };

  if (Held_.Moves() && Held_.DurationS() > 0.0) {
    Held_.Advances(Declared_.Fps > 0.0 ? 1.0 / Declared_.Fps : 0.0,
                   Declared_.Animation == Scenario::AssetAnimation::Loop);
    const size_t beforePose = Heap::TakenUnder("live-pose");
    {
      static const Heap::Tag kPosingTag("live-pose");
      const Heap::Tagged posing(kPosingTag);
      if (!Pose(Held_.AtS(), error)) { return false; }
    }
    TookPosing_ = took("live-pose", beforePose);
    const size_t beforeSubmit = Heap::TakenUnder("live-submit");
    {
      static const Heap::Tag kSubmittingTag("live-submit");
      const Heap::Tagged submitting(kSubmittingTag);
      if (!Submit(error)) { return false; }
    }
    TookSubmitting_ = took("live-submit", beforeSubmit);
  }

  const bool orbits = Declared_.OrbitDegPerFrame != 0.0 && Shaped_.TriangleCount() > 0;
  if (orbits) { Camera_.AdvanceOrbit(Declared_.OrbitDegPerFrame); }
  if (orbits || Camera_.NeedsBinding()) {
    const size_t beforeAim = Heap::TakenUnder("live-aim");
    {
      static const Heap::Tag kAimingTag("live-aim");
      const Heap::Tagged aiming(kAimingTag);
      if (!Look(error)) { return false; }
    }
    Camera_.MarkBound();
    TookAiming_ = took("live-aim", beforeAim);
  }
  return true;
}

bool Live::Draw(std::string &error) {
  if (Renderer_ == nullptr) {
    error = "no device stands, so there is nothing to draw with";
    return false;
  }
  if (Camera_.NeedsBinding()) {
    if (!Look(error)) { return false; }
    Camera_.MarkBound();
  }
  const size_t beforeDraw = Heap::TakenUnder("render-frame");
  {
    static const Heap::Tag kDrawingTag("render-frame");
    const Heap::Tagged drawing(kDrawingTag);
    auto rendered = Renderer_->RenderFrame();
    if (!rendered) {
      error = std::move(rendered.error());
      return false;
    }
    if (Held_.Moves()) {
      CapturesRenderedPositions();
      Stood_.Posed(RenderedPositionsM_);
    }
  }
  TookDrawing_ = Heap::TakenUnder("render-frame") - beforeDraw;
  return true;
}

}
