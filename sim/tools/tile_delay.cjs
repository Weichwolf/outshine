/* THE ORDER THE TILE ANSWERS COME BACK IN, AS AN INPUT. Every request is forwarded to the tile
 * server unchanged and its response held back by a delay derived from the path and a seed, so one
 * seed is one arrival order and the same seed is the same order again. Nothing here is a fixture:
 * the bytes, the status and the headers are the server's own.
 *
 * It exists because a gate cannot sample what it does not control -- the host decides the completion
 * order, and on a warm cache it decided the same one six times running.
 *
 * Usage: node tools/tile_delay.cjs --port 8099 --upstream http://localhost:8081 --seed 1 --spread 400
 */
const http = require('http');

function arg(name, def) {
  const i = process.argv.indexOf('--' + name);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : def;
}

const port = Number(arg('port', 8099));
const upstream = new URL(arg('upstream', 'http://localhost:8081'));
const seed = Number(arg('seed', 1));
const spreadMs = Number(arg('spread', 400));

/* FNV-1a over the path, so the delay is a property of WHICH tile is asked for and of the seed --
 * a request repeated after a retry must not overtake itself. */
function delayMs(path) {
  let h = (2166136261 ^ seed) >>> 0;
  for (let i = 0; i < path.length; i++) {
    h = (h ^ path.charCodeAt(i)) >>> 0;
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h % (spreadMs + 1);
}

const server = http.createServer((req, res) => {
  const out = http.request(
    { host: upstream.hostname, port: upstream.port, path: req.url, method: req.method,
      headers: { ...req.headers, host: upstream.host } },
    (up) => {
      const chunks = [];
      up.on('data', (c) => chunks.push(c));
      up.on('end', () => {
        const headers = { ...up.headers };
        delete headers['transfer-encoding'];
        delete headers['connection'];
        setTimeout(() => {
          res.writeHead(up.statusCode, headers);
          res.end(Buffer.concat(chunks));
        }, delayMs(req.url));
      });
    });
  out.on('error', (e) => {
    res.writeHead(502, { 'content-type': 'text/plain' });
    res.end(String(e));
  });
  req.pipe(out);
});

server.listen(port, () => {
  process.stdout.write(`tile_delay: :${port} -> ${upstream.origin} seed=${seed} spread=${spreadMs}ms\n`);
});
