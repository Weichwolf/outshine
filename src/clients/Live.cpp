#include "Live.h"

#include <cmath>

#include <cstdio>
#include <vector>

#include "Heap.h"
#include "Image.h"

#include "Framing.h"

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
                 Render::PlanSpec &declaration) {
  declaration.Outputs = {Render::Resource::FrameTex};
  if (presents) { declaration.Outputs.push_back(Render::Resource::Surface); }

  if (moves) { declaration.Outputs.push_back(Render::Resource::SceneVelocity); }
  declaration.Content = {Render::Stage::Subjects, Render::Stage::Overlay};
  if (sky) { declaration.Content.push_back(Render::Stage::Sky); }
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
    ResolveDeclaredSurface(Geometry_, Declared_.Surface, Table_);
  }
  if (!Declared_.Stands.empty()) {
    if (!Declared_.Variant.empty()) { Variant_ = Gltf::VariantSelection(Declared_.Variant); }
    if (!File_.ReadFile(Declared_.Stands)) {
      error = File_.Error();
      return false;
    }

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
    if (!Pose(0, error)) { return false; }
    ResolveSurfaceTable(File_, Geometry_, true, true, Table_);
    if (!ResolveFileSurface(File_, Geometry_, ColourFrom::Row, ColourCarrier::Texture, Table_,
                            error)) {
      return false;
    }
    if (Declared_.Built != nullptr) {
      SurfaceTable joining;
      ResolveDeclaredSurface(*Declared_.Built, Declared_.Surface, joining);
      if (joining.Slots.empty()) {
        error = "the declared surface for the built geometry resolved to no slot, so the parts "
                "joining this picture would name a surface that is not there";
        return false;
      }
      const uint32_t slot = (uint32_t)Table_.Slots.size();
      Table_.Slots.push_back(joining.Slots.front());
      if (!Geometry_.Append(*Declared_.Built)) {
        error = Geometry_.Error();
        return false;
      }
      Table_.PartSlot.resize(Geometry_.Parts().size(), slot);
      Joined_ = Geometry_.Parts().size() - Declared_.Built->Parts().size();
    }
  }

  Render::PlanSpec declaration;
  DeclarePlan(File_, Moves_, Declared_.Presents, Declared_.DrawsSky, declaration);
  if (Declared_.Exposure > 0.0) {
    declaration.Exposure = Render::Declared<float>((float)Declared_.Exposure);
  } else if (Declared_.KeyLux > 0.0) {
    const double ev100 = std::log2(Declared_.KeyLux / 2.5);
    declaration.Exposure =
        Render::Declared<float>((float)(1.0 / (1.2 * std::pow(2.0, ev100))));
  }
  if (!Render::RenderPlan::Compile(declaration, &Plan_, error)) { return false; }
  Renderer_->Init(Declared_.SurfaceWidthPx, Declared_.SurfaceHeightPx, Plan_);
  if (!Renderer_->DeviceUsable()) {
    error = "the device did not come up, so this scenario cannot be stood up";
    return false;
  }
  if (Declared_.AtlasRgba != nullptr &&
      !Renderer_->SetOverlayAtlas(Declared_.AtlasRgba, Declared_.AtlasWidthPx,
                                  Declared_.AtlasHeightPx, error)) {
    return false;
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
    if (!first) { Previous_ = Geometry_; }
    Motion_.At((double)frame / Declared_.Fps, Locals_, Weights_);
    if (Geometry_.Build(File_, Span<const Gltf::Transform>(Locals_.data(), Locals_.size()),
                        Span<const double>(Weights_.data(), Weights_.size()),
                        Variant_)) {
      if (first) { Previous_ = Geometry_; }
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

bool Live::Look(std::string &error) {
  Gltf::Placement framed;
  if (HaveEye_) {
    Stood_.Eye = Eye_;
    Stood_.EyeStandsInside = true;
    return Aim(*Renderer_, Geometry_, Stood_.Eye, error, true);
  }
  if (!Geometry_.Frame(framed, Framing())) {
    error = "the subject has no extent, so no camera can be derived from it";
    return false;
  }
  const double centre[3] = {(Geometry_.MinM()[0] + Geometry_.MaxM()[0]) * 0.5,
                            (Geometry_.MinM()[1] + Geometry_.MaxM()[1]) * 0.5,
                            (Geometry_.MinM()[2] + Geometry_.MaxM()[2]) * 0.5};
  const double turn = Around_ * 3.14159265358979323846 / 180.0;
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
  if (Moves_) { Stood_.Previous = &Previous_; }
  Stood_.EmittedRadiance.assign(Geometry_.Parts().size(), {0.0f, 0.0f, 0.0f});
  Stood_.PartSurface = Table_.PartSlot;
  Stood_.Surfaces = Table_.Slots;

  for (const Gltf::PlacedLight &placed : Geometry_.Lights()) {
    Stood_.Lights.push_back(placed.Light);
  }
  if (Declared_.KeyLux > 0.0) {

    const double elevation = Declared_.KeyElevationDeg * 3.14159265358979323846 / 180.0;
    const double bearing = Declared_.KeyBearingDeg * 3.14159265358979323846 / 180.0;
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
  return Renderer_->SetSubjectPlacements(Stood_.PartPlacement.front().data(),
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
