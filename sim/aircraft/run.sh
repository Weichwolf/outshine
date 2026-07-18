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
    export FBW SPAWN_SPEED FB_CRUISE FB_CLIMB_THR FB_CLIMB_PITCH FB_STALL FB_BANK FB_BANK_CLIMB \
           LOITER_ALT LOITER_RADIUS
fi

echo "[aircraft] starting iNav SITL (eeprom=$EEPROM chanmap=$CHANMAP)..."
/app/SITL.elf --path="$EEPROM" --sim=xp --simip=127.0.0.1 --simport=49000 \
    --chanmap="$CHANMAP" > /tmp/sitl.log 2>&1 &
sleep 2
echo "[aircraft] starting xp_bridge (FDM + MSP + flightbox link)..."
exec /usr/local/bin/xp_bridge
