/* The .fbc campaign format parser (doc/missions/campaign.md). Pure string-in/struct-out, exactly like
 * FBMissionFile.h beside it. A campaign is an ORDERED LIST of unchanged .fbm files plus the three
 * knobs the carry between them needs: which facts travel, when to stop, and one clock for the missions
 * that declare none. It holds no mission data of its own — the .fbm files stay the statement of what
 * was flown. */
#ifndef FBCAMPAIGNFILE_H
#define FBCAMPAIGNFILE_H

#include <cstdint>
#include <string>
#include <vector>

namespace FlightBox {

/* The carry mask may only be NARROWED (doc/missions/campaign.md §2): there is nothing to widen it to,
 * and a campaign able to invent a carried quantity would be inventing state. */
enum FBCarryBit : uint8_t {
  FBCarryUnits = 1u << 0,    /* aircraft destroyed -> the unit block is dropped */
  FBCarryGround = 1u << 1,   /* ground targets destroyed -> same */
  FBCarryStores = 1u << 2,   /* stores expended -> the surplus `set store` lines are dropped */
};
inline constexpr uint8_t kFBCarryAll = FBCarryUnits | FBCarryGround | FBCarryStores;

std::string FBCarryMaskStr(uint8_t mask);

/* Never is the default because attrition IS the subject: two of the ten campaign specs ask what a
 * force looks like after it has lost, and stopping at the first loss deletes the answer. */
enum class FBCampaignStop { Never, Fail, Crash };
const char *FBCampaignStopStr(FBCampaignStop s);

struct FBCampaign {
  std::string Name;
  int64_t UtcT0S = 0;     /* `time` — applies to every mission declaring none; never advances (§6) */
  bool    HaveTime = false;
  uint8_t Carry = kFBCarryAll;
  FBCampaignStop StopOn = FBCampaignStop::Never;
  std::vector<std::string> Missions;   /* file order IS run order, relative to the campaign file */
};

/* Returns false with a "line N: ..." message in *err; `out` is only fully valid on true. */
bool FBParseCampaignFile(const std::string &text, FBCampaign &out, std::string *err = nullptr);

} // namespace FlightBox
#endif
