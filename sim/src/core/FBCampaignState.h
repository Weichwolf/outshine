/* The three monotone facts a campaign carries from one mission to the next, as a canonical TEXT
 * artefact (doc/missions/campaign.md §3/§4): who is dead, which ground target is dead, and what is
 * left on the racks. Written after every mission, read before the next one — so a single step can be
 * re-run standalone by handing it the same file, which is the acceptance criterion of the whole layer.
 *
 * It is DATA, not a judge: nothing here decides anything. Damage, fuel and position are deliberately
 * absent and each omission has a reason in the spec; the struct has no room for them by design.
 *
 * The overlay below is the ONLY way the carry reaches a mission, and it can only ever REMOVE: a unit
 * block, or a surplus `set store` line. There is no code path that adds one — the moment a campaign
 * could add a line, the .fbm would stop being the statement of what was flown. */
#ifndef FBCAMPAIGNSTATE_H
#define FBCAMPAIGNSTATE_H

#include <cstdint>
#include <string>
#include <vector>
#include "FBCampaignFile.h"
#include "FBMissionFile.h"
#include "FBStore.h"

namespace FlightBox {

inline constexpr int kFBStoreKinds = (int)(sizeof(kStoreCatalogue) / sizeof(kStoreCatalogue[0]));

struct FBCampaignUnitState {
  std::string Id;
  bool Ground = false;      /* a `target_*` module: carried under the `ground` bit, not `units` */
  bool Destroyed = false;   /* monotone: FBCampaignState::Note never clears it */
  /* Remaining stock per catalogue index; -1 = this unit has never been seen carrying that kind, so
   * nothing caps it. A kind enters the book on the first sortie that declares it and can only fall. */
  int Stock[kFBStoreKinds];

  FBCampaignUnitState() { for (int i = 0; i < kFBStoreKinds; i++) Stock[i] = -1; }
};

/* One applied overlay change, for the `campaign CARRY` line: the effective mission is reconstructible
 * from events.log without re-deriving it. */
struct FBCarryChange {
  enum class Action { DropUnit, DropStore };
  Action      What = Action::DropUnit;
  std::string UnitId;
  std::string Store;      /* DropStore: the catalogue key */
  int         Station = 0;
};

class FBCampaignState {
public:
  const std::vector<FBCampaignUnitState> &Units() const { return Units_; }
  bool Empty() const { return Units_.empty(); }

  /* What a finished mission observed about one declared actor. Monotone by construction: destroyed
   * never clears, a stock never rises, and a kind the run did not carry keeps its previous entry. */
  void Note(const std::string &id, bool ground, bool destroyed, const int *remaining);

  /* Canonical text, in first-appearance order — never hash or filesystem order, because it is part of
   * the campaign fingerprint. `missionIndex` only names the file's header comment. */
  std::string Format(int missionIndex) const;

private:
  FBCampaignUnitState &Entry(const std::string &id);
  std::vector<FBCampaignUnitState> Units_;

  friend bool FBParseCampaignState(const std::string &text, FBCampaignState &out, std::string *err);
};

bool FBParseCampaignState(const std::string &text, FBCampaignState &out, std::string *err = nullptr);

/* Applies the carry to a PARSED mission. Removes destroyed units' blocks and every `set store` line
 * beyond the carried stock, records what it did, and asserts afterwards that the mission only got
 * smaller. Untouched by anything the mask does not name. */
void FBApplyCampaignCarry(const FBCampaignState &state, uint8_t mask, FBMission &mission,
                          std::vector<FBCarryChange> &changes);

/* The catalogue index of a `set store <station> <key>` line's key, or -1. Also the reader of that
 * line's station, so mission-side store syntax is parsed in exactly one place. */
int FBStoreKindIndex(const char *key);
bool FBParseStoreSetValue(const std::string &value, int &station, int &kindIndex);

} // namespace FlightBox
#endif
