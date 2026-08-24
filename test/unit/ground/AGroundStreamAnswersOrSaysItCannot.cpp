#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

#include "ContentStore.h"
#include "SourceSet.h"
#include "TerrainLoader.h"
#include "TilePool.h"
#include "Transport.h"

using outshine::GroundSample;
using outshine::Data::ContentStore;
using outshine::Data::SourceSet;
using outshine::Data::Ticket;
using outshine::Data::Transport;
using outshine::Data::Wire;
using outshine::Ground::GroundPoolConfig;
using outshine::Ground::GroundStream;
using outshine::Ground::GroundSurface;
using outshine::Ground::TileHeightAslM;
using outshine::Ground::TilePool;

namespace {

// board:1806, reopened: this class is drawn in the CURRENT map and no source under test/ named
// it in CODE -- the claim that was meant to catch that found its own comment instead. It is
// the terrain streamer every ground query in the tree eventually reaches, and its contract has
// two halves: the arithmetic that turns four postings into a height, and what it answers when
// nothing has been fetched.
class Silent final : public Transport {
public:
  [[nodiscard]] Ticket Begin(const std::string &url) override {
    (void)url;
    ++Asked_;
    return Ticket{};
  }
  [[nodiscard]] Wire Collect(Ticket ticket) override {
    (void)ticket;
    return Wire::Unreachable();
  }
  void Cancel(Ticket ticket) override { (void)ticket; }
  [[nodiscard]] long Asked() const { return Asked_; }

private:
  long Asked_ = 0;
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // 1. the arithmetic, alone: TileHeightAslM is the bilinear sample every height query ends at.
  {
    constexpr int kSide = 2;
    constexpr uint32_t kPostings = 2;
    const float nodes[4] = {100.0f, 200.0f, 300.0f, 400.0f};
    Note("the four corner postings", 4.0, "postings");
    const double topLeft = TileHeightAslM(nodes, kSide, kPostings, 0.0, 0.0);
    const double topRight = TileHeightAslM(nodes, kSide, kPostings, 1.0, 0.0);
    const double bottomLeft = TileHeightAslM(nodes, kSide, kPostings, 0.0, 1.0);
    const double middle = TileHeightAslM(nodes, kSide, kPostings, 0.5, 0.5);
    std::printf("NOTE corners %.3f %.3f %.3f, centre %.3f\n", topLeft, topRight, bottomLeft,
                middle);
    CHECK_NEAR(topLeft, 100.0, 1.0e-9, "m", "a sample at a corner IS that corner's posting");
    CHECK_NEAR(middle, 250.0, 1.0e-9, "m",
               "**AND THE HEIGHT BETWEEN POSTINGS IS THE BILINEAR ONE**: the centre of four "
               "corners is their mean, which is what makes a terrain continuous across a tile "
               "rather than stepped at every posting (board:1806)");
    CHECK(topRight > topLeft && bottomLeft > topLeft,
          "and the sample follows the postings in both axes rather than one");

    // monotone along an edge: no overshoot, no ringing, between two postings.
    bool climbs = true;
    double before = TileHeightAslM(nodes, kSide, kPostings, 0.0, 0.0);
    for (int step = 1; step <= 20; ++step) {
      const double now = TileHeightAslM(nodes, kSide, kPostings, (double)step / 20.0, 0.0);
      climbs = climbs && now >= before - 1.0e-12;
      before = now;
    }
    CHECK(climbs,
          "and it is monotone between two postings, so a slope reconstructed from real data "
          "carries no bump the data does not have");
  }

  // 2. the declaration: a pool config is a function of where the ground is wanted.
  {
    const TilePool::Config munich = GroundPoolConfig(48.137, 11.575);
    const TilePool::Config sydney = GroundPoolConfig(-33.868, 151.209);
    Note("the origin the config takes for Munich", munich.OriginLatDeg, "deg");
    Note("the byte budget it declares", (double)munich.ByteBudget, "bytes");
    Note("the threads it asks for", (double)munich.Threads, "threads");
    CHECK_NEAR(munich.OriginLatDeg, 48.137, 1.0e-9, "deg",
               "the pool is focused where the ground is wanted, not at a fixed origin");
    CHECK_NEAR(sydney.OriginLonDeg, 151.209, 1.0e-9, "deg", "and the same the other side of it");
    CHECK(munich.ByteBudget > 0 && munich.Threads > 0,
          "and it declares a byte budget and a worker count rather than leaving both at zero");
  }

  // 3. the contract that matters: a stream over a pool that fetches NOTHING answers that it
  //    cannot, and never a height.
  {
    const std::string store =
        (std::filesystem::temp_directory_path() / "outshine-groundstream-probe").string();
    std::error_code made;
    std::filesystem::create_directories(store, made);
    ContentStore held(ContentStore::Config{store, ContentStore::Use::On, 0});
    SourceSet none(held);
    Silent silent;
    Note("sources the set declares", (double)none.Count(), "sources");

    TilePool pool(GroundPoolConfig(48.137, 11.575), none, silent);
    GroundStream stream(pool, GroundSurface{12, 64});

    const GroundSample asked = stream.At(48.137, 11.575);
    double aslM = -1.0;
    const bool resolved = asked.TryAslM(&aslM);
    std::printf("NOTE the sample says %s\n",
                asked.Where() == GroundSample::State::Resolved  ? "resolved"
                : asked.Where() == GroundSample::State::Pending ? "pending"
                                                                : "a hole");
    CHECK(!resolved,
          "**AND GROUND NOBODY FETCHED IS NOT A HEIGHT**: a stream over a set that declares no "
          "source answers pending or a hole, never a resolved zero -- every field above it "
          "counts an unresolved sample and refuses to place anything there, and a silent zero "
          "would put the whole world at sea level (board:1806)");
    CHECK(aslM == -1.0,
          "and it leaves the caller's own variable alone, so a caller that ignores the verdict "
          "keeps whatever it had rather than being handed a number");

    Note("the posting spacing at this latitude", stream.PostM(48.137), "m");
    Note("the spacing at the equator", stream.PostM(0.0), "m");
    CHECK(stream.PostM(48.137) > 0.0 && stream.PostM(0.0) > stream.PostM(48.137),
          "and the posting spacing is a function of latitude -- wider at the equator, because "
          "a web-mercator tile covers more ground there");

    std::error_code why;
    std::filesystem::remove_all(store, why);
  }

  Covers("I.4.11 the ground stream answers a height or says it cannot: the bilinear sample "
         "between four postings is their weighted mean and monotone along an edge, the pool it "
         "reads is focused where the ground is wanted, and a stream with nothing fetched "
         "returns pending or a hole rather than a zero (board:1806)");
  return Report();
}
