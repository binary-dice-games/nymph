// MIT License © 2026 Binary Dice Games
// A minimal WebTorrent tracker for the collaboration tests
// (spec/18_collaboration plan step 4): the tracker protocol browsers use to
// trade WebRTC offers in a swarm, over a WebSocket server written here (no
// dependencies), on 127.0.0.1 and a free port. It keeps a log of every
// message so tests can check that no readable SDP went through.

import { createHash } from 'node:crypto';
import { createServer } from 'node:http';

const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

/** One WebSocket connection: text messages in and out. */
class Socket {
  constructor(socket, onMessage, onClose) {
    this.socket = socket;
    this.buffer = Buffer.alloc(0);
    this.parts = [];
    socket.on('data', (d) => {
      this.buffer = Buffer.concat([this.buffer, d]);
      this.parse(onMessage);
    });
    socket.on('close', onClose);
    socket.on('error', () => socket.destroy());
  }

  parse(onMessage) {
    for (;;) {
      const b = this.buffer;
      if (b.length < 2) {
        return;
      }
      const fin = (b[0] & 0x80) !== 0;
      const op = b[0] & 0x0f;
      const masked = (b[1] & 0x80) !== 0;
      let len = b[1] & 0x7f;
      let at = 2;
      if (len === 126) {
        if (b.length < 4) {
          return;
        }
        len = b.readUInt16BE(2);
        at = 4;
      } else if (len === 127) {
        if (b.length < 10) {
          return;
        }
        len = Number(b.readBigUInt64BE(2));
        at = 10;
      }
      const maskAt = at;
      if (masked) {
        at += 4;
      }
      if (b.length < at + len) {
        return;
      }
      const payload = Buffer.from(b.subarray(at, at + len));
      if (masked) {
        for (let i = 0; i < payload.length; ++i) {
          payload[i] ^= b[maskAt + (i % 4)];
        }
      }
      this.buffer = b.subarray(at + len);
      if (op === 8) {
        this.socket.end();
        return;
      }
      if (op === 9) {
        this.frame(10, payload); // ping: pong
        continue;
      }
      if (op === 1 || op === 0) {
        this.parts.push(payload);
        if (fin) {
          onMessage(Buffer.concat(this.parts).toString('utf8'));
          this.parts = [];
        }
      }
    }
  }

  frame(op, payload) {
    const len = payload.length;
    const head = len < 126 ? Buffer.from([0x80 | op, len]) : len < 65536 ? Buffer.from([0x80 | op, 126, len >> 8, len & 255]) : Buffer.concat([Buffer.from([0x80 | op, 127]), (() => {
      const l = Buffer.alloc(8);
      l.writeBigUInt64BE(BigInt(len));
      return l;
    })()]);
    if (!this.socket.destroyed) {
      this.socket.write(Buffer.concat([head, payload]));
    }
  }

  send(text) {
    this.frame(1, Buffer.from(text, 'utf8'));
  }

  close() {
    this.socket.destroy();
  }
}

/**
 * Starts a tracker. @returns {Promise<{url, log: object[], clients: () => number,
 * close: () => void}>}
 */
export async function startTracker() {
  const swarms = new Map(); // info_hash -> Map(peer_id -> socket)
  const log = [];
  const sockets = new Set();
  let connections = 0;
  const server = createServer((req, res) => res.writeHead(426).end());
  server.on('upgrade', (req, socket) => {
    const key = req.headers['sec-websocket-key'];
    if (!key) {
      socket.destroy();
      return;
    }
    const accept = createHash('sha1').update(key + GUID).digest('base64');
    socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`);
    connections++;
    const joined = new Set();
    const ws = new Socket(
      socket,
      (text) => {
        let m;
        try {
          m = JSON.parse(text);
        } catch {
          return;
        }
        log.push(m);
        if (m.action !== 'announce' || typeof m.info_hash !== 'string' || typeof m.peer_id !== 'string') {
          return;
        }
        const swarm = swarms.get(m.info_hash) ?? new Map();
        swarms.set(m.info_hash, swarm);
        swarm.set(m.peer_id, ws);
        joined.add([m.info_hash, m.peer_id]);
        if (m.answer && typeof m.to_peer_id === 'string') {
          swarm.get(m.to_peer_id)?.send(JSON.stringify({ action: 'announce', info_hash: m.info_hash, peer_id: m.peer_id, answer: m.answer, offer_id: m.offer_id }));
          return;
        }
        ws.send(JSON.stringify({ action: 'announce', interval: 120, info_hash: m.info_hash, complete: 0, incomplete: swarm.size }));
        // Each offer to a different other peer of the swarm, as public trackers do.
        const others = [...swarm.entries()].filter(([id]) => id !== m.peer_id).sort(() => Math.random() - 0.5);
        (m.offers ?? []).forEach((o, i) => {
          const to = others[i % Math.max(1, others.length)];
          if (to) {
            to[1].send(JSON.stringify({ action: 'announce', info_hash: m.info_hash, peer_id: m.peer_id, offer: o.offer, offer_id: o.offer_id }));
          }
        });
      },
      () => {
        sockets.delete(ws);
        for (const [hash, id] of joined) {
          if (swarms.get(hash)?.get(id) === ws) {
            swarms.get(hash).delete(id);
          }
        }
      },
    );
    sockets.add(ws);
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  return {
    url: `ws://127.0.0.1:${server.address().port}`,
    log,
    connections: () => connections,
    clients: () => sockets.size,
    close: () => {
      for (const s of sockets) {
        s.close();
      }
      server.close();
    },
  };
}
