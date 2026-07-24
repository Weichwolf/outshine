#!/bin/sh
# Aircraft container entrypoint: real iNav SITL firmware + the FDM bridge.
# Per-aircraft config comes from the AIRCRAFT MODEL (models/<name>/profile.env): chanmap, eeprom,
# JSBSim spawn speed, FBW-override, and the airframe-agnostic autopilot flight profile. Nothing
# aircraft-specific is hardcoded here or in the C — set AIRCRAFT to pick a model.
set -e

EEPROM=/app/eeprom.bin
CHANMAP=M01-01,S01-02,S02-03,S03-04
if [ -n "${AIRCRAFT:-}" ] && [ -f "/app/models/$AIRCRAFT/profile.env" ]; then
    echo "[aircraft] profile: models/$AIRCRAFT/profile.env"
    . "/app/models/$AIRCRAFT/profile.env"
    [ -f "/app/models/$AIRCRAFT/eeprom.bin" ] && EEPROM="/app/models/$AIRCRAFT/eeprom.bin"
    export SERVO_SLEW AUXMAP FBW SPAWN_ALT SPAWN_SPEED FB_VR FB_HANDOFF FB_CRUISE FB_CLIMB_THR FB_CLIMB_PITCH FB_STALL FB_BANK FB_BANK_CLIMB \
           LOITER_ALT LOITER_RADIUS
fi

# FB_TIME_SCALE>1 accelerates the whole aircraft (vanilla SITL + bridge share the shimmed clock)
# so headless missions run faster than real time. Default 1 = real time. libfbclock passthrough at 1.
export FB_TIME_SCALE="${FB_TIME_SCALE:-1}"
PRELOAD=/usr/local/lib/libfbclock.so

# Truth-attitude (attitude given via --sim=xp, NOT --useimu): flies stable with no AHRS-convergence lag.
# navIsHeadingUsable is satisfied by SENSOR_MAG (mag_hardware=FAKE) + the given yaw — so native NAV works
# WITHOUT the old gpsHeadingInitialized patch. iNav stays bit-vanilla.
echo "[aircraft] starting iNav SITL (eeprom=$EEPROM chanmap=$CHANMAP, time-scale=$FB_TIME_SCALE)..."
LD_PRELOAD="$PRELOAD" /app/SITL.elf --path="$EEPROM" --sim=xp --simip=127.0.0.1 --simport=49000 \
    --chanmap="$CHANMAP" > /tmp/sitl.log 2>&1 &
sleep 2
echo "[aircraft] starting xpjsb (JSBSim World <-> iNav --sim=xp; no autopilot, iNav flies)..."
exec env LD_PRELOAD="$PRELOAD" MODELS_ROOT=/app/models /usr/local/bin/xpjsb
