#include "AssetSourceRecipe.h"
#include "Check.h"
#include "SourceMetadataFixture.h"
#include <array>
#include <iostream>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  namespace Fixture = SourceMetadata;
  Data::RawSourceCache raw({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  Data::SourceSet single(raw), pair(raw), reverse(raw);
  Fixture::RegisterRecipe(single, false);
  Fixture::RegisterRecipe(pair, true);
  Fixture::RegisterRecipe(reverse, true, true);
  CHECK(single.Count() == 1 && pair.Count() == 2,
        "the accepted inputs have different source cardinalities and fallback behavior");
  constexpr auto version = "prepared-ground-region-97";
  const auto kinds = std::array{Data::DataKind::Elevation, Data::DataKind::VectorMap};
  const auto one = Generators::AssetSourceRecipe(version, single, kinds);
  const auto two = Generators::AssetSourceRecipe(version, pair, kinds);
  CHECK(one.size() == 64 && two.size() == 64 && one != two,
        "a free schema cannot absorb the complete next source record");
  constexpr auto legacy = "43f51dc152e7a9a50942d44c57f921d8c2f567f97a7888002eb21d57712d2fa8";
  CHECK(one != legacy && two != legacy,
        "new framed recipes cannot silently reuse the old ambiguous recipe identity");
  CHECK(Generators::AssetSourceRecipe(version, pair, kinds) == two &&
            Generators::AssetSourceRecipe(version, reverse, kinds) == two,
        "unchanged source sets are stable across registration order");
  const auto reordered =
      std::array{Data::DataKind::VectorMap, Data::DataKind::Elevation, Data::DataKind::Elevation};
  CHECK(Generators::AssetSourceRecipe(version, pair, reordered) == two,
        "the data-kind filter's spelling and duplicates do not change selected input identity");
  const auto unrelated = std::array{Data::DataKind::VectorMap};
  CHECK(Generators::AssetSourceRecipe(version, single, unrelated) ==
            Generators::AssetSourceRecipe(version, pair, unrelated),
        "ignored kinds cannot invalidate a recipe");
  CHECK(Generators::AssetSourceRecipe("another-product-version", pair, kinds) != two,
        "the explicit product version remains part of persistent identity");
  std::cout << "SOURCE_RECIPE keys_equal=" << (one == two) << " single=" << one << " pair=" << two
            << '\n';
  return Report();
}
