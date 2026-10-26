#include "TilePool.h"
#include "ClassificationPreparation.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <Transport.h>
#include "RegionSurfaceRecipes.h"
#include "Check.h"

#include <memory>
#include <string>

namespace {
using namespace outshine::Data;

class NoTransport final : public Transport {
public:
  FetchStart Begin(const std::string & /*url*/) override { return Ticket::None; }

  Wire Collect(Ticket /*ticket*/) override { return Wire::Never(); }

  void Cancel(Ticket /*ticket*/) override {}
};

class PendingSource final : public Source {
public:
  SourceDecl Decl{.Id = "pending",
                  .Revision = {},
                  .Endpoint = {},
                  .Keeps = Cacheability::Never,
                  .PayloadSha256 = {},
                  .Schema = {}};

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl; }

  [[nodiscard]] Coverage Covers(const SourceRequest & /*request*/) const noexcept override {
    return Coverage::Inside;
  }

  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address & /*at*/, Transport & /*transport*/) const override {
    return Ticket::None;
  }

  SourceResponse
  Collect(const Address & /*at*/, Ticket /*ticket*/, Transport & /*transport*/) const override {
    return SourceResponse::Working();
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  outshine::Generators::GroundMaterialCatalog materials;
  outshine::Generators::RegionSurfaceRecipes templates;
  CHECK(materials.Load("data/world/ground-materials.json"), "materials load");
  CHECK(templates.Load("data/world/vegetation.json", materials), "classification rules load");
  Data::RawSourceCache store({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  Data::SourceSet sources(store);
  NoTransport transport;
  CHECK(sources.Add(std::make_unique<PendingSource>()) == Data::SourceSet::Registration::Accepted,
        "pending vector source registers");
  TilePool pool({.PollAttempts = 1, .Carriers = 1, .PreparedFields = {}}, sources, transport);
  Tasks compute(1);
  ClassificationPreparation field;
  field.SetRecipes(&templates);
  field.Open(0, 0, compute);
  CHECK(field.Update(pool, {}).has_value(), "classification requests its source tiles");
  CHECK(field.PendingTiles() > 0, "the source fields remain incomplete");
  CHECK(field.FineSubmits() == 0 && field.CoarseSubmits() == 0,
        "partial source windows do not start work that their next tile invalidates");
  return Report();
}
