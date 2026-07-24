#!/bin/sh
# Entrypoint for the merged sim container: bring up the World + Pilot, then the CC gateway.
# run.sh backgrounds iNav SITL and execs the JSBSim bridge (xpjsb); the hub (fbserver) then runs in the
# foreground and connects to iNav over loopback (AIRCRAFT_ADDR defaults to 127.0.0.1, UART3 :5762). The
# hub retries that connect ~1 Hz until iNav is up, so ordering is not a race.
set -e
/app/run.sh &
cd /app/flightbox
exec fbserver
