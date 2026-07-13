#!/bin/sh
# Aircraft container entrypoint: real iNav SITL firmware + the X-Plane FDM bridge.
set -e
echo "[aircraft] starting iNav SITL..."
/app/SITL.elf --path=/app/eeprom.bin --sim=xp --simip=127.0.0.1 --simport=49000 \
    --chanmap=M01-01,S01-02,S02-03,S03-04 > /tmp/sitl.log 2>&1 &
sleep 2
echo "[aircraft] starting xp_bridge (FDM + MSP + flightbox link)..."
exec /usr/local/bin/xp_bridge
