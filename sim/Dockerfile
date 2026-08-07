# fb-sim: lean web host for the browser client. Serves the WASM app (web/) over HTTP and injects
# runtime env config (ORIGIN_LAT/LON, TILES_URL, SIM_UTC) into /config.js. The engine itself runs IN
# the browser; this container only hosts the app.
# Build context = sim/ (see .containerignore: only src/clients/SimHost.cpp is needed).
FROM docker.io/library/debian:trixie-slim
RUN apt-get update && apt-get install -y --no-install-recommends g++ libc6-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY src/clients/SimHost.cpp ./
RUN g++ -std=c++17 -O2 -Wall SimHost.cpp -o /usr/local/bin/fb-sim -lm
# web/ is a runtime bind-mount (live iteration: `make wasm`/`make worker` rebuilds show up on refresh).
# WEB_ROOT in SimHost.cpp is "web" relative to CWD, so the dir just needs to exist as a mount point.
RUN mkdir -p /app/web /app/shots
EXPOSE 8080
CMD ["fb-sim"]
