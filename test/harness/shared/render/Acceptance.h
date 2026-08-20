#ifndef RENDER_ACCEPTANCE_H
#define RENDER_ACCEPTANCE_H

#include <string>
#include <vector>

#include "Json.h"

namespace outshine::Render::Parity {

enum class SubjectClass { OpaqueAtLeastOnePixel, SubPixelPresent };

enum class ExactnessClass { Exact, GeneralPosition };

enum class CriterionKind { Numeric, SelfDescribing, StatedInvariant, LimitsProbe };

enum class OracleRole { Reference, CannotExpressTheCriterion };

constexpr double kBoundaryP95OpaquePx = 0.1;
constexpr double kBoundaryP95SubPixelPx = 0.5;

constexpr double kCoverageFractionMin = 0.001;

constexpr double kFrameFractionTolerance = 5e-7;

inline double DefaultBoundaryP95Px(SubjectClass subject) {
  return subject == SubjectClass::OpaqueAtLeastOnePixel ? kBoundaryP95OpaquePx
                                                        : kBoundaryP95SubPixelPx;
}

struct Acceptance {
  SubjectClass Subject = SubjectClass::OpaqueAtLeastOnePixel;
  double BoundaryP95MaxPx = kBoundaryP95OpaquePx;
  double CoverageFractionMin = kCoverageFractionMin;
  double FrameFractionTolerance = kFrameFractionTolerance;

  bool EnforceBoundary = true;
};

[[nodiscard]] inline bool ReadDeclaredNumber(const Json::Ref &entry, const char *name, double &out,
                                             std::string &error) {
  if (!entry.Valid() || entry.GetKind() != Json::Kind::Object) {
    error = std::string(name) + " is not an object carrying value, unit and origin";
    return false;
  }
  const Json::Ref value = entry["value"];
  if (!value.Valid() || value.GetKind() != Json::Kind::Number) {
    error = std::string(name) + " carries no numeric value";
    return false;
  }
  const std::string origin = entry["origin"].Str("");
  if (origin != "SET" && origin != "derived" && origin != "measured") {
    error = std::string(name) + " has origin '" + origin +
            "', and a number's origin is SET, derived or measured";
    return false;
  }
  if (origin == "derived" && entry["derivation"].Str("").empty()) {
    error = std::string(name) + " is derived and states no derivation";
    return false;
  }
  if (entry["unit"].Str("").empty()) {
    error = std::string(name) + " states no unit";
    return false;
  }
  out = value.Num(0.0);
  return true;
}

[[nodiscard]] inline bool ReadAcceptance(const Json::Ref &declared, Acceptance &out,
                                         std::string &error) {
  if (!declared.Valid()) { return true; }
  if (declared.GetKind() != Json::Kind::Object) {
    error = "acceptance is not an object";
    return false;
  }
  for (size_t entry = 0; entry < declared.Size(); ++entry) {
    const std::string key = declared.Key(entry);
    double value = 0;
    if (!ReadDeclaredNumber(declared[key.c_str()], key.c_str(), value, error)) { return false; }
    if (declared[key.c_str()]["reason"].Str("").empty()) {
      error = "acceptance." + key + " overrides a declared default and states no reason";
      return false;
    }
    double *slot = nullptr;
    double fallback = 0;
    if (key == "boundaryP95MaxPx") {
      slot = &out.BoundaryP95MaxPx;
      fallback = DefaultBoundaryP95Px(out.Subject);
    } else if (key == "coverageFractionMin") {
      slot = &out.CoverageFractionMin;
      fallback = kCoverageFractionMin;
    } else if (key == "frameFractionTolerance") {
      slot = &out.FrameFractionTolerance;
      fallback = kFrameFractionTolerance;
    } else {
      error = "acceptance." + key +
              " is not an overridable threshold; the set is boundaryP95MaxPx, coverageFractionMin, "
              "frameFractionTolerance -- and IoU has no threshold at all, because it is reported";
      return false;
    }
    if (value == fallback) {
      error = "acceptance." + key + " restates the declared default " + std::to_string(fallback) +
              " instead of overriding it, and a restated threshold is a quotation that goes stale";
      return false;
    }
    *slot = value;
  }
  return true;
}

[[nodiscard]] inline bool ReadCriterionKind(const std::string &spelling, CriterionKind &out,
                                            std::string &error) {
  if (spelling == "numeric") {
    out = CriterionKind::Numeric;
    return true;
  }
  if (spelling == "self-describing") {
    out = CriterionKind::SelfDescribing;
    return true;
  }
  if (spelling == "stated-invariant") {
    out = CriterionKind::StatedInvariant;
    return true;
  }
  if (spelling == "limits-probe") {
    out = CriterionKind::LimitsProbe;
    return true;
  }
  error = "criterion.kind '" + spelling +
          "' is none of numeric, self-describing, stated-invariant, limits-probe";
  return false;
}

[[nodiscard]] inline bool ReadOracleRole(const Json::Ref &criterion, OracleRole &out,
                                         std::string &error) {
  const std::string spelling = criterion["oracleRole"].Str("");
  if (spelling == "reference") {
    out = OracleRole::Reference;
    return true;
  }
  if (spelling != "cannot-express-the-criterion") {
    error = "criterion.oracleRole '" + spelling +
            "' is neither reference nor cannot-express-the-criterion, and a self-describing case "
            "states which of the two its Blender render is";
    return false;
  }
  out = OracleRole::CannotExpressTheCriterion;
  if (criterion["oracleLimitation"].Str("").empty()) {
    error = "criterion.oracleRole is cannot-express-the-criterion and no oracleLimitation names "
            "what the oracle cannot do";
    return false;
  }
  const Json::Ref measured = criterion["oracleLimitationMeasured"];
  if (measured.Size() == 0) {
    error = "criterion.oracleRole is cannot-express-the-criterion and no oracleLimitationMeasured "
            "carries the measurement that shows it";
    return false;
  }
  for (size_t entry = 0; entry < measured.Size(); ++entry) {
    double value = 0;
    if (!ReadDeclaredNumber(measured[entry], "criterion.oracleLimitationMeasured[]", value,
                            error)) {
      return false;
    }
    if (measured[entry]["of"].Str("").empty()) {
      error = "criterion.oracleLimitationMeasured[] states no `of`, so the number names no subject";
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool ReadExactnessClass(const Json::Ref &root, ExactnessClass &out,
                                             std::string &error) {
  const Json::Ref declared = root["acceptanceClass"];
  const std::string spelling = declared["is"].Str("");
  if (spelling == "exact") {
    out = ExactnessClass::Exact;
  } else if (spelling == "general-position") {
    out = ExactnessClass::GeneralPosition;
  } else {
    error = "acceptanceClass.is '" + spelling +
            "' is neither exact nor general-position, and a case states which of the two its "
            "placement claims -- there is no default";
    return false;
  }

  if (declared["because"].Str("").empty()) {
    error = "acceptanceClass states no `because`, so the class is a word with no argument under it";
    return false;
  }
  return true;
}

[[nodiscard]] inline bool ReadSubjectClass(const std::string &spelling, SubjectClass &out,
                                           std::string &error) {
  if (spelling == "opaque-min-1px") {
    out = SubjectClass::OpaqueAtLeastOnePixel;
    return true;
  }
  if (spelling == "sub-pixel-present") {
    out = SubjectClass::SubPixelPresent;
    return true;
  }
  error = "subjectClass '" + spelling + "' is neither opaque-min-1px nor sub-pixel-present";
  return false;
}

}
#endif
