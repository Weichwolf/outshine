#include "FBCampaignState.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace FlightBox {

int FBStoreKindIndex(const char *key) {
  if (!key) return -1;
  for (int i = 0; i < kFBStoreKinds; i++)
    if (std::strcmp(kStoreCatalogue[i]->Key, key) == 0) return i;
  return -1;
}

bool FBParseStoreSetValue(const std::string &value, int &station, int &kindIndex) {
  std::istringstream in(value);
  std::string key;
  if (!(in >> station) || !(in >> key)) return false;
  kindIndex = FBStoreKindIndex(key.c_str());
  return kindIndex >= 0;
}

FBCampaignUnitState &FBCampaignState::Entry(const std::string &id) {
  for (auto &u : Units_)
    if (u.Id == id) return u;
  Units_.push_back(FBCampaignUnitState{});
  Units_.back().Id = id;
  return Units_.back();
}

void FBCampaignState::Note(const std::string &id, bool ground, bool destroyed, const int *remaining) {
  FBCampaignUnitState &u = Entry(id);
  u.Ground = ground;
  u.Destroyed = u.Destroyed || destroyed;
  if (!remaining) return;
  for (int i = 0; i < kFBStoreKinds; i++) {
    if (remaining[i] < 0) continue;                       /* not carried this sortie: the book stands */
    u.Stock[i] = u.Stock[i] < 0 ? remaining[i] : (remaining[i] < u.Stock[i] ? remaining[i] : u.Stock[i]);
  }
}

std::string FBCampaignState::Format(int missionIndex) const {
  std::ostringstream out;
  char idx[8];
  snprintf(idx, sizeof idx, "%02d", missionIndex);
  out << "# campaign-state after mission " << idx << "\n";
  for (const auto &u : Units_) {
    out << (u.Ground ? "ground " : "unit ") << u.Id << ' ' << (u.Destroyed ? "destroyed" : "alive");
    bool first = true;
    for (int i = 0; i < kFBStoreKinds; i++) {
      if (u.Stock[i] < 0) continue;
      out << (first ? " stores " : " ") << kStoreCatalogue[i]->Key << '=' << u.Stock[i];
      first = false;
    }
    out << "\n";
  }
  return out.str();
}

bool FBParseCampaignState(const std::string &text, FBCampaignState &out, std::string *err) {
  out = FBCampaignState{};
  int lineNo = 0;
  auto fail = [&](const std::string &msg) {
    if (err) *err = "line " + std::to_string(lineNo) + ": " + msg;
    return false;
  };

  std::istringstream lines(text);
  std::string raw;
  while (std::getline(lines, raw)) {
    lineNo++;
    std::string line = raw;
    size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    std::istringstream ls(line);
    std::string kw;
    if (!(ls >> kw)) continue;
    if (kw != "unit" && kw != "ground") return fail("unknown keyword '" + kw + "'");

    std::string id, alive;
    if (!(ls >> id)) return fail("'" + kw + "' needs a callsign");
    if (!(ls >> alive)) return fail("'" + kw + " " + id + "' needs alive|destroyed");
    if (alive != "alive" && alive != "destroyed") return fail("want alive|destroyed, not '" + alive + "'");
    for (const auto &u : out.Units_)
      if (u.Id == id) return fail("duplicate entry '" + id + "'");

    FBCampaignUnitState e;
    e.Id = id;
    e.Ground = kw == "ground";
    e.Destroyed = alive == "destroyed";
    std::string tok;
    if (ls >> tok) {
      if (tok != "stores") return fail("want 'stores' or end of line, not '" + tok + "'");
      while (ls >> tok) {
        size_t eq = tok.find('=');
        if (eq == std::string::npos) return fail("want '<store>=<count>', not '" + tok + "'");
        int k = FBStoreKindIndex(tok.substr(0, eq).c_str());
        if (k < 0) return fail("unknown store '" + tok.substr(0, eq) + "'");
        int n = std::atoi(tok.c_str() + eq + 1);
        if (n < 0) return fail("negative store count in '" + tok + "'");
        e.Stock[k] = n;
      }
    }
    out.Units_.push_back(e);
  }
  return true;
}

void FBApplyCampaignCarry(const FBCampaignState &state, uint8_t mask, FBMission &mission,
                          std::vector<FBCarryChange> &changes) {
  const size_t unitsBefore = mission.Units.size();
  size_t setLinesBefore = 0;
  for (const auto &u : mission.Units) setLinesBefore += u.SetKV.size();

  for (size_t i = 0; i < mission.Units.size();) {
    const FBCampaignUnitState *carried = nullptr;
    for (const auto &c : state.Units())
      if (c.Id == mission.Units[i].Id) { carried = &c; break; }
    if (!carried) { i++; continue; }

    const uint8_t bit = carried->Ground ? FBCarryGround : FBCarryUnits;
    if (carried->Destroyed && (mask & bit)) {
      changes.push_back({FBCarryChange::Action::DropUnit, mission.Units[i].Id, "", 0});
      mission.Units.erase(mission.Units.begin() + (long)i);
      continue;
    }

    if (mask & FBCarryStores) {
      int seen[kFBStoreKinds] = {};
      auto &kv = mission.Units[i].SetKV;
      for (size_t j = 0; j < kv.size();) {
        int station = 0, kind = -1;
        if (kv[j].first != "store" || !FBParseStoreSetValue(kv[j].second, station, kind)) { j++; continue; }
        /* File order decides which rounds survive: the surplus is dropped from the END, so the
         * effective load is a prefix of what the file declares and never a selection. */
        if (carried->Stock[kind] >= 0 && ++seen[kind] > carried->Stock[kind]) {
          changes.push_back({FBCarryChange::Action::DropStore, mission.Units[i].Id,
                             kStoreCatalogue[kind]->Key, station});
          kv.erase(kv.begin() + (long)j);
          continue;
        }
        j++;
      }
    }
    i++;
  }

  /* The binding rule of §4, asserted rather than trusted: an overlay may only ever make a mission
   * smaller. Nothing above can add — this is the second lock on the same door. */
  size_t setLinesAfter = 0;
  for (const auto &u : mission.Units) setLinesAfter += u.SetKV.size();
  assert(mission.Units.size() <= unitsBefore && setLinesAfter <= setLinesBefore);
  (void)unitsBefore;
  (void)setLinesBefore;
  (void)setLinesAfter;
}

} // namespace FlightBox
