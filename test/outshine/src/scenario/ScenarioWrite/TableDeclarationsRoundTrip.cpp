#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Tables.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  source.Tables = {{.Id = "inventory & <state>",
                    .Columns = {"key", "amount", "label"},
                    .Types = {false, true},
                    .Rows = {{"", "+2.5e1", "a\tb\nc\r\"'&<>"}, {"next", "-0", ""}}},
                   {.Id = "empty", .Columns = {"key"}},
                   {.Id = "lexical", .Columns = {"key"}, .Types = {true}, .Rows = {{"01"}, {"1"}}}};
  const auto text = WriteScenario(source);
  Scenario::Document copy;
  std::string error;
  const bool parsed = ReadScenario(text.data(), text.size(), copy, error);
  CHECK(parsed, error.c_str());
  CHECK(copy.Tables.size() == source.Tables.size(), "table count survives serialization");
  if (!parsed || copy.Tables.size() != source.Tables.size()) { return Report(); }
  for (size_t at = 0; at < source.Tables.size(); ++at) {
    const auto &a = source.Tables[at];
    const auto &b = copy.Tables[at];
    CHECK(a.Id == b.Id && a.Columns == b.Columns && a.Rows == b.Rows,
          "table ordering, schema names and exact cell spellings survive");
    CHECK(b.Types.size() == b.Columns.size(), "XML materializes one type per column");
    if (b.Types.size() != a.Columns.size()) { continue; }
    for (size_t column = 0; column < a.Columns.size(); ++column) {
      CHECK(b.Types[column] == (column < a.Types.size() && a.Types[column]),
            "omitted native trailing types retain their text meaning");
    }
  }
  const auto book = TableBook::Stand(copy.Tables);
  CHECK(book.has_value(), "round-tripped schema builds a typed table book");
  if (book) {
    const auto *number = book->Number({source.Tables[0].Id, "", "amount"});
    CHECK(number && *number == 25, "numeric cell remains queryable by an empty text key");
    const auto *label = book->Text({source.Tables[0].Id, "", "label"});
    CHECK(label && *label == source.Tables[0].Rows[0][2], "escaped text remains exact");
    CHECK(book->Number({"lexical", "01", "key"}) && book->Number({"lexical", "1", "key"}),
          "distinct numeric key spellings remain distinct keys");
    CHECK(!book->Number({source.Tables[0].Id, "", "label"}), "text never becomes a number");
  }
  CHECK(WriteScenario({}).find("<tables>") == std::string::npos, "absent tables remain absent");
  return Report();
}
