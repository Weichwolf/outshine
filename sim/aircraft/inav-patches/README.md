# iNav SITL patches

`sim/inav-src/` (the iNav 9.1.0 checkout) is gitignored — it's vendored and rebuilt.
These patches capture the FlightBox-specific source changes needed for the X-Plane
bridge, so a fresh checkout is reproducible.

## 0001-xplane-gps-heading-init.patch

In `--sim=xp` (truth-attitude) mode, iNav sets attitude directly via `imuSetAttitudeRPY`
and skips `imuCalculateEstimatedAttitude`, where `gpsHeadingInitialized` is normally set.
Without a magnetometer that leaves `estHeadingStatus = EST_NONE` forever, so home never
gets set and every RTH degrades to emergency landing. The patch initializes GPS heading
from the (valid) GPS course in the truth-attitude branch.

Apply + rebuild:

```
cd sim/inav-src
git apply ../aircraft/inav-patches/0001-xplane-gps-heading-init.patch
cd build-sitl && make SITL.elf
cp bin/SITL.elf ../../aircraft/SITL.elf
```

The other half of the RC-loss→RTH fix (GPS `fixType` enum, RTH config) lives in
`aircraft/xp_bridge.c` and `aircraft/inav-config.txt` respectively — both tracked.
