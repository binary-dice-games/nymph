// MIT License © 2026 Binary Dice Games
/**
 * @file collab.js Live collaboration in the app
 * (spec/18_collaboration/DESIGN.md section 6): the owner shares the open
 * chart, guests see it live in View, and everyone draws on a whiteboard
 * over it. The session and the whiteboard are the standalone packages in
 * `collab/`; this file is the nymph side: the doc channel, guest
 * documents, Follow the owner, the commands, and the People panel's
 * actions.
 */

import { viewBlock, withView, withoutView } from '../document.js';
import { nymphAdapter } from './adapter.js';
import { pngWithDrawings } from './export.js';
import { PeoplePanel } from './panel.js';

/** The protocol peers must share: a guest of another version is told to reload. */
export const PROTOCOL = 'nymph/1';
const PALETTE = ['#e5484d', '#f5a524', '#30a46c', '#0f9f86', '#3e63dd', '#8e4ec6', '#d6409f', '#12a594'];
const RESUME_KEY = 'nymph.collab.resume';

function storage(kind) {
  try {
    return kind === 'session' ? sessionStorage : localStorage;
  } catch {
    return null;
  }
}

function readJson(kind, key) {
  try {
    return JSON.parse(storage(kind)?.getItem(key) ?? 'null');
  } catch {
    return null;
  }
}

function writeJson(kind, key, value) {
  try {
    if (value === null) {
      storage(kind)?.removeItem(key);
    } else {
      storage(kind)?.setItem(key, JSON.stringify(value));
    }
  } catch {
    // no storage: it lasts for the page
  }
}

/** Whether this browser can take part (WebRTC, WebCrypto, compression). */
export function collabSupported() {
  return typeof RTCPeerConnection === 'function' && !!globalThis.crypto?.subtle && typeof CompressionStream === 'function';
}

export class Collab {
  /**
   * @param {object} hooks what the shell gives: app, commands, config,
   *   base (URL of the site), stage, dock, button, ui {toast, banner, ask,
   *   dialog, dialogButtons, el, download}, module() (viewer or editor shown),
   *   currentYaml() (the owner's source with the view), ownYaml() (a guest's),
   *   openGuest(source, name), applyGuest(source), closeDocument(), updateTitle()
   */
  constructor(hooks) {
    this.h = hooks;
    this.session = null;
    this.wb = null;
    this.adapter = null;
    this.role = null;
    this.following = true;
    this.version = 0;
    this.lastSent = '';
    this.guestVersion = 0;
    this.ownerSource = null;
    this.drawingLocked = false;
    this.ended = null;
    this.panel = new PeoplePanel(this, hooks);
    this.config = hooks.config ?? {};
    hooks.button.hidden = !collabSupported();
    hooks.button.addEventListener('click', () => this.h.act(this.session ? 'collab_panel' : 'collab_start'));
    this.defineCommands();
  }

  /** A session is open (or a guest's ended one still shows its chart). */
  get active() {
    return !!this.session && this.session.state !== 'ended';
  }

  /** This tab hosts a running session (opening another chart keeps it). */
  get hosting() {
    return this.role === 'host' && this.active && !this.isGuestDocument;
  }

  /** The guest's document is from a session (open or ended). */
  get isGuestDocument() {
    return this.h.app.doc?.origin.kind === 'collab';
  }

  // ── Commands ───────────────────────────────────────────────────────────

  /** @private */
  defineCommands() {
    const c = this.h.commands;
    const schema = (properties = {}, required = []) => ({ type: 'object', properties, required, additionalProperties: false });
    c.define({
      name: 'collab_start',
      agent: false, // shares the chart with people the user chooses (DESIGN.md section 6.5)
      title: 'Collaborate',
      description: 'Shares the open chart live with people who have the invite link; everyone can draw on a whiteboard over it.',
      scope: 'document',
      effect: 'external',
      available: () => collabSupported() && !this.isGuestDocument,
      run: () => this.start(),
    });
    c.define({
      name: 'collab_join',
      agent: false,
      title: 'Join a live chart',
      description: 'Joins the live session of an invite link.',
      effect: 'external',
      inputSchema: schema({ token: { type: 'string', description: 'The token of the invite link (#collab=...).' } }, ['token']),
      available: collabSupported,
      run: ({ token }) => this.join(token),
    });
    c.define({
      name: 'collab_stop',
      agent: false,
      title: 'End or leave the session',
      description: 'Ends the live session (the owner) or leaves it (a guest).',
      run: () => this.stop({ ask: false }),
    });
    c.define({
      name: 'collab_panel',
      agent: false,
      title: 'People',
      description: 'Shows or hides the People panel of the live session.',
      run: () => this.panel.toggle(),
    });
    c.define({
      name: 'collab_state',
      title: 'Live session state',
      description: 'Whether the chart is shared live: your role (owner or guest), the people in the session and how each is connected, and how many drawings are on the whiteboard.',
      run: () => this.state(),
    });
    c.define({
      name: 'collab_download_png',
      agent: false,
      title: 'Download PNG with drawings',
      description: 'Downloads the chart as a PNG with the whiteboard drawn over it (the PNG still opens in nymph as the chart, without the drawings).',
      scope: 'document',
      effect: 'external',
      available: () => !!this.wb,
      run: () => this.downloadWithDrawings(),
    });
  }

  /** What collab_state returns. */
  state() {
    if (!this.session) {
      return { active: false };
    }
    return {
      active: this.active,
      role: this.role === 'host' ? 'owner' : 'guest',
      state: this.session.state,
      people: this.session.peers().map((p) => ({ name: p.name, role: p.role === 'host' ? 'owner' : 'guest', you: p.id === this.session.me.id, path: p.path })),
      drawings: this.wb?.items().length ?? 0,
      following: this.role === 'guest' ? this.following : undefined,
      drawingLocked: this.drawingLocked,
    };
  }

  // ── Starting ───────────────────────────────────────────────────────────

  /** @private The packages, loaded on first use. */
  async load() {
    if (!this.modules) {
      const base = this.h.base();
      this.modules = Promise.all([import(new URL('collab/collab-session.js', base).href), import(new URL('collab/collab-whiteboard.js', base).href)]).then(([session, whiteboard]) => ({ session, whiteboard }));
      this.modules.catch(() => (this.modules = null));
    }
    return this.modules;
  }

  /** @private The rendezvous of config.json (trackers) and the manual codes. */
  rendezvous() {
    const trackers = (this.config.trackers ?? []).filter((u) => typeof u === 'string');
    const list = [];
    if (trackers.length) {
      list.push({ kind: 'torrent', urls: trackers, ...(this.config.torrent ?? {}) });
    }
    list.push({ kind: 'manual' });
    return list;
  }

  /** @private STUN from config.json, plus the user's TURN server. */
  iceServers() {
    const list = Array.isArray(this.config.iceServers) ? [...this.config.iceServers] : [];
    const turn = readJson('local', 'nymph.collab.turn');
    if (turn?.urls) {
      list.push({ urls: turn.urls, username: turn.username || undefined, credential: turn.credential || undefined });
    }
    return list;
  }

  /** This person's name and colour (asked once, kept in this browser). */
  me() {
    const me = readJson('local', 'nymph.collab.me');
    return me?.name ? me : null;
  }

  setMe(me) {
    const next = { ...(this.me() ?? {}), ...me };
    writeJson('local', 'nymph.collab.me', next);
    this.session?.setMe(next);
    if (this.wb && me.color) {
      this.wb.setStyle({ color: me.color });
    }
  }

  /** @private Asks for a name the first time. */
  async askName(title) {
    const known = this.me();
    if (known) {
      return known;
    }
    const { el, dialog, dialogButtons } = this.h.ui;
    const name = await dialog(title, (body, close) => {
      const input = el('input', { type: 'text', maxlength: '40', placeholder: 'Your name', autofocus: true, 'aria-label': 'Your name', autocomplete: 'nickname' });
      const form = el('form', {}, el('p', {}, 'Others in the session see this name next to your cursor.'), input, dialogButtons(close, [{ label: 'Cancel', value: undefined }, { label: 'Continue', value: 'submit', kind: 'primary' }]));
      form.onsubmit = (e) => (e.preventDefault(), close(input.value.trim()));
      const ok = form.querySelector('.primary');
      ok.type = 'submit';
      ok.onclick = null;
      body.append(form);
    });
    if (!name) {
      return null;
    }
    const me = { name, color: PALETTE[Math.floor(Math.random() * PALETTE.length)] };
    writeJson('local', 'nymph.collab.me', me);
    return me;
  }

  /** @private What sharing means, said once per browser. */
  async agreed() {
    if (storage('local')?.getItem('nymph.collab.agreed') === '1') {
      return true;
    }
    const { el, ask } = this.h.ui;
    const ok = await ask(
      'Share this chart live',
      el(
        'div',
        {},
        el('p', {}, 'People with the link see this chart as you edit it, unsaved changes included, and can draw on a whiteboard over it. Only you change the chart.'),
        el('p', {}, 'Your browsers connect directly and learn each other\'s IP address. Public trackers help you find each other; what they carry is encrypted, so they see nothing of the chart.'),
        el('p', { class: 'muted' }, 'The drawings are never saved in the file.'),
      ),
      [
        { label: 'Cancel', value: false },
        { label: 'Share', value: true, kind: 'primary', focus: true },
      ],
    );
    if (ok === true) {
      storage('local')?.setItem('nymph.collab.agreed', '1');
    }
    return ok === true;
  }

  /** Owner: shares the open document (or opens the panel). */
  async start(resume = null) {
    const doc = this.h.app.doc;
    if (this.session && !this.ended) {
      this.panel.open();
      return this.state();
    }
    if (!doc || this.isGuestDocument) {
      throw new Error('open a chart of yours to share it');
    }
    if (!resume && !(await this.agreed())) {
      return { active: false };
    }
    const me = resume ? this.me() ?? { name: 'Owner', color: PALETTE[0] } : await this.askName('Your name');
    if (!me) {
      return { active: false };
    }
    const { session: cs } = await this.h.ui.busy(this.load());
    this.reset();
    const session = await cs.createSession({
      role: 'host',
      token: resume?.token,
      peerId: resume?.peerId,
      protocol: PROTOCOL,
      me,
      rendezvous: this.rendezvous(),
      iceServers: this.iceServers(),
      accept: (channel, message) => this.accept(channel, message),
    });
    this.session = session;
    this.role = 'host';
    this.docId = doc.id;
    this.wireDocChannel();
    await this.makeWhiteboard();
    if (resume?.snapshot) {
      this.wb.load(resume.snapshot);
    }
    this.saveResume();
    this.pollTimer = setInterval(() => this.poll(), 400);
    this.panel.open();
    this.h.updateTitle();
    this.h.sessionChanged?.();
    return this.state();
  }

  /** Guest: joins the session of an invite token. */
  async join(token) {
    if (this.session && !this.ended) {
      if (this.session.token === token) {
        this.panel.open();
        return this.state();
      }
      await this.stop({ ask: true });
      if (this.session) {
        return this.state();
      }
    }
    const { session: cs } = await this.h.ui.busy(this.load());
    if (!cs.isToken(token)) {
      throw new Error('This invite link is not complete: ask for it again.');
    }
    const me = await this.askName('Join a live chart');
    if (!me) {
      return { active: false };
    }
    this.reset();
    const peers = readJson('session', 'nymph.collab.peer') ?? {};
    const session = await cs.createSession({
      role: 'guest',
      token,
      peerId: peers[token],
      protocol: PROTOCOL,
      me,
      rendezvous: this.rendezvous(),
      iceServers: this.iceServers(),
    });
    writeJson('session', 'nymph.collab.peer', { ...peers, [token]: session.me.id });
    writeJson('session', 'nymph.collab.join', token);
    this.session = session;
    this.role = 'guest';
    this.following = true;
    this.wireDocChannel();
    this.panel.connecting();
    return this.state();
  }

  /** @private */
  reset() {
    clearInterval(this.pollTimer);
    clearTimeout(this.sendTimer);
    this.unwatchInput?.();
    this.unwatchInput = null;
    this.wb?.destroy();
    this.wb = null;
    this.adapter?.destroy();
    this.adapter = null;
    this.version = 0;
    this.lastSent = '';
    this.guestVersion = 0;
    this.ownerSource = null;
    this.drawingLocked = false;
    this.ended = null;
    this.opened = false;
  }

  /** @private The overlay over the stage, its adapter following the module shown. */
  async makeWhiteboard() {
    const { whiteboard } = await this.load();
    this.adapter = nymphAdapter(this.h.stage, () => this.h.module());
    const me = this.session.me;
    this.wb = whiteboard.createWhiteboard(this.h.stage, { session: this.session, adapter: this.adapter, channel: 'wb', strings: { readOnly: 'The owner locked drawing' } });
    this.wb.setStyle({ color: me.color });
    this.wb.on('error', (e) => this.h.ui.toast(e.message));
    if (this.role === 'host') {
      this.wb.on('change', () => this.saveResume());
    }
    this.adapter.watch();
    if (this.role === 'guest') {
      this.watchGuestInput();
    }
  }

  /** The shell replaced the stage's content (a mode switch, a new viewer). */
  stageChanged() {
    if (this.wb && !this.wb.element.isConnected) {
      this.h.stage.append(this.wb.element);
    }
    this.adapter?.watch();
  }

  // ── Doc channel (DESIGN.md section 6.3) ────────────────────────────────

  /** @private */
  wireDocChannel() {
    const s = this.session;
    const doc = s.channel('doc');
    if (this.role === 'host') {
      doc.on('join', (peer) => this.sendDoc(peer.id));
    } else {
      doc.on('message', (m, from) => {
        if (from === s.host()?.id) {
          this.ownerMessage(m);
        }
      });
    }
    s.on('peers', () => this.panel.render());
    s.on('status', () => this.panel.render());
    s.on('error', (e) => this.panel.problem(e));
    s.on('state', (st) => this.stateChanged(st));
  }

  /** @private Host: drops what a guest may not send (DESIGN.md section 6.1). */
  accept(channel, message) {
    if (channel === 'doc') {
      return false; // one way: only the host sends the chart
    }
    if (channel === 'wb' && this.drawingLocked && message?.t !== 'cur') {
      return false;
    }
    return true;
  }

  /** The owner's document changed (the shell calls it on every change). */
  documentChanged() {
    if (this.role !== 'host' || !this.active) {
      return;
    }
    clearTimeout(this.sendTimer);
    this.sendTimer = setTimeout(() => this.sendDoc(null), 150);
  }

  /** @private In Edit mode the editor has no view event: compare now and then. */
  poll() {
    if (this.role === 'host' && this.active && this.session.peers().length > 1 && this.h.app.mode === 'edit') {
      this.documentChanged();
    }
  }

  /** @private Host: the source with the view to one guest (`to`) or all. */
  sendDoc(to) {
    if (!this.active || this.role !== 'host' || !this.h.app.doc) {
      return;
    }
    let source;
    try {
      source = this.h.currentYaml();
    } catch {
      return;
    }
    if (!to && source === this.lastSent) {
      return;
    }
    const doc = this.session.channel('doc');
    if (!to || this.version === 0) {
      // Versions start at 1: a guest takes only a version above its own (0).
      this.version += 1;
      this.lastSent = source;
    }
    const opts = to ? { to } : {};
    doc.send({ t: 'doc', v: this.version, source }, opts);
    doc.send({ t: 'meta', name: this.h.app.doc.name, mode: this.h.app.mode }, opts);
    doc.send({ t: 'lock', drawing: this.drawingLocked }, opts);
  }

  /** Host: locks or unlocks drawing for the guests. */
  setDrawingLocked(on) {
    this.drawingLocked = !!on;
    this.session?.channel('doc').send({ t: 'lock', drawing: this.drawingLocked });
    this.panel.render();
  }

  /** @private Guest: a message of the doc channel, from the host only. */
  async ownerMessage(m) {
    if (!m || typeof m !== 'object') {
      return;
    }
    if (m.t === 'meta') {
      this.ownerMeta = { name: typeof m.name === 'string' ? m.name.slice(0, 200) : 'chart.yaml', mode: m.mode === 'edit' ? 'edit' : 'view' };
      if (this.opened && this.isGuestDocument && this.h.app.doc) {
        this.h.app.doc.name = this.ownerMeta.name;
        this.h.updateTitle();
      }
      this.panel.render();
    } else if (m.t === 'lock') {
      this.drawingLocked = m.drawing === true;
      if (this.wb) {
        this.wb.readOnly = this.drawingLocked;
      }
      this.panel.render();
    } else if (m.t === 'doc' && typeof m.source === 'string' && Number.isInteger(m.v) && m.v > this.guestVersion) {
      this.guestVersion = m.v;
      this.ownerSource = m.source;
      this.showOwner();
    }
  }

  /** @private Guest: shows the latest owner source; updates never overlap. */
  showOwner() {
    if (this.showing) {
      this.showAgain = true;
      return;
    }
    this.showing = (async () => {
      try {
        do {
          this.showAgain = false;
          await this.showOnce();
        } while (this.showAgain);
      } finally {
        this.showing = null;
      }
    })();
  }

  /** @private */
  async showOnce() {
    const source = this.ownerSource;
    if (!this.opened) {
      // The first chart: open it as the guest's document.
      this.panel.connected();
      await this.h.openGuest(source, this.ownerMeta?.name ?? 'chart.yaml');
      if (!this.isGuestDocument) {
        return;
      }
      this.opened = true;
      this.lastShown = source;
      await this.makeWhiteboard();
      this.wb.readOnly = this.drawingLocked;
      this.panel.open();
      return;
    }
    if (!this.isGuestDocument) {
      return;
    }
    // Not following: the owner's chart with this guest's own view key.
    const shown = this.following ? source : withView(source, viewBlock(this.h.ownYaml() ?? ''));
    try {
      await this.h.applyGuest(shown);
      this.lastShown = shown;
      if (this.chartError) {
        this.chartError = false;
        this.h.ui.banner(null);
      }
    } catch {
      this.chartError = true;
      this.h.ui.banner({ kind: 'warning', text: 'The owner\'s chart has an error; showing the last version that worked.' });
    }
  }

  /** Guest: follows the owner's view again, or stops. */
  setFollowing(on) {
    this.following = !!on;
    this.panel.render();
    if (this.following && this.ownerSource) {
      this.showOwner();
    }
  }

  /** @private Guest: a pan or zoom by hand stops following the owner. */
  watchGuestInput() {
    const stage = this.h.stage;
    const own = (e) => {
      if (this.following && this.wb?.tool === 'pointer' && e.target?.tagName === 'CANVAS') {
        this.following = false;
        this.panel.render();
        this.h.ui.toast('You moved the view: no longer following the owner (People panel to follow again).');
      }
    };
    stage.addEventListener('pointerdown', own, true);
    stage.addEventListener('wheel', own, { capture: true, passive: true });
    this.unwatchInput = () => {
      stage.removeEventListener('pointerdown', own, true);
      stage.removeEventListener('wheel', own, { capture: true });
    };
  }

  // ── State ──────────────────────────────────────────────────────────────

  /** @private */
  stateChanged(st) {
    if (st.state === 'ended') {
      this.ended = st;
      clearInterval(this.pollTimer);
      if (this.role === 'guest') {
        writeJson('session', 'nymph.collab.join', null);
        if (this.wb) {
          this.wb.readOnly = true;
        }
        if (st.reason !== 'closed') {
          this.h.ui.banner({ kind: 'info', text: st.message ?? 'The session has ended.' });
        }
        if (!this.opened) {
          this.panel.problem({ reason: st.reason, message: st.message });
        }
      } else {
        writeJson('session', RESUME_KEY, null);
        this.reset();
        this.session = null;
        this.panel.close();
        this.h.sessionChanged?.();
      }
      this.h.updateTitle();
    }
    this.panel.render();
  }

  /** @private Host: what a reload of this tab needs to resume the room. */
  saveResume() {
    if (this.role !== 'host' || !this.active) {
      return;
    }
    clearTimeout(this.resumeTimer);
    this.resumeTimer = setTimeout(() => {
      writeJson('session', RESUME_KEY, { token: this.session.token, peerId: this.session.me.id, docId: this.docId, snapshot: this.wb?.snapshot() ?? null });
    }, 300);
  }

  /** A document was opened: resume this tab's room if it was this document's. */
  async documentOpened(doc) {
    const resume = readJson('session', RESUME_KEY);
    if (resume && doc && resume.docId === doc.id && !this.session && collabSupported()) {
      try {
        await this.start(resume);
      } catch {
        writeJson('session', RESUME_KEY, null);
      }
    }
  }

  /**
   * Host: another chart was opened. The session stays; guests get the new
   * chart, and the drawings (which belong to the old chart) are cleared.
   */
  documentSwitched(doc) {
    this.docId = doc.id;
    this.version = 0;
    this.lastSent = '';
    this.wb?.clear();
    this.sendDoc(null);
    this.saveResume();
    this.h.updateTitle();
  }

  /** A guest's reload: rejoin the session this tab was in. */
  rejoinToken() {
    return readJson('session', 'nymph.collab.join');
  }

  /**
   * Before the document closes: ends (owner) or leaves (guest) the session,
   * asking first when `ask`. Resolves false when the user stays.
   */
  async stop({ ask = true } = {}) {
    if (!this.session) {
      return true;
    }
    if (ask && this.active) {
      const host = this.role === 'host';
      const ok = await this.h.ui.ask(host ? 'End the live session?' : 'Leave the live session?', host ? 'The people watching this chart will see the session end.' : 'You will stop seeing the owner\'s changes.', [
        { label: 'Cancel', value: false },
        { label: host ? 'End the session' : 'Leave', value: true, kind: 'danger', focus: true },
      ]);
      if (ok !== true) {
        return false;
      }
    }
    const s = this.session;
    if (this.role === 'host') {
      writeJson('session', RESUME_KEY, null);
    } else {
      writeJson('session', 'nymph.collab.join', null);
    }
    s.close();
    this.reset();
    this.session = null;
    this.role = null;
    this.panel.close();
    this.h.ui.banner(null);
    this.h.updateTitle();
    this.h.sessionChanged?.();
    return true;
  }

  /** The document closed: nothing of the session stays. */
  documentClosed() {
    if (this.session && this.ended) {
      this.reset();
      this.session = null;
      this.role = null;
      this.panel.close();
    }
  }

  // ── Export (DESIGN.md section 6.7) ─────────────────────────────────────

  /** Downloads the chart PNG with the whiteboard over it. */
  async downloadWithDrawings() {
    const target = this.h.module();
    const size = this.adapter?.contentSize();
    if (!target || !size || !this.wb) {
      throw new Error('the chart is not shown');
    }
    const svg = this.wb.toSvg({ x: 0, y: 0, width: size.width, height: size.height });
    const bytes = await pngWithDrawings(target.toPng(), svg);
    const name = `${(this.h.app.doc?.name ?? 'chart').replace(/\.(ya?ml|png)$/i, '')}-with-drawings.png`;
    this.h.ui.download(bytes, name, 'image/png');
    return { file: name };
  }

  /** The invite link of the open session (owner). */
  inviteLink() {
    return this.session ? `${this.h.base()}#${this.session.inviteFragment()}` : '';
  }

  /** Whether `source` differs from the owner's only by the view (tests). */
  static sameChart(a, b) {
    return withoutView(a) === withoutView(b);
  }
}
