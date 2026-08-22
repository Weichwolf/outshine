#include "Live.h"

#include <numbers>
#include <cmath>

#include <cstdio>
#include <vector>

#include "Heap.h"
#include "Image.h"

#include "Framing.h"
#include "GltfStudio.h"

namespace outshine::Clients {
namespace {

void AsOverlay(const std::vector<Ui::Quad> &from, double offsetX, double offsetY,
               std::vector<Render::OverlayQuad> &out) {
  out.reserve(out.size() + from.size());
  for (const Ui::Quad &quad : from) {
    Render::OverlayQuad to;
    to.LeftPx = (float)(quad.X + offsetX);
    to.TopPx = (float)(quad.Y + offsetY);
    to.WidthPx = (float)quad.Width;
    to.HeightPx = (float)quad.Height;
    to.U0 = (float)quad.U0;
    to.V0 = (float)quad.V0;
    to.U1 = (float)quad.U1;
    to.V1 = (float)quad.V1;
    to.Red = (float)((quad.Colour >> 24) & 0xFFu) / 255.0f;
    to.Green = (float)((quad.Colour >> 16) & 0xFFu) / 255.0f;
    to.Blue = (float)((quad.Colour >> 8) & 0xFFu) / 255.0f;
    to.Alpha = (float)(quad.Colour & 0xFFu) / 255.0f;
    to.ClipLeftPx = (float)(quad.ClipX + offsetX);
    to.ClipTopPx = (float)(quad.ClipY + offsetY);
    to.ClipWidthPx = (float)quad.ClipWidth;
    to.ClipHeightPx = (float)quad.ClipHeight;
    to.RadiusPx = (float)quad.Radius;
    to.Opacity = (float)quad.Opacity;
    out.push_back(to);
  }
}

void DeclarePlan(const Gltf::Document &file, bool moves, bool presents, bool sky,
                 bool shadows, Render::PlanSpec &declaration) {
  declaration.Outputs = {Render::Resource::FrameTex};
  if (presents) { declaration.Outputs.push_back(Render::Resource::Surface); }

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
    : Renderer_(&renderer), Font_(font), Declared_(std::move(declaration)) {}

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

double Live::Framing(void) const {
  return Declared_.Fill > 0.0 ? Declared_.Fill : Gltf::kFramingFill;
}

bool Live::Build(std::string &error) {
  if (Declared_.Built != nullptr && Declared_.Stands.empty()) {
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

    if (!File_.Animations().empty()) {
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

  Render::PlanSpec declaration;
  DeclarePlan(File_, Moves_, Declared_.Presents, Declared_.DrawsSky,
              Declared_.ShadowRadiusM > 0.0, declaration);
  if (Declared_.Exposure > 0.0) {
    declaration.Exposure = Render::Declared<float>((float)Declared_.Exposure);
  } else if (Declared_.KeyLux > 0.0) {
    const double ev100 = std::log2(Declared_.KeyLux / 2.5);
    declaration.Exposure =
        Render::Declared<float>((float)(1.0 / (1.2 * std::pow(2.0, ev100))));
  }
  if (Plan_ == nullptr) {
    if (!Render::RenderPlan::Compile(declaration, &Plan_, error)) { return false; }
    Renderer_->Init(Declared_.SurfaceWidthPx, Declared_.SurfaceHeightPx, Plan_);
    if (!Renderer_->DeviceUsable()) {
      error = Renderer_->WhyNot().empty()
                  ? std::string("the device did not come up, so this scenario cannot be stood up")
                  : Renderer_->WhyNot();
      return false;
    }
    if (Declared_.AtlasRgba != nullptr &&
        !Renderer_->SetOverlayAtlas(Declared_.AtlasRgba, Declared_.AtlasWidthPx,
                                    Declared_.AtlasHeightPx, error)) {
      return false;
    }
  }
  if (Declared_.DrawsSky) {
    Renderer_->SetMedium(Render::Medium{});

    const double elevation = Declared_.KeyElevationDeg * std::numbers::pi / 180.0;
    const double bearing = Declared_.KeyBearingDeg * std::numbers::pi / 180.0;
    const double toSunGltf[3] = {std::cos(elevation) * std::sin(bearing), std::sin(elevation),
                                 std::cos(elevation) * std::cos(bearing)};
    const double upGltf[3] = {0.0, 1.0, 0.0};
    double toSunEngine[3], upEngine[3];
    EcefFromGltf(toSunGltf, toSunEngine);
    EcefFromGltf(upGltf, upEngine);
    const float toSun[3] = {(float)toSunEngine[0], (float)toSunEngine[1], (float)toSunEngine[2]};
    const float up[3] = {(float)upEngine[0], (float)upEngine[1], (float)upEngine[2]};

    for (int axis = 0; axis < 3; ++axis) {
      SkyToSun_[axis] = toSun[axis];
      SkyUp_[axis] = up[axis];
    }
    SkyStands_ = true;
    Renderer_->SetSky(toSun, up, (float)Declared_.KeyLux, 0.0f);
    if (Declared_.ShadowRadiusM > 0.0) {
      Renderer_->SetShadowFrame(toSun, up, Declared_.ShadowRadiusM);
    }
  }

  const double eye[3] = {0.0, 0.0, 0.0}, forward[3] = {0.0, 0.0, -1.0};
  const double right[3] = {1.0, 0.0, 0.0}, up[3] = {0.0, 1.0, 0.0};
  Renderer_->SetCameraBasis(eye, forward, right, up);

  if (Geometry_.TriangleCount() > 0) {

    Renderer_->SetPictureRegion(Declared_.PictureLeftFrac, Declared_.PictureTopFrac,
                                Declared_.PictureWidthFrac, Declared_.PictureHeightFrac, 0.0);
    if (!Stand(error) || !Surface(*Renderer_, Stood_, Scratch_, error) || !Submit(error)) {
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

bool Live::PlacedBounds(double least[3], double most[3], std::string &error) {
  if (!BoundsPlaced_) {
    bool first = true;
    const auto fold = [this, &first](void) {
      const std::vector<double> &at = Geometry_.PositionsM();
      for (size_t part = 0; part < Geometry_.Parts().size(); ++part) {
        const Gltf::Part &one = Geometry_.Parts()[part];
        const std::array<double, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const std::array<double, 16> &placed =
            part < Stood_.PartPlacement.size() ? Stood_.PartPlacement[part] : identity;
        for (size_t vertex = one.FirstVertex; vertex < one.FirstVertex + one.VertexCount;
             ++vertex) {
          const double *const from = at.data() + vertex * 3;
          for (int axis = 0; axis < 3; ++axis) {
            const double out = placed[axis] * from[0] + placed[4 + axis] * from[1] +
                               placed[8 + axis] * from[2] + placed[12 + axis];
            if (first || out < PlacedLeast_[axis]) { PlacedLeast_[axis] = out; }
            if (first || out > PlacedMost_[axis]) { PlacedMost_[axis] = out; }
          }
          first = false;
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
    if (first) {
      for (int axis = 0; axis < 3; ++axis) {
        PlacedLeast_[axis] = 0.0;
        PlacedMost_[axis] = 0.0;
      }
    }
    BoundsPlaced_ = true;
  }
  for (int axis = 0; axis < 3; ++axis) {
    least[axis] = PlacedLeast_[axis];
    most[axis] = PlacedMost_[axis];
  }
  return true;
}

bool Live::Look(std::string &error) {
  Gltf::Placement framed;
  if (HaveEye_) {
    Stood_.Eye = Eye_;
    Stood_.EyeStandsInside = true;
    return Aim(*Renderer_, Geometry_, Stood_.Eye, error, true);
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
  Stood_.Eye = framed;
  Stood_.EyeStandsInside = false;
  return Aim(*Renderer_, Geometry_, Stood_.Eye, error, false);
}

bool Live::Stand(std::string &error) {
  Stood_ = Studio{};
  Stood_.PartPlacement.assign(Geometry_.Parts().size(),
                              {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
  Stood_.Geometry = &Geometry_;
  if (Moves_) { Stood_.PreviousPositionsM = &PreviousPositionsM_; }
  Stood_.EmittedRadiance.assign(Geometry_.Parts().size(), {0.0f, 0.0f, 0.0f});
  Stood_.PartSurface = Table_.PartSlot;
  Stood_.Surfaces = Table_.Slots;

  for (const Gltf::PlacedLight &placed : Geometry_.Lights()) {
    Stood_.Lights.push_back(placed.Light);
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
    Stood_.Lights.push_back(key);
  }
  for (int channel = 0; channel < 3; ++channel) {
    Stood_.Environment.RadianceLinear[channel] = (float)Declared_.Environment[channel];
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
      Stood_.Environment.RadianceLinear[channel] +=
          skylight[channel] / std::numbers::pi_v<float> * Declared_.KeyLux;
    }
  }

  std::string why;
  const bool declared = !File_.Cameras().empty() &&
                        Gltf::DeclaredPlacement(File_, 0, Stood_.Eye, why);
  if (Declared_.Fill > 0.0 || !declared) {

    double least[3], most[3];
    for (int axis = 0; axis < 3; ++axis) {
      least[axis] = Geometry_.MinM()[axis];
      most[axis] = Geometry_.MaxM()[axis];
    }
    for (int frame = 1; frame < Frames_; ++frame) {
      if (!Pose(frame, error)) { return false; }
      for (int axis = 0; axis < 3; ++axis) {
        least[axis] = Geometry_.MinM()[axis] < least[axis] ? Geometry_.MinM()[axis] : least[axis];
        most[axis] = Geometry_.MaxM()[axis] > most[axis] ? Geometry_.MaxM()[axis] : most[axis];
      }
    }
    if (Frames_ > 1 && !Pose(0, error)) { return false; }
    if (!Gltf::FramingFor(least, most, Stood_.Eye, Framing())) {
      error = "the subject has no extent over its own grid, so no camera can be derived from it";
      return false;
    }
  }
  return true;
}

bool Live::Submit(std::string &error) {
  if (!Stoodup_) {
    Stoodup_ = Place(*Renderer_, Stood_, Scratch_, error);
    return Stoodup_;
  }
  return Move(*Renderer_, Stood_, Scratch_, error);
}

bool Live::Compose(std::string &error) {
  Laid_.clear();
  Quads_.clear();
  Laid_.resize(Declared_.Surfaces.size());
  for (size_t at = 0; at < Declared_.Surfaces.size(); ++at) {
    const Shows &declared = Declared_.Surfaces[at];
    Laid &laid = Laid_[at];
    const double widthPx = declared.WidthFrac * (double)Declared_.SurfaceWidthPx;
    const double heightPx = declared.HeightFrac * (double)Declared_.SurfaceHeightPx;
    if (widthPx <= 0.0 || heightPx <= 0.0) { continue; }
    if (Font_ == nullptr) {
      error = "surface " + std::to_string(at) + " is declared and no face was handed over to set it in";
      return false;
    }
    laid.LeftPx = declared.LeftFrac * (double)Declared_.SurfaceWidthPx;
    laid.TopPx = declared.TopFrac * (double)Declared_.SurfaceHeightPx;
    if (!laid.Tree.Read(declared.Markup, error)) { return false; }
    laid.Sheet.Read(Ui::UserAgentSheet());
    if (!declared.Style.empty()) { laid.Sheet.Read(declared.Style); }
    laid.Sheet.Read(laid.Tree.StyleText());
    if (!laid.Placed.Build(laid.Tree, laid.Sheet, widthPx, heightPx, *Font_, error)) { return false; }
    if (!laid.Painted.Build(laid.Placed, *Font_, error)) { return false; }
    AsOverlay(laid.Painted.Quads(), laid.LeftPx, laid.TopPx, Quads_);
  }
  return Renderer_->SetOverlay(Quads_.data(), Quads_.size(), error);
}

bool Live::Redeclare(std::vector<Shows> surfaces, std::string &error) {
  Declared_.Surfaces = std::move(surfaces);
  return Compose(error);
}

void Live::SkyEye(double aboveGroundM) {
  if (!SkyStands_ || Renderer_ == nullptr) { return; }

  constexpr double kSkyEyeStepM = 2.0;
  const double quantisedM =
      std::floor(std::fmax(0.0, aboveGroundM) / kSkyEyeStepM + 0.5) * kSkyEyeStepM;
  Renderer_->SetSky(SkyToSun_, SkyUp_, (float)Declared_.KeyLux, (float)quantisedM);
}

bool Live::ReadPixels(std::vector<uint8_t> &rgba, std::string &error) {
  if (Renderer_ == nullptr || !Stoodup_) {
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
  if (Renderer_ == nullptr || !Stoodup_) {
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

bool Live::Carry(const double body[16], const double built[16], std::string &error) {
  if (Joined_ == 0) {
    error = "nothing joined this picture from a file, so there is no body to carry -- every part "
            "stands where the world put it";
    return false;
  }
  if (Stood_.PartPlacement.size() != Geometry_.Parts().size()) {
    Stood_.PartPlacement.assign(Geometry_.Parts().size(),
                                {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
  }
  for (size_t part = 0; part < Stood_.PartPlacement.size(); ++part) {
    const double *const from = part < Joined_ ? body : built;
    for (int at = 0; at < 16; ++at) { Stood_.PartPlacement[part][at] = from[at]; }
  }
  BoundsPlaced_ = false;
  {
    const double centreM[3] = {body[12], body[13], body[14]};
    double centreEngine[3];
    EcefFromGltf(centreM, centreEngine);
    for (int axis = 0; axis < 3; ++axis) { centreEngine[axis] += kStudioAnchorEcefM[axis]; }
    Renderer_->ShadowCentre(centreEngine);
  }
  Placements(Stood_, Scratch_.Placements);
  return Renderer_->SetSubjectPlacements(Scratch_.Placements.data(),
                                         Stood_.PartPlacement.size(), error);
}

bool Live::Restand(const Gltf::Subject &built, std::string &error) {
  Declared_.Built = &built;
  Stoodup_ = false;
  if (!Build(error)) { return false; }
  if (!Stand(error)) { return false; }
  return Submit(error);
}

size_t Live::TookPosing_ = 0, Live::TookSubmitting_ = 0, Live::TookAiming_ = 0, Live::TookDrawing_ = 0;
size_t Live::AssetReads_ = 0;

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
  const size_t beforeDraw = Heap::LiveBytes();
  const Heap::Tagged drawing("render-frame");
  Renderer_->RenderFrame();
  TookDrawing_ = took(beforeDraw);
  return true;
}

Ui::Touched Live::Under(double xPx, double yPx) const {
  for (size_t at = Laid_.size(); at > 0; --at) {
    const Laid &laid = Laid_[at - 1];
    Ui::Touched found = Ui::Under(laid.Placed, laid.Tree, xPx - laid.LeftPx, yPx - laid.TopPx);
    if (found.Node >= 0) { return found; }
  }
  return Ui::Touched{};
}

}
