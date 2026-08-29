#include <chrono>
#include "Live.h"

#include "Shaped.h"
#include "Surfaces.h"

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


bool DeclarePlan(const Gltf::Document &file, bool sky, bool shadows,
                 const std::vector<std::string> &stages, const std::vector<std::string> &outputs,
                 Render::PlanSpec &declaration, std::string &error) {
  declaration.Outputs = {Render::Resource::FrameTex, Render::Resource::Surface};
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
    declaration.Outputs.push_back(Render::Resource::SceneVelocity);
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
  return true;
}

}

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

bool Live::Open(Render::SceneRenderer &renderer, Declaration declaration, const Ui::Font *font,
                std::unique_ptr<Live> &out, std::string &error) {

  out.reset();
  std::unique_ptr<Live> live(new Live(renderer, std::move(declaration), font));
  if (!live->Build(error)) { return false; }
  out = std::move(live);
  return true;
}

double Live::Framing() const {
  return Declared_.Fill > 0.0 ? Declared_.Fill : Render::kFramingFill;
}

namespace {


}

// THE ENGINE FILLS THE VIEW THE RENDERER READS. `src/render/` no longer names the importer's
// carrier: it takes a `Render::Shape`, which is spans over whatever is held, so inverting the
// dependency costs no bytes. The importer knows the engine; the engine hands the renderer a view;
// the renderer never learns what file anything came from -- which is Unreal's arrow (the glTF
// importer is a module depending on the engine) and RAGE's (tools depend on the runtime).
// THE WORLD'S PRODUCER NEEDS NO CONVERSION AT ALL. A `Geometry` already holds float per part in
// the layout the device binds, so the shape's parts VIEW it and only the indices are joined --
// uint32_t on both sides, a copy with an offset rather than a reshaping. This is the whole point
// of the goal: `Assemble` widened 28 M vertices to double so that `PackVertices` could narrow them
// back, and neither pass had a reader that wanted double.
Render::Shape Shaped(const outshine::Geometry &from, Render::ShapeStore &into) {
  into.Clear();
  const int parts = from.parts();
  size_t wholeIndices = 0;
  for (int part = 0; part < parts; ++part) { wholeIndices += from.trianglesOf(part).size(); }
  into.Indices.reserve(wholeIndices);
  for (int surface = 0; surface < from.surfaces(); ++surface) {
    into.Surfaces.push_back(from.surfaceAt(MaterialInstance(surface)));
  }
  for (int lamp = 0; lamp < from.lamps(); ++lamp) {
    PunctualLight standing = from.lampAt(lamp);
    const double *const at = from.lampPlacementOf(lamp);
    for (int axis = 0; axis < 3; ++axis) { standing.Position[axis] = (float)at[12 + axis]; }
    into.Lamps.push_back(standing);
  }

  into.Parts.reserve((size_t)parts);
  size_t firstVertex = 0;
  size_t firstIndex = 0;
  for (int part = 0; part < parts; ++part) {
    Render::ShapePart made;
    made.Name = from.nameOf(part);
    made.Material = from.materialOf(part).index();
    made.PositionsM = from.positionsOf(part);
    made.Normals = from.normalsOf(part);
    made.Tangents = from.tangentsOf(part);
    made.Uv = from.textureOf(part, 0);
    made.Uv1 = from.textureOf(part, 1);
    made.Colours = from.coloursOf(part);
    made.HasUv = !made.Uv.empty();
    made.HasUv1 = !made.Uv1.empty();
    made.HasNormal = !made.Normals.empty();
    made.HasColour = !made.Colours.empty();
    made.HasTangent = !made.Tangents.empty();
    made.VertexCount = made.PositionsM.size() / 3;
    made.FirstVertex = firstVertex;
    const std::span<const uint32_t> order = from.trianglesOf(part);
    made.FirstIndex = firstIndex;
    made.IndexCount = order.size();
    for (const uint32_t index : order) { into.Indices.push_back((uint32_t)firstVertex + index); }
    firstVertex += made.VertexCount;
    firstIndex += order.size();
    into.Parts.push_back(made);
  }
  Render::Shape out;
  out.Parts = into.Parts;
  out.Surfaces = into.Surfaces;
  out.Lamps = into.Lamps;
  out.Indices = into.Indices;
  for (const Render::ShapePart &one : into.Parts) {
    out.CarriesUv = out.CarriesUv || one.HasUv;
    out.CarriesUv1 = out.CarriesUv1 || one.HasUv1;
    out.CarriesNormal = out.CarriesNormal || one.HasNormal;
    out.CarriesTangent = out.CarriesTangent || one.HasTangent;
    out.CarriesColour = out.CarriesColour || one.HasColour;
  }
  return out;
}



// ONE SHAPE, ONE STORE, ONE PRODUCER. Five call sites used to build a temporary over the SAME
// buffer, so each one silently invalidated the spans the standing shape was holding. The shape is
// built here and nowhere else, and which producer fills it is the only question left.
void Live::Reshape() {
  Shaped_ = Held_.HoldsBuilt() ? Shaped(Held_.Built(), ShapeParts_)
                               : Gltf::Shaped(Held_.Assembled(), ShapeParts_);
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
    // FOUR CANDIDATES UNDER ONE PHASE, and guessing between them has cost three rounds today.
    const auto tookFrom = std::chrono::steady_clock::now();
    if (Declared_.Built != nullptr) { Held_.Carries(*Declared_.Built); }
    CarryMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tookFrom)
                   .count();
    const auto resolvedFrom = std::chrono::steady_clock::now();
    Reshape();
    Render::ResolveDeclaredSurface(Shaped_, Declared_.Surfacing.front(), Table_);
    ResolveMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - resolvedFrom)
            .count();
  }
  if (!Declared_.Stands.empty()) {
    if (!Held_.Stands()) {
      if (!Held_.Reads(Declared_.Stands, Declared_.Variant, Declared_.Animation, Declared_.Clip,
                       Declared_.Fps,
                       error)) {
        return false;
      }
      AssetReads_ += 1;
    }
    if (!Pose(0, error)) { return false; }
    for (const std::string &joining : Declared_.Joins) {
      Core::Posed arriving;
      if (!arriving.Reads(joining, "", Declared_.Animation, Declared_.Clip, Declared_.Fps, error)) {
        return false;
      }
      if (!arriving.Poses(0, Declared_.Fps, error)) { return false; }
      AssetReads_ += 1;
      if (!Held_.Assembled().Append(arriving.Assembled())) {
        error = "the subject '" + joining + "' would not append onto the one before it";
        return false;
      }
    }
    Gltf::ResolveSurfaceTable(Held_.File(), Held_.Assembled(), true, true, Table_);
    if (!Gltf::ResolveFileSurface(Held_.File(), Held_.Assembled(), Render::ColourFrom::Row, Render::ColourCarrier::Texture, Table_,
                            error)) {
      return false;
    }
    // THE FILE'S MATERIALS ARE A DEFAULT, NOT A FACT. A client rendering somebody else's asset
    // against a reference states what the surfaces ARE; 107 of the 148 Khronos cases do exactly
    // that, and before this the only way to say it was to reach past the door. Matched by NAME
    // because that is what a file states and a manifest quotes -- an index moves when the file is
    // re-exported and a name does not.
    if (!Declared_.Overriding.empty()) {
      size_t took = 0;
      for (size_t slot = 0; slot < Table_.Slots.size(); ++slot) {
        const int index = Table_.Material[slot];
        if (index < 0 || (size_t)index >= Held_.File().Materials().size()) { continue; }
        const std::string &named = Held_.File().Materials()[(size_t)index].Name;
        for (const SurfaceOverride &said : Declared_.Overriding) {
          if (said.Named != named) { continue; }
          Table_.Slots[slot].Row = said.Row;
          ++took;
          break;
        }
      }
      if (took == 0) {
        error = "this declaration names " + std::to_string(Declared_.Overriding.size()) +
                " surface(s) of '" + Declared_.Stands + "' and the file carries none of those "
                "names -- a surface declared onto nothing changes no pixel and says it did";
        return false;
      }
    }
    if (Declared_.Built != nullptr) {
      const uint32_t base = (uint32_t)Table_.Slots.size();
      for (const Material &declaredSurface : Declared_.Surfacing) {
        Render::SurfaceTable joining;
        Render::ShapeStore joiningParts;
        Render::ResolveDeclaredSurface(Gltf::Shaped(*Declared_.Built, joiningParts), declaredSurface,
                                       joining);
        if (joining.Slots.empty()) {
          error = "a declared surface for the built geometry resolved to no slot, so the parts "
                  "joining this picture would name a surface that is not there";
          return false;
        }
        Table_.Slots.push_back(joining.Slots.front());
      }
      const size_t before = Held_.Assembled().Parts().size();
      if (!Held_.Assembled().Append(*Declared_.Built)) {
        error = Held_.Assembled().Error();
        return false;
      }
      Table_.PartSlot.resize(Held_.Assembled().Parts().size(), base);
      for (size_t part = before; part < Held_.Assembled().Parts().size(); ++part) {
        const int wanted = Held_.Assembled().Parts()[part].Material;
        const uint32_t at = wanted > 0 && (size_t)wanted < Declared_.Surfacing.size()
                                ? (uint32_t)wanted
                                : 0u;
        Table_.PartSlot[part] = base + at;
      }
      Joined_ = Held_.Assembled().Parts().size() - Declared_.Built->Parts().size();
    }
  }

  if (!Held_.HoldsBuilt()) { Reshape(); }
  if (Declared_.Built == nullptr) { Joined_ = Shaped_.Parts.size(); }
  if (Carrying_ > 0) { Joined_ = Carrying_; }
  ShadowRadiusStoodM_ = Declared_.ShadowRadiusM;
  if (!(ShadowRadiusStoodM_ > 0.0) && Shaped_.TriangleCount() > 0) {
    double least[3], most[3];
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
  if (!DeclarePlan(Held_.File(), Declared_.DrawsSky, ShadowRadiusStoodM_ > 0.0,
                   Declared_.Stages, Declared_.Outputs, declaration, error)) {
    return false;
  }
  if (Declared_.Exposure > 0.0) {
    declaration.Exposure = Render::Declared<float>((float)Declared_.Exposure);
  } else if (Declared_.KeyLux > 0.0) {
    const double ev100 = std::log2(Declared_.KeyLux / 2.5);
    declaration.Exposure =
        Render::Declared<float>((float)(1.0 / (1.2 * std::pow(2.0, ev100))));
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
  if (Declared_.KeyLux > 0.0) {
    if (Declared_.DrawsSky) { Renderer_->SetMedium(Render::Medium{}); }

    const double elevation = Declared_.KeyElevationDeg * std::numbers::pi / 180.0;
    const double bearing = Declared_.KeyBearingDeg * std::numbers::pi / 180.0;
    // ONE FRAME, and it is the one the PRODUCERS write. A swap mapped a producer frame onto a
    // device frame and every vector went through it together -- positions, normals, tangents,
    // previous positions, placements, the light and the eye -- so the mapping was a relabelling
    // that cost 28 M vertices a rebuild. Measured with one known vertex either side: (-2006, 426,
    // -2111) in, (426, -2006, 2111) out, exactly (x,y,z) -> (y,x,-z).
    //
    // THE OWNER LOOKED AND THE WALLS ARE RIGHT NOW. The flatness statistic beside this moved when
    // the swap went, and I read that as a regression and reverted -- wrongly: that statistic is a
    // BLANK-frame guard and says on its own page that it does not judge how a picture looks. Two
    // non-blank frames are outside what it can decide, and the eye is the oracle this directory
    // exists for.
    const float toSun[3] = {(float)(std::cos(elevation) * std::sin(bearing)),
                            (float)std::sin(elevation),
                            (float)(std::cos(elevation) * std::cos(bearing))};
    const float up[3] = {0.0f, 1.0f, 0.0f};

    Renderer_->SetSky(toSun, up, (float)Declared_.KeyLux, 0.0f);
    if (ShadowRadiusStoodM_ > 0.0) {
      Renderer_->SetShadowFrame(toSun, up, ShadowRadiusStoodM_);
    }
  }

  const double eye[3] = {0.0, 0.0, 0.0}, forward[3] = {0.0, 0.0, -1.0};
  const double right[3] = {1.0, 0.0, 0.0}, up[3] = {0.0, 1.0, 0.0};
  Renderer_->SetCameraBasis(eye, forward, right, up);

  if (Shaped_.TriangleCount() > 0) {

    Renderer_->SetPictureRegion(Declared_.PictureLeftFrac, Declared_.PictureTopFrac,
                                Declared_.PictureWidthFrac, Declared_.PictureHeightFrac, 0.0);
    auto insideFrom = std::chrono::steady_clock::now();
    const auto sinceInside = [&insideFrom]() {
      const double ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - insideFrom)
              .count();
      insideFrom = std::chrono::steady_clock::now();
      return ms;
    };
    const auto wholeFrom = std::chrono::steady_clock::now();
    if (!Stand(error)) { return false; }
    StandMs_ = sinceInside();
    if (!Render::Surface(*Renderer_, Stood_, Looking_, Scratch_, error)) { return false; }
    SurfaceMs_ = sinceInside();
    if (!Submit(error)) { return false; }
    SubmitMs_ = sinceInside();
    InsideMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wholeFrom)
            .count();
    InsideMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - insideFrom)
            .count();
  } else {
    Renderer_->SetPictureRegion(0, 0, 0, 0, 0);
  }
  return Compose(error);
}

// A POSE MOVES THE VERTICES, SO THE SHAPE FOLLOWS IT IN THE SAME CALL. The shape narrows a
// document's doubles into its own store, so it is a COPY of the posed positions rather than a view
// of them, and an animation that re-poses without re-shaping draws the frame it was standing on
// before. This is what the five throwaway shapes were buying, and it costs one line to buy it once.
// A POSE MOVES THE VERTICES, SO THE ONE SHAPE FOLLOWS IT HERE. There is exactly one shape and the
// proxy stands on it; a pose that changed the carrier without re-forming it would draw the frame
// before. That this costs a re-form at all is board:2037:  REBUILDS the carrier from the
// document every frame, which neither Unreal (a fixed FStaticMeshRenderData with a pose buffer)
// nor RAGE (a fixed grmGeometry with crSkeleton matrices) does.
// A POSE MOVES THE VERTICES, SO THE ONE SHAPE FOLLOWS IT HERE. There is exactly one shape and the
// proxy stands on it, so a pose that changed the carrier without re-forming it would draw the
// frame before. That this costs a re-form AT ALL is board:2037's finding: `Poses` rebuilds the
// carrier from the document every frame, which neither Unreal (a fixed FStaticMeshRenderData with
// its own pose buffer) nor RAGE (a fixed grmGeometry with crSkeleton matrices) does.
bool Live::Pose(int frame, std::string &error) {
  if (!Held_.Poses(frame, Declared_.Fps, error)) { return false; }
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
  const auto fold = [this, parts]() {
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
  for (int frame = 0; frame < Held_.Frames(); ++frame) {
    if (frame == Held_.At()) { continue; }
    if (!Pose(frame, error)) { return false; }
    fold();
  }
  if (Held_.Frames() > 1 && !Pose(Held_.At(), error)) { return false; }
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
  Render::Viewpoint framed;
  if (HaveEye_) {
    Looking_.Eye = Eye_;
    Looking_.StandsInside = true;
    Render::ShapeStore aiming;
    return Render::Aim(*Renderer_, Gltf::Shaped(Held_.Assembled(), aiming), Looking_, Stood_.Anchor(),
                       error);
  }
  double least[3], most[3];
  if (!PlacedBounds(least, most, error)) { return false; }
  Gltf::Viewpoint fromFile;
  if (!Gltf::FramingFor(least, most, fromFile, Framing())) {
    error = "the subject has no extent, so no camera can be derived from it";
    return false;
  }
  framed = fromFile;
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
  // AIMING READS THE POSE THAT IS STANDING RIGHT NOW, which is not always the one the proxy stood
  // with: `Poses` rebuilds the carrier from the document and drops whatever was APPENDED onto it,
  // so refreshing the shared shape here would leave the proxy standing over three parts while its
  // surface table names nine. Its own store, and the standing shape is left alone.
  Render::ShapeStore aiming;
  return Render::Aim(*Renderer_, Gltf::Shaped(Held_.Assembled(), aiming), Looking_, Stood_.Anchor(),
                     error);
}

bool Live::Stand(std::string &error) {
  Stood_ = Render::SubjectProxy{};
  const double anchorEcefM[3] = {Data::kWgs84A, 0.0, 0.0};
  Reshape();
  Stood_.Stands(Shaped_, anchorEcefM);
  const double standingM16[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (size_t part = 0; part < Stood_.Parts(); ++part) {
    if (!Stood_.Places(part, standingM16)) { return false; }
  }
  Looking_ = {HaveEye_ ? Eye_ : Render::Viewpoint{}, HaveEye_, Joined_};
  for (std::array<double, 16> &one : SentBody_) {
    one.fill(std::numeric_limits<double>::quiet_NaN());
  }
  SentBuilt_.fill(std::numeric_limits<double>::quiet_NaN());
  if (Held_.Moves()) { Stood_.Posed(&Held_.Previous()); }
  if (!Stood_.Wears(Table_.PartSlot, Table_.Slots, error)) { return false; }

  for (const PunctualLight &placed : Shaped_.Lamps) { Stood_.Lit(placed); }
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
    environment.RadianceLinear[channel] = (float)Declared_.IndirectLight[channel];
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
    float sunReach[3];
    toSun(medium.BottomRadiusKm + Render::kMediumGroundLiftKm, cosSun, sunReach);
    const float straightDown = cosSun > 0.0f ? cosSun : 0.0f;
    for (int channel = 0; channel < 3; ++channel) {
      environment.RadianceLinear[channel] +=
          skylight[channel] / std::numbers::pi_v<float> * Declared_.KeyLux;
      const float onTheGround =
          (float)Declared_.KeyLux * (straightDown * sunReach[channel] + skylight[channel]);
      environment.GroundLinear[channel] +=
          (float)GroundAlbedo_[channel] * onTheGround / std::numbers::pi_v<float>;
    }
  }
  Stood_.Around(environment);

  std::string why;
  Render::Viewpoint eye = Looking_.Eye;
  Gltf::Viewpoint placed;
  const bool declared =
      !Held_.File().Cameras().empty() && Gltf::DeclaredPlacement(Held_.File(), 0, placed, why);
  if (declared) { eye = placed; }
  Looking_.Eye = eye;
  // A DECLARED CAMERA IS NOT REFITTED. `Fill` frames a subject when nobody said where to stand; it
  // may not overrule a client that did. It did: the places declare a view AND a fill, and the
  // framing derived from the geometry's bounds replaced the declared eye -- carrying with it a near
  // plane taken from the scene radius, 1 904 878 m over a 388 km ring. Reverse-Z writes
  // `near / distance`, so every surface nearer than 1 905 km clamped to one depth and the depth
  // test stopped discriminating: distant towers drew and the buildings beside the camera did not.
  // Filament's `Camera` is authoritative and its `View` does not refit it; Unreal's is the same.
  if (!HaveEye_ && (Declared_.Fill > 0.0 || !declared)) {

    double least[3], most[3];
    const auto boundedFrom = std::chrono::steady_clock::now();
    Shaped_.BoundsOf(Joined_, least, most);
    BoundsMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - boundedFrom)
            .count();
    for (int frame = 1; frame < Held_.Frames(); ++frame) {
      if (!Pose(frame, error)) { return false; }
      double posedLeast[3], posedMost[3];
      Shaped_.BoundsOf(Joined_, posedLeast, posedMost);
      for (int axis = 0; axis < 3; ++axis) {
        least[axis] = posedLeast[axis] < least[axis] ? posedLeast[axis] : least[axis];
        most[axis] = posedMost[axis] > most[axis] ? posedMost[axis] : most[axis];
      }
    }
    if (Held_.Frames() > 1 && !Pose(0, error)) { return false; }
    Gltf::Viewpoint fitted;
    if (!Gltf::FramingFor(least, most, fitted, Framing())) {
      error = "the subject has no extent over its own grid, so no camera can be derived from it";
      return false;
    }
    eye = fitted;
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

// PRESENTING IS THE HALF OF A FRAME THAT REACHES A SCREEN, and headless has no such half. A
// bracket that ends without a window has still ended a frame -- the picture is drawn and readable
// -- so this answers TRUE rather than refusing, and `Presents` is where a client asks which it is.
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

bool Live::Carries(size_t bodies, std::string &error) {
  if (bodies == 0) {
    error = "a picture was asked to carry no bodies at all, and that is a different statement "
            "from carrying one that has not moved";
    return false;
  }
  const size_t stood = Stood_.Instances();
  if (!Stood_.Carries(bodies)) {
    error = "the subject proxy stands over nothing, so it cannot carry " +
            std::to_string(bodies) + " bodies";
    return false;
  }
  if (stood != bodies && Stoodup_) {
    Stoodup_ = false;
    if (!Submit(error)) { return false; }
  }
  if (SentBody_.size() != bodies) {
    std::array<double, 16> unsent;
    unsent.fill(std::numeric_limits<double>::quiet_NaN());
    SentBody_.resize(bodies, unsent);
  }
  return true;
}

bool Live::Carry(const double worldFromBodyM[16], const double built[16], std::string &error) {
  return Carry(0, worldFromBodyM, built, error);
}

bool Live::Carry(size_t body, const double worldFromBodyM[16], const double built[16],
                 std::string &error) {
  const double perUnit = Declared_.MetresPerUnit > 0.0 ? Declared_.MetresPerUnit : 1.0;
  double bodyM[16];
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      bodyM[column * 4 + row] =
          column < 3 ? worldFromBodyM[column * 4 + row] * perUnit : worldFromBodyM[column * 4 + row];
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
    std::array<double, 16> unsent;
    unsent.fill(std::numeric_limits<double>::quiet_NaN());
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
  bool bodyMoved = false, builtMoved = false;
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
  Renderer_->CastsBelow((uint32_t)Joined_);

  double ecef[16];
  if (bodyMoved && joined > 0) {
    for (int at = 0; at < 16; ++at) { ecef[at] = bodyM[at]; }
    if (!Render::MovedInstance(*Renderer_, rows, instances, body, 0, joined, ecef, error)) {
      return false;
    }
  }
  if (builtMoved && joined < parts) {
    for (int at = 0; at < 16; ++at) { ecef[at] = built[at]; }
    if (!Render::MovedInstance(*Renderer_, rows, instances, body, joined, parts, ecef, error)) {
      return false;
    }
  }
  for (int at = 0; at < 16; ++at) {
    SentBody_[body][at] = bodyM[at];
    SentBuilt_[at] = built[at];
  }
  return true;
}

bool Live::Restands(std::string stands, std::string variant, AssetAnimation animation, int clip,
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

bool Live::Restand(outshine::Geometry &&built, size_t carried, const Material &wearing,
                   std::string &error) {
  Aimed_ = false;
  const std::vector<Material> wore = std::move(Declared_.Surfacing);
  Declared_.Surfacing.assign(1u, wearing);
  Declared_.Built = nullptr;
  Held_.Carries(std::move(built));
  Stoodup_ = false;
  Carrying_ = carried;
  auto phaseAt = std::chrono::steady_clock::now();
  const bool stood = Build(error);
  BuildMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
                 .count();
  StandMs_ = 0.0;
  SubmitMs_ = 0.0;
  Carrying_ = 0;
  Declared_.Surfacing = wore;
  return stood;
}

bool Live::Restand(const Gltf::Subject &built, size_t carried, const Material &wearing,
                   std::string &error) {
  Aimed_ = false;
  const std::vector<Material> wore = std::move(Declared_.Surfacing);
  Declared_.Surfacing.assign(1u, wearing);
  Declared_.Built = &built;
  Stoodup_ = false;
  Carrying_ = carried;
  // THREE STEPS UNDER ONE NAME, and the rebuild's clock could not tell them apart: BUILD walks the
  // subject into the proxy's own arrays, STAND settles the placements and lights, SUBMIT hands the
  // streams to the device. 25 of Shibuya's 40 seconds are spent here and the phase that holds them
  // has to be nameable before it can be answered.
  auto phaseAt = std::chrono::steady_clock::now();
  const auto since = [&phaseAt]() {
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
  // `Build` HAS ALREADY STOOD AND SUBMITTED, and doing it again here cost a third of the rebuild.
  // The tail that stood here read
  //
  //     Joined_ = carried;  if (!Stand(error)) return false;  return Submit(error);
  //
  // and neither line changed anything: `Build` sets `Joined_ = Carrying_` itself and `Carrying_` is
  // `carried` when it runs, and restoring `Declared_.Surfacing` above does not re-resolve the
  // surface table, so the second pass ran on the same state as the first. It was also the LESSER
  // pass -- `Build`'s own tail sets the picture region and lays the surface, which this never did.
  //
  // MEASURED on Shibuya before removing it: standing and submitting inside `Build` 13 844 ms, then
  // 773 + 8 265 ms doing it again, out of a 24 163 ms hand-over.
  Joined_ = carried;
  return true;
}

size_t Live::TookPosing_ = 0, Live::TookSubmitting_ = 0, Live::TookAiming_ = 0, Live::TookDrawing_ = 0;
size_t Live::AssetReads_ = 0;
size_t Live::PlanInits_ = 0;

bool Live::Advance(std::string &error) {
  const Heap::Tagged advancing("live-advance");
  const auto took = [](const char *tag, size_t before) { return Heap::TakenUnder(tag) - before; };

  if (Held_.Moves() && Held_.Frames() > 1) {
    Held_.Advances(Held_.Frames());
    const size_t beforePose = Heap::TakenUnder("live-pose");
    {
      const Heap::Tagged posing("live-pose");
      if (!Pose(Held_.At(), error)) { return false; }
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
  const size_t beforeDraw = Heap::TakenUnder("render-frame");
  {
    const Heap::Tagged drawing("render-frame");
    Renderer_->RenderFrame();
  }
  TookDrawing_ = Heap::TakenUnder("render-frame") - beforeDraw;
  return true;
}



}
