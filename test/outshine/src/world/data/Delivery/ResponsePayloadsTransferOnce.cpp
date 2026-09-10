#include "Delivery.h"
#include "Fetched.h"
#include "Transport.h"
#include "Check.h"
#include <type_traits>
#include <utility>
#include <vector>

namespace {
template <class T> void CheckOwnership(T source, T target) {
  using namespace outshine::Test;
  CHECK(!std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T>,
        "response payload has a single owner");
  static_assert(std::is_nothrow_move_constructible_v<T>);
  static_assert(std::is_nothrow_move_assignable_v<T>);
  T moved(std::move(source));
  CHECK(!source.Take(), "move construction consumes the source");
  target = std::move(moved);
  CHECK(!moved.Take(), "move assignment consumes the source");
  CHECK(target.Take().has_value(), "destination receives one answer");
  CHECK(!target.Take(), "answer cannot be taken twice");
}
}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  CheckOwnership(Wire::Answered(200, {1, 2}), Wire::Working());
  CheckOwnership(Fetched::Delivered({3, 4}), Fetched::Working());
  CheckOwnership(Delivery::From("source", Address::Whole(7), {5, 6}), Delivery::Waiting());
  std::vector<uint8_t> bytes{7, 8, 9};
  const auto *storage = bytes.data();
  auto wire = Wire::Answered(200, std::move(bytes), 0.5);
  auto response = wire.Take();
  CHECK(response && response->Status == 200 && response->Body.data() == storage,
        "wire transfers bytes without copying and preserves status");
  CHECK(wire.RetryAfterS() == 0.5, "retry metadata remains available after take");
  CHECK(!wire.Take(), "wire payload is consumed exactly once");
  if (!response) { return Report(); }
  auto fetched = Fetched::Delivered(std::move(response->Body));
  auto settled = fetched.Take();
  CHECK(settled && settled->What == Meaning::Bytes && settled->Bytes.data() == storage,
        "source interpretation transfers the same allocation");
  CHECK(!fetched.Take(), "source payload is consumed exactly once");
  if (!settled) { return Report(); }
  auto delivered = Delivery::From("origin", Address::Whole(7), std::move(settled->Bytes));
  const auto answer = delivered.Take();
  CHECK(answer && answer->SourceId == "origin" && answer->Bytes.data() == storage &&
            answer->Bytes == std::vector<uint8_t>({7, 8, 9}),
        "delivery transfers original storage and source identity");
  CHECK(!delivered.Take(), "delivery payload is consumed exactly once");
  auto empty = Wire::Answered(204, {});
  CHECK(empty.Take().has_value() && !empty.Take(), "empty answered payload still transfers once");
  auto retry = Fetched::MeantAfter(Meaning::Retry, 1.25);
  CHECK(retry.Take().has_value() && !retry.Take() && retry.RetryAfterS() == 1.25,
        "non-byte source responses also transfer once without losing retry metadata");
  auto pending = Delivery::Waiting();
  CHECK(!pending.Take() && pending.Where() == Delivery::State::Pending,
        "unsuccessful take does not consume a pending state");
  return Report();
}
