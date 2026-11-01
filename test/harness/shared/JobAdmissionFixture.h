#ifndef OUTSHINE_TEST_JOBADMISSIONFIXTURE_H
#define OUTSHINE_TEST_JOBADMISSIONFIXTURE_H
#include "Check.h"
#include "JobAdmission.h"
#include <cstdlib>
#include <memory>
#include <utility>

namespace outshine::Test {
inline const std::shared_ptr<JobAdmission> &JobAdmissions() {
  static const auto admission = std::make_shared<JobAdmission>(16);
  return admission;
}

inline const std::shared_ptr<JobAdmission> &ContextAdmissions() {
  static const auto admission = std::make_shared<JobAdmission>(16);
  return admission;
}

inline JobAdmission::Lease JobLease() {
  auto lease = JobAdmissions()->TryAcquire();
  CHECK(lease.has_value(), "the fixture admits its bounded job");
  if (!lease) { std::abort(); }
  return std::move(*lease);
}
}
#endif
