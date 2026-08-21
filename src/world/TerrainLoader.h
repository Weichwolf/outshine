#ifndef TERRAINLOADER_H
#define TERRAINLOADER_H
#include <stdint.h>

#include "GroundSample.h"
#include "TilePool.h"

namespace outshine::Data {
class SourceSet;
class Transport;
}

namespace outshine::World {

struct GroundSurface { int Z; int Grid; };

int  OpenGround(outshine::Data::SourceSet &sources, outshine::Data::Transport &transport,
                    double lat, double lon, GroundSurface surface);
void CloseGround(void);

TilePool *GroundTiles(void);

GroundSample GroundAt(double lat, double lon);

double GroundPostM(double latDeg);

class GroundBlock;

GroundBlock GroundBlockAt(int z, long x, long y);

class GroundBlock {
public:
  enum class State { Resolved, Pending, Missing };

  [[nodiscard]] State Where() const noexcept { return Where_; }

  void AslMRow(double latDeg, double lonFromDeg, double lonStepDeg, int count,
               double *out) const noexcept;

private:
  friend GroundBlock GroundBlockAt(int z, long x, long y);

  const float *Nodes_ = nullptr;
  long X_ = 0, Y_ = 0;
  int Zoom_ = 0, Side_ = 0;
  uint32_t Postings_ = 0;
  State Where_ = State::Missing;
};


struct FetchedStars {
  enum class State { Pending, Complete };
  State Where;
  int Bytes;
};
FetchedStars FetchStars(uint8_t *dst, int cap);

}

#endif
