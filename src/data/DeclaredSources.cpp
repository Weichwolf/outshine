#include "DeclaredSources.h"

#include <memory>

#include "StarBands.h"
#include "TerrariumDem.h"
#include "VersatilesVector.h"

namespace outshine::Data {

Registered RegisterDeclared(SourceSet &set, const Declared &declared) {
  if (set.Add(std::make_unique<StarBands>(declared.StarDirectory)) != SourceSet::Registration::Accepted)
    return Registered::RankClash;
  if (!declared.WithUpstreams) return Registered::Complete;
  if (set.Add(std::make_unique<TerrariumDem>()) != SourceSet::Registration::Accepted)
    return Registered::RankClash;
  if (set.Add(std::make_unique<VersatilesVector>()) != SourceSet::Registration::Accepted)
    return Registered::RankClash;
  return Registered::Complete;
}

}
