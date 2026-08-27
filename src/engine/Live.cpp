#include "Live.h"

#include <limits>

#include <algorithm>

#include <numbers>
#include <cmath>
#include <filesystem>

#include <cstdio>
#include <vector>

#include "Heap.h"
#include "Image.h"

#include "Framing.h"
#include "SubjectProxy.h"
#include "Wgs84.h"

namespace outshine::Core {
namespace {


void DeclarePlan(const Gltf::Document &file, bool moves, bool sky, bool shadows,
                 Render::PlanSpec &declaration) {
  declaration.Outputs = {Render::Resource::FrameTex, Render::Resource::Surface};

  if (moves) { declaration.Outputs.push_back(Render::Resource::SceneVelocity); }
  declaration.Content = {Render::Stage::Subjects, Render::Stage::Overlay};
  if (sky) { declaration.Content.push_back(Render::Stage::Sky); }
  if (shadows) { declaration.Content.push_back(Render::Stage::LightVisibility); }
  bool carriesGlass = false;
  for (const Gltf::MaterialRef &material : file.Materials()) {
    const SurfaceKind kind = StateOf(material.Surface).Kind();
    carriesGlass = carriesGlass || kind == SurfaceKind::ThinTransmissive ||
                   kind == SurfaceKind::Refractive;
  }
  if (carriesGlass) {
    declaration.Content.push_back(Render::Stage::SubjectsTransmissive);
    declaration.Content.push_back(Render::Stage::CompositeTransmission);
  }

  declaration.Display = Render::Declared<Render::Transfer>(Render::Transfer::Filmic);
  declaration.Exposure = Render::Declared<float>(1.0f);
}

}

Live::Live(Render::Renderer &renderer, Declaration declaration, const Ui::Font *font)
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

bool Live::Open(Render::Renderer &renderer, Declaration declaration, const Ui::Font *font,
                std::unique_ptr<Live> &out, std::string &error) {

  out.reset();
  std::unique_ptr<Live> live(new Live(renderer, std::move(declaration), font));
  if (!live->Build(error)) { return false; }
  out = std::move(live);
  return true;
}

double Live::Framing() const {
  return Declared_.Fill > 0.0 ? Declared_.Fill : Gltf::kFramingFill;
}

bool Live::Build(std::string &error) {
  if (Declared_.Built == nullptr && Declared_.Stands.empty()) {
    Geometry_ = Gltf::Subject();
    File_ = Gltf::Document();
    Table_ = SurfaceTable();
    ShadowRadiusStoodM_ = 0.0;
    Joined_ = 0;
    Carrying_ = 0;
    FileStands_ = false;
    Stoodup_ = false;
    Moves_ = false;
    Frames_ = 1;
    At_ = 0;
    PartBounds_.clear();
    if (Renderer_ != nullptr) {
      std::string ignored;
      (void)Renderer_->SetSubjectMesh(Render::SubjectMesh{}, ignored);
      (void)Renderer_->SetSubjectPlacements(nullptr, 0, ignored);
    }
  }
  if (Declared_.Built != nullptr && Declared_.Stands.empty()) {
    if (Declared_.Surfacing.empty()) {
      error = "the declaration carries a built subject and no surface -- a body without a "
              "material cannot be resolved, and an empty list is a refusal, not a "
              "dereference";
      return false;
    }
    Geometry_ = *Declared_.Built;
    ResolveDeclaredSurface(Geometry_, Declared_.Surfacing.front(), Table_);
  }
  if (!Declared_.Stands.empty()) {
    if (!FileStands_) {
    if (!Declared_.Variant.empty()) { Variant_ = Gltf::VariantSelection(Declared_.Variant); }
    if (!File_.ReadFile(Declared_.Stands)) {
      error = File_.Error();
      return false;
    }
    AssetReads_ += 1;

    if (!File_.Animations().empty() && Declared_.Animation == AssetAnimation::Play) {
      std::vector<int> all((size_t)File_.Animations().size());
      for (size_t at = 0; at < all.size(); ++at) { all[at] = (int)at; }
      if (!Gltf::Pose::Build(File_, Span<const int>(all.data(), all.size()), Motion_, error)) {
        return false;
      }
      Moves_ = Motion_.EndS() > 0.0;

      Frames_ = Moves_ ? (int)(Motion_.EndS() * Declared_.Fps + 0.5) : 1;
      if (Frames_ < 1) { Frames_ = 1; }
    }
    FileStands_ = true;
    }
    if (!Pose(0, error)) { return false; }
    ResolveSurfaceTable(File_, Geometry_, true, true, Table_);
    if (!ResolveFileSurface(File_, Geometry_, ColourFrom::Row, ColourCarrier::Texture, Table_,
                            error)) {
      return false;
    }
    if (Declared_.Built != nullptr) {
      const uint32_t base = (uint32_t)Table_.Slots.size();
      for (const Material &declaredSurface : Declared_.Surfacing) {
        SurfaceTable joining;
        ResolveDeclaredSurface(*Declared_.Built, declaredSurface, joining);
        if (joining.Slots.empty()) {
          error = "a declared surface for the built geometry resolved to no slot, so the parts "
                  "joining this picture would name a surface that is not there";
          return false;
        }
        Table_.Slots.push_back(joining.Slots.front());
      }
      const size_t before = Geometry_.Parts().size();
      if (!Geometry_.Append(*Declared_.Built)) {
        error = Geometry_.Error();
        return false;
      }
      Table_.PartSlot.resize(Geometry_.Parts().size(), base);
      for (size_t part = before; part < Geometry_.Parts().size(); ++part) {
        const int wanted = Geometry_.Parts()[part].Material;
        const uint32_t at = wanted > 0 && (size_t)wanted < Declared_.Surfacing.size()
                                ? (uint32_t)wanted
                                : 0u;
        Table_.PartSlot[part] = base + at;
      }
      Joined_ = Geometry_.Parts().size() - Declared_.Built->Parts().size();
    }
  }

  if (Declared_.Built == nullptr) { Joined_ = Geometry_.Parts().size(); }
  if (Carrying_ > 0) { Joined_ = Carrying_; }
  ShadowRadiusStoodM_ = Declared_.ShadowRadiusM;
  if (!(ShadowRadiusStoodM_ > 0.0) && Geometry_.TriangleCount() > 0) {
    double least[3], most[3];
    Geometry_.BoundsOf(Joined_, least, most);
    double across = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double span = (most[axis] - least[axis]) * Declared_.MetresPerUnit;
      across += span * span;
    }
    ShadowRadiusStoodM_ = 0.5 * std::sqrt(across);
  }

  Render::PlanSpec declaration;
  DeclarePlan(File_, Moves_, Declared_.DrawsSky,
              ShadowRadiusStoodM_ > 0.0, declaration);
  if (Declared_.Exposure > 0.0) {
    declaration.Exposure = Render::Declared<float>((float)Declared_.Exposure);
  } else if (Declared_.KeyLux > 0.0) {
    const double ev100 = std::log2(Declared_.KeyLux / 2.5);
    declaration.Exposure =
        Render::Declared<float>((float)(1.0 / (1.2 * std::pow(2.0, ev100))));
  }
  if (Plan_ != nullptr && !(PlanDeclared_ == declaration)) { Plan_ = nullptr; }
  if (Plan_ == nullptr) {
    auto made = Render::RenderPlan::Compile(declaration);
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
  if (Declared_.KeyLux > 0.0) {
    if (Declared_.DrawsSky) { Renderer_->SetMedium(Render::Medium{}); }

    const double elevation = Declared_.KeyElevationDeg * std::numbers::pi / 180.0;
    const double bearing = Declared_.KeyBearingDeg * std::numbers::pi / 180.0;
    const double toSunGltf[3] = {std::cos(elevation) * std::sin(bearing), std::sin(elevation),
                                 std::cos(elevation) * std::cos(bearing)};
    const double upGltf[3] = {0.0, 1.0, 0.0};
    double toSunEngine[3], upEngine[3];
    Gltf::InEcef(toSunGltf, toSunEngine);
    Gltf::InEcef(upGltf, upEngine);
    const float toSun[3] = {(float)toSunEngine[0], (float)toSunEngine[1], (float)toSunEngine[2]};
    const float up[3] = {(float)upEngine[0], (float)upEngine[1], (float)upEngine[2]};

    Renderer_->SetSky(toSun, up, (float)Declared_.KeyLux, 0.0f);
    if (ShadowRadiusStoodM_ > 0.0) {
      Renderer_->SetShadowFrame(toSun, up, ShadowRadiusStoodM_);
    }
  }

  const double eye[3] = {0.0, 0.0, 0.0}, forward[3] = {0.0, 0.0, -1.0};
  const double right[3] = {1.0, 0.0, 0.0}, up[3] = {0.0, 1.0, 0.0};
  Renderer_->SetCameraBasis(eye, forward, right, up);

  if (Geometry_.TriangleCount() > 0) {

    Renderer_->SetPictureRegion(Declared_.PictureLeftFrac, Declared_.PictureTopFrac,
                                Declared_.PictureWidthFrac, Declared_.PictureHeightFrac, 0.0);
    if (!Stand(error) || !Render::Surface(*Renderer_, Stood_, Looking_, Scratch_, error) || !Submit(error)) {
      return false;
    }
  } else {
    Renderer_->SetPictureRegion(0, 0, 0, 0, 0);
  }
  return Compose(error);
}

bool Live::Pose(int frame, std::string &error) {
  if (Moves_) {

    const bool first = Geometry_.VertexCount() == 0;
    if (!first) { PreviousPositionsM_ = Geometry_.PositionsM(); }
    Motion_.At((double)frame / Declared_.Fps, Locals_, Weights_);
    if (Geometry_.Build(File_, Span<const Gltf::Transform>(Locals_.data(), Locals_.size()),
                        Span<const double>(Weights_.data(), Weights_.size()),
                        Variant_)) {
      if (first) { PreviousPositionsM_ = Geometry_.PositionsM(); }
      return true;
    }
  } else if (Geometry_.Build(File_, Variant_)) {
    return true;
  }
  error = Geometry_.Error();
  return false;
}

void Live::Eye(const Gltf::Placement &from) {
  Eye_ = from;
  HaveEye_ = true;
  Aimed_ = false;
}

bool Live::PartVolumes(std::string &error) {
  if (!PartBounds_.empty()) { return true; }
  const size_t parts = Geometry_.Parts().size();
  if (parts == 0) { return true; }
  PartBounds_.assign(parts, Volume{});
  const auto fold = [this, parts]() {
    const std::vector<double> &at = Geometry_.PositionsM();
    for (size_t part = 0; part < parts; ++part) {
      const Gltf::Part &one = Geometry_.Parts()[part];
      Volume &held = PartBounds_[part];
      for (size_t vertex = one.FirstVertex; vertex < one.FirstVertex + one.VertexCount; ++vertex) {
        const double *const from = at.data() + vertex * 3;
        for (int axis = 0; axis < 3; ++axis) {
          if (held.Empty || from[axis] < held.LeastM[axis]) { held.LeastM[axis] = from[axis]; }
          if (held.Empty || from[axis] > held.MostM[axis]) { held.MostM[axis] = from[axis]; }
        }
        held.Empty = false;
      }
    }
  };
  fold();
  for (int frame = 0; frame < Frames_; ++frame) {
    if (frame == At_) { continue; }
    if (!Pose(frame, error)) { return false; }
    fold();
  }
  if (Frames_ > 1 && !Pose(At_, error)) { return false; }
  return true;
}

bool Live::PlacedBounds(double least[3], double most[3], std::string &error) {
  if (!PartVolumes(error)) { return false; }
  const size_t framed = Joined_ > 0 && Joined_ < PartBounds_.size() ? Joined_ : PartBounds_.size();
  bool first = true;
  double leastM[3] = {0.0, 0.0, 0.0}, mostM[3] = {0.0, 0.0, 0.0};
  const std::array<double, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (size_t part = 0; part < framed; ++part) {
    const Volume &held = PartBounds_[part];
    if (held.Empty) { continue; }
    const std::array<double, 16> &placed =
        part < Stood_.Parts() ? Stood_.Placement(part) : identity;
    for (int corner = 0; corner < 8; ++corner) {
      const double from[3] = {(corner & 1) ? held.MostM[0] : held.LeastM[0],
                              (corner & 2) ? held.MostM[1] : held.LeastM[1],
                              (corner & 4) ? held.MostM[2] : held.LeastM[2]};
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
  Gltf::Placement framed;
  if (HaveEye_) {
    Looking_.Eye = Eye_;
    Looking_.StandsInside = true;
    return Render::Aim(*Renderer_, Geometry_, Looking_, Stood_.Anchor(), error);
  }
  double least[3], most[3];
  if (!PlacedBounds(least, most, error)) { return false; }
  if (!Gltf::FramingFor(least, most, framed, Framing())) {
    error = "the subject has no extent, so no camera can be derived from it";
    return false;
  }
  const double centre[3] = {(least[0] + most[0]) * 0.5, (least[1] + most[1]) * 0.5,
                            (least[2] + most[2]) * 0.5};
  const double turn = Around_ * std::numbers::pi / 180.0;
  const double cosine = std::cos(turn), sine = std::sin(turn);
  const auto spun = [cosine, sine](const double from[3], double out[3]) {
    out[0] = from[0] * cosine + from[2] * sine;
    out[1] = from[1];
    out[2] = -from[0] * sine + from[2] * cosine;
  };
  const double offset[3] = {framed.EyeM[0] - centre[0], framed.EyeM[1] - centre[1],
                            framed.EyeM[2] - centre[2]};
  double turned[3];
  spun(offset, turned);
  for (int axis = 0; axis < 3; ++axis) { framed.EyeM[axis] = centre[axis] + turned[axis]; }
  double basis[3];
  spun(framed.Forward, basis);
  for (int axis = 0; axis < 3; ++axis) { framed.Forward[axis] = basis[axis]; }
  spun(framed.Right, basis);
  for (int axis = 0; axis < 3; ++axis) { framed.Right[axis] = basis[axis]; }
  spun(framed.Up, basis);
  for (int axis = 0; axis < 3; ++axis) { framed.Up[axis] = basis[axis]; }
  Looking_ = {framed, false, Joined_};
  return Render::Aim(*Renderer_, Geometry_, Looking_, Stood_.Anchor(), error);
}

bool Live::Stand(std::string &error) {
  Stood_ = Render::SubjectProxy{};
  const double anchorEcefM[3] = {Data::kWgs84A, 0.0, 0.0};
  Stood_.Stands(Geometry_, anchorEcefM);
  Looking_ = {HaveEye_ ? Eye_ : Gltf::Placement{}, HaveEye_, Joined_};
  SentBody_.fill(std::numeric_limits<double>::quiet_NaN());
  SentBuilt_.fill(std::numeric_limits<double>::quiet_NaN());
  if (Moves_) { Stood_.Posed(&PreviousPositionsM_); }
  if (!Stood_.Wears(Table_.PartSlot, Table_.Slots, error)) { return false; }

  for (const Gltf::PlacedLight &placed : Geometry_.Lights()) {
    Stood_.Lit(placed.Light);
  }
  if (Declared_.KeyLux > 0.0) {

    const double elevation = Declared_.KeyElevationDeg * std::numbers::pi / 180.0;
    const double bearing = Declared_.KeyBearingDeg * std::numbers::pi / 180.0;
    PunctualLight key;
    key.Kind = LightKind::Directional;
    key.Intensity = (float)Declared_.KeyLux;
    key.Direction[0] = (float)(-std::cos(elevation) * std::sin(bearing));
    key.Direction[1] = (float)(-std::sin(elevation));
    key.Direction[2] = (float)(-std::cos(elevation) * std::cos(bearing));
    Stood_.Lit(key);
  }
  Render::SubjectEnvironment environment;
  for (int channel = 0; channel < 3; ++channel) {
    environment.RadianceLinear[channel] = (float)Declared_.Environment[channel];
  }
  if (Declared_.DrawsSky && Declared_.KeyLux > 0.0) {

    const Render::Medium medium;
    const float cosSun =
        (float)std::sin(Declared_.KeyElevationDeg * std::numbers::pi / 180.0);
    const auto toSun = [&](float radiusKm, float cosZenith, float out[3]) {
      Render::MediumTransmittance(medium, radiusKm, cosZenith, Render::kTransmittanceSteps, out);
    };
    const auto secondOrder = [&](float radiusKm, float cosZenith, float out[3]) {
      float luminance[3], transfer[3];
      const float unitU = cosZenith * 0.5f + 0.5f;
      const float unitV = (radiusKm - medium.BottomRadiusKm) /
                          (medium.TopRadiusKm - medium.BottomRadiusKm);
      Render::MediumMultiScatterTexel(medium, unitU, unitV, toSun, luminance, transfer);
      for (int channel = 0; channel < 3; ++channel) {
        out[channel] = luminance[channel] / (1.0f - transfer[channel]);
      }
    };
    float skylight[3];
    Render::MediumSkyIrradiance(medium, medium.BottomRadiusKm + Render::kMediumGroundLiftKm,
                                cosSun, toSun, secondOrder, skylight);
    for (int channel = 0; channel < 3; ++channel) {
      environment.RadianceLinear[channel] +=
          skylight[channel] / std::numbers::pi_v<float> * Declared_.KeyLux;
    }
  }
  Stood_.Around(environment);

  std::string why;
  Gltf::Placement eye = Looking_.Eye;
  const bool declared = !File_.Cameras().empty() && Gltf::DeclaredPlacement(File_, 0, eye, why);
  Looking_.Eye = eye;
  if (Declared_.Fill > 0.0 || !declared) {

    double least[3], most[3];
    Geometry_.BoundsOf(Joined_, least, most);
    for (int frame = 1; frame < Frames_; ++frame) {
      if (!Pose(frame, error)) { return false; }
      double posedLeast[3], posedMost[3];
      Geometry_.BoundsOf(Joined_, posedLeast, posedMost);
      for (int axis = 0; axis < 3; ++axis) {
        least[axis] = posedLeast[axis] < least[axis] ? posedLeast[axis] : least[axis];
        most[axis] = posedMost[axis] > most[axis] ? posedMost[axis] : most[axis];
      }
    }
    if (Frames_ > 1 && !Pose(0, error)) { return false; }
    if (!Gltf::FramingFor(least, most, eye, Framing())) {
      error = "the subject has no extent over its own grid, so no camera can be derived from it";
      return false;
    }
    Looking_.Eye = eye;
  }
  return true;
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
  Renderer_->SetSkyEye((float)quantisedM);
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
  const size_t want =
      (size_t)Declared_.SurfaceWidthPx * (size_t)Declared_.SurfaceHeightPx * 4u;
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

bool Live::Carry(const double worldFromBodyM[16], const double built[16], std::string &error) {
  const double perUnit = Declared_.MetresPerUnit > 0.0 ? Declared_.MetresPerUnit : 1.0;
  double body[16];
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      body[column * 4 + row] =
          column < 3 ? worldFromBodyM[column * 4 + row] * perUnit : worldFromBodyM[column * 4 + row];
    }
  }
  if (Joined_ == 0) {
    error = "nothing joined this picture from a file, so there is no body to carry -- every part "
            "stands where the world put it";
    return false;
  }
  const size_t parts = Geometry_.Parts().size();
  if (Stood_.Parts() != parts) {
    error = "the subject proxy stands over " + std::to_string(Stood_.Parts()) +
            " parts and the geometry carries " + std::to_string(parts) +
            ", so nothing standing was built from what is being carried";
    return false;
  }
  bool bodyMoved = false, builtMoved = false;
  for (int at = 0; at < 16 && !bodyMoved; ++at) { bodyMoved = SentBody_[at] != body[at]; }
  for (int at = 0; at < 16 && !builtMoved; ++at) { builtMoved = SentBuilt_[at] != built[at]; }
  if (!bodyMoved && !builtMoved) { return true; }

  const size_t joined = Joined_ < parts ? Joined_ : parts;
  if (bodyMoved) {
    for (size_t part = 0; part < joined; ++part) {
      if (!Stood_.Places(part, body)) { return false; }
    }
  }
  if (builtMoved) {
    for (size_t part = joined; part < parts; ++part) {
      if (!Stood_.Places(part, built)) { return false; }
    }
  }
  PartBounds_.clear();
  Renderer_->CastsBelow((uint32_t)Joined_);

  double ecef[16];
  if (bodyMoved && joined > 0) {
    Gltf::PlacedInEcef(body, ecef);
    if (!Render::Moved(*Renderer_, parts, 0, joined, ecef, error)) { return false; }
  }
  if (builtMoved && joined < parts) {
    Gltf::PlacedInEcef(built, ecef);
    if (!Render::Moved(*Renderer_, parts, joined, parts, ecef, error)) { return false; }
  }
  for (int at = 0; at < 16; ++at) {
    SentBody_[at] = body[at];
    SentBuilt_[at] = built[at];
  }
  return true;
}

bool Live::Restands(std::string stands, std::string variant, AssetAnimation animation,
                    std::string &error) {
  Declared_.Stands = std::move(stands);
  Declared_.Variant = std::move(variant);
  Declared_.Animation = animation;
  Declared_.Built = nullptr;
  FileStands_ = false;
  Stoodup_ = false;
  Moves_ = false;
  Frames_ = 1;
  At_ = 0;
  return Build(error);
}

bool Live::Restand(const Gltf::Subject &built, size_t carried, std::string &error) {
  return Restand(built, carried, Declared_.Surfacing.front(), error);
}

bool Live::Restand(const Gltf::Subject &built, size_t carried, const Material &wearing,
                   std::string &error) {
  Aimed_ = false;
  const std::vector<Material> wore = std::move(Declared_.Surfacing);
  Declared_.Surfacing.assign(1u, wearing);
  Declared_.Built = &built;
  Stoodup_ = false;
  Carrying_ = carried;
  const bool stood = Build(error);
  Carrying_ = 0;
  Declared_.Surfacing = wore;
  if (!stood) { return false; }
  Joined_ = carried;
  if (!Stand(error)) { return false; }
  return Submit(error);
}

size_t Live::TookPosing_ = 0, Live::TookSubmitting_ = 0, Live::TookAiming_ = 0, Live::TookDrawing_ = 0;
size_t Live::AssetReads_ = 0;
size_t Live::PlanInits_ = 0;

bool Live::Advance(std::string &error) {
  const auto took = [](size_t before) { return Heap::LiveBytes() - before; };

  if (Moves_ && Frames_ > 1) {
    At_ = (At_ + 1) % Frames_;
    const size_t beforePose = Heap::LiveBytes();
    if (!Pose(At_, error)) { return false; }
    const size_t beforeSubmit = Heap::LiveBytes();
    TookPosing_ = took(beforePose);
    if (!Submit(error)) { return false; }
    TookSubmitting_ = took(beforeSubmit);
  }

  const bool orbits = Declared_.OrbitDegPerFrame != 0.0 && Geometry_.TriangleCount() > 0;
  if (orbits) { Around_ += Declared_.OrbitDegPerFrame; }
  if (orbits || !Aimed_) {
    const size_t beforeAim = Heap::LiveBytes();
    if (!Look(error)) { return false; }
    Aimed_ = true;
    TookAiming_ = took(beforeAim);
  }
  return true;
}

bool Live::Draw(std::string &error) {
  if (Renderer_ == nullptr) {
    error = "no device stands, so there is nothing to draw with";
    return false;
  }
  const auto took = [](size_t before) { return Heap::LiveBytes() - before; };
  const size_t beforeDraw = Heap::LiveBytes();
  const Heap::Tagged drawing("render-frame");
  Renderer_->RenderFrame();
  TookDrawing_ = took(beforeDraw);
  return true;
}



}
