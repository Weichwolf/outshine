#include "Check.h"
#include "SourceTransport.h"
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  constexpr auto headers = "Content-Type: application/pbf\r\nX-Identity: pinned \t\r\n\r\n";
  CHECK(SingleResponseHeader(headers, "x-identity") == "pinned",
        "a field name is case insensitive and surrounding HTTP whitespace is removed");
  CHECK(!SingleResponseHeader(headers, "missing"), "a missing identity is not invented");
  CHECK(!SingleResponseHeader("X-Identity: a\r\nx-identity: a\r\n", "x-identity"),
        "even equal duplicate identity fields are ambiguous");
  CHECK(!SingleResponseHeader("X-Identity: a\r\nx-identity: b\r\n", "x-identity"),
        "conflicting identity fields cannot qualify a response");
  CHECK(!SingleResponseHeader("X-Identity: a\r\n continuation\r\n", "x-identity"),
        "obsolete folded fields cannot establish identity");
  CHECK(SingleResponseHeader("X-Identity:\r\n", "x-identity") == "",
        "an explicitly empty field remains distinct from a missing field");
  CHECK(!SingleResponseHeader(headers, ""), "an empty requested field is invalid");
  const std::string excessive(MaximumResponseHeaderBytes + 1, 'x');
  CHECK(!SingleResponseHeader(excessive, "x-identity"), "header parsing has a fixed byte limit");
  auto first = Wire::Answered(200, {1, 2}, 0.0, headers);
  auto moved = std::move(first);
  CHECK(first.Where() == Wire::State::Consumed, "moving consumes the original response owner");
  auto assigned = Wire::Working();
  assigned = std::move(moved);
  CHECK(moved.Where() == Wire::State::Consumed, "assignment transfers the same ownership");
  const auto response = assigned.Take();
  CHECK(response && response->Body == std::vector<uint8_t>({1, 2}) &&
            SingleResponseHeader(response->Headers, "x-identity") == "pinned",
        "body and final response identity survive both moves together");
  CHECK(!assigned.Take(), "body and response metadata can only be taken once");
  return Report();
}
