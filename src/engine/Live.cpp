#include <span>
#include <array>
#include <chrono>
#include "Units.h"
#include "math/Mat4.h"
#include "Live.h"

#include "Shaped.h"
#include "Surfaces.h"

#include <cstdint>
#include <limits>

#include <algorithm>

#include <memory>
#include <numbers>
#include <cmath>
#include <filesystem>

#include <cstdio>
#include <string>
#include <optional>
#include <utility>
#include <ratio>
#include <system_error>
#include <vector>

#include "Heap.h"
#include "Image.h"

#include "Framing.h"
#include "SubjectProxy.h"
#include "Wgs84.h"

namespace outshine::Core {
namespace {

bool DeclarePlan(const std::vector<Render::SubjectMaterial> &surfaces,
                 bool sky,
                 bool shadows,
                 bool presents,
                 const std::vector<std::string> &stages,
                 const std::vector<std::string> &outputs,
                 Render::PlanSpec &declaration,
                 std::string &error) {
  declaration.Outputs = {Render::Resource::FrameTex};
  if (presents) { declaration.Outputs.push_back(Render::Resource::Surface); }
  for (const std::string &named : outputs) {
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

  if (!stages.empty()) {

    declaration.Content.clear();
    for (const std::string &named : stages) {
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

} // namespace

Live::Live(Render::SceneRenderer &renderer, Declaration declaration, const Ui::Font *font)
    : Renderer_(&renderer), Declared_(std::move(declaration)) {
  Over_.Faces(font);
}

Live::~Live() {
  if (Renderer_ == nullptr) { return; }

  std::string ignored;
  (void)Renderer_->SetSubjectMesh(Render::SubjectMesh{}, ignored);
  (void)Renderer_->SetOverlay(nullptr, 0, ignored);
  Renderer_->SetPictureRegion(0, 0, 0, 0, 0);
}

bool Live::Open(Render::SceneRenderer &renderer,
                Declaration declaration,
                const Ui::Font *font,
                std::unique_ptr<Live> &out,
                std::string &error) {
  out.reset();
  std::unique_ptr<Live> live(new Live(renderer, std::move(declaration), font));
  if (!live->Build(error)) { return false; }
  out = std::move(live);
  return true;
}

double Live::Framing() const {
  return Declared_.Fill > 0.0 ? Declared_.Fill : Render::kFramingFill;
}

namespace {}

void Live::Reshape() {
  if (EverShaped_ && ShapedAt_ == Held_.Changed()) { return; }
  ShapedAt_ = Held_.Changed();
  EverShaped_ = true;
  const bool alsoStands = Held_.Stands() && !Held_.Assembled().Parts().empty();
  Shaped_ = Held_.HoldsBuilt()
                ? (alsoStands ? Gltf::Shaped(Held_.Assembled(), Held_.Built(), ShapeParts_)
                              : Gltf::Shaped(Held_.Built(), ShapeParts_))
                : Gltf::Shaped(Held_.Assembled(), ShapeParts_);
}

namespace {

constexpr double kSolarIlluminanceLx = 133000.0;

double Photopic(const Vec3f &triple) {
  return 0.2126 * static_cast<double>(triple[0]) + 0.7152 * static_cast<double>(triple[1]) +
         0.0722 * static_cast<double>(triple[2]);
}

} // namespace

void Live::SunThroughTheAir(double cosSun, Vec3f &sunReach, Vec3f &skylight) const {
  if (std::fabs(cosSun - AirStoodAt_) > 1.0e-6) {
    const Render::Medium medium;
    const float stoodAt = medium.BottomRadiusKm + Render::kMediumGroundLiftKm;
    const auto toSun = [&](float radiusKm, float cosZenith, Vec3f &out) {
      Render::MediumTransmittance(medium, radiusKm, cosZenith, Render::kTransmittanceSteps, out);
    };
    const auto secondOrder = [&](float radiusKm, float cosZenith, Vec3f &out) {
      Vec3f luminance;
      Vec3f transfer;
      const float unitU = cosZenith * 0.5f + 0.5f;
      const float unitV =
          (radiusKm - medium.BottomRadiusKm) / (medium.TopRadiusKm - medium.BottomRadiusKm);
      Render::MediumMultiScatterTexel(medium, unitU, unitV, toSun, luminance, transfer);
      for (int channel = 0; channel < 3; ++channel) {
        out[channel] = luminance[channel] / (1.0f - transfer[channel]);
      }
    };
    Render::MediumSkyIrradiance(
        medium, stoodAt, static_cast<float>(cosSun), toSun, secondOrder, SkylightStood_);
    toSun(stoodAt, static_cast<float>(cosSun), SunReachStood_);
    AirStoodAt_ = cosSun;
  }
  for (int channel = 0; channel < 3; ++channel) {
    sunReach[channel] = SunReachStood_[channel];
    skylight[channel] = SkylightStood_[channel];
  }
}

double Live::MeteredLux() const {
  if (!Declared_.KeyFromClock) { return Declared_.KeyLux; }
  const double cosSun = std::sin(Declared_.KeyElevationDeg * kDeg2Rad);
  Vec3f sunReach;
  Vec3f skylight;
  SunThroughTheAir(cosSun, sunReach, skylight);
  const double straightDown = cosSun > 0.0 ? cosSun : 0.0;
  return kSolarIlluminanceLx * (straightDown * Photopic(sunReach) + Photopic(skylight));
}

bool Live::Build(std::string &error) {
  if (Declared_.Built == nullptr && !Held_.HoldsBuilt() && Declared_.Stands.empty()) {
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
  if ((Declared_.Built != nullptr || Held_.HoldsBuilt()) && Declared_.Stands.empty()) {
    if (Declared_.Surfacing.empty()) {
      error = "the declaration carries a built subject and no surface -- a body without a "
              "material cannot be resolved, and an empty list is a refusal, not a "
              "dereference";
      return false;
    }
    const auto tookFrom = std::chrono::steady_clock::now();
    if (Declared_.Built != nullptr) { Held_.Carries(*Declared_.Built); }
    CarryMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tookFrom)
            .count();
    const auto reshapedFrom = std::chrono::steady_clock::now();
    Reshape();
    ReshapeMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - reshapedFrom)
            .count();
    const auto resolvedFrom = std::chrono::steady_clock::now();
    Render::ResolveDeclaredSurface(Shaped_, Declared_.Surfacing.front(), Table_);
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
  }
  if (!Declared_.Stands.empty()) {
    if (!Held_.Stands()) {
      if (!Held_.Reads(Declared_.Stands,
                       Declared_.Variant,
                       Declared_.Animation,
                       Declared_.Clip,
                       Declared_.Fps,
                       error)) {
        return false;
      }
      AssetReads_ += 1;
    }
    if (!Pose(0.0, error)) { return false; }
    for (const std::string &joining : Declared_.Joins) {
      Core::Posed arriving;
      if (!arriving.Reads(joining, "", Declared_.Animation, Declared_.Clip, Declared_.Fps, error)) {
        return false;
      }
      if (!arriving.Poses(0.0, error)) { return false; }
      AssetReads_ += 1;
      if (!Held_.Appends(arriving.Assembled())) {
        error = "the subject '" + joining + "' would not append onto the one before it";
        return false;
      }
    }
    Gltf::ResolveSurfaceTable(Held_.File(), Held_.Assembled(), true, true, Table_);
    if (!Gltf::ResolveFileSurface(Held_.File(),
                                  Held_.Assembled(),
                                  Render::ColourFrom::Row,
                                  Render::ColourCarrier::Texture,
                                  Table_,
                                  error)) {
      return false;
    }
    if (!Declared_.Overriding.empty()) {
      size_t took = 0;
      for (size_t slot = 0; slot < Table_.Slots.size(); ++slot) {
        const int index = Table_.Material[slot];
        if (index < 0 || static_cast<size_t>(index) >= Held_.File().Materials().size()) {
          continue;
        }
        const std::string &named = Held_.File().Materials()[static_cast<size_t>(index)].Name;
        for (const Scenario::SurfaceOverride &said : Declared_.Overriding) {
          if (said.Named != named) { continue; }
          if (!said.KeepsMaps) { Table_.Slots[slot] = Render::SubjectMaterial{}; }
          Table_.Slots[slot].Row = said.Row;
          ++took;
          break;
        }
      }
      {
        const std::vector<Gltf::Part> &standing = Held_.Assembled().Parts();
        const size_t many =
            standing.size() < Table_.PartSlot.size() ? standing.size() : Table_.PartSlot.size();
        std::vector<uint32_t> wearers(Table_.Slots.size(), 0u);
        for (const uint32_t worn : Table_.PartSlot) {
          if (worn < wearers.size()) { wearers[worn] += 1u; }
        }
        Table_.Slots.reserve(Table_.Slots.size() + many);
        Table_.Material.reserve(Table_.Material.size() + many);
        Table_.Decoded.reserve(Table_.Decoded.size() + many);
        for (size_t part = 0; part < many; ++part) {
          const uint32_t slot = Table_.PartSlot[part];
          if (slot >= Table_.Slots.size()) { continue; }
          for (const Scenario::SurfaceOverride &said : Declared_.Overriding) {
            const bool byNode = !said.Node.empty() && said.Node == standing[part].NodeName;
            const bool byPart = said.Part >= 0 && std::cmp_equal(said.Part, part);
            if (!byNode && !byPart) { continue; }
            Render::SubjectMaterial made =
                said.KeepsMaps ? Table_.Slots[slot] : Render::SubjectMaterial{};
            made.Row = said.Row;
            if (slot < wearers.size() && wearers[slot] == 1u) {
              Table_.Slots[slot] = made;
            } else {
              if (slot < wearers.size()) { wearers[slot] -= 1u; }
              const int carried = slot < Table_.Material.size() ? Table_.Material[slot] : -1;
              Table_.Slots.push_back(made);
              Table_.Material.push_back(carried);
              Table_.Decoded.emplace_back();
              Table_.PartSlot[part] = static_cast<uint32_t>(Table_.Slots.size() - 1u);
            }
            ++took;
            break;
          }
        }
      }
      if (took == 0) {
        error = "this declaration names " + std::to_string(Declared_.Overriding.size()) +
                " surface(s) of '" + Declared_.Stands +
                "' and the file carries neither those "
                "material names, those node names nor those part indices -- a surface declared "
                "onto nothing changes "
                "no pixel and says it did";
        return false;
      }
    }
    if (Declared_.Built != nullptr) {
      const auto base = static_cast<uint32_t>(Table_.Slots.size());
      for (const Material &declaredSurface : Declared_.Surfacing) {
        Render::SurfaceTable joining;
        Render::ShapeStore joiningParts;
        Render::ResolveDeclaredSurface(
            Gltf::Shaped(*Declared_.Built, joiningParts), declaredSurface, joining);
        if (joining.Slots.empty()) {
          error = "a declared surface for the built geometry resolved to no slot, so the parts "
                  "joining this picture would name a surface that is not there";
          return false;
        }
        Table_.Slots.push_back(joining.Slots.front());
      }
      const size_t before = Held_.Assembled().Parts().size();
      if (!Held_.Appends(*Declared_.Built)) {
        error = Held_.Assembled().Error();
        return false;
      }
      Table_.PartSlot.resize(Held_.Assembled().Parts().size(), base);
      for (size_t part = before; part < Held_.Assembled().Parts().size(); ++part) {
        const int wanted = Held_.Assembled().Parts()[part].Material;
        const uint32_t at = wanted > 0 && static_cast<size_t>(wanted) < Declared_.Surfacing.size()
                                ? static_cast<uint32_t>(wanted)
                                : 0u;
        Table_.PartSlot[part] = base + at;
      }
      Joined_ = Held_.Assembled().Parts().size() - Declared_.Built->Parts().size();
    }

    if (Declared_.Built == nullptr && Held_.HoldsBuilt()) {
      const auto base = static_cast<uint32_t>(Table_.Slots.size());
      const outshine::Geometry &also = Held_.Built();
      for (int surface = 0; surface < also.surfaces(); ++surface) {
        Render::SubjectMaterial made;
        made.Row = also.surfaceAt(MaterialInstance(surface));
        Table_.Slots.push_back(made);
        Table_.Material.push_back(-1);
        Table_.Decoded.emplace_back();
      }
      const size_t before = Table_.PartSlot.size();
      Table_.PartSlot.resize(before + static_cast<size_t>(also.parts()), base);
      for (int part = 0; part < also.parts(); ++part) {
        const int wears = also.materialOf(part).index();
        const uint32_t at =
            wears >= 0 && wears < also.surfaces() ? base + static_cast<uint32_t>(wears) : base;
        Table_.PartSlot[before + static_cast<size_t>(part)] = at;
      }
      Joined_ = before;
      Carrying_ = before;
    }
  }

  if (!Held_.HoldsBuilt()) { Reshape(); }
  if (Declared_.Built == nullptr) { Joined_ = Shaped_.Parts.size(); }
  if (Carrying_ > 0) { Joined_ = Carrying_; }
  ShadowRadiusStoodM_ = Declared_.ShadowRadiusM;
  if (!(ShadowRadiusStoodM_ > 0.0) && Shaped_.TriangleCount() > 0) {
    Vec3 least;
    Vec3 most;
    const auto boundedFrom = std::chrono::steady_clock::now();
    Shaped_.BoundsOf(Joined_, least, most);
    BoundsMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - boundedFrom)
            .count();
    double across = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double span = (most[axis] - least[axis]) * Declared_.MetresPerUnit;
      across += span * span;
    }
    ShadowRadiusStoodM_ = 0.5 * std::sqrt(across);
  }

  Render::PlanSpec declaration;
  if (!DeclarePlan(Table_.Slots,
                   Declared_.DrawsSky,
                   ShadowRadiusStoodM_ > 0.0,
                   Renderer_ != nullptr && Renderer_->Presents(),
                   Declared_.Stages,
                   Declared_.Outputs,
                   declaration,
                   error)) {
    return false;
  }
  if (Declared_.Transfer == "linear") {
    declaration.Display = Render::Declared<Render::Transfer>(Render::Transfer::Linear);
  } else if (Declared_.Transfer == "filmic") {
    declaration.Display = Render::Declared<Render::Transfer>(Render::Transfer::Filmic);
  } else if (!Declared_.Transfer.empty()) {
    error = "the declaration transfers the frame as '" + Declared_.Transfer +
            "', and this engine has two: linear and filmic";
    return false;
  }
  if (Declared_.Precision == "float") {
    declaration.Precision = Render::Declared<Render::ScenePrecision>(Render::ScenePrecision::Float);
  } else if (Declared_.Precision == "half") {
    declaration.Precision = Render::Declared<Render::ScenePrecision>(Render::ScenePrecision::Half);
  } else if (!Declared_.Precision.empty()) {
    error = "the declaration carries the scene at '" + Declared_.Precision +
            "' precision, and this engine has two: half and float";
    return false;
  }
  if (Declared_.Exposure > 0.0) {
    declaration.Exposure = Render::Declared<float>(static_cast<float>(Declared_.Exposure));
  } else {
    const double metered = MeteredLux();
    if (metered > 0.0) {
      const double ev100 = std::log2(metered / 2.5);
      declaration.Exposure =
          Render::Declared<float>(static_cast<float>(1.0 / (1.2 * std::pow(2.0, ev100))));
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
    PlanDeclared_ = declaration;
    PlanInits_ += 1;
    Renderer_->Init(Declared_.SurfaceWidthPx, Declared_.SurfaceHeightPx, Plan_);
    if (!Renderer_->DeviceUsable()) {
      error = Renderer_->WhyNot().empty()
                  ? std::string("the device did not come up, so this scenario cannot be stood up")
                  : Renderer_->WhyNot();
      return false;
    }
  }
  if (Declared_.KeyLux > 0.0 || Declared_.KeyFromClock) {
    if (Declared_.DrawsSky) { Renderer_->SetMedium(Render::Medium{}); }

    const double elevation = Declared_.KeyElevationDeg * kDeg2Rad;
    const double bearing = Declared_.KeyBearingDeg * kDeg2Rad;
    const Vec3f toSun = {{static_cast<float>(std::cos(elevation) * std::sin(bearing)),
                          static_cast<float>(std::sin(elevation)),
                          static_cast<float>(std::cos(elevation) * std::cos(bearing))}};
    const Vec3f up = {{0.0f, 1.0f, 0.0f}};

    Renderer_->SetSky(
        toSun,
        up,
        static_cast<float>(Declared_.KeyFromClock ? kSolarIlluminanceLx : Declared_.KeyLux),
        0.0f);
    if (ShadowRadiusStoodM_ > 0.0) { Renderer_->SetShadowFrame(toSun, up, ShadowRadiusStoodM_); }
  }

  const Vec3 eye = {{0.0, 0.0, 0.0}};
  const Vec3 forward = {{0.0, 0.0, -1.0}};
  const Vec3 right = {{1.0, 0.0, 0.0}};
  const Vec3 up = {{0.0, 1.0, 0.0}};
  Renderer_->SetCameraBasis(eye, forward, right, up);

  if (Shaped_.TriangleCount() > 0) {
    Renderer_->SetPictureRegion(Declared_.PictureLeftFrac,
                                Declared_.PictureTopFrac,
                                Declared_.PictureWidthFrac,
                                Declared_.PictureHeightFrac,
                                0.0);
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
    if (!Stand(error)) { return false; }
    StandMs_ = sinceInside();
    if (!Render::Surface(*Renderer_, Stood_, Looking_, Scratch_, error)) { return false; }
    SurfaceMs_ = sinceInside();
    if (!Submit(error)) { return false; }
    SubmitMs_ = sinceInside();
    InsideMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wholeFrom)
            .count();
  } else {
    Renderer_->SetPictureRegion(0, 0, 0, 0, 0);
  }
  const auto composedFrom = std::chrono::steady_clock::now();
  const bool composed = Compose(error);
  ComposeMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - composedFrom)
          .count();
  return composed;
}

bool Live::Pose(double seconds, std::string &error) {
  if (!Held_.Poses(seconds, error)) { return false; }
  Reshape();
  return true;
}

bool Live::Measure(double seconds, std::string &error) {
  if (!Held_.Measures(seconds, error)) { return false; }
  Reshape();
  return true;
}

void Live::Eye(const Render::Viewpoint &from) {
  Eye_ = from;
  HaveEye_ = true;
  Aimed_ = false;
}

bool Live::PartVolumes(std::string &error) {
  if (!PartBounds_.empty()) { return true; }
  const size_t parts = Shaped_.Parts.size();
  if (parts == 0) { return true; }
  PartBounds_.assign(parts, Volume{});
  const auto fold = [this, parts] {
    for (size_t part = 0; part < parts; ++part) {
      const Render::ShapePart &one = Shaped_.Parts[part];
      Volume &held = PartBounds_[part];
      for (size_t vertex = 0; vertex < one.VertexCount && (vertex + 1) * 3 <= one.PositionsM.size();
           ++vertex) {
        const float *const from = one.PositionsM.data() + vertex * 3;
        for (int axis = 0; axis < 3; ++axis) {
          if (held.Empty || from[axis] < held.LeastM[axis]) { held.LeastM[axis] = from[axis]; }
          if (held.Empty || from[axis] > held.MostM[axis]) { held.MostM[axis] = from[axis]; }
        }
        held.Empty = false;
      }
    }
  };
  fold();
  for (int sample = 0; sample < Sweeps(); ++sample) {
    if (Seconds(sample) == Held_.AtS()) { continue; }
    if (!Measure(Seconds(sample), error)) { return false; }
    fold();
  }
  if (Held_.Frames() > 1 && !Measure(Held_.AtS(), error)) { return false; }
  return true;
}

bool Live::PlacedBounds(Vec3 &least, Vec3 &most, std::string &error) {
  if (!PartVolumes(error)) { return false; }
  const size_t framed = Joined_ > 0 && Joined_ < PartBounds_.size() ? Joined_ : PartBounds_.size();
  bool first = true;
  std::array<double, 3> leastM = {0.0, 0.0, 0.0};
  std::array<double, 3> mostM = {0.0, 0.0, 0.0};
  const Mat4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (size_t part = 0; part < framed; ++part) {
    const Volume &held = PartBounds_[part];
    if (held.Empty) { continue; }
    const Mat4 &placed = part < Stood_.Parts() ? Stood_.Placement(part) : identity;
    for (uint32_t corner = 0; corner < 8u; ++corner) {
      std::array<double, 3> from = {((corner & 1u) != 0) ? held.MostM[0] : held.LeastM[0],
                                    ((corner & 2u) != 0) ? held.MostM[1] : held.LeastM[1],
                                    ((corner & 4u) != 0) ? held.MostM[2] : held.LeastM[2]};
      for (int axis = 0; axis < 3; ++axis) {
        const double out = placed[axis] * from[0] + placed[4 + axis] * from[1] +
                           placed[8 + axis] * from[2] + placed[12 + axis];
        if (first || out < leastM[axis]) { leastM[axis] = out; }
        if (first || out > mostM[axis]) { mostM[axis] = out; }
      }
      first = false;
    }
  }
  for (int axis = 0; axis < 3; ++axis) {
    least[axis] = leastM[axis];
    most[axis] = mostM[axis];
  }
  return true;
}

bool Live::Look(std::string &error) {
  Render::Viewpoint framed;
  if (HaveEye_) {
    Looking_.Eye = Eye_;
    Looking_.StandsInside = true;
    Render::ShapeStore aiming;
    return Render::Aim(
        *Renderer_, Gltf::Shaped(Held_.Assembled(), aiming), Looking_, Stood_.Anchor(), error);
  }
  Vec3 least;
  Vec3 most;
  if (!PlacedBounds(least, most, error)) { return false; }
  Gltf::Viewpoint fromFile;
  if (!Gltf::FramingFor(least, most, fromFile, Framing())) {
    error = "the subject has no extent, so no camera can be derived from it";
    return false;
  }
  framed = fromFile;
  const Vec3 centre = {
      {(least[0] + most[0]) * 0.5, (least[1] + most[1]) * 0.5, (least[2] + most[2]) * 0.5}};
  const double turn = Around_ * kDeg2Rad;
  const double cosine = std::cos(turn);
  const double sine = std::sin(turn);
  const auto spun = [cosine, sine](const Vec3 &from) {
    return Vec3{{from[0] * cosine + from[2] * sine, from[1], -from[0] * sine + from[2] * cosine}};
  };
  framed.EyeM = centre + spun(framed.EyeM - centre);
  framed.Forward = spun(framed.Forward);
  framed.Right = spun(framed.Right);
  framed.Up = spun(framed.Up);
  Looking_ = {.Eye = framed, .StandsInside = false, .FramedParts = Joined_};
  Render::ShapeStore aiming;
  return Render::Aim(
      *Renderer_, Gltf::Shaped(Held_.Assembled(), aiming), Looking_, Stood_.Anchor(), error);
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
  const Vec3 anchorEcefM = {{Data::kWgs84A, 0.0, 0.0}};
  Reshape();
  ReshapeAgainMs_ = sinceStand();
  Stood_.Stands(Shaped_, anchorEcefM);
  ProxyStandsMs_ = sinceStand();
  Mat4 standing;
  for (size_t part = 0; part < Stood_.Parts(); ++part) {
    if (!Stood_.Places(part, standing)) { return false; }
  }
  Looking_ = {.Eye = HaveEye_ ? Eye_ : Render::Viewpoint{},
              .StandsInside = HaveEye_,
              .FramedParts = Joined_};
  for (Mat4 &one : SentBody_) { one.Column.fill(std::numeric_limits<double>::quiet_NaN()); }
  SentBuilt_.Column.fill(std::numeric_limits<double>::quiet_NaN());
  if (Held_.Moves()) { Stood_.Posed(&Held_.Previous()); }
  PlacesMs_ = sinceStand();
  if (!Stood_.Wears(Table_.PartSlot, Table_.Slots, error)) { return false; }
  WearsMs_ = sinceStand();
  for (size_t part = 0; part < Table_.PartSlot.size(); ++part) {
    const uint32_t slot = Table_.PartSlot[part];
    if (slot >= Table_.Slots.size()) { continue; }
    const Material &row = Table_.Slots[slot].Row;
    const bool emits = row.Emission[0] > 0.0f || row.Emission[1] > 0.0f || row.Emission[2] > 0.0f;
    std::array<float, 3> radiance{};
    for (int channel = 0; channel < 3; ++channel) {
      radiance[static_cast<size_t>(channel)] =
          emits ? row.Emission[channel]
                : row.BaseColour[channel] * static_cast<float>(Declared_.IndirectLight[channel]);
    }
    (void)Stood_.Emits(part, radiance);
  }

  LampsMs_ = sinceStand();
  for (const PunctualLight &placed : Shaped_.Lamps) { Stood_.Lit(placed); }
  if (Declared_.KeyLux > 0.0 || Declared_.KeyFromClock) {
    const double elevation = Declared_.KeyElevationDeg * kDeg2Rad;
    const double bearing = Declared_.KeyBearingDeg * kDeg2Rad;
    PunctualLight key;
    key.Kind = LightKind::Directional;
    key.Intensity = static_cast<float>(Declared_.KeyLux);
    if (Declared_.KeyFromClock) {
      Vec3f sunReach;
      Vec3f skylight;
      SunThroughTheAir(std::sin(elevation), sunReach, skylight);
      key.Intensity = static_cast<float>(kSolarIlluminanceLx);
      for (int channel = 0; channel < 3; ++channel) { key.Colour[channel] = sunReach[channel]; }
    }
    key.Direction[0] = static_cast<float>(-std::cos(elevation) * std::sin(bearing));
    key.Direction[1] = static_cast<float>(-std::sin(elevation));
    key.Direction[2] = static_cast<float>(-std::cos(elevation) * std::cos(bearing));
    Stood_.Lit(key);
  }
  Render::SubjectEnvironment environment;
  for (int channel = 0; channel < 3; ++channel) {
    environment.RadianceLinear[channel] = static_cast<float>(Declared_.IndirectLight[channel]);
  }
  LitMs_ = sinceStand();
  if (Declared_.DrawsSky && (Declared_.KeyLux > 0.0 || Declared_.KeyFromClock)) {
    const double cosSun = std::sin(Declared_.KeyElevationDeg * kDeg2Rad);
    const double aboveTheAir = Declared_.KeyFromClock ? kSolarIlluminanceLx : Declared_.KeyLux;
    Vec3f sunReach;
    Vec3f skylight;
    SunThroughTheAir(cosSun, sunReach, skylight);
    const double straightDown = cosSun > 0.0 ? cosSun : 0.0;
    for (int channel = 0; channel < 3; ++channel) {
      environment.RadianceLinear[channel] +=
          static_cast<double>(skylight[channel] / std::numbers::pi_v<float>) * aboveTheAir;
      const float onTheGround =
          static_cast<float>(aboveTheAir) *
          static_cast<float>(straightDown * sunReach[channel] + skylight[channel]);
      environment.GroundLinear[channel] +=
          GroundAlbedo_[channel] * static_cast<double>(onTheGround / std::numbers::pi_v<float>);
    }
  }
  for (int channel = 0; channel < 3; ++channel) {
    AmbientStood_[channel] = environment.RadianceLinear[channel];
    GroundStood_[channel] = environment.GroundLinear[channel];
  }
  Stood_.Around(environment);
  MediumMs_ = sinceStand();

  std::string why;
  Render::Viewpoint eye = Looking_.Eye;
  Gltf::Viewpoint placed;
  const bool declared =
      !Held_.File().Cameras().empty() && Gltf::DeclaredPlacement(Held_.File(), 0, placed, why);
  if (declared) { eye = placed; }
  Looking_.Eye = eye;
  if (!HaveEye_ && (Declared_.Fill > 0.0 || !declared)) {
    Vec3 least;
    Vec3 most;
    const auto boundedFrom = std::chrono::steady_clock::now();
    Shaped_.BoundsOf(Joined_, least, most);
    BoundsMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - boundedFrom)
            .count();
    for (int sample = 1; sample < Sweeps(); ++sample) {
      if (!Measure(Seconds(sample), error)) { return false; }
      Vec3 posedLeast;
      Vec3 posedMost;
      Shaped_.BoundsOf(Joined_, posedLeast, posedMost);
      for (int axis = 0; axis < 3; ++axis) {
        least[axis] = posedLeast[axis] < least[axis] ? posedLeast[axis] : least[axis];
        most[axis] = posedMost[axis] > most[axis] ? posedMost[axis] : most[axis];
      }
    }
    if (Held_.Frames() > 1 && !Measure(0.0, error)) { return false; }
    Gltf::Viewpoint fitted;
    if (!Gltf::FramingFor(least, most, fitted, Framing())) {
      error = "the subject has no extent over its own grid, so no camera can be derived from it";
      return false;
    }
    eye = fitted;
    Looking_.Eye = eye;
  }
  return true;
  FramingMs_ = sinceStand();
}

bool Live::Submit(std::string &error) {
  if (!Stoodup_) {
    Stoodup_ = Render::Place(*Renderer_, Stood_, Looking_, Scratch_, error);
    return Stoodup_;
  }
  return Render::Move(*Renderer_, Stood_, Looking_, Scratch_, error);
}

const std::string &Live::ProgrammeOf(size_t surface) const {
  static const std::string kNone;
  return surface < Declared_.Surfaces.size() ? Declared_.Surfaces[surface].Programme : kNone;
}

bool Live::Redeclare(std::vector<Shows> surfaces, std::string &error) {
  Declared_.Surfaces = std::move(surfaces);
  return Compose(error);
}

void Live::SkyEye(double aboveGroundM) {
  if (Renderer_ == nullptr) { return; }

  constexpr double kSkyEyeStepM = 2.0;
  const double quantisedM =
      std::floor(std::fmax(0.0, aboveGroundM) / kSkyEyeStepM + 0.5) * kSkyEyeStepM;
  Renderer_->SetSkyEye(static_cast<float>(quantisedM));
}

bool Live::ReadPixels(std::vector<uint8_t> &rgba, std::string &error) {
  if (Renderer_ == nullptr || !Renderer_->Drew()) {
    error = "nothing has been drawn yet, so there is no frame to read";
    return false;
  }
  if (Renderer_->ReadPixels(rgba) != Render::ReadState::Ready) {
    error = "the frame did not come back from the device";
    return false;
  }
  return true;
}

bool Live::ReadBuffer(outshine::Buffer which, std::vector<float> &out, std::string &error) {
  if (Renderer_ == nullptr || !Renderer_->Drew()) {
    error = "nothing has been drawn yet, so there is no frame to read";
    return false;
  }
  Render::ReadState state = Render::ReadState::Failed;
  switch (which) {
    case outshine::Buffer::Colour:
      error = "the displayed picture is read as bytes, not as scene-referred float";
      return false;
    case outshine::Buffer::Linear: state = Renderer_->ReadSceneLinear(out); break;
    case outshine::Buffer::Depth: state = Renderer_->ReadDepth(out); break;
    case outshine::Buffer::ShadingNormal: state = Renderer_->ReadShadingNormal(out); break;
    case outshine::Buffer::SurfaceIdentity: state = Renderer_->ReadSurfaceIdentity(out); break;
    case outshine::Buffer::Velocity:
      if (!Renderer_->Plan().Holds(Render::Resource::SceneVelocity)) {
        error = "this plan carries no velocity, so no frame of it has one to read";
        return false;
      }
      state = Renderer_->ReadSceneVelocity(out);
      break;
  }
  if (state != Render::ReadState::Ready) {
    error = "the frame did not come back from the device";
    return false;
  }
  return true;
}

bool Live::Present(std::string &error) {
  if (Renderer_ == nullptr) {
    error = "a frame was ended on an engine that carries no device";
    return false;
  }
  if (!Renderer_->Presents()) { return true; }
  return Draw(error);
}

bool Live::Settle(std::string &error) {
  if (Renderer_ == nullptr) {
    error = "there is no device to wait for";
    return false;
  }
  Renderer_->Settle();
  return true;
}

bool Live::Screenshot(const std::string &path, std::string &error) {
  if (Renderer_ == nullptr || !Renderer_->Drew()) {
    error = "nothing has been drawn yet, so there is no frame to write";
    return false;
  }
  std::vector<uint8_t> rgba;
  if (Renderer_->ReadPixels(rgba) != Render::ReadState::Ready) {
    error = "the frame did not come back from the device";
    return false;
  }
  const size_t want = static_cast<size_t>(Declared_.SurfaceWidthPx) *
                      static_cast<size_t>(Declared_.SurfaceHeightPx) * 4u;
  if (rgba.size() != want) {
    error = "the frame read back " + std::to_string(rgba.size()) + " bytes and " +
            std::to_string(Declared_.SurfaceWidthPx) + " by " +
            std::to_string(Declared_.SurfaceHeightPx) + " rgba is " + std::to_string(want);
    return false;
  }
  std::vector<uint8_t> png;
  if (!EncodePng(rgba.data(), Declared_.SurfaceWidthPx, Declared_.SurfaceHeightPx, png)) {
    error = "the frame did not encode as a png";
    return false;
  }
  const std::filesystem::path named(path);
  if (named.has_parent_path()) {
    std::error_code why;
    std::filesystem::create_directories(named.parent_path(), why);
  }
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) {
    error = "the screenshot could not be opened for writing at " + path;
    return false;
  }
  const size_t wrote = std::fwrite(png.data(), 1, png.size(), file);
  std::fclose(file);
  if (wrote != png.size()) {
    error = "the screenshot wrote " + std::to_string(wrote) + " of " + std::to_string(png.size()) +
            " bytes to " + path;
    return false;
  }
  return true;
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
  if (SentBody_.size() != bodies) {
    Mat4 unsent{};
    unsent.Column.fill(std::numeric_limits<double>::quiet_NaN());
    SentBody_.resize(bodies, unsent);
  }
  return true;
}

bool Live::Carry(const Mat4 &worldFromBodyM, const Mat4 &built, std::string &error) {
  return Carry(0, worldFromBodyM, built, error);
}

bool Live::Carry(size_t body, const Mat4 &worldFromBodyM, const Mat4 &built, std::string &error) {
  const double perUnit = Declared_.MetresPerUnit > 0.0 ? Declared_.MetresPerUnit : 1.0;
  Mat4 bodyM{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      bodyM[column * 4 + row] = column < 3 ? worldFromBodyM[column * 4 + row] * perUnit
                                           : worldFromBodyM[column * 4 + row];
    }
  }
  if (Joined_ == 0) {
    error = "nothing joined this picture from a file, so there is no body to carry -- every part "
            "stands where the world put it";
    return false;
  }
  const size_t parts = Shaped_.Parts.size();
  if (Stood_.Parts() != parts) {
    error = "the subject proxy stands over " + std::to_string(Stood_.Parts()) +
            " parts and the geometry carries " + std::to_string(parts) +
            ", so nothing standing was built from what is being carried";
    return false;
  }
  if (SentBody_.empty()) {
    Mat4 unsent{};
    unsent.Column.fill(std::numeric_limits<double>::quiet_NaN());
    SentBody_.resize(1, unsent);
  }
  if (body >= SentBody_.size() || body >= Stood_.Instances()) {
    error = "a body numbered " + std::to_string(body) + " was carried into a picture standing " +
            std::to_string(Stood_.Instances()) +
            " deep -- a picture carries the bodies it was told to carry and no others";
    return false;
  }
  const size_t instances = Stood_.Instances();
  const size_t rows = parts * instances;
  bool bodyMoved = false;
  bool builtMoved = false;
  for (int at = 0; at < 16 && !bodyMoved; ++at) { bodyMoved = SentBody_[body][at] != bodyM[at]; }
  for (int at = 0; at < 16 && !builtMoved; ++at) { builtMoved = SentBuilt_[at] != built[at]; }
  if (!bodyMoved && !builtMoved) { return true; }

  const size_t joined = Joined_ < parts ? Joined_ : parts;
  if (bodyMoved) {
    for (size_t part = 0; part < joined; ++part) {
      if (!Stood_.Places(part, body, bodyM)) { return false; }
    }
  }
  if (builtMoved) {
    for (size_t part = joined; part < parts; ++part) {
      if (!Stood_.Places(part, body, built)) { return false; }
    }
  }
  PartBounds_.clear();
  Renderer_->CastsBelow(static_cast<uint32_t>(Joined_));

  if (bodyMoved && joined > 0) {
    if (!Render::MovedInstance(*Renderer_, rows, instances, body, 0, joined, bodyM, error)) {
      return false;
    }
  }
  if (builtMoved && joined < parts) {
    if (!Render::MovedInstance(*Renderer_, rows, instances, body, joined, parts, built, error)) {
      return false;
    }
  }
  SentBody_[body] = bodyM;
  SentBuilt_ = built;
  return true;
}

bool Live::Restands(std::string stands,
                    std::string variant,
                    Scenario::AssetAnimation animation,
                    int clip,
                    std::string &error) {
  Declared_.Stands = std::move(stands);
  Declared_.Variant = std::move(variant);
  Declared_.Animation = animation;
  Declared_.Clip = clip;
  Declared_.Built = nullptr;
  Stoodup_ = false;
  Held_.Clears();
  return Build(error);
}

bool Live::Restand(const Gltf::Subject &built, size_t carried, std::string &error) {
  return Restand(built, carried, Declared_.Surfacing.front(), error);
}

bool Live::Restand(outshine::Geometry &&built,
                   size_t carried,
                   const Material &wearing,
                   std::string &error) {
  Aimed_ = false;
  const std::vector<Material> wore = std::move(Declared_.Surfacing);
  Declared_.Surfacing.assign(1u, wearing);
  Declared_.Built = nullptr;
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

bool Live::Restand(const Gltf::Subject &built,
                   size_t carried,
                   const Material &wearing,
                   std::string &error) {
  Aimed_ = false;
  const std::vector<Material> wore = std::move(Declared_.Surfacing);
  Declared_.Surfacing.assign(1u, wearing);
  Declared_.Built = &built;
  Stoodup_ = false;
  Carrying_ = carried;
  auto phaseAt = std::chrono::steady_clock::now();
  const auto since = [&phaseAt] {
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
            .count();
    phaseAt = std::chrono::steady_clock::now();
    return ms;
  };
  const bool stood = Build(error);
  BuildMs_ = since();
  Carrying_ = 0;
  Declared_.Surfacing = wore;
  if (!stood) { return false; }
  Joined_ = carried;
  return true;
}

size_t Live::TookPosing_ = 0, Live::TookSubmitting_ = 0, Live::TookAiming_ = 0,
       Live::TookDrawing_ = 0;
size_t Live::AssetReads_ = 0;
size_t Live::PlanInits_ = 0;

bool Live::Advance(std::string &error) {
  const Heap::Tagged advancing("live-advance");
  const auto took = [](const char *tag, size_t before) { return Heap::TakenUnder(tag) - before; };

  if (Held_.Moves() && Held_.DurationS() > 0.0) {
    Held_.Advances(Declared_.Fps > 0.0 ? 1.0 / Declared_.Fps : 0.0,
                   Declared_.Animation == Scenario::AssetAnimation::Loop);
    const size_t beforePose = Heap::TakenUnder("live-pose");
    {
      const Heap::Tagged posing("live-pose");
      if (!Pose(Held_.AtS(), error)) { return false; }
    }
    TookPosing_ = took("live-pose", beforePose);
    const size_t beforeSubmit = Heap::TakenUnder("live-submit");
    {
      const Heap::Tagged submitting("live-submit");
      if (!Submit(error)) { return false; }
    }
    TookSubmitting_ = took("live-submit", beforeSubmit);
  }

  const bool orbits = Declared_.OrbitDegPerFrame != 0.0 && Shaped_.TriangleCount() > 0;
  if (orbits) { Around_ += Declared_.OrbitDegPerFrame; }
  if (orbits || !Aimed_) {
    const size_t beforeAim = Heap::TakenUnder("live-aim");
    {
      const Heap::Tagged aiming("live-aim");
      if (!Look(error)) { return false; }
    }
    Aimed_ = true;
    TookAiming_ = took("live-aim", beforeAim);
  }
  return true;
}

bool Live::Draw(std::string &error) {
  if (Renderer_ == nullptr) {
    error = "no device stands, so there is nothing to draw with";
    return false;
  }
  if (!Aimed_) {
    if (!Look(error)) { return false; }
    Aimed_ = true;
  }
  const size_t beforeDraw = Heap::TakenUnder("render-frame");
  {
    const Heap::Tagged drawing("render-frame");
    Renderer_->RenderFrame();
  }
  TookDrawing_ = Heap::TakenUnder("render-frame") - beforeDraw;
  return true;
}

} // namespace outshine::Core
