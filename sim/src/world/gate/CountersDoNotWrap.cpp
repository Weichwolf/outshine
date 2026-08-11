/* THE COUNTER THAT DIAGNOSES A STUCK LOAD, DRIVEN PAST 2^31 ON THE TARGET THAT BREAKS IT.
 *
 * `long` is 64 bits natively and 32 on wasm32, so an accumulator declared `long` behind a 64-bit
 * telemetry column wraps where the code actually ships and nowhere the oracle can see it — and a
 * signed overflow is undefined behaviour (ES.103), not a wrapped column. Every acceptance CSV in the
 * archive was written by the native oracle, so this gate exists because the run that would show the
 * defect is the one nobody takes.
 *
 * It is a RUN, not a syntax check: the value comes back out of the real TilePool through the real
 * Counters() copy and the real TelemetryRow, which is the whole chain the column travels. */
#include "TilePool.h"

#include <cstdint>
#include <cstdio>

#include "Telemetry.h"

/* The premise. On a target where a long is already 64 bits this gate proves nothing, and saying so
 * in the compiler is cheaper than saying it in a report. */
static_assert(sizeof(long) == 4, "counter gate is only evidence on wasm32");
static_assert(sizeof(outshine::World::TilePool::Ledger().Repeats) == 8, "Ledger::Repeats is not 64 bits");
static_assert(sizeof(outshine::World::TilePool::Ledger().Posts) == 8, "Ledger::Posts is not 64 bits");

int main() {
  outshine::World::TilePool::Config config;
  config.TilesBase = "http://localhost:8081";
  config.OriginLatDeg = 52.10602;
  config.OriginLonDeg = 9.43453;
  config.Threads = 1;
  config.ByteBudget = 8u << 20;
  config.DemCacheTiles = 2;
  outshine::World::TilePool pool(config);

  const long long past = ((long long)1 << 31) + 1024;
  /* One tile, asked for over and over: an ask that finds its job already under way is exactly what
   * Repeats counts, and it is the pool's fastest accumulator. */
  outshine::World::TileBuild build;
  long long repeats = 0, posts = 0;
  for (long long block = 0; repeats < past; block++) {
    for (int i = 0; i < 1000000; i++) pool.Mesh(14, 8621, 5404, 128, &build);
    const outshine::World::TilePool::Ledger ledger = pool.Counters();
    repeats = ledger.Repeats;
    posts = ledger.Posts;
    if (repeats < 0 || posts < 0) {
      std::printf("GATE FAIL negative after %lld blocks: posts=%lld repeats=%lld\n", block, posts,
                  repeats);
      return 1;
    }
  }

  outshine::TelemetryRow row;
  row.Push(posts);
  row.Push(repeats);
  std::printf("GATE poolPosts=%s poolRepeats=%s sizeofLong=%d\n", row.Fields()[0].c_str(),
              row.Fields()[1].c_str(), (int)sizeof(long));
  std::printf("GATE %s\n", repeats > (long long)1 << 31 ? "PASS" : "FAIL");
  return repeats > (long long)1 << 31 ? 0 : 1;
}
