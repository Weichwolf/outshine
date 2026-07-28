/* FlightBox — FBIrstSystem: the PASSIVE OPTICAL slot, and the third way a unit can learn that another
 * one exists. It is neither of the two that came before it:
 *
 *   FBRadarSystem  asks "what is out there", transmits to find out, and pays with a beam that warns
 *                  exactly whom it is looking at.
 *   FBRwrSystem    asks "who is looking at ME", and can only ever hear what somebody else RADIATES.
 *   FBIrstSystem   asks "what is out there" and pays NOTHING — it sees HEAT. Nobody is warned, nothing
 *                  is transmitted, and there is no reply to interrogate.
 *
 * THE PRICE IS THE SHAPE OF WHAT IT PRODUCES. An infrared search-and-track head measures an ANGLE. It
 * does not measure range (FBIrstContact carries none, except from the laser), it cannot ask IFF (the
 * interrogator does not operate with it), and what it can see at all depends on where the aircraft's
 * heat is pointing: a jet seen from behind shows its nozzles and is visible far out, the same jet
 * head-on is a warm skin at less than half that range. That aspect law, the afterburner exception and
 * the cloud deck in the line of sight are the whole detection model, and each of the three is a
 * separate documented statement rather than one fudge factor.
 *
 * THE PERCEPTION BOUNDARY GROWS BY ONE FILE. This class reads units/FBUnitRegistry, which makes it the
 * FIFTH file in the tree allowed to (three sensor slots + the missile's uplink receiver were four).
 * That is a deliberate widening, listed in tools/verify_layers.py's RESTRICTED table and argued in
 * doc/sensors.md §1.2: a passive sensor is still a SENSOR, and the boundary was never "at most four
 * files" — it is "only simulated sensors, and each one pays a stated price". This one pays in range,
 * in identity and in weather.
 *
 * WHAT IT PUBLISHES: exactly one FBState block (FBIrstBlock), fixed capacity, no allocation in the
 * tick, absolute time base, three-state validity — the same contract as every other slot. */
#ifndef FBIRSTSYSTEM_H
#define FBIRSTSYSTEM_H

#include "FBCloudDensity.h"
#include "FBIrstContact.h"
#include "FBRadarSystem.h"
#include "FBState.h"
#include "FBTeam.h"
#include "FBTelemetry.h"
#include "FBUnits.h"
#include "FBFdm.h"

#include "FBUnit.h"

namespace FlightBox::Units { class FBUnitRegistry; }

namespace FlightBox::Sensors {

/* The head's FIELD OF REGARD, deliberately the same value type the radar uses for its antenna pattern:
 * a window in body-referenced az/el, swept in FrameS, with a gate and the two behaviour flags. The
 * fields mean what they say for an optical head too, with one reading noted here so nobody has to
 * guess: `RangeM` is the CLEAN-AIR REAR-ASPECT reach — the best case, which the aspect law then scales
 * down per target — and `Active` means the head is uncaged and looking, not that it radiates (it never
 * does). */
using FBIrstFieldOfRegard = FBRadarScanVolume;

class FBIrstSystem : public FBTelemetrySource {
public:
  /* Explicit PLACEHOLDERS for "some infrared search head", exactly as FBRadarSystem's kGeneric*: a
   * module with a real one overrides ActiveField(). */
  static constexpr double kGenericRangeM = 15000.0;
  static constexpr double kGenericAzHalfDeg = 30.0;
  static constexpr double kGenericElHalfDeg = 15.0;
  static constexpr double kGenericFrameS = 5.0;

  /* Two consecutive looks, the same figure and the same reason as the radar's: acquisition has to cost
   * measurable TIME, and a source flashing once through the field is never reported. */
  static constexpr int kHitsToFirm = 2;
  static constexpr double kCoastFrames = 3.0;
  static constexpr double kMinCoastS = 1.0;

  /* ---- THE ASPECT LAW [SET, endpoints DOC]. Sources give two numbers and no curve: detection is
   * "more effective from the rear", with clean-air detection running from the far figure to the near
   * one. Aspect A is the standard air-combat one: the angle at the TARGET between its own heading and
   * the line of sight FROM the sensor — A = 0 means this sensor sits directly astern of it and looks
   * straight into the nozzle, A = 180 is head-on.
   *
   *     reach(A) = RearM * f + FrontFraction * RearM * (1 - f),   f = (1 + cos A) / 2
   *
   * f is the smoothest law that hits both documented endpoints exactly; a cos-clamped plume-area law
   * would be equally defensible and the source supports neither, which is why the choice is marked and
   * kept in ONE line instead of spread through the scan. */
  static constexpr double kFrontFraction = 0.4;   /* 10 km / 25 km, the documented pair's own ratio */

  /* [SET] The afterburner exception. The source says an afterburning aircraft is an exception to the
   * size rule and gives no figure. For a point source, irradiance falls as I/r², so range scales as
   * the square root of radiant intensity: 1.5 corresponds to a plume about 2.25x the intensity of the
   * unaugmented aircraft. Stated that way so the number can be argued with instead of tuned. */
  static constexpr double kAfterburnerRangeFactor = 1.5;

  /* [SET] The cover above which a cloud deck is treated as a LID. A deck below this is broken enough
   * that a line of sight through a hole is the normal case, and modelling that needs the horizontal
   * structure of the density field (core/FBCloudDensity), which is the declared next step. Same
   * threshold core/FBCloudDensity already calls "broken". */
  static constexpr double kCloudMaskCover = 0.5;

  ~FBIrstSystem() override = default;

  void SetIdentity(int selfId, FBUnitTeam team) { SelfId_ = selfId; SelfTeam_ = team; }

  void SetPowered(bool on);
  bool Powered() const { return Powered_; }

  /* The LASER RANGEFINDER: the one active thing an optical station does, and the only source of metres
   * this sensor has. Armed by command, effective only inside its own (much shorter) reach and only on
   * the tracked source. It does NOT produce an emission — the target's RWR cannot detect it
   * (doc/modules/mig29/radar-sensors.md §6.4), which is exactly why the "stealth attack" of that file
   * is possible at all. */
  void SetLaserArmed(bool on);
  bool LaserArmed() const { return LaserArmed_; }

  /* The pilot's designation, the same verb and the same anonymity as the radar's: the value is the
   * PUBLISHED track number, 0 = break. A field with AutoAcquire locks by itself (the close-combat
   * property); a wide search field does not. */
  bool Designate(int trackNum, double simTimeS);
  bool Locked() const { return LockedNum_ != 0; }
  int  LockedTrackNum() const { return LockedNum_; }

  /* The weather the OWNER resolved, handed in exactly like the ground elevation the radar altimeter
   * reads: one sample per decision tick, never a second query of its own. Never set = a clear sky,
   * which is what every mission without a `wx` line has. */
  void SetSky(const FBCloudSky &sky) { Sky_ = sky; }

  virtual void Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
                   double simTimeS);

  const char *TelemetryName() const override { return "irst"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* THE override point, the same one-getter-carries-a-mode-set construction as FBRadarSystem's. */
  virtual const FBIrstFieldOfRegard &ActiveField() const { return Search_; }
  virtual int ModeOrdinal() const { return 0; }

  /* The laser's own reach. Its own hook because it is a different device from the IR head, with its
   * own (much shorter) range, and because a head without a laser returns 0 and is done. */
  virtual double LaserRangeM() const { return 0.0; }

  void SetSearchField(const FBIrstFieldOfRegard &f) { Search_ = f; }

  /* Clean-air reach against THIS target: the aspect law, the afterburner exception, nothing else. Kept
   * virtual so an airframe with a documented countermeasure or atmospheric law can extend it in one
   * place rather than reimplementing the scan. */
  virtual double DetectRangeM(const FBIrstFieldOfRegard &f, const Units::FBUnitPose &tgt,
                              bool afterburner, double bearingDeg) const;

  /* Does a cloud deck stand between the two altitudes? The whole weather coupling, deliberately in one
   * overridable function. */
  bool CloudMasked(double ownAltM, double tgtAltM) const;

private:
  struct Track {
    int    UnitId = 0;
    int    TrackNum = 0;
    double BearingDeg = 0.0, ElevAngleDeg = 0.0, AzDeg = 0.0, ElDeg = 0.0;
    double LastLookS = 0.0;
    double RangeM = 0.0;        /* the TRUE geometric range: internal only, never published */
    bool   HasLaserRange = false;
    double LaserRangeM = 0.0;
    int    Hits = 0;
    bool   Firm = false;
  };

  void ScanFrame(const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry &net, double simTimeS);
  void UpdateLock(double simTimeS, bool autoAcquire);
  void DropAllTracks(const char *reason, double simTimeS);

  Track Tracks_[kMaxIrstContacts]{};
  int TrackCount_ = 0;
  int NextTrackNum_ = 1;
  int LockedNum_ = 0;
  bool Designated_ = false;

  int SelfId_ = 0;
  FBUnitTeam SelfTeam_ = FBUnitTeam::Friendly;
  bool Powered_ = false;      /* an optical station is switched on deliberately ("turn on IR mode after
                               * takeoff"), so the default is a caged head and an Invalid block */
  bool LaserArmed_ = false;
  FBIrstFieldOfRegard Search_{};
  FBCloudSky Sky_{};
  double NextScanS_ = 0.0;

  int MaskedCount_ = 0;
  int ContactCount_ = 0;
  float LockAzDeg_ = 0.0f, LockElDeg_ = 0.0f, LockAgeS_ = 0.0f, LockNm_ = -1.0f;
  int BlockStatus_ = 0;
};

} // namespace FlightBox::Sensors
#endif
