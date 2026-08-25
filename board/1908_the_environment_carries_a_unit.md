Type: bug
State: open
Area: render, scenario
Tags: measured, lighting, units

# `<environment>` carries an illuminance, and the sky that now stands supplies it

`<key lux="40000">` declares an ILLUMINANCE. `<environment r="0.06" g="0.07" b="0.09">` declares
a bare triple with no unit at all, and the two are summed after the same exposure divides both.

## The arithmetic, and it is the whole finding

    ev100    = log2(40000 / 2.5)
    Exposure = 1 / (1.2 * 2^ev100)          = 5.208e-05
    key      = 40000 * Exposure             = 2.0833      -- independent of KeyLux
    ambient  = 0.06  * Exposure             = 3.125e-06
    ambient as sRGB                         = 0.80 of 255

**The declared environment contributes less than one level.** Whatever the key does not reach is
black by construction, and no value a scenario writes in that attribute can change that unless it
is written in the key's units -- a real clear sky's diffuse component is of order 1e4 lx, five
orders of magnitude from 0.06.

Measured on `apps/driver` at ab25c64f, first-person, 1280x720:

| region | mean max(RGB) | below RGB 8 |
|---|---|---|
| bonnet, key-lit | 105.29 | 24.1 % |
| sky through the screen | 103.33 | 3.1 % |
| cabin interior | 34.73 | **29.5 %** |
| ground through the side glass | 36.09 | 15.2 % |

## What will be true

- [ ] The ambient a subject sees is an ILLUMINANCE in the key's own units, or it is not a number
      a scenario types at all: a sphere with air now stands (board:1870) and its radiance is the
      irradiance every surface under it receives. TARGET names that stage `irradiance`.
- [ ] Proving case: a subject under a declared sphere reads a floor ABOVE the key's own shadow
      side, and the floor tracks the sun's elevation because the sky does. Negative control: the
      same subject with no sphere declared falls to the engine's own default and the case goes
      red on the floor it no longer has.
