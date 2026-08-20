#ifndef TERRAINLOADER_H
#define TERRAINLOADER_H
#include <stdint.h>

#include "GroundSample.h"
#include "TilePool.h"

namespace outshine::Data {
class SourceSet;
class Transport;
}

struct FbGroundSurface { int Z; int Grid; };

int  fb_stream_open(outshine::Data::SourceSet &sources, outshine::Data::Transport &transport,
                    double lat, double lon, FbGroundSurface surface);
void fb_stream_close(void);

outshine::World::TilePool *fb_tile_pool(void);

outshine::GroundSample fb_stream_ground(double lat, double lon);

double fb_stream_ground_post_m(double latDeg);

class FbGroundBlock;

FbGroundBlock fb_stream_ground_block(int z, long x, long y);

class FbGroundBlock {
public:
  enum class State { Resolved, Pending, Missing };

  [[nodiscard]] State Where() const noexcept { return Where_; }

  void AslMRow(double latDeg, double lonFromDeg, double lonStepDeg, int count,
               double *out) const noexcept;

private:
  friend FbGroundBlock fb_stream_ground_block(int z, long x, long y);

  const float *Nodes_ = nullptr;
  long X_ = 0, Y_ = 0;
  int Zoom_ = 0, Side_ = 0;
  uint32_t Postings_ = 0;
  State Where_ = State::Missing;
};

int  fb_load_image_file(const char *path, uint8_t **rgba, int *w, int *h);

struct FbStarBands {
  enum class State { Pending, Complete };
  State Where;
  int Bytes;
};
FbStarBands fb_fetch_stars(uint8_t *dst, int cap);

#endif
