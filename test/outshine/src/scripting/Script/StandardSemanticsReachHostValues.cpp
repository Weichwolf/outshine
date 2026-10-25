#include "Script.h"
#include "ScriptHostFixture.h"
#include "Check.h"
#include <cmath>
#include <string>
#include <string_view>

int main() {
  using namespace outshine::Script;
  using namespace outshine::Test;
  ScriptHostFixture host;
  Program program;
  std::string error;
  constexpr std::string_view source = R"(
    capture(-"12", "6"*7, 1/0, 0/0, null === undefined, !!(0/0), "10" < "2", typeof missing);
    capture(null, undefined, true, false, "Grüße\u0000Welt");
    const twice = x => x * 2;
    class Counter { constructor(n) { this.n = n; } value() { return twice(this.n); } }
    const numbers = [1, 2, 3];
    capture(new Counter(numbers.reduce((a,b) => a+b, 0)).value(), `${2n ** 64n}`);
    capture(-0, 0 % 0, -"0", 1/(-0));
    capture(typeof globalThis['capture\u0000tail'], object[Symbol.toStringTag] === undefined);
  )";
  CHECK(program.Read(source, error) && host.Calls.empty(), "compilation executes no host action");
  CHECK(program.Run(host, error), error.c_str());
  CHECK(host.Calls.size() == 5, "all independent semantic groups ran");
  if (host.Calls.size() != 5) { return Report(); }
  const auto &numeric = host.Calls[0];
  CHECK(numeric[0].Number == -12 && numeric[1].Number == 42 && std::isinf(numeric[2].Number) &&
            std::isnan(numeric[3].Number),
        "string numeric coercion and IEEE division follow JavaScript");
  CHECK(numeric[4].What == Kind::Boolean && !numeric[4].Truth() && !numeric[5].Truth() &&
            numeric[6].Truth() && numeric[7].Text == "undefined",
        "null differs from undefined, NaN is false and string comparisons are lexical");
  CHECK(host.Calls[1][0].What == Kind::Null && host.Calls[1][1].What == Kind::Nothing &&
            host.Calls[1][2].What == Kind::Boolean && host.Calls[1][2].Truth() &&
            !host.Calls[1][3].Truth() && host.Calls[1][4].Text.contains('\0'),
        "typed null, undefined, booleans and embedded-NUL Unicode strings cross the host boundary");
  CHECK(host.Calls[2][0].Number == 12 && host.Calls[2][1].Text == "18446744073709551616",
        "closures, arrays, classes and BigInt execute in the standard runtime");
  CHECK(std::signbit(host.Calls[3][0].Number) && std::isnan(host.Calls[3][1].Number) &&
            std::signbit(host.Calls[3][2].Number) && host.Calls[3][3].Number < 0 &&
            std::isinf(host.Calls[3][3].Number),
        "signed zero and invalid remainders retain their semantics");
  CHECK(host.Calls[4][0].Text == "undefined" && host.Calls[4][1].Truth(),
        "embedded-NUL and symbol property keys never alias native string names");
  return Report();
}
