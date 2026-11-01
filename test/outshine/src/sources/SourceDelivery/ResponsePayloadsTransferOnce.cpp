#include <cstdint>
#include "Address.h"
#include "DataKind.h"
#include "SourceDelivery.h"
#include "SourceFailure.h"
#include "SourceFailureReason.h"
#include <SourceResponse.h>
#include <SourceTransport.h>
#include "Check.h"
#include <array>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
template <class T, class Empty> void CheckOwnership(T source, Empty empty) {
  using namespace outshine::Test;
  CHECK(!std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T>,
        "response payload has a single owner");
  CHECK(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>,
        "response ownership transfers without throwing");
  T moved(std::move(source));
  CHECK(!std::exchange(source, empty()).Take(), "move construction consumes the source");
  auto target = empty();
  target = std::move(moved);
  CHECK(!std::exchange(moved, empty()).Take(), "move assignment consumes the source");
  CHECK(target.Take().has_value(), "destination receives one answer");
  CHECK(!target.Take(), "answer cannot be taken twice");
}

void CheckFailureOwnership() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  SourceFailure failure{.Kind = DataKind::VectorMap,
                        .Requested = Address::Whole(7),
                        .Served = Address::Whole(3),
                        .SourceId = std::string(96, 's'),
                        .SourceRevision = "revision",
                        .SourceKey = std::string(64, 'k'),
                        .Reason = SourceFailureReason::Unavailable,
                        .HttpStatus = 503,
                        .Retries = 4};
  const auto *identity = failure.SourceId.data();
  auto response = SourceDelivery::WireAfter(125, "origin", "revision", std::move(failure));
  auto moved = std::move(response);
  auto consumed = std::exchange(response, SourceDelivery::Waiting());
  CHECK(consumed.Where() == SourceDelivery::State::Consumed && !consumed.TakeFailure(),
        "moving a refused delivery consumes its original failure owner");
  const auto owned = moved.TakeFailure();
  CHECK(owned && owned->SourceId.data() == identity && owned->HttpStatus == 503 &&
            owned->Retries == 4 && owned->Requested == Address::Whole(7) &&
            owned->Served == Address::Whole(3) && owned->SourceKey == std::string(64, 'k'),
        "failure ownership transfers without copying or discarding source metadata");
  CHECK(!moved.TakeFailure() && !moved.Failure() &&
            moved.Where() == SourceDelivery::State::Consumed && moved.AfterMs() == 125,
        "the failure transfers once while its retry hint remains available");
  CHECK(owned && owned->Describe().contains("http=503; retries=4"),
        "the final diagnostic retains actual HTTP status and retry count");
  std::array others{SourceDelivery::Waiting(),
                    SourceDelivery::Nothing(),
                    SourceDelivery::NoSource(),
                    SourceDelivery::From("origin", "revision", Address::Whole(7), {1})};
  for (auto &other : others) {
    const auto state = other.Where();
    CHECK(!other.TakeFailure() && other.Where() == state,
          "extracting a failure cannot consume pending, absent, undeclared or delivered input");
  }
  CHECK(others.back().Take(), "a delivered payload remains available after failure inspection");
}
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  CheckFailureOwnership();
  CheckOwnership(Wire::Answered(200, {1, 2}), Wire::Working);
  CheckOwnership(SourceResponse::Delivered({3, 4}), SourceResponse::Working);
  CheckOwnership(SourceDelivery::From("source", "revision", Address::Whole(7), {5, 6}),
                 SourceDelivery::Waiting);
  std::vector<uint8_t> bytes{7, 8, 9};
  const auto *storage = bytes.data();
  auto wire = Wire::Answered(200, std::move(bytes), 0.5);
  auto response = wire.Take();
  CHECK(response && response->Status == 200 && response->Body.data() == storage,
        "wire transfers bytes without copying and preserves status");
  CHECK(wire.RetryAfterS() == 0.5, "retry metadata remains available after take");
  CHECK(!wire.Take(), "wire payload is consumed exactly once");
  if (!response) { return Report(); }
  auto fetched = SourceResponse::Delivered(std::move(response->Body));
  auto settled = fetched.Take();
  CHECK(settled && settled->What == Meaning::Bytes && settled->Bytes.data() == storage,
        "source interpretation transfers the same allocation");
  CHECK(!fetched.Take(), "source payload is consumed exactly once");
  if (!settled) { return Report(); }
  auto delivered = SourceDelivery::From(
      "origin", "revision", Address::Whole(7), std::move(settled->Bytes), "source-key");
  const auto answer = delivered.Take();
  CHECK(answer && answer->SourceId == "origin" && answer->SourceRevision == "revision" &&
            answer->SourceKey == "source-key" && answer->Bytes.data() == storage &&
            answer->Bytes == std::vector<uint8_t>({7, 8, 9}),
        "delivery transfers original storage and source identity");
  CHECK(!delivered.Take(), "delivery payload is consumed exactly once");
  auto empty = Wire::Answered(204, {});
  CHECK(empty.Take().has_value() && !empty.Take(), "empty answered payload still transfers once");
  auto retry = SourceResponse::MeantAfter(Meaning::Retry, 1.25);
  CHECK(retry.Take().has_value() && !retry.Take() && retry.RetryAfterS() == 1.25,
        "non-byte source responses also transfer once without losing retry metadata");
  auto pending = SourceDelivery::Waiting();
  CHECK(!pending.Take() && pending.Where() == SourceDelivery::State::Pending,
        "unsuccessful take does not consume a pending state");
  return Report();
}
