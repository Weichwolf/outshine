#include "GeometryCullStage.h"
#include <expected>

namespace outshine::Render {
Native::Result<void> GeometryCullStage::EncodeCompletion(const FrameContext &ctx,
                                                         const PassRecording &into,
                                                         const SelectionProjection &projection) {
  Cache_.Invalidate();
  Swept_ = Source_.Jobs;
  CullThisFrame_ = Source_.Resident != nullptr && Source_.Jobs != 0;
  if (!CullThisFrame_) { return {}; }
  if (Source_.Selection != SelectionKind::Completion || PyramidBuffer_ == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "completion requires current scene depth"});
  }
  OccludeThisFrame_ = true;
  Standing(ctx, projection);
  ++PredictionEpoch_;
  View_.PredictionEpoch = {static_cast<uint32_t>(PredictionEpoch_),
                           static_cast<uint32_t>(PredictionEpoch_ >> 32)};
  LastGeometryGeneration_ = Source_.Generation;
  LastPredictionGeneration_ = Source_.PredictionGeneration;
  return EncodeCull(into);
}
}
