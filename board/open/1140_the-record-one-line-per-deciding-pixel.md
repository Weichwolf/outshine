Type: task
Parent: 1137
Area: render
Tags: instrument
Depends: 1138, 1139

**The record: one line per deciding pixel, and the population is the failing one**

The verdict shape of this instrument, decided rather than left to the round that builds it.

**IT IS A PRINTED RECORD, NOT A METRIC.** Per pixel, one line, every quantity side by side: coordinates ·
both sides' linear values and their codes · what each side says covers it (`board:1138`) · both uvs, their
difference in texels and both taps (`board:1139`) · where the case shades, both sides' term split
(`board:1142`). Counts of the classes it finds go out as `Direction::Reported` metrics — *pixels where the
surface identity disagrees*, *pixels where the uvs disagree beyond a texel*, *pixels refused and why* —
and **nothing here gets a bound**. A threshold on an investigation instrument is a number nobody derived,
and the population it would be computed over changes with every case.

**WHAT STOPS IT BECOMING A METRIC NOBODY READS** is that its population is *derived from the verdict it
explains*: the pixels are the ones the picture bound's own worst-channel table already selects, so the
record exists exactly where a case fails and is empty where it passes. A fixed list of coordinates would
survive the reframing that invalidates it and would then read as a finding — the failure mode this tree
names as *the number was right and about something else*.

**THE COUNT COMES BEFORE THE VERDICT**, as `ScoreShadingNormal` already does it: how many pixels were
interrogated, how many were refused, and by which predicate. A record over three pixels and one over
three hundred thousand are different claims and only one of them is a verdict.

**Every refusal is by predicate and never by threshold.** A pixel with no shading normal, a pixel whose
material identity disagrees, a pixel on a blended surface — each is excluded by a statement about what it
is, counted, and named in the record.

**Done when** running the four cases of `board:1136` prints, for each of their deciding pixels, a line
that names a mechanism or a refusal — and when `coverage/negative-scale`'s single pixel reads as a surface
swap rather than as a colour difference.
