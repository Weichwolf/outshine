/* THE ONE ENTRY POINT EVERY RENDER CORPUS'S HARNESS CALLS (board:1196). The scoring instrument is
 * shared because a case is decided the same way whoever authored the asset; the harness is not,
 * because a corpus is a suite and a suite is a folder. */
#ifndef SHARED_RENDER_RENDERCASE_H
#define SHARED_RENDER_RENDERCASE_H

#include <memory>
#include <string>

namespace outshine::Render {
class Renderer;
}

[[nodiscard]] int ScoreRenderCase(int argc, char **argv);

/* A CASE CONFIGURED FOR OUTSHINE, WITH NOTHING SCORED (board:1443).
 *
 * **THE RENDERER IS THE LIBRARY AND NEITHER HOST RENDERS.** The runner and the viewer under
 * `test/viewer/` both read a manifest and configure outshine from it; what they do afterwards
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
  /* The renderer brought up on this case's own plan and its own frame size. */
  [[nodiscard]] bool Start(outshine::Render::Renderer &renderer, std::string &error);
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
