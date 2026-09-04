#ifndef OUTSHINE_CLIENT_PLACECAMERA_H
#define OUTSHINE_CLIENT_PLACECAMERA_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Outshine.h>

namespace outshine {
class Engine;
class LogSink;
} // namespace outshine

namespace outshine::Shots {

inline constexpr int kWidePx = 1280;
inline constexpr int kHighPx = 720;
inline constexpr double kFrameBudgetMs = 1000.0 / 60.0;

struct Place {
  /// How a place is looked at. An EYE stands on the ground at head height and looks along a
  /// bearing, which is what a person would see and what the frame budget is measured against. A
  /// PLAN looks straight down through an orthographic camera over a stated span of ground: no
  /// perspective, no horizon, every metre the same size, which is the view that shows whether the
  /// generators put things where the map says. One is the product; the other is the drawing.
  enum class Seen : uint8_t { Eye, Plan };

  const char *Name = "";
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  double BearingDeg = 0.0;

  Seen From = Seen::Eye;

  /// PLAN only: the ground the frame covers top to bottom, in metres.
  double SpanM = 0.0;

  /// The instant the place is standing at, ISO 8601 UTC. The engine stands the sun from this
  /// and the place's coordinates, so a hand-set elevation would be a second answer to a question
  /// the clock already answers. Stated rather than live, because a live clock moves the picture's
  /// digest and the tree's determinism is not optional.
  const char *WhenUtc = "";
};

struct Shot {
  std::string Digest;
  std::string Wrote;
  std::string Why;

  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0;
  /// The same frames split at the handover: what the SIMULATION spent, and what the RENDERER did.
  /// A frame that reads seconds is not drawing for seconds, and until these two stand apart there
  /// is no way to say which half a late frame was in.
  double AdvanceP99Ms = 0.0, RenderP99Ms = 0.0;
  /// And the worst single frame of each, because a p99 over 120 frames hides one outlier.
  double AdvanceWorstMs = 0.0, RenderWorstMs = 0.0;
  std::size_t Frames = 0, OverBudget = 0, WorstAt = 0;

  /// The most the heap ever held live, over the preload AND the timed frames -- the third number
  /// this instrument owes, beside the frame and the preload. Sampled once a frame rather than
  /// continuously, so it is a floor on the true peak and never an overstatement; `PeakCostMs` says
  /// what asking cost, because a probe nobody can afford is a probe that gets switched off.
  double PeakHeapMB = 0.0;
  double PeakCostMs = 0.0;

  /// Building triangles the LAST REBUILD meshed -- a delta and not a total, so a settled frame
  /// over a world that changed nothing reads zero with a city in front of it (board:2063).
  double Triangles = 0.0;

  /// Tiles the LAST REBUILD laid on the ellipsoid because no elevation mesh stood for them. Same
  /// caveat: it describes that pass, never the picture that was written (board:2063).
  double BareTiles = 0.0;

  /// Mean absolute difference between neighbouring pixels along each row, in counts of 255. A
  /// picture of nothing is a vertical gradient and has none of it.
  double VariationAlongRows = 0.0;

  double StandingMs = 0.0, LoadingMs = 0.0, StreamedS = 0.0;

  /// How many frames the plan wanted before the picture settled, and the instant the SHOT itself
  /// was taken at -- a still of a moving subject says nothing without the second of these.
  double SettledOver = 0.0, PosedAtS = 0.0;
  bool Preloaded = false;
  bool Kept = false;

  /// Every measure the engine published for the frame this shot was taken on.
  std::vector<::outshine::Measure> Measures;
};

/// The declaration a place stands on, on its own, so a reader can write it down and read
/// it back rather than take the camera's word for what it declared.
[[nodiscard]] ::outshine::Scenario::Document ScenarioFor(const Place &place);

[[nodiscard]] Shot Draw(class ::outshine::Engine &engine,
                        std::string_view name,
                        bool tells,
                        std::string_view under = "places");

[[nodiscard]] std::span<const Place> Places();
[[nodiscard]] const Place *PlaceNamed(std::string_view name);
[[nodiscard]] double VariationAlongRows(std::span<const std::uint8_t> rgba, int wide, int high);

/// The statistic's own negative control: what a bare vertical gradient varies by along its rows.
/// A picture of nothing IS a vertical gradient, so this must come in far under the bar a real
/// frame is held to -- and if it ever does not, the bar separates nothing and every green under
/// it is worthless.
[[nodiscard]] double ControlVariation();
/// Where the engine says what it is doing while a place is taken. Null keeps it silent.
extern ::outshine::LogSink *Telling;

/// Whether the engine walks its own geometry and publishes what it finds -- coincident corners,
/// edges on one triangle, needles. Off by default: it costs 11.3 s of Shibuya's load and answers
/// questions that change when a GENERATOR changes, never between two frames.
extern bool Audits;

/// Whether the places are drawn with the GPU height-field lattice (board:2115) instead of the
/// CPU's ring mesh. Off by default: the references under build/shots/reference/ are the ring's.
extern bool Lattice;

[[nodiscard]] Shot Take(const Place &place, bool tells);

} // namespace outshine::Shots
#endif
