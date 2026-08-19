#include "Live.h"

#include <cmath>

#include "Heap.h"

#include "Framing.h"

namespace outshine::Clients {
namespace {

/* THE UI'S RECTANGLE AND THE RENDERER'S, AND THIS IS THE ONLY PLACE THE TWO MEET. A box, a glyph and
 * a page are the UI's vocabulary and no content noun has a spelling in the renderer, so somebody has
 * to translate -- and one place is what keeps a shift from disagreeing with itself. */
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

/* WHAT THE SUBJECT'S OWN DOCUMENT OWES THE PLAN, asked of the file and of nothing else. A declaration
 * that could disagree with the document about whether it carries glass or moves would be a second
 * answer to a question the file already settles. */
void DeclarePlan(const Gltf::Document &file, bool moves, bool presents,
                 Render::PlanSpec &declaration) {
  declaration.Outputs = {Render::Resource::FrameTex};
  if (presents) { declaration.Outputs.push_back(Render::Resource::Surface); }
  /* THE MOTION TARGET IS REQUESTED BY A SEQUENCE AND BY NOTHING ELSE (board:1169): the plan prunes a
   * target nothing reads, so a still subject's pipelines and shader text are exactly what they were. */
  if (moves) { declaration.Outputs.push_back(Render::Resource::SceneVelocity); }
  declaration.Content = {Render::Stage::Subjects, Render::Stage::Overlay};
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
  /* **THE EXPOSURE IS DECLARED AND NOT METERED, because the picture is a function of the declaration.**
   * A metered one reads an irradiance buffer that a scenario with no sky never fills, so the frame
   * would be decided by a chain nobody declared -- and the same scenario would look different the
   * moment a sky was added for an unrelated reason. */
  declaration.Display = Render::Declared<Render::Transfer>(Render::Transfer::Filmic);
  declaration.Exposure = Render::Declared<float>(1.0f);
}

} // namespace

Live::Live(Render::Renderer &renderer, Declaration declaration, const Ui::Font *font)
    : Renderer_(&renderer), Font_(font), Declared_(std::move(declaration)) {}

Live::~Live() {
  if (Renderer_ == nullptr) { return; }
  /* **WHAT THIS SCENARIO PUT IN THE RENDERER LEAVES WITH IT.** An empty mesh is a subject with no
   * index run, which draws nothing; the picture region goes back to *the whole surface*, which is
   * what a scenario with no body means. Without this the last body drawn stays in the frame behind
   * whatever is shown next, because a mesh outlives the thing that set it. */
  std::string ignored;
  (void)Renderer_->SetSubjectMesh(Render::SubjectMesh{}, ignored);
  (void)Renderer_->SetOverlay(nullptr, 0, ignored);
  Renderer_->SetPictureRegion(0, 0, 0, 0, 0);
}

bool Live::Open(Render::Renderer &renderer, Declaration declaration, const Ui::Font *font,
                std::unique_ptr<Live> &out, std::string &error) {
  /* THE PREVIOUS SCENARIO IS DESTROYED BEFORE THIS ONE IS BUILT, which is what makes the transition
   * a single statement rather than a teardown a consumer has to remember. */
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
  if (!Declared_.Stands.empty()) {
    if (!Declared_.Variant.empty()) { Variant_ = Gltf::VariantSelection(Declared_.Variant); }
    if (!File_.ReadFile(Declared_.Stands)) {
      error = File_.Error();
      return false;
    }
    /* EVERY ANIMATION THE FILE DECLARES DRIVES IT, because a file that carries motion carries it to
     * be seen. A consumer picking a subset would be answering a question about the document. */
    if (!File_.Animations().empty()) {
      std::vector<int> all((size_t)File_.Animations().size());
      for (size_t at = 0; at < all.size(); ++at) { all[at] = (int)at; }
      if (!Gltf::Pose::Build(File_, Span<const int>(all.data(), all.size()), Motion_, error)) {
        return false;
      }
      Moves_ = Motion_.EndS() > 0.0;
      Frames_ = Moves_ ? (int)(Motion_.EndS() * Declared_.Fps) + 1 : 1;
    }
    if (!Pose(0, error)) { return false; }
    ResolveSurfaceTable(File_, Geometry_, true, true, Table_);
    if (!ResolveFileSurface(File_, Geometry_, ColourFrom::Row, ColourCarrier::Texture, Table_,
                            error)) {
      return false;
    }
  }

  Render::PlanSpec declaration;
  DeclarePlan(File_, Moves_, Declared_.Presents, declaration);
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

  /* A CAMERA EXISTS EVEN WHERE NOTHING STANDS, because a plan carrying a geometry stage will not
   * render a frame without one -- and that is right: a stage that PLACES things needs to know from
   * where. A scenario with no body places nothing and the basis is still the honest thing to hand
   * over, rather than a special case in the library for a plan that happens to be empty. */
  const double eye[3] = {0.0, 0.0, 0.0}, forward[3] = {0.0, 0.0, -1.0};
  const double right[3] = {1.0, 0.0, 0.0}, up[3] = {0.0, 1.0, 0.0};
  Renderer_->SetCameraBasis(eye, forward, right, up);

  if (Geometry_.TriangleCount() > 0) {
    /* THE PICTURE FILLS THE REGION IT WAS DECLARED IN and the camera is derived for that shape, so
     * no aspect is imposed on top: a scenario that letterboxed itself inside its own rectangle would
     * be answering a question the consumer already answered by naming the rectangle. */
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
    /* **AT THE FIRST POSE THE PREVIOUS ONE IS THIS ONE, because nothing has moved yet.** The velocity
     * target needs a pose to difference against and there is no earlier frame to take it from; a
     * subject differenced against an EMPTY one is what the studio refuses, and rightly -- no vertex
     * would have a place it moved from. Capturing before the build on every later frame is what makes
     * the pair a motion. */
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

/* **THE EYE MOVES AND NOTHING ELSE DOES.** The framing rule answers where a camera stands to see this
 * body; the orbit turns that standpoint about the subject's own centre, and `Aim` carries it -- no
 * mesh, no material and no image is touched. The angle is ACCUMULATED and the basis DERIVED from it,
 * because rotating a basis by a small angle ninety times is ninety chances to drift. */
bool Live::Look(std::string &error) {
  Gltf::Placement framed;
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
  return Aim(*Renderer_, Geometry_, Stood_.Eye, error);
}

/* **THE STUDIO IS BUILT ONCE AND ONLY WHAT MOVED IS WRITTEN AGAIN** (board:1463). It used to be rebuilt
 * from an empty one on every advance -- every surface copied, every emitted radiance assigned, every
 * light pushed -- and [MEASURED] that made SUBMITTING the heaviest allocator in the frame: it moved the
 * engine's own heap on **223 of 250 frames**, against 15 for posing and 0 for aiming. What actually
 * changes between two frames of an animated subject is which body the pointers name, and that is two
 * stores.
 *
 * `CLAUDE.md` asks for exactly this and the shipped engines arrange it the same way: a frame path made
 * of bounded terms, with the allocation at load. */
bool Live::Stand(std::string &error) {
  Stood_ = Studio{};
  Stood_.Geometry = &Geometry_;
  if (Moves_) { Stood_.Previous = &Previous_; }
  Stood_.EmittedRadiance.assign(Geometry_.Parts().size(), {0.0f, 0.0f, 0.0f});
  Stood_.PartSurface = Table_.PartSlot;
  Stood_.Surfaces = Table_.Slots;
  /* THE FILE'S OWN LIGHTS, because a document that declares how it is lit has answered the question.
   * A file that declares none is drawn by whatever its rows emit, which is what an unlit asset is. */
  for (const Gltf::PlacedLight &placed : Geometry_.Lights()) {
    Stood_.Lights.push_back(placed.Light);
  }
  if (Declared_.KeyLux > 0.0) {
    /* THE BEAM, IN glTF's OWN FRAME -- right-handed, +Y up, and the light travels along its node's
     * -Z. Elevation is degrees above the horizon and bearing is degrees around +Y from -Z, so a key
     * at 30 deg and 40 deg stands high and to one side, which is where a key light stands. */
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

  /* **THE CAMERA IS THE SUBJECT'S OWN BOUNDS WHERE A FILL WAS DECLARED, and the document's where it
   * was not.** A runner comparing against an oracle must keep the shot the file states; a consumer
   * SHOWING a model wants to see it, and a model filling a tenth of the frame because its author
   * stood far back is a model nobody can look at. A file carrying no camera is framed either way,
   * because there is nothing else to take. */
  std::string why;
  const bool declared = !File_.Cameras().empty() &&
                        Gltf::DeclaredPlacement(File_, 0, Stood_.Eye, why);
  if (Declared_.Fill > 0.0 || !declared) {
    /* **THE BOUNDS THE CAMERA IS DERIVED FROM ARE THE GRID'S AND NOT THE REST POSE'S** (board:1433,
     * board:1463). A camera framed on frame 0 and held still is what a scenario wants -- a viewpoint
     * that jumped with every pose would be a camera nobody placed -- and a body that moves toward it
     * then walks INSIDE the near plane, which `Aim` refuses and rightly. [MEASURED] `BoxAnimated`
     * stopped advancing after 22 frames the moment the per-pose reframing was removed.
     *
     * The union over the whole grid is taken ONCE, at stand-up, where an allocation and a walk are
     * allowed to live. The eye and the aim stay the rest pose's; what opens is the depth window. */
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

/* **ONLY THE BODY CROSSES PER FRAME.** The surfaces and the lighting were handed over at stand-up and
 * nothing about them changes when a subject moves (board:1463). */
bool Live::Submit(std::string &error) { return Place(*Renderer_, Stood_, Scratch_, error); }

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

/* WHERE AN ADVANCE'S BYTES GO, published per phase so a cost has a cause (board:1463). It is four
 * relaxed loads a frame and it is read by an instrument, never by the frame. */
size_t Live::TookPosing_ = 0, Live::TookSubmitting_ = 0, Live::TookAiming_ = 0, Live::TookDrawing_ = 0;

bool Live::Advance(std::string &error) {
  const auto took = [](size_t before) { return Heap::LiveBytes() - before; };
  /* **A STILL SUBJECT SUBMITS NOTHING**, which is the whole of this class: the device already holds
   * every vertex, index, material and image, and nothing about them changed. */
  if (Moves_ && Frames_ > 1) {
    At_ = (At_ + 1) % Frames_;
    const size_t beforePose = Heap::LiveBytes();
    if (!Pose(At_, error)) { return false; }
    const size_t beforeSubmit = Heap::LiveBytes();
    TookPosing_ = took(beforePose);
    if (!Submit(error)) { return false; }
    TookSubmitting_ = took(beforeSubmit);
  }
  /* **AN ORBIT MOVES THE EYE AND NEVER THE BODY**, so it costs an aim and not a submission. It runs
   * after the pose because a posed subject's bounds are this frame's, and a camera framed against the
   * previous frame's would lag the body it is following by one. */
  if (Declared_.OrbitDegPerFrame != 0.0 && Geometry_.TriangleCount() > 0) {
    Around_ += Declared_.OrbitDegPerFrame;
    const size_t beforeAim = Heap::LiveBytes();
    if (!Look(error)) { return false; }
    TookAiming_ = took(beforeAim);
  }
  const size_t beforeDraw = Heap::LiveBytes();
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

} // namespace outshine::Clients
