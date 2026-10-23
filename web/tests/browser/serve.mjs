// MIT License © 2026 Binary Dice Games
// A static file server for the web tests: `root` folders searched in order,
// on 127.0.0.1 and a free port. Shared by editor.mjs and app.mjs.

import { readFile, stat } from 'node:fs/promises';
import { createServer } from 'node:http';
import { extname, join, normalize } from 'node:path';

export const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css',
  '.js': 'text/javascript',
  '.mjs': 'text/javascript',
  '.wasm': 'application/wasm',
  '.json': 'application/json',
  '.webmanifest': 'application/manifest+json',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
  '.ttf': 'font/ttf',
  '.txt': 'text/plain; charset=utf-8',
  '.md': 'text/plain; charset=utf-8',
  '.yaml': 'text/plain; charset=utf-8',
  '.csv': 'text/csv',
};

/**
 * Serves `roots` (a list of folders), with `extra` routes (path -> handler(req, res)).
 * @returns {Promise<{base: string, close: () => void, requests: string[]}>}
 */
export async function serve(roots, extra = {}, options = {}) {
  const requests = [];
  const server = createServer(async (req, res) => {
    const url = new URL(req.url, 'http://x');
    const path = decodeURIComponent(url.pathname);
    requests.push(path);
    if (options.cors) {
      res.setHeader('access-control-allow-origin', '*');
    }
    if (extra[path]) {
      await extra[path](req, res, url);
      return;
    }
    for (const root of roots) {
      let file = join(root, normalize(path));
      try {
        if ((await stat(file)).isDirectory()) {
          file = join(file, 'index.html');
        }
        const body = await readFile(file);
        res.writeHead(200, { 'content-type': TYPES[extname(file)] ?? 'application/octet-stream' }).end(body);
        return;
      } catch {
        // next root
      }
    }
    res.writeHead(404).end('not found');
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  return { base: `http://127.0.0.1:${server.address().port}`, close: () => server.close(), requests };
}
