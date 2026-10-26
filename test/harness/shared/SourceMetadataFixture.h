#ifndef OUTSHINE_TEST_SOURCEMETADATAFIXTURE_H
#define OUTSHINE_TEST_SOURCEMETADATAFIXTURE_H

#include "Check.h"
#include "RawSourceCache.h"
#include "Source.h"
#include "SourceSet.h"
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace outshine::Test::SourceMetadata {
class MetadataOnlySource final : public Data::Source {
public:
  explicit MetadataOnlySource(Data::SourceDecl declaration) : Decl_(std::move(declaration)) {}

  [[nodiscard]] const Data::SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Data::Coverage
  Covers([[maybe_unused]] const Data::SourceRequest &request) const noexcept override {
    return Data::Coverage::Inside;
  }

  [[nodiscard]] Data::Address Serves(const Data::SourceRequest &request) const noexcept override {
    return request.Where();
  }

  [[nodiscard]] Data::FetchStart
  Begin([[maybe_unused]] const Data::Address &at,
        [[maybe_unused]] Data::SourceTransport &transport) const override {
    return std::unexpected(Data::SourceFailureReason::ProviderRefused);
  }

  [[nodiscard]] Data::SourceResponse
  Collect([[maybe_unused]] const Data::Address &at,
          [[maybe_unused]] Data::Ticket ticket,
          [[maybe_unused]] Data::SourceTransport &transport) const override {
    return Data::SourceResponse::Meant(Data::Meaning::Refused);
  }

private:
  Data::SourceDecl Decl_;
};

[[nodiscard]] inline Data::SourceDecl Declaration(std::string id, int32_t rank = 0) {
  Data::SourceDecl declaration{};
  declaration.Id = std::move(id);
  declaration.Revision = "r1";
  declaration.Kind = Data::DataKind::Elevation;
  declaration.Order = Data::Rank{rank};
  declaration.MaxZoom = 14;
  return declaration;
}

inline void Register(Data::SourceSet &sources, Data::SourceDecl declaration) {
  CHECK(sources.Add(std::make_unique<MetadataOnlySource>(std::move(declaration))) ==
            Data::SourceSet::Registration::Accepted,
        "the production source registry accepts this metadata fixture");
}

inline void RegisterRecipe(Data::SourceSet &sources, bool pair, bool reverse = false) {
  auto primary = Declaration("primary-elevation");
  auto secondary = Declaration("secondary-elevation", 1);
  if (!pair) {
    primary.Schema = Data::ContentKey(secondary, Data::Address::Whole(0)) + ":0:0:1:0:0:0:14:0:";
  }
  if (pair && reverse) { Register(sources, secondary); }
  Register(sources, std::move(primary));
  if (pair && !reverse) { Register(sources, std::move(secondary)); }
}
}
#endif
