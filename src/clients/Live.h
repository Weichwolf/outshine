/* A SCENARIO STOOD UP AGAINST A RENDERER, AND TORN DOWN WITH WHAT IT PUT THERE (board:1455).
 *
 * **A CONSUMER NAMES A DECLARATION AND GETS A RUNNING WORLD.** `Open` reads the document, builds the
 * subject, resolves its surfaces, takes its lights, derives the plan the subject needs, derives or
 * accepts the camera, lays out every declared surface and hands the whole frame to the renderer --
 * ONCE. `Advance` then draws frames and touches the device only where something MOVED. **Standing a
 * second scenario up is one assignment**, and the previous one's geometry, picture region and
 * overlay leave with it.
 *
 * **THE CONSUMER SETS UP A SCENARIO AND DOES NOTHING ELSE.** A viewer and a test case are suppliers:
 * they name a file, a rectangle and what is written over it. Reading glTF, posing it, deciding which
 * targets the plan owes, framing the subject and turning markup into quads are all the engine's,
 * because every client that ever loads a document needs all five and none of them is that client's
 * business.
 *
 * **THE COST THIS SHAPE EXISTS TO REMOVE IS MEASURED AND IT WAS NOT SMALL.** A consumer restating its
 * studio every frame paid 124.4 ms p50 for `WaterBottle` and 1174.5 ms for `ABeautifulGame` while the
 * frame itself cost 0.000 ms -- 0.85 frames a second for a body that is not moving. `Aim`'s own
 * header had already written the rule down: the geometry is set up once and the eye moves every frame.
 *
 * **AN ANIMATED SUBJECT IS THE ONE THING THAT DOES RESUBMIT, and it resubmits because it MOVED.** The
 * scenario owns its document, its pose and its geometry, so advancing a frame is the engine's work
 * and not a question put back to the consumer -- which is what lets a client select a case and then
 * stop talking to the library.
 *
 * **THE CONSUMER'S CALLBACK IS THE CONSUMER'S.** `Under` answers what a point hit and what that
 * element declared, exactly as `Ui::Pointer` does; nothing here dispatches and a scenario never calls
 * into a client. A client that wants a different world declares a different one. */
#ifndef LIVE_H
#define LIVE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Document.h"
#include "GltfStudio.h"
#include "Layout.h"
#include "Markup.h"
#include "Paint.h"
#include "Pointer.h"
#include "Pose.h"
#include "Renderer.h"
#include "Style.h"
#include "Subject.h"
#include "Surfaces.h"

namespace outshine::Clients {

/* ONE DECLARED SURFACE: markup, style, and the rectangle of the frame it occupies IN FRACTIONS -- a
 * surface that named pixels would be a different surface on every device, and the picture is a
 * function of the declaration and not of the machine. */
struct Shows {
  std::string Markup;
  /* Style beyond what the markup carries in its own `<style>`, read under it -- a linked sheet, a
   * host's theme. The user-agent sheet is read first by the runtime and is not declared here. */
  std::string Style;
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;
};

/* WHAT A SCENARIO IS, AND EVERY FIELD IS SOMETHING ONLY THE CONSUMER KNOWS. Nothing here is derivable
 * from the document, and everything derivable from it is absent on purpose. */
struct Declaration {
  /* The host's surface in pixels -- a window, a swapchain, a texture. Everything else is a fraction
   * of it. */
  int SurfaceWidthPx = 0, SurfaceHeightPx = 0;

  /* **WHAT STANDS, NAMED AS A FILE.** The scenario reads it, poses it and owns it, because a consumer
   * that handed over finished vertices could not have them animated without being asked a question
   * every frame. Empty is a scenario with no body, which is what a document or a program is. */
  std::string Stands;
  /* Which glTF material variant the subject wears, by the name the document declares it under.
   * Empty is the file's own materials, which is what a document with no variants has. */
  std::string Variant;
  /* The grid an animated subject is sampled on. The engine reads the file's own animations and their
   * length; this says how finely, and 60 is the frame budget's own rate [SET, CLAUDE.md]. */
  double Fps = 60.0;

  /* HOW MUCH OF ITS REGION THE SUBJECT FILLS. Above zero derives the camera from the subject's own
   * bounds; zero keeps the one the file declares, which is what a runner comparing against an oracle
   * asks for. */
  double Fill = 0.0;
  /* **HOW FAR THE EYE TRAVELS AROUND THE SUBJECT EACH FRAME, in degrees about the world's up axis.**
   * Zero is a standing camera. It is the smallest declaration that puts a run under a MOVING one, and
   * it moves the eye alone: the body is submitted once and `Aim` carries the camera, which is the
   * separation `Clients::Aim` exists for. A run whose frame cost matches a still one's is that
   * separation holding; a run whose cost is higher is something being rebuilt that nobody asked for. */
  double OrbitDegPerFrame = 0.0;
  /* WHERE THE PICTURE GOES, in fractions of the surface. All zero is the whole of it. */
  double PictureLeftFrac = 0.0, PictureTopFrac = 0.0;
  double PictureWidthFrac = 0.0, PictureHeightFrac = 0.0;

  /* **HOW IT IS LIT WHERE THE DOCUMENT DECLARES NOTHING, and most do not.** A glTF carrying no
   * `KHR_lights_punctual` is not an unlit asset; it is one whose author expected an environment, so a
   * scenario that took the file's word for it would draw a black body and be exactly right about a
   * picture nobody can see. **The key light is DECLARED and not derived from an ephemeris**: a still
   * life judges FORM, and a civil time here would be a number nobody chose.
   *
   * `KeyLux` at zero declares no key light; `Environment` is a constant radiance from every
   * direction, scene-referred, and zero declares none. A document that DOES carry lights keeps them
   * and these are added to what it declares. */
  double Environment[3] = {0.0, 0.0, 0.0};
  double KeyLux = 0.0;
  double KeyElevationDeg = 0.0, KeyBearingDeg = 0.0;

  /* WHETHER A HOST WILL PRESENT THE FRAME INTO A SURFACE IT ACQUIRED. False is the runner's arm --
   * the frame is read back off `FrameTex` and no surface exists to draw into. */
  bool Presents = false;
  /* WHAT IS WRITTEN OVER IT, in front-to-back declaration order. */
  std::vector<Shows> Surfaces;
  /* The glyph sheet every declared surface draws from, RGBA8 and tightly packed. */
  const uint8_t *AtlasRgba = nullptr;
  int AtlasWidthPx = 0, AtlasHeightPx = 0;
};

class Live {
public:
  ~Live();
  Live(const Live &) = delete;
  Live &operator=(const Live &) = delete;

  /* Stands the declaration up. `out` is REPLACED, so the scenario it held is destroyed before this
   * one is built -- which is what makes *select another case* a single statement with no way to leave
   * the previous body in the frame. */
  /* `font` IS NULL WHERE NOTHING IS WRITTEN, and that is a scenario rather than an omission: a run
   * over a moving camera carries no interface, and requiring a face for it would make every consumer
   * carry a glyph sheet to render a body. A surface declared with no face to set it in is REFUSED and
   * says so, which is the case a default face would have drawn wrong and silently. */
  [[nodiscard]] static bool Open(Render::Renderer &renderer, Declaration declaration,
                                 const Ui::Font *font, std::unique_ptr<Live> &out,
                                 std::string &error);

  /* **THE SURFACES SAY SOMETHING ELSE, and nothing else about the world changed.** A menu that
   * scrolled, a status line that moved, a page that turned: the body, the plan and every image on the
   * device are what they were, so this lays the markup out again and hands over new rectangles. It is
   * the cheap half of `Open` and it is separate because standing a scenario up rebuilds pipelines. */
  [[nodiscard]] bool Redeclare(std::vector<Shows> surfaces, std::string &error);

  /* ONE FRAME. A still scenario submits nothing and draws; an animated one poses, resubmits and
   * draws. Either way the consumer says nothing about what is in the world. */
  [[nodiscard]] bool Advance(std::string &error);

  /* WHAT IS UNDER A POINT ON THE SURFACE, in the surface's own pixels, searched over the declared
   * surfaces from the front. `Node` is -1 where the point hit no surface. */
  [[nodiscard]] Ui::Touched Under(double xPx, double yPx) const;

  /* **WHAT EACH PHASE OF AN ADVANCE TOOK FROM THE ENGINE'S ALLOCATOR** (board:1463), as a signed
   * difference wrapped into a size_t: a phase that returned more than it took reads as a huge number,
   * which is exactly the sign an instrument wants to see rather than a clamp that hides it. Static
   * because it is a diagnostic and not scenario state -- there is one frame in the engine at a time. */
  static size_t TookPosing_, TookSubmitting_, TookAiming_, TookDrawing_;
  [[nodiscard]] static size_t TookPosing(void) { return TookPosing_; }
  [[nodiscard]] static size_t TookSubmitting(void) { return TookSubmitting_; }
  [[nodiscard]] static size_t TookAiming(void) { return TookAiming_; }
  [[nodiscard]] static size_t TookDrawing(void) { return TookDrawing_; }

  /* Which frame of its own grid it is showing, and how many it has -- one for a still. */
  [[nodiscard]] int At(void) const { return At_; }
  [[nodiscard]] int Frames(void) const { return Frames_; }

private:
  Live(Render::Renderer &renderer, Declaration declaration, const Ui::Font *font);
  [[nodiscard]] bool Build(std::string &error);
  [[nodiscard]] double Framing(void) const;
  [[nodiscard]] bool Pose(int frame, std::string &error);
  [[nodiscard]] bool Look(std::string &error);
  [[nodiscard]] bool Stand(std::string &error);
  [[nodiscard]] bool Submit(std::string &error);
  [[nodiscard]] bool Compose(std::string &error);

  /* ONE SURFACE, LAID OUT AND PAINTED, with the origin its quads were offset to -- so a hit test
   * subtracts it rather than laying the surface out a second time. */
  struct Laid {
    Ui::Markup Tree;
    Ui::Stylesheet Sheet;
    Ui::Layout Placed;
    Ui::Painting Painted;
    double LeftPx = 0.0, TopPx = 0.0;
  };

  Render::Renderer *Renderer_ = nullptr;
  const Ui::Font *Font_ = nullptr;
  Declaration Declared_;
  std::shared_ptr<const Render::RenderPlan> Plan_;
  Gltf::Document File_;
  Gltf::Subject Geometry_, Previous_;
  Gltf::Pose Motion_;
  Gltf::VariantSelection Variant_;
  std::vector<Gltf::Transform> Locals_;
  std::vector<double> Weights_;
  SurfaceTable Table_;
  Studio Stood_;
  StudioScratch Scratch_;
  std::vector<Laid> Laid_;
  std::vector<Render::OverlayQuad> Quads_;
  bool Moves_ = false;
  /* WHETHER THE SUBJECT'S TOPOLOGY HAS CROSSED, so the first submission stands it up and every later
   * one moves it (board:1464). */
  bool Stoodup_ = false;
  int Frames_ = 1;
  int At_ = 0;
  /* THE EYE'S ANGLE AROUND THE SUBJECT, accumulated in degrees so a long run cannot drift by
   * repeated rotation of a basis -- the placement is derived from the angle every frame and never
   * from the previous placement. */
  double Around_ = 0.0;
};

} // namespace outshine::Clients
#endif
