#include "ClassField.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "Transport.h"
#include "VegetationTemplates.h"
#include "Check.h"

#include <memory>

namespace {
using namespace outshine::Data;

class NoTransport final : public Transport {
public:
  Ticket Begin(const std::string &) override { return Ticket::None; }

  Wire Collect(Ticket) override { return Wire::Never(); }

  void Cancel(Ticket) override {}
};

class PendingSource final : public Source {
public:
  SourceDecl Decl{.Id = "pending", .Keeps = Cacheability::Never};

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &) const noexcept override { return Coverage::Inside; }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  Ticket Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Working();
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "materials load");
  CHECK(templates.Load("src/assets/world/vegetation.json", materials), "classification rules load");
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  NoTransport transport;
  CHECK(sources.Add(std::make_unique<PendingSource>()) == Data::SourceSet::Registration::Accepted,
        "pending vector source registers");
  TilePool pool({.PollAttempts = 1, .Carriers = 1}, sources, transport);
  ClassField field;
  field.SetVegetation(&templates);
  field.Open(0, 0);
  CHECK(field.Update(pool, {}).has_value(), "classification requests its source tiles");
  CHECK(field.PendingTiles() > 0, "the source fields remain incomplete");
  CHECK(field.FineSubmits() == 0 && field.CoarseSubmits() == 0,
        "partial source windows do not start work that their next tile invalidates");
  return Report();
}
