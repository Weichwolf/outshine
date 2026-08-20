/* THE ONE ENTRY POINT EVERY RENDER CORPUS'S HARNESS CALLS (board:1196). The scoring instrument is
 * shared because a case is decided the same way whoever authored the asset; the harness is not,
 * because a corpus is a suite and a suite is a folder. */
#ifndef SHARED_RENDER_RENDERCASE_H
#define SHARED_RENDER_RENDERCASE_H

#include <memory>
#include <string>
#include <vector>

#include "RenderCatalogue.h"

namespace outshine::Render {
class Renderer;
}

[[nodiscard]] int ScoreRenderCase(int argc, char **argv);

/* A CASE CONFIGURED FOR OUTSHINE, WITH NOTHING SCORED (board:1443).
 *
 * **THE RENDERER IS THE LIBRARY AND NEITHER HOST RENDERS.** The runner and the viewer under
 * `tools/viewer/` both read a manifest and configure outshine from it; what they do afterwards
 * differs entirely -- one scores a picture against an oracle and the other shows it in a window --
 * and the configuring is the part that must not be written twice. A second reading of the same
 * manifest would be a second answer to *what is this case*, and the two would drift on the first
 * field either of them learned.
 *
 * IT OWNS THE CASE because a `Studio` points into it: the subject's geometry, its decoded surface
 * images and its previous pose are all borrowed, so the thing that holds them has to outlive the
 * studio built from it. That is why this is a handle and not a struct of values.
 *
 * IT SCORES NOTHING AND READS NO ORACLE. `Prepare` is the runner's own entry, it refuses through
 * `CHECK`, and it belongs to the side that has a verdict to give. */
class ConfiguredCase {
public:
  ConfiguredCase();
  ~ConfiguredCase();
  ConfiguredCase(const ConfiguredCase &) = delete;
  ConfiguredCase &operator=(const ConfiguredCase &) = delete;

  /* The case's own directory, as prepared -- the manifest and the subject the manifest names. */
  [[nodiscard]] bool Read(const std::string &directory, std::string &error);
  /* The renderer brought up on this case's own plan and its own frame size.
   *
   * **`alsoContent` IS WHAT THE HOST WANTS IN THE SAME PICTURE, and it is the host's to declare**
   * (board:1447). A browser draws its own interface over the case it is showing; that is one plan with
   * one more content stage, not a second renderer and not a blit. The case decides what its SUBJECT
   * is and the host decides what else is in the frame -- which is the same division the plan already
   * makes between machinery and content. */
  /* `surfaceW` and `surfaceH` are the SURFACE the host owns, where it owns one bigger than the case:
   * a browser's window holds the case beside its lists, so the frame is the window's and the picture's
   * own rectangle is declared separately through `Renderer::SetPictureRegion`. Zero means *the case's
   * own frame*, which is what a runner reading a picture back asks for. */
  [[nodiscard]] bool Start(outshine::Render::Renderer &renderer, std::string &error,
                           const std::vector<outshine::Render::Stage> &alsoContent = {},
                           int surfaceW = 0, int surfaceH = 0);
  /* **THE CAMERA THE SUBJECT'S OWN BOUNDS GIVE, instead of the one the case declares.** A runner
   * comparing against an oracle must keep the case's shot; a BROWSER is showing a model, and a model
   * that fills a tenth of the frame because its author put the camera far away is a model nobody can
   * see. `fill` is how much of the shorter axis it spans. It is the host's call, taken after `Start`
   * and before `Draw`. */
  [[nodiscard]] bool FrameToFill(double fill, std::string &error);

  /* The subject at one frame of the case's declared grid; frame 0 for a still. */
  [[nodiscard]] bool PoseAt(int frame, std::string &error);
  /* One picture, into whatever surface the renderer's host declared. */
  [[nodiscard]] bool Draw(outshine::Render::Renderer &renderer, std::string &error);

  [[nodiscard]] int Frames(void) const;
  [[nodiscard]] double Fps(void) const;
  [[nodiscard]] int WidthPx(void) const;
  [[nodiscard]] int HeightPx(void) const;
  [[nodiscard]] const std::string &Title(void) const;
  /* WHETHER THIS CASE'S DECLARED VERDICT IS THAT THE ENGINE DECLINES IT (`board:1424`). `limits-probe`
   * is the criterion that says so, and a reader refusing such a subject is the case behaving as its own
   * manifest requires -- so a host announces it and does not count it a failure. Answerable after
   * `Read` has parsed the manifest, which happens before the subject is built and therefore before the
   * refusal. */
  [[nodiscard]] bool Declines(void) const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

#endif
