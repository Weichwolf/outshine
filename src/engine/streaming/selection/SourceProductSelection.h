#ifndef OUTSHINE_ENGINE_STREAMING_SELECTION_SOURCEPRODUCTSELECTION_H
#define OUTSHINE_ENGINE_STREAMING_SELECTION_SOURCEPRODUCTSELECTION_H
#include "PlanHierarchy.h"
#include "ProjectedErrorBudget.h"
#include "Vec3.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine {
enum class SourceProductSelectionError { InvalidInput, ForeignRevision, InvalidProduct };

class SourceProductSelection {
public:
  struct Source {
    uint32_t Cohort = 0;
    bool FieldSupported = false;
  };
  enum class Kind { Geometry, MaterialField, Unresolved };

  struct Product {
    uint32_t Node = 0;
    Kind Representation = Kind::Unresolved;
    uint32_t Pixels = 0;
    double CentredErrorPx = 0;
  };

  struct Cut {
    std::string Revision;
    std::vector<Product> Products;
    size_t VisitedNodes = 0;
  };

  [[nodiscard]] static std::expected<SourceProductSelection, SourceProductSelectionError>
  Of(std::shared_ptr<const PlanHierarchy> hierarchy,
     std::span<const Source> sources,
     std::string revision);
  // Candidates only; native pose/material/coverage validation grants replacement authority.
  [[nodiscard]] std::expected<Cut, SourceProductSelectionError>
  Select(const Vec3 &eye, ProjectedErrorBudget projection, bool cohortGroups = true) const;
  [[nodiscard]] std::expected<std::span<const uint32_t>, SourceProductSelectionError>
  Members(const Cut &cut, const Product &product) const;
  [[nodiscard]] const Box &Bounds(const Product &product) const;

private:
  struct Node {
    Vec3 Centre;
    double Radius = 0;
    std::optional<uint32_t> Cohort;
    bool Supported = false;
  };

  [[nodiscard]] std::optional<Product>
  Candidate(uint32_t at, const Vec3 &eye, ProjectedErrorBudget projection) const;
  std::shared_ptr<const PlanHierarchy> Hierarchy_;
  std::vector<Node> Nodes_;
  std::string Revision_;
};
}
#endif
