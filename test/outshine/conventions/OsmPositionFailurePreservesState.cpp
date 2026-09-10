#include "OsmField.h"
#include "GroundStack.h"
#include "Check.h"
#include <array>
#include <cfenv>
#include <limits>

namespace {
class NoTransport final : public outshine::Data::Transport {
public:
  outshine::Data::Ticket Begin(const std::string &) override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel(outshine::Data::Ticket) override {}
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  NoTransport transport;
  TilePool pool({}, sources, transport);
  ClassField classes;
  classes.Open(0, 0);
  GroundStack stack;
  const std::array<std::string, 1> layers{"transportation"};
  const std::array<OsmField::Declared, 1> input{
      {{.Layer = "transportation", .Key = "kind", .Value = "road", .LatLon = {0, 0, 1, 1}}}};
  OsmField field(14, layers);
  CHECK(field.Declare(input, LongitudeLatitude{}).has_value(), "valid declaration succeeds");
  const auto generation = field.Generation();
  const auto heap = field.HeapBytes();
  const auto *points = field.Points().data();
  const int pending = field.PendingTiles();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  for (const LongitudeLatitude at : std::array<LongitudeLatitude, 10>{{{nan, 0},
                                                                       {0, nan},
                                                                       {inf, 0},
                                                                       {0, inf},
                                                                       {181, 0},
                                                                       {-181, 0},
                                                                       {0, 91},
                                                                       {0, -91},
                                                                       {0, 90},
                                                                       {0, -90}}}) {
    std::feclearexcept(FE_ALL_EXCEPT);
    const auto declared = field.Declare({}, at);
    CHECK(!declared && !declared.error().empty(), "invalid declaration returns a diagnostic");
    const auto built = field.Build(pool, at, 1);
    CHECK(!built && !built.error().empty(), "invalid build is not reported as an empty success");
    CHECK(field.Generation() == generation && field.HeapBytes() == heap &&
              field.Points().data() == points && field.Features().size() == 1 &&
              field.Points().size() == 4 && field.PendingTiles() == pending &&
              field.CentreX() == 8192 && field.CentreY() == 8192,
          "failure preserves declaration, storage and streaming state");
    const auto classified = classes.Update(pool, at);
    const auto streamed = stack.Restand(at);
    CHECK(!classified && !streamed,
          "position errors propagate through classification and ground stack");
    CHECK(classes.FineSubmits() == 0 && classes.CoarseSubmits() == 0 && !stack.Opened(),
          "invalid positions publish no jobs or stack state");
    CHECK(std::fetestexcept(FE_INVALID | FE_OVERFLOW) == 0,
          "refusal occurs before unsafe projection");
  }
  CHECK(field.Declare(input, LongitudeLatitude{}).has_value() && field.Generation() == generation,
        "valid repeat after failures retains exact identity");
  for (const int zoom : {-1, 32, std::numeric_limits<int>::max()}) {
    OsmField invalid(zoom, layers);
    const auto before = invalid.PendingTiles();
    CHECK(!invalid.Declare(input, LongitudeLatitude{}) && !invalid.Build(pool, {}, 1),
          "unsupported signed tile zoom is refused by both entry points");
    CHECK(invalid.Generation() == 0 && invalid.Features().empty() &&
              invalid.PendingTiles() == before,
          "invalid zoom leaves field untouched");
  }
  for (const int zoom : {0, 14, std::numeric_limits<int>::digits}) {
    OsmField edge(zoom, layers);
    CHECK(edge.Declare(input, LongitudeLatitude{.LongitudeDeg = 180}).has_value(),
          "east dateline accepted");
    const auto last = (int64_t{1} << zoom) - 1;
    CHECK(edge.CentreX() == last, "east dateline selects last valid signed column");
    CHECK(edge.Declare(input, LongitudeLatitude{.LongitudeDeg = -180}).has_value() &&
              edge.CentreX() == 0,
          "west dateline selects first column");
  }
  return Report();
}
