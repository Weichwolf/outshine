#include <cstdint>
#include "Check.h"

#include <SourceRequest.h>
#include <SourceResponse.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  const auto at = Address::AtCell({.SouthDeg = 54, .WestDeg = 9});
  const ByteRange range{.First = 100, .Length = 3};
  std::string pin = "\"revision-1\"";
  const SourceRequest partial(DataKind::Elevation, at, range, pin);
  pin.clear();
  CHECK(partial.Where() == at && partial.Range() == range &&
            partial.EntityTag() == "\"revision-1\"",
        "native demand owns its interval and revision independently of caller storage");
  const SourceRequest whole(DataKind::Elevation, at);
  const SourceRequest unpinned(DataKind::Elevation, at, range);
  CHECK(!whole.Range() && whole.EntityTag().empty() && whole.Key() != partial.Key() &&
            partial.Key() != unpinned.Key(),
        "partial and whole demands and different revision pins have separate identities");
  CHECK((range.Valid() && !ByteRange{.First = 0, .Length = 0}.Valid() &&
         !ByteRange{.First = std::numeric_limits<uint64_t>::max(), .Length = 2}.Valid()),
        "empty and overflowing intervals are refused");
  CHECK(StrongEntityTag("\"\"") && StrongEntityTag("\"revision\"") &&
            !StrongEntityTag("W/\"revision\"") && !StrongEntityTag("\"bad\nvalue\"") &&
            !StrongEntityTag("\"bad\"value\"") &&
            !StrongEntityTag("\"" + std::string(MaximumEntityTagBytes, 'x') + "\""),
        "HTTP, provider and cache share the same strong-tag boundary");
  const RangeResponse origin{.Bytes = range, .TotalBytes = 1000, .EntityTag = "\"revision-1\""};
  CHECK((origin.Valid(3) && !origin.Valid(2) &&
         !RangeResponse{.Bytes = range, .TotalBytes = 102, .EntityTag = "\"revision-1\""}.Valid(3)),
        "receipt validates actual byte length and complete-file bounds");
  std::vector<uint8_t> bytes{4, 5, 6};
  const auto *original = bytes.data();
  auto received = SourceResponse::Delivered(std::move(bytes), origin);
  auto moved = std::move(received);
  auto assigned = SourceResponse::Working();
  assigned = std::move(moved);
  const auto reply = assigned.Take();
  CHECK(reply && reply->Bytes.data() == original &&
            reply->Bytes == std::vector<uint8_t>({4, 5, 6}) && reply->Range &&
            reply->Range->Bytes == range && reply->Range->TotalBytes == 1000 &&
            reply->Range->EntityTag == partial.EntityTag(),
        "receipt and original allocation survive both moves and a single ownership transfer");
  CHECK(std::exchange(received, SourceResponse::Working()).Where() ==
                SourceResponse::State::Consumed &&
            std::exchange(moved, SourceResponse::Working()).Where() ==
                SourceResponse::State::Consumed &&
            !assigned.Take(),
        "moved and consumed replies cannot transfer source bytes twice");
  return Report();
}
