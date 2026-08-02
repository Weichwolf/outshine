#include "FBMfdSystem.h"

namespace FlightBox::Systems {

namespace {
/* WHAT A PAGE NEEDS TO EXIST, asked of the published blocks and of nothing else. Two kinds of answer
 * are mixed here on purpose because the aircraft mixes them: a page dies when its BOX dies (the block
 * head goes Invalid) and it dies when its SUBJECT is gone (the last store left the rails). The mission
 * text is not consulted — it says what somebody wrote down, the blocks say what the jet has. */
bool PageAvailable(FBMfdPage p, const FBState &s) {
  switch (p) {
    case FBMfdPage::None: return false;
    case FBMfdPage::Fcr: return s.Radar.H.Readable();
    case FBMfdPage::Sms:
      return (s.Stores.H.Readable() && s.Stores.LoadedCount > 0)
          || (s.Gun.H.Readable() && s.Gun.RoundsRemaining > 0);
    case FBMfdPage::Hsd:
      return s.Nav.H.Readable() || s.Datalink.H.Readable() || s.NetLink.H.Readable();
    case FBMfdPage::Rwr: return s.Rwr.H.Readable();
    case FBMfdPage::Irst: return s.Irst.H.Readable();
    case FBMfdPage::Sys: return s.Airframe.H.Readable();
  }
  return false;
}
} // namespace

void FBMfdSystem::DeclarePages(const FBMfdPage *pages, int n) {
  PageCount_ = n < kMfdMaxPages ? n : kMfdMaxPages;
  for (int i = 0; i < PageCount_; i++) Pages_[i] = pages[i];
}

bool FBMfdSystem::Select(int ordinal, double nowS) {
  if (ordinal < 0 || ordinal >= PageCount_ || (Available_ & (1u << ordinal)) == 0) return false;
  Bay_[kMfdAttentionBay] = ordinal;
  LastSelectPage_ = ordinal;
  LastSelectS_ = nowS;
  PlaceBays();
  return true;
}

/* The middle bay is the pilot's; the other two are filled from the catalogue in its own order with
 * whatever is selectable and not already up. A bay with nothing left to show goes dark rather than
 * repeating its neighbour. */
void FBMfdSystem::PlaceBays() {
  int &att = Bay_[kMfdAttentionBay];
  if (att < 0 || att >= PageCount_) {
    att = -1;
    for (int i = 0; i < PageCount_ && att < 0; i++)
      if (Available_ & (1u << i)) att = i;   /* power-up default: the catalogue's first live page */
  }
  int next = 0;
  for (int b = 0; b < kMfdBays; b++) {
    if (b == kMfdAttentionBay) continue;
    Bay_[b] = -1;
    while (next < PageCount_) {
      int ord = next++;
      if (ord == att || (Available_ & (1u << ord)) == 0) continue;
      Bay_[b] = ord;
      break;
    }
  }
}

void FBMfdSystem::Run(FBState &state, double nowS) {
  Available_ = 0;
  for (int i = 0; i < PageCount_; i++)
    if (PageAvailable(Pages_[i], state)) Available_ |= 1u << i;
  PlaceBays();

  FBMfdBlock &b = state.Mfd;
  b.PageCount = PageCount_;
  for (int i = 0; i < PageCount_; i++) b.Pages[i] = Pages_[i];
  b.Available = Available_;
  for (int i = 0; i < kMfdBays; i++) b.Bay[i] = Bay_[i];
  b.LastSelectPage = LastSelectPage_;
  b.LastSelectS = LastSelectS_;
  b.H.Publish(nowS);
}

} // namespace FlightBox::Systems
