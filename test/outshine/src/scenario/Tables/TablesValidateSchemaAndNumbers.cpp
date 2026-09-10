#include "Tables.h"
#include "Check.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Scenario::Table valid{.Id = "stats",
                              .Columns = {"id", "value", "label"},
                              .Types = {false, true},
                              .Rows = {{"a", "1.25", "first"}}};
  const auto build = [](const Scenario::Table &table) {
    return TableBook::Stand(std::span(&table, 1));
  };
  for (int failure = 0; failure < 7; ++failure) {
    auto bad = valid;
    switch (failure) {
      case 0: bad.Id.clear(); break;
      case 1: bad.Columns[1] = bad.Columns[0]; break;
      case 2: bad.Columns[1].clear(); break;
      case 3: bad.Types.resize(4); break;
      case 4: bad.Rows[0].pop_back(); break;
      case 5: bad.Rows.push_back(bad.Rows[0]); break;
      default:
        bad.Columns.clear();
        bad.Types.clear();
        bad.Rows.clear();
        break;
    }
    CHECK(!build(bad), "ambiguous or malformed table schema is rejected");
  }
  for (const auto text : {"nan", "inf", "-inf", "1x", "1e309", "1e-9999", "", " 1"}) {
    auto bad = valid;
    bad.Rows[0][1] = text;
    CHECK(!build(bad), "numeric cells require a complete finite in-range decimal");
  }
  auto source = valid;
  auto book = build(source);
  CHECK(book.has_value(), "valid table builds");
  if (!book) { return Report(); }
  source.Rows[0][1] = "99";
  source.Rows[0][2] = "changed";
  const auto *number = book->Number({"stats", "a", "value"});
  const auto *label = book->Text({"stats", "a", "label"});
  CHECK(number && *number == 1.25 && label && *label == "first",
        "book owns parsed values and text");
  CHECK(!book->Text({"stats", "a", "value"}) && !book->Number({"stats", "a", "label"}),
        "lookup does not coerce column types");
  CHECK(!book->Number({"absent", "a", "value"}) && !book->Number({"stats", "absent", "value"}) &&
            !book->Number({"stats", "a", "absent"}),
        "missing addresses return no cell");
  auto signedNumber = valid;
  signedNumber.Rows[0][1] = "+2.5e1";
  auto signedBook = build(signedNumber);
  CHECK(signedBook.has_value(), "shared finite decimal grammar accepts explicit plus and exponent");
  if (signedBook) {
    const auto *value = signedBook->Number({"stats", "a", "value"});
    CHECK(value && *value == 25, "exponent parsed exactly");
  }
  auto empty = valid;
  empty.Rows.clear();
  CHECK(build(empty).has_value(), "a schema may have no rows");
  empty.Rows = {{"", "0", ""}};
  auto emptyKey = build(empty);
  CHECK(emptyKey.has_value(), "empty text is a valid unique row key");
  if (emptyKey) {
    const auto *value = emptyKey->Number({"stats", "", "value"});
    CHECK(value && *value == 0, "empty key remains addressable");
  }
  const Scenario::Table lexical{.Id = "keys",
                                .Columns = {"key", "label"},
                                .Types = {true},
                                .Rows = {{"01", "leading"}, {"1", "plain"}}};
  auto keys = build(lexical);
  CHECK(keys.has_value(), "numeric keys retain distinct source spellings");
  if (keys) {
    const auto *leading = keys->Text({"keys", "01", "label"});
    const auto *plain = keys->Text({"keys", "1", "label"});
    CHECK(leading && *leading == "leading" && plain && *plain == "plain",
          "numeric conversion does not merge lexical row keys");
  }
  const std::array duplicate{valid, valid};
  CHECK(!TableBook::Stand(duplicate), "duplicate table identifiers are rejected");
  auto bounded = valid;
  bounded.Rows.clear();
  for (size_t row = 0; row < TableBook::kMostRows; ++row) {
    bounded.Rows.push_back({std::to_string(row), "0", ""});
  }
  CHECK(build(bounded).has_value(), "row limit is inclusive");
  bounded.Rows.push_back({"overflow", "0", ""});
  CHECK(!build(bounded), "one row beyond the limit is rejected");
  return Report();
}
