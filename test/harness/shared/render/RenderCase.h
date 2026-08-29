#ifndef SHARED_RENDER_RENDERCASE_H
#define SHARED_RENDER_RENDERCASE_H

#include <memory>
#include <string>
#include <vector>

#include "RenderCatalogue.h"

namespace outshine::Render {
class SceneRenderer;
}

[[nodiscard]] int ScoreRenderCase(int argc, char **argv);

class ConfiguredCase {
public:
  ConfiguredCase();
  ~ConfiguredCase();
  ConfiguredCase(const ConfiguredCase &) = delete;
  ConfiguredCase &operator=(const ConfiguredCase &) = delete;

  [[nodiscard]] bool Read(const std::string &directory, std::string &error);

  [[nodiscard]] bool Start(outshine::Render::SceneRenderer &renderer, std::string &error,
                           const std::vector<outshine::Render::Stage> &alsoContent = {},
                           int surfaceW = 0, int surfaceH = 0);

  [[nodiscard]] bool FrameToFill(double fill, std::string &error);

  [[nodiscard]] bool PoseAt(int frame, std::string &error);

  [[nodiscard]] bool Draw(outshine::Render::SceneRenderer &renderer, std::string &error);

  [[nodiscard]] int Frames(void) const;
  [[nodiscard]] double Fps(void) const;
  [[nodiscard]] int WidthPx(void) const;
  [[nodiscard]] int HeightPx(void) const;
  [[nodiscard]] const std::string &Title(void) const;

  [[nodiscard]] bool Declines(void) const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

#endif
