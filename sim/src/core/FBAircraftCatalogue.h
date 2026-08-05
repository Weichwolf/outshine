/* THE ROWS A SCENARIO DECLARES, owned — an empty container in core/ that only a mod's manifest fills
 * (missions/FBCatalogueBoot.h). It is deliberately not a lookup service: there is no global instance
 * and no find-by-name, so the ONLY way to a row is to be handed one. A catalogue aircraft therefore
 * cannot learn what else exists, which is CLAUDE.md Prinzip 3 held in the type system rather than in a
 * comment.
 *
 * Rows are heap-stable and never move: the module registry's factories point INTO this storage. */
#ifndef FBAIRCRAFTCATALOGUE_H
#define FBAIRCRAFTCATALOGUE_H

#include <memory>
#include <string>
#include <vector>
#include "FBAircraft.h"

namespace FlightBox {

class FBAircraftCatalogue {
public:
  /* The three strings are COPIED and `spec`'s pointers re-seated on this catalogue's own storage, so a
   * caller may hand over a parser's temporaries. The damage layout is derived from span and length. */
  void Add(const std::string &key, const std::string &name, const std::string &fdmModel,
           const FBAircraftSpec &spec, double spanM, double lenM);

  size_t Size() const { return Rows_.size(); }
  const FBAircraftSpec &At(size_t i) const { return Rows_[i]->Spec; }

private:
  struct Row {
    std::string Key, Name, FdmModel;
    FBDamageZoneSpec Zones[4];
    FBAircraftSpec Spec;
  };
  std::vector<std::unique_ptr<Row>> Rows_;
};

} // namespace FlightBox
#endif
