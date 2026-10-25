// MIT License © 2026 Binary Dice Games
// spec/18_collaboration plan steps 2, 3 and 5: the whiteboard model
// converges, the session core (rooms, roster, relay, framing, sealing)
// works over in-memory links, and web/collab/ imports nothing outside
// itself.
//
//   node collab.mjs <build>/web <nymph source dir>

import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [webDir, sourceDir] = process.argv.slice(2);
void webDir;
const collabDir = join(sourceDir, 'web', 'collab');
const wb = await import(pathToFileURL(join(collabDir, 'collab-whiteboard.js')).href);
const cs = await import(pathToFileURL(join(collabDir, 'collab-session.js')).href);

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const eq = (a, b, what) => {
  if (a !== b) {
    throw new Error(`${what}: expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`);
  }
};
const ok = (v, what) => eq(!!v, true, what);

/** A seeded PRNG (mulberry32), so failures repeat. */
function rng(seed) {
  return () => {
    seed |= 0;
    seed = (seed + 0x6d2b79f5) | 0;
    let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/** Waits for `cond`, at most `ms`. */
async function until(cond, ms = 3000, what = 'condition') {
  const end = Date.now() + ms;
  while (!(await cond())) {
    if (Date.now() > end) {
      throw new Error(`timed out waiting for ${what}`);
    }
    await new Promise((r) => setTimeout(r, 5));
  }
}

const state = (m) => JSON.stringify(m.live().map((i) => [i.id, i.stamp, i.kind, i.text ?? i.points ?? i.name, i.x ?? null]));

// ── Whiteboard model ──────────────────────────────────────────────────────────

test('three models given the same operations in random orders converge', () => {
  const random = rng(18);
  for (let round = 0; round < 200; ++round) {
    // Three peers make changes concurrently, each on what it has seen.
    const peers = ['aaaa', 'bbbb', 'cccc'].map((id) => wb.createModel(id));
    const ops = [];
    for (let k = 0; k < 12; ++k) {
      const p = peers[Math.floor(random() * 3)];
      const live = p.live();
      const roll = random();
      let op;
      if (roll < 0.45 || live.length === 0) {
        op = p.add({ kind: 'text', x: Math.round(random() * 100), y: 5, size: 18, text: `n${k}`, color: '#e5484d' });
      } else if (roll < 0.7) {
        op = p.change(live[Math.floor(random() * live.length)].id, { x: Math.round(random() * 100) });
      } else if (roll < 0.9) {
        op = p.remove(live[Math.floor(random() * live.length)].id);
      } else {
        op = p.clear();
      }
      ops.push(op);
      // Some peers see it at once (so later changes build on it).
      for (const q of peers) {
        if (q !== p && random() < 0.5) {
          q.apply(op);
        }
      }
    }
    // Each peer then gets every operation, in its own order, some twice.
    const finals = peers.map((q) => {
      const order = [...ops, ...ops.filter(() => random() < 0.3)].sort(() => random() - 0.5);
      for (const op of order) {
        q.apply(op);
      }
      return state(q);
    });
    eq(finals[1], finals[0], `round ${round}, peer b`);
    eq(finals[2], finals[0], `round ${round}, peer c`);
  }
});

test('snapshots round-trip and diverged snapshots merge the same both ways', () => {
  const a = wb.createModel('aaaa');
  const b = wb.createModel('bbbb');
  a.add({ kind: 'ink', points: [0, 0, 10, 10], color: '#3e63dd', width: 3 });
  b.add({ kind: 'sticker', x: 1, y: 2, size: 32, name: 'check' });
  const shared = a.add({ kind: 'text', x: 3, y: 4, size: 18, text: 'hi', color: '#1c2024' });
  b.apply(shared);
  b.change(shared.item.id, { text: 'hello' });
  a.remove(shared.item.id);
  const copy = wb.createModel('cccc');
  copy.merge(JSON.parse(JSON.stringify(a.snapshot())));
  eq(state(copy), state(a), 'round trip');
  const ab = wb.createModel('dddd');
  ab.merge(a.snapshot());
  ab.merge(b.snapshot());
  const ba = wb.createModel('eeee');
  ba.merge(b.snapshot());
  ba.merge(a.snapshot());
  eq(state(ab), state(ba), 'both ways');
});

test('a delete beats a late add; clear removes only older items', () => {
  const a = wb.createModel('aaaa');
  const add = a.add({ kind: 'sticker', x: 0, y: 0, size: 20, name: 'star' });
  const del = a.remove(add.item.id);
  const b = wb.createModel('bbbb');
  b.apply(del);
  b.apply(add);
  eq(b.live().length, 0, 'still deleted');
  const c = wb.createModel('cccc');
  c.apply(add);
  const clear = c.clear();
  const after = c.add({ kind: 'sticker', x: 1, y: 1, size: 20, name: 'heart' });
  const d = wb.createModel('dddd');
  d.apply(after);
  d.apply(add);
  d.apply(clear);
  eq(d.live().map((i) => i.name).join(), 'heart', 'newer item stays');
});

test('undo and redo take back only this peer\'s own changes', () => {
  const a = wb.createModel('aaaa');
  const b = wb.createModel('bbbb');
  const mine = a.add({ kind: 'text', x: 0, y: 0, size: 18, text: 'mine', color: '#1c2024' });
  const theirs = b.add({ kind: 'text', x: 0, y: 30, size: 18, text: 'theirs', color: '#1c2024' });
  a.apply(theirs);
  a.change(mine.item.id, { text: 'edited' });
  a.undo();
  eq(a.get(mine.item.id).text, 'mine', 'edit undone');
  a.undo();
  eq(a.get(mine.item.id), null, 'add undone');
  eq(a.get(theirs.item.id).text, 'theirs', 'other peer untouched');
  eq(a.undo(), null, 'nothing more to undo');
  a.redo();
  eq(a.get(mine.item.id).text, 'mine', 'redo');
});

test('limits refuse with a message; malformed operations are rejected', () => {
  const a = wb.createModel('aaaa');
  for (let i = 0; i < wb.LIMITS.items; ++i) {
    a.add({ kind: 'sticker', x: i, y: 0, size: 10, name: 'check' });
  }
  let threw = null;
  try {
    a.add({ kind: 'sticker', x: 0, y: 0, size: 10, name: 'check' });
  } catch (e) {
    threw = e;
  }
  ok(threw instanceof wb.WhiteboardError && /full/.test(threw.message), 'full');
  const b = wb.createModel('bbbb');
  const bad = [
    null,
    { t: 'nope' },
    { t: 'put', item: { id: 'x.1', author: 'x', stamp: [1, 'x'], kind: 'text', x: 0, y: 0, size: 18, text: 'a'.repeat(501), color: '#000000' } },
    { t: 'put', item: { id: 'x.1', author: 'x', stamp: [1, 'x'], kind: 'text', x: 0, y: 0, size: 18, text: 'hi', color: 'red' } },
    { t: 'put', item: { id: '<b>', author: 'x', stamp: [1, 'x'], kind: 'sticker', x: 0, y: 0, size: 18, name: 'check' } },
    { t: 'put', item: { id: 'x.1', author: 'x', stamp: [1, 'x'], kind: 'ink', points: [0, 0, 1], color: '#000000', width: 2 } },
    { t: 'put', item: { id: 'x.1', author: 'x', stamp: [1, 'x'], kind: 'script', x: 0, y: 0, size: 18 } },
    { t: 'clear', stamp: ['1', 'x'] },
  ];
  for (const op of bad) {
    eq(b.apply(op), null, `rejected ${JSON.stringify(op)?.slice(0, 60)}`);
  }
  const extra = b.apply({ t: 'put', item: { id: 'x.1', author: 'x', stamp: [1, 'x'], kind: 'sticker', x: 0, y: 0, size: 18, name: 'check', onclick: 'alert(1)' } });
  eq(extra.length, 1, 'valid item applied');
  eq('onclick' in b.get('x.1'), false, 'unknown fields dropped');
});

test('point simplification keeps the shape and rounds to 0.1', () => {
  const pts = [];
  for (let i = 0; i <= 100; ++i) {
    pts.push(i, 0.001 * i);
  }
  pts.push(100, 50);
  const s = wb.simplifyPoints(pts);
  eq(s.length, 6, 'straight run reduced to its ends and the corner');
  eq(s[2], 100, 'corner x');
  eq(s.every((v) => Math.abs(v * 10 - Math.round(v * 10)) < 1e-9), true, 'rounded');
});

// ── Session core ──────────────────────────────────────────────────────────────

async function room(guests = 2, extra = {}) {
  const network = cs.createMemoryNetwork();
  const rendezvous = [{ kind: 'memory', network }];
  const host = await cs.createSession({ role: 'host', me: { name: 'Host', color: '#e5484d' }, rendezvous, ...extra.host });
  const list = [];
  for (let i = 0; i < guests; ++i) {
    list.push(await cs.createSession({ role: 'guest', token: host.token, me: { name: `G${i}`, color: '#3e63dd' }, rendezvous, ...extra.guest }));
  }
  await until(() => list.every((g) => g.state === 'open') && host.peers().length === guests + 1, 3000, 'everyone in');
  await until(() => list.every((g) => g.peers().length === guests + 1), 3000, 'rosters');
  return { host, guests: list, network, rendezvous };
}

test('a host and three guests see the same roster', async () => {
  const { host, guests } = await room(3);
  const ids = (s) => s.peers().map((p) => `${p.id}:${p.name}:${p.role}`).sort().join();
  for (const g of guests) {
    eq(ids(g), ids(host), 'same roster');
  }
  eq(guests[0].host().id, host.me.id, 'the host is known');
  guests[1].setMe({ name: 'Renamed' });
  await until(() => guests[0].peers().some((p) => p.name === 'Renamed'), 2000, 'rename seen');
  host.close();
});

test('a guest\'s broadcast reaches everyone else once, from the right sender', async () => {
  const { host, guests } = await room(3);
  const got = new Map();
  for (const s of [host, ...guests]) {
    got.set(s, []);
    s.channel('chat').on('message', (m, from) => got.get(s).push([m.text, from]));
  }
  guests[0].channel('chat').send({ text: 'hi' });
  await until(() => got.get(host).length === 1 && got.get(guests[1]).length === 1 && got.get(guests[2]).length === 1, 2000, 'delivery');
  await new Promise((r) => setTimeout(r, 30));
  eq(got.get(guests[0]).length, 0, 'not back to the sender');
  for (const s of [host, guests[1], guests[2]]) {
    eq(got.get(s)[0][1], guests[0].me.id, 'from');
    eq(got.get(s).length, 1, 'once');
  }
  // To one peer only, through the host.
  guests[1].channel('chat').send({ text: 'psst' }, { to: guests[2].me.id });
  await until(() => got.get(guests[2]).length === 2, 2000, 'direct');
  await new Promise((r) => setTimeout(r, 30));
  eq(got.get(guests[0]).length, 0, 'not to others');
  eq(got.get(host).length, 1, 'not to the host');
  host.close();
});

test('the host\'s accept hook drops a message before anyone sees it', async () => {
  let room_;
  const accept = (channel, message, from) => !(channel === 'doc' && from !== room_?.host.me.id);
  room_ = await room(2, { host: { accept } });
  const { host, guests } = room_;
  const seen = [];
  host.channel('doc').on('message', (m) => seen.push(m));
  guests[1].channel('doc').on('message', (m) => seen.push(m));
  guests[0].channel('doc').send({ forged: true });
  guests[0].channel('ok').send({ fine: true });
  await new Promise((r) => setTimeout(r, 50));
  eq(seen.length, 0, 'dropped');
  host.close();
});

test('a 5 MB message arrives whole; a 20 MB one is refused', async () => {
  const { host, guests } = await room(1);
  const big = 'x'.repeat(5 * 1024 * 1024);
  let got = null;
  guests[0].channel('doc').on('message', (m) => (got = m));
  host.channel('doc').send({ source: big + 'end' });
  await until(() => got, 10000, 'big message');
  eq(got.source.length, big.length + 3, 'length');
  eq(got.source.endsWith('end'), true, 'whole');
  let refused = null;
  try {
    await cs.encodeFrames({ s: 'y'.repeat(20 * 1024 * 1024) });
  } catch (e) {
    refused = e;
  }
  eq(refused?.reason, 'too-large', 'refused');
  host.close();
});

test('frames: a compressible message is compressed, chunks reassemble, bad frames throw', async () => {
  const value = { text: 'abc'.repeat(30000) };
  const frames = await cs.encodeFrames(value);
  ok(frames.every((f) => f.length <= cs.FRAME_BYTES), 'frame size');
  eq(frames[0][0] & 1, 1, 'compressed');
  const noise = crypto.getRandomValues(new Uint8Array(60000));
  const random = Array.from(noise, (b) => String.fromCharCode(33 + (b % 90))).join('');
  const chunked = await cs.encodeFrames({ random });
  ok(chunked.length > 1, 'chunked');
  const d = new cs.FrameDecoder();
  let out;
  for (const f of chunked) {
    out = await d.push(f);
  }
  eq(out.random, random, 'reassembled');
  let threw = false;
  try {
    await new cs.FrameDecoder().push(chunked[1]);
  } catch {
    threw = true;
  }
  eq(threw, true, 'out of order');
  let fast = null;
  try {
    await cs.encodeFrames({ random }, { fast: true });
  } catch (e) {
    fast = e;
  }
  eq(fast?.reason, 'too-large', 'fast message must fit a frame');
});

test('protocol mismatch, removal and lock refuse with a reason', async () => {
  const { host, guests, rendezvous } = await room(1);
  const other = await cs.createSession({ role: 'guest', token: host.token, protocol: 'collab/2', me: { name: 'Old' }, rendezvous });
  await until(() => other.state === 'ended', 2000, 'protocol refused');
  const ended = [];
  guests[0].on('state', (s) => s.state === 'ended' && ended.push(s.reason));
  host.remove(guests[0].me.id);
  await until(() => ended.length, 2000, 'removed');
  eq(ended[0], 'removed', 'reason');
  host.lock(true);
  const late = await cs.createSession({ role: 'guest', token: host.token, me: { name: 'Late' }, rendezvous });
  const reasons = [];
  late.on('state', (s) => s.state === 'ended' && reasons.push(s.reason));
  await until(() => reasons.length, 2000, 'locked');
  eq(reasons[0], 'locked', 'locked out');
  eq(host.peers().length, 1, 'only the host left');
  host.close();
});

test('sealed messages: the right key opens them, an altered byte does not', async () => {
  const token = cs.newToken();
  eq(cs.isToken(token), true, 'token shape');
  const keys = await cs.roomKeys(token);
  eq(keys.topic.length, 40, 'topic is 20 bytes');
  const sealed = await cs.sealMessage(keys, { sdp: 'v=0', n: 1 });
  eq((await cs.unsealMessage(keys, sealed)).sdp, 'v=0', 'opens');
  const bytes = cs.fromBase64Url(sealed);
  bytes[20] ^= 1;
  eq(await cs.unsealMessage(keys, cs.toBase64Url(bytes)), null, 'altered');
  eq(await cs.unsealMessage(await cs.roomKeys(cs.newToken()), sealed), null, 'other room');
  eq(sealed.includes('v=0'), false, 'not readable');
  let threw = null;
  try {
    await cs.roomKeys('short');
  } catch (e) {
    threw = e;
  }
  eq(threw?.reason, 'bad-token', 'bad token');
});

test('a host that closes ends every guest; a dropped host link reconnects', async () => {
  const { host, guests, network, rendezvous } = await room(2, { guest: { reconnectMs: 2000 } });
  // A dropped link (a network change): the guest looks for the host again.
  const states = [];
  guests[0].on('state', (s) => states.push(s.state));
  [...host.links.values()].find((e) => e.peer.id === guests[0].me.id).link.close();
  await until(() => states.includes('reconnecting'), 2000, 'reconnecting');
  await until(() => guests[0].state === 'open', 3000, 'back');
  void network;
  void rendezvous;
  const ends = [];
  for (const g of guests) {
    g.on('state', (s) => s.state === 'ended' && ends.push(s.reason));
  }
  host.close();
  await until(() => ends.length === 2, 2000, 'ended');
  eq(ends.every((r) => r === 'host-ended'), true, 'reason');
});

test('the whiteboard model syncs over a session: late joiners get the items', async () => {
  // The model and the session together, without the DOM: the protocol the
  // overlay speaks (op and sync messages on a channel).
  const { host, guests, rendezvous } = await room(1);
  const attach = (s) => {
    const m = wb.createModel(s.me.id);
    const ch = s.channel('wb');
    ch.on('message', (msg) => {
      if (msg.t === 'op') {
        m.apply(msg.op);
      } else if (msg.t === 'sync') {
        const ops = m.merge(msg.snapshot);
        if (s.role === 'host') {
          for (const op of ops) {
            ch.send({ t: 'op', op });
          }
        }
      }
    });
    ch.on('join', (peer) => ch.send({ t: 'sync', snapshot: m.snapshot() }, { to: peer.id }));
    return { m, ch };
  };
  const h = attach(host);
  const g = attach(guests[0]);
  g.ch.send({ t: 'op', op: g.m.add({ kind: 'sticker', x: 1, y: 1, size: 20, name: 'star' }) });
  h.ch.send({ t: 'op', op: h.m.add({ kind: 'text', x: 2, y: 2, size: 18, text: 'from host', color: '#1c2024' }) });
  const late = await cs.createSession({ role: 'guest', token: host.token, me: { name: 'Late' }, rendezvous });
  const l = attach(late);
  await until(() => l.m.live().length === 2 && g.m.live().length === 2 && h.m.live().length === 2, 3000, 'converged');
  eq(state(l.m), state(h.m), 'late joiner has everything');
  host.close();
});

// ── Independence ──────────────────────────────────────────────────────────────

test('web/collab imports nothing outside itself; the whiteboard not the session', async () => {
  for (const file of ['collab-session.js', 'collab-whiteboard.js']) {
    const text = await readFile(join(collabDir, file), 'utf8');
    const imports = [...text.matchAll(/^\s*import\s[^'"]*['"]([^'"]+)['"]/gm), ...text.matchAll(/import\(\s*['"]([^'"]+)['"]/g)].map((m) => m[1]);
    eq(imports.length, 0, `${file} imports ${imports.join(', ')}`);
  }
});

let failed = 0;
for (const { name, fn } of tests) {
  try {
    await fn();
    console.log(`ok   ${name}`);
  } catch (e) {
    failed++;
    console.log(`FAIL ${name}: ${e.stack ?? e.message}`);
  }
}
console.log(`${tests.length - failed}/${tests.length} passed`);
process.exit(failed ? 1 : 0);
