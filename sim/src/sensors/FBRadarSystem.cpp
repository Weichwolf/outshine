#include "FBRadarSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include <cmath>

namespace FlightBox::Sensors {

/* Das WELT-Paar ist die gemeldete Position (der Konsument muesste sonst einen look-alten Koerpervektor
 * durch eine jetzt-aktuelle Lage zurueckdrehen); das KOERPER-Paar ist die Groesse der Antenne. */
void FBRadarSystem::RelativeLos(const Fdm::fb_fdm_state &own, double tgtLatDeg, double tgtLonDeg,
                                double tgtAltM, double &rangeM, double &bearingDeg, double &elevAngleDeg,
                                double &azDeg, double &elDeg) {
  double e = 0.0, n = 0.0;
  FBEnuOffsetM(own.lat, own.lon, tgtLatDeg, tgtLonDeg, e, n);
  double u = tgtAltM - own.elev;
  rangeM = std::sqrt(e * e + n * n + u * u);
  bearingDeg = std::atan2(e, n) * kRad2Deg;
  if (bearingDeg < 0.0) bearingDeg += 360.0;
  elevAngleDeg = std::atan2(u, std::sqrt(e * e + n * n)) * kRad2Deg;
  FBEnuToBodyLos(own.roll, own.pitch, own.yaw, e, n, u, azDeg, elDeg);
}

/* Die CLUTTER-Doppler: was ein STEHENDER Punkt in dieser Richtung schliessen wuerde. */
double FBRadarSystem::OwnClosureOn(const Fdm::fb_fdm_state &st, double bearingDeg, double elevAngleDeg) {
  double cb = std::cos(bearingDeg * kDeg2Rad), sb = std::sin(bearingDeg * kDeg2Rad);
  double ce = std::cos(elevAngleDeg * kDeg2Rad), se = std::sin(elevAngleDeg * kDeg2Rad);
  double ue = sb * ce, un = cb * ce, uu = se;
  return st.vx * ue + (-st.vz) * un + st.vy * uu;
}

/* Jede Wolke gegen DASSELBE Volumen wie das Flugzeug, staerkster Rueckstrahler gewinnt: Zweiweg-
 * Radargleichung RCS/r^4. Deterministisch — kein Wurf, keine Wahrscheinlichkeit. */
const FBChaffCloud *FBRadarSystem::SelectDecoy(const FBChaffCloud *clouds, const Fdm::fb_fdm_state &st,
                                               const FBRadarScanVolume &v, double simTimeS) const {
  const FBChaffCloud *best = nullptr;
  double bestPower = 0.0;
  for (int i = 0; i < kMaxChaffClouds; i++) {
    const FBChaffCloud &c = clouds[i];
    if (!c.Active) continue;
    double rcs = FBChaffRcsNorm(simTimeS - c.BloomS);
    if (rcs <= 0.0) continue;
    double r = 0.0, brg = 0.0, ev = 0.0, az = 0.0, el = 0.0;
    RelativeLos(st, c.LatDeg, c.LonDeg, c.AltM, r, brg, ev, az, el);
    if (r > GateRangeM(v) || r < 1.0) continue;
    if (std::fabs(FBWrap180(az - v.AzCenterDeg)) > v.AzHalfDeg) continue;
    if (std::fabs(el - v.ElCenterDeg) > v.ElHalfDeg) continue;
    double power = rcs / (r * r * r * r);
    if (power > bestPower) { bestPower = power; best = &c; }
  }
  return best;
}

/* Ein wieder hochgefahrenes Set startet ein FRISCHES Frame-Raster — und zwar wirklich frisch. Nur
 * NextScanS_ zu nullen genuegte nicht: der Nachhol-Waechter in Run() sieht dann ein Raster, das um die
 * ganze bisherige Missionszeit zurueckliegt, holt bis zu 64 Frames IM SELBEN Tick nach und macht daraus
 * einen festen Track im Moment des Einschaltens. Gemessen an mig29-intercept: Kontakt in derselben
 * Zehntelsekunde, in der der Emissionsschalter umging, statt nach den dokumentierten 2 x FrameS.
 * Deshalb ein Resync-FLAG, das der naechste Run() gegen die Uhr aufloest, die er ohnehin bekommt. */
void FBRadarSystem::SetPowered(bool on) {
  if (on && !Powered_) Resync_ = true;
  Powered_ = on;
  if (!on) NextScanS_ = 0.0;
}

FBIffReply FBRadarSystem::Interrogate(const Units::FBUnit &u) const {
  if (!IffInterrogator_) return FBIffReply::NotInterrogated;
  bool validReply = u.GetSignature().IffXpdr && u.GetTeam() == SelfTeam_;
  return validReply ? FBIffReply::Friendly : FBIffReply::NoReply;
}

/* Ein Antennen-Frame. Registry IN REIHENFOLGE, damit die Tracknummer einer Neuerfassung an der
 * Missionsdeklaration haengt und nie daran, wer zuerst gesehen wurde. */
void FBRadarSystem::ScanFrame(const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry &net, double simTimeS) {
  const FBRadarScanVolume &v = ActiveVolume();
  bool seen[kMaxRadarContacts] = {};

  /* STT: jede andere Einheit wird in diesem Frame gar nicht erst angeschaut und altert unten aus. */
  bool sttOnly = v.SingleTarget && LockedNum_ != 0;

  if (v.Active) {
    for (const Units::FBUnit *u : net.Units()) {
      if (!u || u->GetId() == SelfId_) continue;   /* das Set malt sich nicht selbst */
      /* Ein Luft-Luft-Set sucht FLUGZEUGE; eine Bombe als Kontakt zu malen waere Erfindung. */
      if (u->GetKind() != Units::FBUnitKind::Aircraft) continue;

      int slot = -1;
      for (int i = 0; i < TrackCount_; i++)
        if (Tracks_[i].UnitId == u->GetId()) { slot = i; break; }
      if (sttOnly && (slot < 0 || Tracks_[slot].TrackNum != LockedNum_)) continue;

      Units::FBUnitPose p = u->GetPose();
      Units::FBUnitSignature sig = u->GetSignature();
      double rangeM = 0.0, bearingDeg = 0.0, elevAngleDeg = 0.0, azDeg = 0.0, elDeg = 0.0;
      RelativeLos(st, p.LatDeg, p.LonDeg, p.ElevM, rangeM, bearingDeg, elevAngleDeg, azDeg, elDeg);

      /* Die Doppler-Entscheidung faellt VOR dem Volumentest: ein verfuehrter Look misst die WOLKE, und
       * dann muss die WOLKE in der Keule liegen. Nur ein Track mit Look-Paar kann getaeuscht werden. */
      const FBChaffCloud *decoy = nullptr;
      double tgtRadialMs = 0.0, ownClosureMs = 0.0, measuredClosureMs = 0.0;
      bool notched = false;
      double dtDwell = slot >= 0 ? simTimeS - Tracks_[slot].DopplerRefS : 0.0;
      bool dwellDone = slot >= 0 && Tracks_[slot].DopplerRefS > 0.0 && dtDwell >= kDopplerDwellS;
      if (slot >= 0 && (dwellDone || Tracks_[slot].Seduced)) {
        const Track &prev = Tracks_[slot];
        ownClosureMs = OwnClosureOn(st, bearingDeg, elevAngleDeg);
        measuredClosureMs = dwellDone ? (prev.DopplerRefRangeM - rangeM) / dtDwell : prev.ClosureMs;
        tgtRadialMs = ownClosureMs - measuredClosureMs;
        double notchMs = DopplerNotchMs(rangeM);
        /* Ein Set mit dokumentierter Erfassungsschwelle SIEHT im Filter nichts — der Look faellt aus,
         * ohne dass der Sender etwas getan haette. Die Messung laeuft trotzdem weiter (unten), sonst
         * koennte das Set nie bemerken, dass das Ziel den Filter wieder verlassen hat. */
        if (NotchRejectsDetection() && std::fabs(tgtRadialMs) < notchMs) notched = true;
        /* KLEBRIGKEIT: hat sich das Tor einmal in den Clutterfilter gesetzt, bleibt es dort, solange
         * dort ein Echo ist. Ohne sie kippte der Test bei jedem Look (gemessen: Seduce/Resolve im
         * 20-Hz-Takt alternierend). doc/sensors.md, Abschnitt 4.7. */
        if (!notched && (prev.Seduced || std::fabs(tgtRadialMs) < notchMs))
          decoy = SelectDecoy(sig.Chaff, st, v, simTimeS);
      }
      if (notched) {
        Track &t = Tracks_[slot];
        /* Der Notch-Bezug wandert weiter, obwohl der Look ausfiel: gemessen wird die Geometrie, die die
         * Antenne messen WUERDE — nur so ist der Wiedereintritt der Moment, in dem die
         * Radialgeschwindigkeit den Filter verlaesst, statt vom Alter des letzten Bezugs abzuhaengen.
         * Steht unter NotchRejectsDetection(), also fasst kein Set ohne Erfassungsschwelle sie an. */
        if (dwellDone) { t.DopplerRefRangeM = rangeM; t.DopplerRefS = simTimeS; }
        if (!t.Firm) {
          for (int j = slot; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
          TrackCount_--;
          for (int j = slot; j < TrackCount_; j++) seen[j] = seen[j + 1];
        } else if (!t.Notched) {
          t.Notched = true;
          FBLog::Info("radar", "NOTCH_LOST", {{"track", t.TrackNum}, {"tgtRadialMs", tgtRadialMs},
              {"notchMs", DopplerNotchMs(rangeM)}, {"rangeNm", rangeM * kMToNm}});
        }
        continue;
      }
      if (decoy)
        RelativeLos(st, decoy->LatDeg, decoy->LonDeg, decoy->AltM, rangeM, bearingDeg, elevAngleDeg,
                    azDeg, elDeg);

      /* Das Tor gegen DIESES Ziel: Rueckstrahlquerschnitt aus seiner publizierten Signatur, vierte
       * Wurzel (Radargleichung). Ein verfuehrter Look misst die WOLKE — deren Groesse steckt schon in
       * SelectDecoy's RCS/r^4-Vergleich, also bleibt fuer sie das unskalierte Tor stehen. */
      bool inVolume = rangeM <= (decoy ? GateRangeM(v) : GateRangeM(v, sig.RcsM2)) *
                                    LookDownFactor(elevAngleDeg) &&
                      std::fabs(FBWrap180(azDeg - v.AzCenterDeg)) <= v.AzHalfDeg &&
                      std::fabs(elDeg - v.ElCenterDeg) <= v.ElHalfDeg;

      if (!inVolume) {
        /* Nur ein FESTER Track verdient den Coast unten — eine gebrochene Trefferserie wird geloescht. */
        if (slot >= 0 && !Tracks_[slot].Firm) {
          for (int j = slot; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
          TrackCount_--;
          for (int j = slot; j < TrackCount_; j++) seen[j] = seen[j + 1];
        }
        continue;
      }

      if (slot < 0) {
        if (TrackCount_ >= kMaxRadarContacts) continue;   /* Trackdatei voll; nichts wird verdraengt */
        slot = TrackCount_++;
        Tracks_[slot] = Track{};
        Tracks_[slot].UnitId = u->GetId();
        Tracks_[slot].PrevRangeM = rangeM;
        Tracks_[slot].PrevLookS = simTimeS;
      }

      Track &t = Tracks_[slot];
      /* Annaeherungsrate aus zwei Looks differenziert: dieselbe Groesse ueber das Beobachtbare, ohne
       * eine Annahme ueber die Vertikalrate, die der Snapshot nicht publiziert. */
      double dtLook = simTimeS - t.PrevLookS;
      if (dtLook > 1e-6) t.ClosureMs = (t.PrevRangeM - rangeM) / dtLook;
      t.PrevRangeM = rangeM;
      t.PrevLookS = simTimeS;

      if (decoy && !t.Seduced)
        FBLog::Info("radar", "CHAFF_SEDUCED", {{"track", t.TrackNum},
            {"tgtRadialMs", tgtRadialMs}, {"notchMs", kDopplerNotchMs},
            {"ownClosMs", ownClosureMs}, {"measClosMs", measuredClosureMs},
            {"cloudAgeS", simTimeS - decoy->BloomS}, {"rangeM", rangeM},
            {"offsetM", FBPlanarDistM(decoy->LatDeg, decoy->LonDeg, p.LatDeg, p.LonDeg)}});
      else if (!decoy && t.Seduced)
        FBLog::Info("radar", "CHAFF_RESOLVED", {{"track", t.TrackNum},
            {"tgtRadialMs", tgtRadialMs}, {"notchMs", kDopplerNotchMs}, {"rangeM", rangeM}});
      t.Seduced = decoy != nullptr;

      if (t.DopplerRefS <= 0.0 || dwellDone) { t.DopplerRefRangeM = rangeM; t.DopplerRefS = simTimeS; }

      t.RangeM = rangeM;
      t.BearingDeg = bearingDeg;
      t.ElevAngleDeg = elevAngleDeg;
      t.AzDeg = azDeg;
      t.ElDeg = elDeg;
      t.LastLookS = simTimeS;
      if (t.Notched) {
        t.Notched = false;
        FBLog::Info("radar", "NOTCH_REGAIN", {{"track", t.TrackNum}, {"tgtRadialMs", tgtRadialMs},
            {"notchMs", DopplerNotchMs(rangeM)}, {"rangeNm", rangeM * kMToNm}});
      }
      seen[slot] = true;
      if (t.Hits < kHitsToFirm) t.Hits++;

      if (!t.Firm && t.Hits >= kHitsToFirm) {
        t.Firm = true;
        t.TrackNum = NextTrackNum_++;
        FBLog::Info("radar", "RADAR_CONTACT", {{"track", t.TrackNum}, {"rangeNm", rangeM * kMToNm},
            {"azDeg", azDeg}, {"elDeg", elDeg}, {"closureKt", t.ClosureMs * kMsToKt}});
      }
      if (t.Firm && simTimeS - t.LastIffS >= kIffPeriodS) {
        FBIffReply reply = Interrogate(*u);
        if (reply != t.Iff)
          FBLog::Info("radar", "IFF_REPLY", {{"track", t.TrackNum},
              {"reply", reply == FBIffReply::Friendly ? "friendly"
                        : reply == FBIffReply::NoReply ? "none" : "not-interrogated"}});
        t.Iff = reply;
        t.LastIffS = simTimeS;
      }
    }
  }

  /* `seen` ist ueber den Slot-Index des Durchlaufs oben indiziert; die Kompaktierung dort hielt beide
   * synchron. */
  double coastS = CoastS(v);
  for (int i = 0; i < TrackCount_;) {
    Track &t = Tracks_[i];
    if (seen[i] || simTimeS - t.LastLookS < coastS) {
      if (!seen[i]) t.Hits = 0;   /* Serie gebrochen; Re-Firming braucht kHitsToFirm frische Looks */
      i++;
      continue;
    }
    FBLog::Info("radar", "RADAR_DROP", {{"track", t.TrackNum}, {"lastRangeNm", t.RangeM * kMToNm},
        {"coastS", simTimeS - t.LastLookS}});
    for (int j = i; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
    for (int j = i; j < TrackCount_ - 1; j++) seen[j] = seen[j + 1];
    TrackCount_--;
  }
}

/* „Der erste" wird als „der NAECHSTE" gelesen: die momentane Keulenposition innerhalb des Sweeps ist
 * nicht modelliert, die Alternative waere ein willkuerlicher Registry-Tiebreak. */
void FBRadarSystem::UpdateLock(double simTimeS, bool autoAcquire) {
  if (LockedNum_ != 0) {
    for (int i = 0; i < TrackCount_; i++)
      if (Tracks_[i].TrackNum == LockedNum_) {
        if (!autoAcquire) break;   /* der Modus haelt keine Locks mehr — weiter zum Drop unten */
        return;
      }
    FBLog::Info("radar", "RADAR_LOST", {{"track", LockedNum_},
        {"reason", autoAcquire ? "track dropped" : "mode change"}});
    LockedNum_ = 0;
    /* Ein vom PILOTEN designierter Lock erfasst nicht von selbst neu — einen neuen zu nehmen ist eine
     * weitere Entscheidung mit demselben Preis. */
    if (Designated_) { Designated_ = false; return; }
  }
  if (!autoAcquire) return;

  const Track *best = nullptr;
  for (int i = 0; i < TrackCount_; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm) continue;
    if (!best || t.RangeM < best->RangeM) best = &t;
  }
  if (!best) return;
  LockedNum_ = best->TrackNum;
  FBLog::Info("radar", "RADAR_LOCK", {{"track", best->TrackNum}, {"rangeNm", best->RangeM * kMToNm},
      {"azDeg", best->AzDeg}, {"elDeg", best->ElDeg}, {"closureKt", best->ClosureMs * kMsToKt},
      {"iff", best->Iff == FBIffReply::Friendly ? "friendly"
              : best->Iff == FBIffReply::NoReply ? "unknown" : "not-interrogated"},
      {"t", simTimeS}});
}

/* Bewusst ein anderes Ereignis als RADAR_LOCK: auf dem Scope sehen sie gleich aus, im Debriefing sind
 * sie Gegensaetze — eines wurde entschieden, das andere geschah. */
bool FBRadarSystem::Designate(int trackNum, double simTimeS) {
  /* Das Raster muss mit dem Muster mitgehen: ohne diese Zeile kaeme der erste Single-Target-Look bis zu
   * einer ganzen Suchperiode spaeter (gemessen: 4 s eingefrorener Lock mit Zielgeschwindigkeit null). */
  NextScanS_ = simTimeS;
  if (trackNum == 0) {
    if (LockedNum_ == 0) return false;
    FBLog::Info("radar", "RADAR_BREAK", {{"track", LockedNum_}, {"t", simTimeS}});
    LockedNum_ = 0;
    Designated_ = false;
    return true;
  }
  for (int i = 0; i < TrackCount_; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm || t.TrackNum != trackNum) continue;
    LockedNum_ = trackNum;
    Designated_ = true;
    FBLog::Info("radar", "RADAR_DESIGNATE", {{"track", trackNum}, {"rangeNm", t.RangeM * kMToNm},
        {"azDeg", t.AzDeg}, {"elDeg", t.ElDeg}, {"closureKt", t.ClosureMs * kMsToKt},
        {"iff", t.Iff == FBIffReply::Friendly ? "friendly"
                : t.Iff == FBIffReply::NoReply ? "unknown" : "not-interrogated"}, {"t", simTimeS}});
    return true;
  }
  return false;
}

void FBRadarSystem::DropAllTracks(const char *reason, double simTimeS) {
  if (LockedNum_ != 0)
    FBLog::Info("radar", "RADAR_LOST", {{"track", LockedNum_}, {"reason", reason}});
  for (int i = 0; i < TrackCount_; i++)
    if (Tracks_[i].Firm)
      FBLog::Info("radar", "RADAR_DROP", {{"track", Tracks_[i].TrackNum}, {"reason", reason},
          {"t", simTimeS}});
  TrackCount_ = 0;
  LockedNum_ = 0;
  Designated_ = false;
}

void FBRadarSystem::Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
                        double simTimeS) {
  const FBRadarScanVolume &v = ActiveVolume();
  FBRadarBlock &b = state.Radar;
  b.Radiating = Powered_ && v.Active;
  b.ModeOrdinal = ModeOrdinal();
  b.IffTransponder = IffXpdr_;
  b.ScanAzHalfDeg = (float)v.AzHalfDeg;

  /* Die Karte haengt am selben Strahl wie das Luftbild: kein Strom, kein Muster, keine Karte. Dass
   * dieses Set beides GLEICHZEITIG liefert, ist die eine benannte Vereinfachung — ein APG-68 teilt
   * seine Modi zeitlich, aber ein A-A-Bild abzuschalten wuerde die Entscheidungen der Piloten-KI
   * aendern, und eine Anzeige darf keine Physik verstellen. */
  if (GroundMapping_ && b.Radiating)
    GroundMap_.Run(state.GroundMap, st.lat, st.lon, st.elev, st.yaw, kGroundMapAzHalfDeg,
                   kGroundMapRangeNm * kNmToM, kGroundMapFrameS, simTimeS);
  else
    GroundMap_.Stop(state.GroundMap);

  if (!b.Radiating || !net) {
    if (TrackCount_ > 0 || LockedNum_ != 0) DropAllTracks(Powered_ ? "radar standby" : "radar off", simTimeS);
    b.ContactCount = 0;
    b.LockIndex = -1;
    /* Ein nicht strahlendes Set hat kein Bild — kein leeres. Invalid, damit „nichts gefunden" nicht mit
     * „nicht geschaut" verwechselt werden kann. */
    b.H.Invalidate();
    ContactCount_ = 0;
    LockNm_ = -1.0f; LockAzDeg_ = LockElDeg_ = LockClosureKt_ = LockAgeS_ = 0.0f; LockIff_ = 0;
    return;
  }

  /* ActiveVolume() wird in JEDER Iteration neu gelesen, weil ein erworbener Lock das Muster und damit
   * die Frame-Zeit aendert. Der Waechter begrenzt das Nachholen und RESYNCHRONISIERT danach. */
  if (Resync_) { NextScanS_ = simTimeS; Resync_ = false; }

  int guard = 0;
  bool swept = false;
  while (NextScanS_ <= simTimeS && guard++ < 64) {
    swept = true;
    ScanFrame(st, *net, simTimeS);
    UpdateLock(simTimeS, ActiveVolume().AutoAcquire);
    NextScanS_ += ActiveVolume().FrameS;
  }
  if (NextScanS_ <= simTimeS) NextScanS_ = simTimeS + ActiveVolume().FrameS;

  /* Zwischen zwei Looks bewegt sich an der Geometrie eines Kontakts nichts — nur sein Alter. */
  int n = 0;
  b.LockIndex = -1;
  LockNm_ = -1.0f; LockAzDeg_ = LockElDeg_ = LockClosureKt_ = LockAgeS_ = 0.0f; LockIff_ = 0;
  for (int i = 0; i < TrackCount_ && n < kMaxRadarContacts; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm) continue;
    FBRadarContact &c = b.Contacts[n];
    c.TrackNum = t.TrackNum;
    c.RangeM = (float)t.RangeM;
    c.BearingDeg = (float)t.BearingDeg;
    c.ElevAngleDeg = (float)t.ElevAngleDeg;
    c.AzDeg = (float)t.AzDeg;
    c.ElDeg = (float)t.ElDeg;
    c.ClosureMs = (float)t.ClosureMs;
    c.LookAgeS = (float)std::fmax(0.0, simTimeS - t.LastLookS);
    c.Coasting = c.LookAgeS > (float)ActiveVolume().FrameS;
    c.Iff = t.Iff;
    if (t.TrackNum == LockedNum_) {
      b.LockIndex = n;
      LockNm_ = (float)(t.RangeM * kMToNm);
      LockAzDeg_ = (float)t.AzDeg;
      LockElDeg_ = (float)t.ElDeg;
      LockClosureKt_ = (float)(t.ClosureMs * kMsToKt);
      LockAgeS_ = c.LookAgeS;
      LockIff_ = t.Iff == FBIffReply::Friendly ? 2 : t.Iff == FBIffReply::NoReply ? 1 : 0;
    }
    n++;
  }
  b.ContactCount = n;
  ContactCount_ = n;
  /* Held statt Publish: der Zeitstempel nennt weiter den letzten abgeschlossenen Sweep. */
  if (swept) b.H.Publish(simTimeS);
  else b.H.Hold();
}

/* Zwei Formen, und ihr Unterschied ist der zwischen „jemand sucht" und „er hat MICH": suchend IST das
 * Scanvolumen das beleuchtete Fenster, im Single-Target-Track faellt es auf einen Bleistift zusammen.
 * Beide koerperfest, deshalb aendert sich das Bild eines Warnempfaengers, wenn der SENDER manoevriert. */
FBEmitterSignature FBRadarSystem::Emission() const {
  FBEmitterSignature e;
  const FBRadarScanVolume &v = ActiveVolume();
  if (!Powered_ || !v.Active) return e;   /* Mode::None — es strahlt nichts */
  e.Kind = EmitterKind();
  e.RangeM = (float)GateRangeM(v);
  if (v.SingleTarget && LockedNum_ != 0) {
    for (int i = 0; i < TrackCount_; i++) {
      if (Tracks_[i].TrackNum != LockedNum_) continue;
      e.Mode = FBEmitterMode::Track;
      e.AzCenterDeg = (float)Tracks_[i].AzDeg;
      e.ElCenterDeg = (float)Tracks_[i].ElDeg;
      e.AzHalfDeg = e.ElHalfDeg = (float)kTrackBeamHalfDeg;
      return e;
    }
  }
  e.Mode = FBEmitterMode::Search;
  e.AzCenterDeg = (float)v.AzCenterDeg;
  e.AzHalfDeg = (float)v.AzHalfDeg;
  e.ElCenterDeg = (float)v.ElCenterDeg;
  e.ElHalfDeg = (float)v.ElHalfDeg;
  return e;
}

void FBRadarSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("fcr_on");
  schema.Add("fcr_mode");
  schema.Add("fcr_contacts");
  schema.Add("fcr_lock");
  schema.Add("fcr_lock_nm", "nm");
  schema.Add("fcr_lock_az", "deg");
  schema.Add("fcr_lock_el", "deg");
  schema.Add("fcr_lock_clos", "kt");
  schema.Add("fcr_lock_age", "s");
  schema.Add("fcr_iff");
  schema.Add("iff_xpdr");
}

void FBRadarSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(Powered_ && ActiveVolume().Active);
  row.Push(ModeOrdinal());
  row.Push(ContactCount_);
  row.Push(LockedNum_);
  row.Push((double)LockNm_);
  row.Push((double)LockAzDeg_);
  row.Push((double)LockElDeg_);
  row.Push((double)LockClosureKt_);
  row.Push((double)LockAgeS_);
  row.Push(LockIff_);
  row.Push(IffXpdr_);
}

} // namespace FlightBox::Sensors
