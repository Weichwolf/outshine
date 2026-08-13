Type: feature
Area: render
Tags: scope

**II.7 Weather**

- [x] Weather provider as an injected seam, with a data-local default and a live implementation
- [x] Wind as the air mass's own NED velocity at altitude, interpolated over pressure levels
- [x] Cloud cover per deck plus a ceiling that can legitimately be absent
- [x] Visibility, with an "unlimited" value outside the format's own window
- [ ] Precipitation: rain intensity, snow, sleet, hail
- [ ] Rain as particles or as a screen-space function — the reference uses particles; ours is undecided
- [ ] Wet-surface response coupled to precipitation history rather than to the current rate
- [ ] Puddles filling and drying
- [ ] Wind gusts as a time series rather than a constant
- [ ] Temperature and humidity fields, because snow line and fog need them
- [ ] Lightning as a light source
- [ ] Weather state blending over a declared interval, reproducibly
- [ ] Weather preset picked every four hours — REFUSED in that form: keying the sky's radiance would make us less physical than we already are. Only the tone shoulder, the fog lobe and the transition length are keyable
