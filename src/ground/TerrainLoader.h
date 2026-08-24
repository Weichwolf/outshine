#ifndef OUTSHINE_GROUND_TERRAINLOADER_H
#define OUTSHINE_GROUND_TERRAINLOADER_H
#include <stdint.h>

#include <memory>
#include <vector>

#include "GroundQuery.h"
#include "GroundSample.h"
#include "TilePool.h"

namespace outshine::Data {
class SourceSet;
class Transport;
}

namespace outshine::Ground {

struct GroundSurface { int Z; int Grid; };

class TerrainField;
class TerrainTiles;

void FillNodeHeights(const TerrainField &field, uint32_t rowPostings, uint32_t colPostings,
                     int nodes, std::vector<float> *out);
[[nodiscard]] double TileHeightAslM(const float *nodes, int side, uint32_t postings,
                                    double fx, double fy);

class GroundBlock {
public:
  enum class State { Resolved, Pending, Missing };

  [[nodiscard]] State Where() const noexcept { return Where_; }

  void AslMRow(double latDeg, double lonFromDeg, double lonStepDeg, int count,
               double *out) const noexcept;

private:
  friend class GroundStream;

  const float *Nodes_ = nullptr;
  long X_ = 0, Y_ = 0;
  int Zoom_ = 0, Side_ = 0;
  uint32_t Postings_ = 0;
  State Where_ = State::Missing;
};

class GroundStream final : public GroundQuery {
public:
  GroundStream(TilePool &tiles, GroundSurface surface);
  ~GroundStream();
  GroundStream(const GroundStream &) = delete;
  GroundStream &operator=(const GroundStream &) = delete;

  [[nodiscard]] GroundSample At(double lat, double lon) const override;

  [[nodiscard]] double PostM(double latDeg) const override;

  [[nodiscard]] GroundBlock BlockAt(int z, long x, long y) const;

  [[nodiscard]] TilePool &Tiles() { return Tiles_; }

private:
  struct Held;
  friend struct Held;

  const struct Tile *TileAt(long x, long y) const;

  TilePool &Tiles_;
  GroundSurface Surface_;
  std::unique_ptr<Held> Held_;
};

[[nodiscard]] TilePool::Config GroundPoolConfig(double lat, double lon, int workers = 0);

}

#endif
