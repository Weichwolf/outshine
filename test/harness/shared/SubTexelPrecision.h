/* HOW FINELY THIS DEVICE'S SAMPLER DIVIDES THE SPAN BETWEEN TWO TEXELS, and it is a MEASURED
 * property of this A18 Pro rather than a number read out of a table.
 *
 * NOTHING PUBLISHES IT. Apple documents no sub-texel precision for Metal; MoltenVK hard-codes
 * Vulkan's minimum of 4 with a comment that Metal does not expose the value, the M1 is reported to
 * behave as 8, and about 89 % of the Vulkan hardware database says 8 -- none of which is a number
 * about this device. So it is measured here and the measurement is a test
 * (`test/shader/TheSamplerSnapsSubTexelWeightsToTheDeclaredCount.cpp`), which is what keeps this
 * line from going stale: the constant below and the device must agree or that test is red.
 *
 * IT STANDS AT test/ ROOT BECAUSE TWO SUITES READ IT. `shader` measures it; `render` derives the
 * picture bound's sampler term from it (`test/render/khronos/glTF/PictureBound.h`). A second spelling in either
 * place would be the same statement twice. */
#ifndef TEST_SUBTEXELPRECISION_H
#define TEST_SUBTEXELPRECISION_H

namespace outshine::Test {

/* [MEASURED] 2026-08-13, Apple A18 Pro, macOS 26.4.1, SDL3 over Metal: a two-texel R32_FLOAT ramp
 * sampled with a linear clamp sampler at 65 536 offsets across the open span between the two texel
 * centres returns 257 distinct weights, every step between them exactly 1/256. The divisions are
 * 2^8; the 257th endpoint is round-to-nearest and not a 257th division. */
inline constexpr int kSubTexelPrecisionBits = 8;

/* The number of positions the sampler admits inside one texel span, which is what the bit count
 * means: Vulkan defines `subTexelPrecisionBits` as exactly this and Metal exposes no equivalent. */
inline constexpr int kSubTexelDivisions = 1 << kSubTexelPrecisionBits;

} // namespace outshine::Test
#endif
