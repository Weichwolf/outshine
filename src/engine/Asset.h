#ifndef OUTSHINE_ENGINE_ASSET_H
#define OUTSHINE_ENGINE_ASSET_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <import/GltfImporter.h>
#include <scene/Geometry.h>
#include <scenario/Scenario.h>

#include "Viewing.h"

namespace outshine::Core {

class Posed {
public:
  void Clears();

  struct Sited {
    std::string Path;
    std::string Variant;
  };

  struct Playing {
    int Clip = 0;
    double Fps = 0.0;
  };

  [[nodiscard]] bool
  Reads(const Sited &asset, Scenario::AssetAnimation animation, Playing at, std::string &error);
  [[nodiscard]] bool Poses(double seconds, std::string &error);

  void Carries(outshine::Geometry &&built);

  [[nodiscard]] bool Appends(Posed &&more, std::string &error);

  [[nodiscard]] uint64_t Changed() const { return Changed_; }

  [[nodiscard]] bool HoldsBuilt() const { return HoldsBuilt_; }

  [[nodiscard]] const outshine::Geometry &Built() const { return Built_; }

  [[nodiscard]] const std::optional<Render::Viewpoint> &Camera() const { return Camera_; }

  [[nodiscard]] bool Measures(double seconds, std::string &error);

  [[nodiscard]] bool Moves() const { return Moves_; }

  [[nodiscard]] double LocalsDigest() const { return LocalsDigest_; }

  [[nodiscard]] double AssembledDigest() const { return AssembledDigest_; }

  [[nodiscard]] bool Stands() const { return Read_; }

  [[nodiscard]] int Frames() const { return Frames_; }

  [[nodiscard]] double AtS() const { return AtS_; }

  [[nodiscard]] double DurationS() const { return DurationS_; }

  void Advances(double stepS, bool loops) {
    const double end = DurationS_;
    const double next = AtS_ + stepS;
    if (!(end > 0.0)) {
      AtS_ = 0.0;
      return;
    }
    AtS_ = loops ? next - end * std::floor(next / end) : std::min(next, end);
  }

private:
  struct Asset {
    std::unique_ptr<GltfImporter> Importer;
    outshine::Geometry Snapshot;
  };

  std::vector<Asset> Assets_;
  std::optional<Render::Viewpoint> Camera_;
  outshine::Geometry Built_;
  bool HoldsBuilt_ = false;
  uint64_t Changed_ = 0;
  [[nodiscard]] bool PoseInto(double seconds, std::string &error);
  [[nodiscard]] bool Rebuild(std::span<const outshine::Geometry> snapshots, std::string &error);
  void RefreshCamera();
  void RefreshDigests();
  bool Moves_ = false;
  double DurationS_ = 0.0;
  double LocalsDigest_ = 0.0;
  double AssembledDigest_ = 0.0;
  bool Read_ = false;
  int Frames_ = 1;
  double AtS_ = 0.0;
};

}
#endif
