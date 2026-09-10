#include "Number.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <string_view>

namespace {
using namespace outshine;
using namespace outshine::Test;

static_assert(noexcept(ParseFiniteNumber(std::string_view{})));

void CheckValues() {
  struct Example {
    std::string_view Text;
    double Value;
  };

  constexpr std::array cases = {
      Example{"0", 0},
      Example{"+0", 0},
      Example{"-0", -0.0},
      Example{"12.5", 12.5},
      Example{"+12.5", 12.5},
      Example{"-12.5", -12.5},
      Example{".5", 0.5},
      Example{"+.5", 0.5},
      Example{"1.", 1},
      Example{"1e2", 100},
      Example{"1e+2", 100},
      Example{"-1.25e-2", -0.0125},
      Example{"1.7976931348623157e308", std::numeric_limits<double>::max()},
      Example{"4.9406564584124654e-324", std::numeric_limits<double>::denorm_min()}};
  for (const auto &example : cases) {
    const auto parsed = ParseFiniteNumber(example.Text);
    CHECK(parsed && *parsed == example.Value, "decimal input has its independently stated value");
    if (parsed && example.Value == 0) {
      CHECK(std::signbit(*parsed) == std::signbit(example.Value), "signed zero is preserved");
    }
  }
  constexpr std::array bounded = {'1', '.', '2', '5', '9'};
  const auto parsed = ParseFiniteNumber(std::string_view(bounded.data(), 4));
  CHECK(parsed && *parsed == 1.25, "only the bounded view is read, without a terminator");
}

void CheckErrors() {
  for (std::string_view text : {"",
                                "+",
                                "-",
                                "+-1",
                                "++1",
                                "--1",
                                " 1",
                                "1 ",
                                "1\n",
                                "12junk",
                                "1e",
                                "1e+",
                                "0x10",
                                "1,5",
                                "."}) {
    const auto parsed = ParseFiniteNumber(text);
    CHECK(!parsed && parsed.error() == NumberError::InvalidSyntax,
          "empty, partial or malformed tokens are syntax errors, never substitute zero");
  }
  for (std::string_view text : {"1e309", "-1e309", "1e-9999"}) {
    const auto parsed = ParseFiniteNumber(text);
    CHECK(!parsed && parsed.error() == NumberError::OutOfRange,
          "overflow and underflow preserve a range error");
  }
  for (std::string_view text : {"nan", "NaN", "nan(payload)", "inf", "-inf", "+inf", "infinity"}) {
    const auto parsed = ParseFiniteNumber(text);
    CHECK(!parsed && parsed.error() == NumberError::NonFinite,
          "nonfinite values cannot enter finite engine configuration");
  }
  constexpr std::array embedded = {'1', '2', '\0', '3'};
  const auto parsed = ParseFiniteNumber(std::string_view(embedded.data(), embedded.size()));
  CHECK(!parsed && parsed.error() == NumberError::InvalidSyntax,
        "embedded zero does not truncate the declared input range");
}
}

int main() {
  CheckValues();
  CheckErrors();
  return outshine::Test::Report();
}
