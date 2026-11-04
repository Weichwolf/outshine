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

namespace outshine::Render {
struct CameraBasis;
}

namespace outshine {
enum class SourceProductSelectionError { InvalidInput, ForeignRevision, InvalidProduct };

class SourceProductSelection {
public:
  struct Source {
    uint32_t Cohort = 0;
    bool FieldSupported = false;
    uint32_t Faces = 0;
    std::optional<double> ShellErrorM;
  };
  enum class Kind { Geometry, MaterialField, Unresolved, Shell, Hull };

  struct Product {
    uint32_t Node = 0;
    Kind Representation = Kind::Unresolved;
    uint32_t Pixels = 0;
    double ProjectedErrorPx = 0;
    double WorldErrorM = 0;
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
  // Declared source errors select candidates; they do not certify native render products.
  // Orthographic FocalPx is pixels per metre; TranslationM expands the eye domain.
  [[nodiscard]] std::expected<Cut, SourceProductSelectionError>
  SelectGeometry(const Render::CameraBasis &camera,
                 ProjectedErrorBudget projection,
                 double translationM = 0,
                 bool orthographic = false,
                 bool cohortGroups = false) const;
  [[nodiscard]] std::expected<std::span<const uint32_t>, SourceProductSelectionError>
  Members(const Cut &cut, const Product &product) const;
  [[nodiscard]] const Box &Bounds(const Product &product) const;

private:
  struct Node {
    Vec3 Centre;
    double Radius = 0;
    std::optional<uint32_t> Cohort;
    bool FieldSupported = false;
    bool GeometrySupported = false;
    std::optional<double> ShellErrorM;
  };

  [[nodiscard]] std::optional<Product>
  Candidate(uint32_t at, const Vec3 &eye, ProjectedErrorBudget projection) const;
  std::shared_ptr<const PlanHierarchy> Hierarchy_;
  std::vector<Node> Nodes_;
  std::string Revision_;
};
}
#endif
