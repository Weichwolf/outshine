/* The MULTIFUNCTION-DISPLAY page vocabulary: what a cockpit page is FOR, one entry per published
 * block group it mirrors. The vocabulary is generic because the QUESTION is ("what does this jet
 * know about X"); WHICH of these pages a cockpit actually offers, and at which ordinal, is the
 * module's own catalogue and lives with the module. doc/modules/f16/cockpit-displays.md.
 * Ordinals are telemetry- and command-visible — append, never reorder. */
#ifndef FBMFDPAGE_H
#define FBMFDPAGE_H

#include <cstdint>

namespace FlightBox {

enum class FBMfdPage : uint8_t {
  None = 0,
  Fcr,    /* the ACTIVE radar picture: FBRadarBlock */
  Sms,    /* stores management: FBStoresBlock + FBGunBlock */
  Hsd,    /* horizontal situation — the heads-down mode: FBNavBlock + FBDatalinkBlock/FBNetLinkBlock */
  Rwr,    /* who is looking at this aircraft: FBRwrBlock + FBCmdsBlock */
  Irst,   /* the PASSIVE optical picture: FBIrstBlock */
  Sys,    /* the aeroplane itself: FBAirframeBlock + FBWarningBlock + FBUfcBlock */
};

/* The three-letter legend the bay carries, so a viewer can name what the AI is looking at. */
inline const char *FBMfdPageLabel(FBMfdPage p) {
  switch (p) {
    case FBMfdPage::None: return "OFF";
    case FBMfdPage::Fcr: return "FCR";
    case FBMfdPage::Sms: return "SMS";
    case FBMfdPage::Hsd: return "HSD";
    case FBMfdPage::Rwr: return "RWR";
    case FBMfdPage::Irst: return "IRST";
    case FBMfdPage::Sys: return "SYS";
  }
  return "?";
}

/* THE BANK: three bays across the bottom row of the 3x3 screen, and the one the pilot's page select
 * lands on. A single command target carries a page and not a display (doc/core.md, Abschnitt 2), so
 * WHERE the chosen page appears is a property of the cockpit: the middle bay, under the HUD, is the
 * attention bay; the flanking two carry the remaining catalogue pages in their declared order. */
inline constexpr int kMfdBays = 3;
inline constexpr int kMfdAttentionBay = 1;
inline constexpr int kMfdMaxPages = 8;

} // namespace FlightBox
#endif
