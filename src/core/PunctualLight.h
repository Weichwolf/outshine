#ifndef PUNCTUALLIGHT_H
#define PUNCTUALLIGHT_H

namespace outshine {

/* THE THREE SHAPES A LIGHT WITH NO AREA CAN HAVE, which is `KHR_lights_punctual`'s own enumeration
 * and therefore the vocabulary content declares them in. A boolean pair would spell
 * a fourth thing that is nothing (`Enum.2`), and the three differ in what they carry rather than in
 * a flag: a directional light has a direction and no position, a point light a position and no
 * direction, a spot both plus its two cone angles. */
enum class LightKind { Directional, Point, Spot };

/* A LIGHT WITH NO AREA, in the format's own units and frame.
 *
 * THE UNITS ARE NOT THE SAME BETWEEN THE ARMS AND THAT IS THE FORMAT'S DOING, not a looseness here:
 * `Intensity` is **lux (lm/m^2)** for a directional light -- the illuminance on a surface facing the
 * beam -- and **candela (lm/sr)** for a point or a spot,
 * from which the illuminance at distance d is `Intensity / d^2`. One field carrying two units is
 * the format's spelling and re-deriving a single unit here would put a conversion inside a reader.
 *
 * `Colour` IS A FILTER AND NOT A BRIGHTNESS. `KHR_lights_punctual`: "The `intensity` represents the
 * luminous intensity that the light would emit if it were colored pure white. The `color` property
 * acts as a wavelength-specific multiplier." So the product is `Colour * Intensity` per channel and
 * three lights of colours (1,0,0), (0,1,0), (0,0,1) are one light of colour (1,1,1) -- which is
 * exactly what `PointLightIntensityTest` measures.
 *
 * `Position` AND `Direction` ARE THE PLACED LIGHT'S, in whatever frame the producer states; the
 * struct carries no frame of its own. glTF places a light on a node and the node's -Z is the beam,
 * so a consumer that wants world coordinates resolves the node first (`Gltf::Subject`).
 *
 * `RangeM` IS THE DISTANCE THE LIGHT ENDS AT, 0 WHERE THE FILE DECLARES NONE, and it is applied
 * rather than dropped. The extension words it as a hint a client MAY use, which reads like something
 * optional -- MEASURED, IT IS NOT: `PointLightIntensityTest` puts six test panels 2.25 m apart, each
 * with its own light 0.2 m in front of it, and declares a range of 1.125 m, which is exactly half
 * that spacing. A light in glTF is scene-wide however its node is parented, so without the range
 * every panel is lit by all eight lights, each panel sees a DIFFERENT set of neighbours, and the
 * asset's own criterion -- that three coloured lights equal one white one -- is measuring the
 * neighbours instead. The range is what makes each panel see its own light and nothing else. */
struct PunctualLight {
  LightKind Kind = LightKind::Directional;
  float Colour[3] = {1.0f, 1.0f, 1.0f};
  float Intensity = 1.0f;       /* lux under Directional, candela under Point and Spot */
  float Position[3] = {0.0f, 0.0f, 0.0f};
  float Direction[3] = {0.0f, 0.0f, -1.0f};  /* the beam, i.e. the node's -Z */
  /* The two cone angles of a spot, radians from the beam axis. glTF's defaults, and the extension
   * requires `inner < outer <= pi/2`, which the reader refuses on rather than clamping. */
  float InnerConeRad = 0.0f;
  float OuterConeRad = 0.7853981633974483f;  /* pi/4 */
  float RangeM = 0.0f;          /* 0 is the format's "no cutoff", not a light of zero reach */
};

} // namespace outshine
#endif
