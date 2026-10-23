#ifndef OUTSHINE_RENDER_SHADOW_CASTERCHANGES_H
#define OUTSHINE_RENDER_SHADOW_CASTERCHANGES_H

#include "math/Box.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace outshine::Render {
class CasterChanges {
public:
  struct Snapshot {
    uint64_t Revision = 0;
    Boxf Bounds;
    bool Complete = false;
  };

  void Include(const std::optional<Boxf> &before, const std::optional<Boxf> &after) noexcept {
    Advance();
    IncludeBounds(before);
    IncludeBounds(after);
  }

  void Unknown() noexcept {
    Advance();
    Unknown_ = true;
  }

  [[nodiscard]] Snapshot Since(uint64_t revision) const noexcept {
    if (!Exhausted_ && revision == Revision_) {
      return {.Revision = Revision_, .Bounds = {}, .Complete = true};
    }
    return {.Revision = Revision_,
            .Bounds = Bounds_,
            .Complete = !Unknown_ && revision >= First_ && revision < Revision_};
  }

  void Acknowledge(uint64_t revision) noexcept {
    if (revision != Revision_ || Exhausted_) { return; }
    First_ = revision;
    Bounds_ = {};
    Unknown_ = false;
  }

  [[nodiscard]] static std::optional<Boxf>
  SphereBounds(const std::array<float, 4> &sphere) noexcept {
    if (!(sphere[3] >= 0) || !std::isfinite(sphere[3])) { return std::nullopt; }
    Boxf bounds;
    for (size_t axis = 0; axis < 3; ++axis) {
      bounds.Min[axis] =
          std::nextafter(sphere[axis] - sphere[3], -std::numeric_limits<float>::infinity());
      bounds.Max[axis] =
          std::nextafter(sphere[axis] + sphere[3], std::numeric_limits<float>::infinity());
      if (!std::isfinite(bounds.Min[axis]) || !std::isfinite(bounds.Max[axis])) {
        return std::nullopt;
      }
    }
    return bounds;
  }

private:
  void IncludeBounds(const std::optional<Boxf> &box) noexcept {
    if (!box) { return; }
    bool valid = true;
    for (size_t axis = 0; axis < 3; ++axis) {
      valid = valid && std::isfinite(box->Min[axis]) && std::isfinite(box->Max[axis]) &&
              box->Min[axis] <= box->Max[axis];
    }
    if (valid) {
      Bounds_.Cover(*box);
    } else {
      Unknown_ = true;
    }
  }

  void Advance() noexcept {
    if (Revision_ == std::numeric_limits<uint64_t>::max()) {
      Unknown_ = Exhausted_ = true;
      return;
    }
    ++Revision_;
  }

  uint64_t Revision_ = 0, First_ = 0;
  Boxf Bounds_;
  bool Unknown_ = false, Exhausted_ = false;
};
}
#endif
