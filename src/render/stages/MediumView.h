#ifndef OUTSHINE_RENDER_STAGES_MEDIUMVIEW_H
#define OUTSHINE_RENDER_STAGES_MEDIUMVIEW_H

#include "Lens.h"
#include "MediumViewParameters.h"
#include "SkyPass.h"
#include "native/Render.h"
#include <expected>

namespace outshine::Render {

enum class MediumViewError { InvalidViewport, InvalidProjection, InvalidBasis, InvalidMedium };

[[nodiscard]] std::expected<MediumViewParameters, MediumViewError>
MakeMediumViewParameters(const Lens &lens,
                         Native::Viewport viewport,
                         const CameraBasis &basis,
                         SkyStanding sky,
                         const Medium &air) noexcept;

}
#endif
