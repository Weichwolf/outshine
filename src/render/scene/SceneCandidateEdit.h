#ifndef OUTSHINE_RENDER_SCENE_SCENECANDIDATEEDIT_H
#define OUTSHINE_RENDER_SCENE_SCENECANDIDATEEDIT_H

#include <cassert>
#include <cstddef>
#include <utility>

namespace outshine::Render {
class SceneRenderer;

// Renderer-thread only; the renderer must outlive every edit lease.
class SceneCandidateEdit {
public:
  SceneCandidateEdit(const SceneCandidateEdit &) = delete;
  SceneCandidateEdit &operator=(const SceneCandidateEdit &) = delete;

  SceneCandidateEdit(SceneCandidateEdit &&other) noexcept
      : Depth_(std::exchange(other.Depth_, nullptr)) {}

  SceneCandidateEdit &operator=(SceneCandidateEdit &&other) noexcept {
    if (this != &other) {
      Release();
      Depth_ = std::exchange(other.Depth_, nullptr);
    }
    return *this;
  }

  ~SceneCandidateEdit() { Release(); }

private:
  friend class SceneRenderer;

  explicit SceneCandidateEdit(size_t &depth) noexcept : Depth_(&depth) { ++*Depth_; }

  void Release() noexcept {
    if (Depth_ == nullptr) { return; }
    assert(*Depth_ > 0);
    --*Depth_;
    Depth_ = nullptr;
  }

  size_t *Depth_;
};

}
#endif
