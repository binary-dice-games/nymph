// MIT License © 2026 Binary Dice Games
/**
 * @file panel.js The People panel of a live session
 * (spec/18_collaboration/DESIGN.md sections 6.1, 6.2 and 7), in its own
 * dock beside the chart: the invite link, who is here and how each is
 * connected, the owner's controls, Follow the owner, the join codes, your
 * name and colour, the TURN setting; and the dialog a guest sees while
 * connecting. Names come from peers: always set as text, never as HTML.
 */

import { icons } from '../icons.js';

const PATHS = { direct: 'direct', relay: 'relayed (TURN)', local: 'this browser' };
const COLORS = ['#e5484d', '#f5a524', '#30a46c', '#0f9f86', '#3e63dd', '#8e4ec6', '#d6409f', '#12a594'];

/** What each connection stage means to a guest who waits. */
const STAGES = {
  tracker: 'Looking for the owner of the chart…',
  answer: 'The owner answered: connecting your browsers…',
  'no-tracker': 'No tracker could be reached.',
};

export class PeoplePanel {
  constructor(collab, hooks) {
    this.c = collab;
    this.h = hooks;
    this.dock = hooks.dock;
    this.isOpen = false;
    this.problems = [];
  }

  // ── Dock ───────────────────────────────────────────────────────────────

  open() {
    this.isOpen = true;
    this.dock.hidden = false;
    this.h.button.setAttribute('aria-pressed', 'true');
    this.render();
  }

  close() {
    this.isOpen = false;
    this.dock.hidden = true;
    this.dock.replaceChildren();
    this.h.button.setAttribute('aria-pressed', 'false');
    this.renderButton();
  }

  toggle() {
    if (this.isOpen) {
      this.close();
    } else {
      this.open();
    }
  }

  /** The bar's button: "Live · 3" while a session is open. */
  renderButton() {
    const b = this.h.button;
    const s = this.c.session;
    if (s && s.state !== 'ended') {
      b.textContent = `Live · ${s.peers().length}`;
      b.classList.add('live');
      b.title = 'The live session: people, invite link, drawing (Ctrl+Shift+L)';
    } else {
      b.textContent = 'Collaborate';
      b.classList.remove('live');
      b.title = 'Share this chart live and draw on it together (Ctrl+Shift+L)';
    }
  }

  render() {
    this.renderButton();
    this.renderConnecting();
    if (!this.isOpen) {
      return;
    }
    const { el } = this.h.ui;
    const s = this.c.session;
    const head = el(
      'div',
      { class: 'agents-head' },
      el('h2', {}, 'Live session'),
      el('button', { type: 'button', class: 'btn icon ghost', 'aria-label': 'Close the People panel', html: icons.close, onclick: () => this.close() }),
    );
    if (!s) {
      this.dock.replaceChildren(head, el('p', { class: 'muted' }, 'No live session.'));
      return;
    }
    const parts = [head, this.statusView()];
    if (this.c.role === 'host' && this.c.active) {
      parts.push(this.inviteView());
    }
    parts.push(this.peopleView());
    if (this.c.role === 'guest' && this.c.active) {
      parts.push(this.followView());
    }
    if (this.c.wb) {
      parts.push(this.drawingView());
    }
    if (this.c.active) {
      parts.push(this.codesView(), this.meView(), this.settingsView());
    }
    parts.push(this.endView());
    this.dock.replaceChildren(...parts);
  }

  /** @private */
  statusView() {
    const { el } = this.h.ui;
    const s = this.c.session;
    const host = s.host();
    let text;
    if (s.state === 'ended') {
      text = this.c.ended?.message ?? 'The session has ended.';
    } else if (s.state === 'reconnecting') {
      text = 'The connection to the owner dropped: reconnecting…';
    } else if (this.c.role === 'host') {
      const n = s.peers().length - 1;
      text = n === 0 ? 'Live: nobody else is here yet. Send them the link.' : `Live: ${n} ${n === 1 ? 'person sees' : 'people see'} this chart.`;
    } else {
      text = host ? `Live: ${host.name || 'the owner'}'s chart.` : 'Connecting…';
    }
    return el('div', { class: 'agents-status' }, s.state === 'ended' ? null : el('span', { class: 'pulse', 'aria-hidden': 'true' }), el('span', { class: 'text' }, text));
  }

  /** @private */
  inviteView() {
    const { el, toast } = this.h.ui;
    const link = this.c.inviteLink();
    const input = el('input', { type: 'text', readonly: true, value: link, 'aria-label': 'Invite link', class: 'collab-link' });
    input.addEventListener('focus', () => input.select());
    return el(
      'section',
      { class: 'agents-section' },
      el('h3', {}, 'Invite'),
      el('p', { class: 'muted' }, 'Anyone with this link sees the chart live and can draw on it. Send it only to the people you mean.'),
      input,
      el(
        'div',
        { class: 'row-buttons' },
        el(
          'button',
          {
            type: 'button',
            class: 'btn primary small',
            onclick: async () => {
              try {
                await navigator.clipboard.writeText(link);
                toast('Invite link copied');
              } catch {
                input.select();
                toast('Select the link and copy it');
              }
            },
          },
          'Copy link',
        ),
        el(
          'button',
          {
            type: 'button',
            class: 'btn small',
            title: 'A new link: the old one stops letting people in; people already here stay.',
            onclick: async () => {
              await this.c.session.newInvite();
              this.c.saveResume();
              this.render();
              toast('New invite link: the old one no longer lets people in');
            },
          },
          'New link',
        ),
      ),
    );
  }

  /** @private */
  peopleView() {
    const { el } = this.h.ui;
    const s = this.c.session;
    const host = this.c.role === 'host' && this.c.active;
    const items = s.peers().map((p) => {
      const you = p.id === s.me.id;
      const label = `${p.name || 'Someone'}${you ? ' (you)' : ''}`;
      const role = p.role === 'host' ? 'owner' : you ? '' : PATHS[p.path] ?? 'connecting';
      return el(
        'li',
        {},
        el('span', { class: 'dot', style: `background:${p.color}` }),
        el('span', { class: 'name' }, label),
        el('span', { class: 'route' }, role),
        host && !you
          ? el(
              'button',
              {
                type: 'button',
                class: 'btn small ghost',
                title: 'Remove from the session',
                onclick: () => {
                  s.remove(p.id);
                  this.h.ui.toast(`${p.name || 'They'} can no longer join with this link. Make a new link to be sure.`);
                },
              },
              'Remove',
            )
          : null,
      );
    });
    const section = el('section', { class: 'agents-section' }, el('h3', {}, 'People'), el('ul', { class: 'agents-list' }, items));
    if (host) {
      section.append(
        el(
          'label',
          { class: 'check' },
          el('input', { type: 'checkbox', checked: s.locked, onchange: (e) => (s.lock(e.target.checked), this.render()) }),
          ' Lock the session (nobody new can join)',
        ),
        el(
          'label',
          { class: 'check' },
          el('input', { type: 'checkbox', checked: this.c.drawingLocked, onchange: (e) => this.c.setDrawingLocked(e.target.checked) }),
          ' Only I can draw',
        ),
      );
    }
    return section;
  }

  /** @private */
  followView() {
    const { el } = this.h.ui;
    return el(
      'section',
      { class: 'agents-section' },
      el(
        'label',
        { class: 'check' },
        el('input', { type: 'checkbox', checked: this.c.following, onchange: (e) => this.c.setFollowing(e.target.checked) }),
        ' Follow the owner\'s view (zoom and pan)',
      ),
    );
  }

  /** @private */
  drawingView() {
    const { el } = this.h.ui;
    return el(
      'section',
      { class: 'agents-section' },
      el('h3', {}, 'Drawings'),
      el('p', { class: 'muted' }, 'Use the tools at the top-left of the chart. Drawings are for this session only: they are never saved in the file.'),
      el(
        'div',
        { class: 'row-buttons' },
        el('button', { type: 'button', class: 'btn small', onclick: () => this.h.act('collab_download_png') }, el('span', { html: icons.image, style: 'display:inline-flex' }), 'PNG with drawings'),
        this.c.role === 'host' && this.c.active
          ? el(
              'button',
              {
                type: 'button',
                class: 'btn small ghost',
                onclick: async () => {
                  if ((await this.h.ui.ask('Clear the drawings?', 'Everyone\'s drawings go, for everyone.', [{ label: 'Cancel', value: false }, { label: 'Clear', value: true, kind: 'danger' }])) === true) {
                    this.c.wb.clear();
                  }
                },
              },
              'Clear all',
            )
          : null,
      ),
    );
  }

  /** @private The manual codes (DESIGN.md section 3.4). */
  codesView() {
    const { el, toast } = this.h.ui;
    const s = this.c.session;
    const details = el('details', { class: 'agents-section collab-codes' }, el('summary', {}, 'Someone cannot connect? Use join codes'));
    if (this.c.role === 'host') {
      const input = el('textarea', { rows: '3', placeholder: 'Paste their join code', 'aria-label': 'Join code' });
      const out = el('textarea', { rows: '3', readonly: true, hidden: true, 'aria-label': 'Answer code' });
      details.append(
        el('p', { class: 'muted' }, 'They open the link, choose "Join with codes" and send you their join code. Paste it here, then send them the answer code.'),
        input,
        el(
          'button',
          {
            type: 'button',
            class: 'btn small',
            onclick: async () => {
              try {
                out.value = await s.manual.acceptOffer(input.value);
                out.hidden = false;
                out.select();
                await navigator.clipboard?.writeText(out.value).then(() => toast('Answer code copied: send it back'), () => {});
              } catch (e) {
                toast(e.message, 6000);
              }
            },
          },
          'Make the answer code',
        ),
        out,
      );
    } else {
      details.append(this.guestCodes());
    }
    return details;
  }

  /** @private A guest's side of the codes (also in the connecting dialog). */
  guestCodes() {
    const { el, toast } = this.h.ui;
    const s = this.c.session;
    const mine = el('textarea', { rows: '3', readonly: true, hidden: true, 'aria-label': 'Your join code' });
    const answer = el('textarea', { rows: '3', placeholder: 'Paste the answer code', hidden: true, 'aria-label': 'Answer code' });
    const done = el('button', { type: 'button', class: 'btn small primary', hidden: true }, 'Connect');
    done.onclick = async () => {
      try {
        await s.manual.acceptAnswer(answer.value);
      } catch (e) {
        toast(e.message, 6000);
      }
    };
    const make = el('button', { type: 'button', class: 'btn small' }, 'Make my join code');
    make.onclick = async () => {
      try {
        make.disabled = true;
        mine.value = await s.manual.offerCode();
        mine.hidden = answer.hidden = done.hidden = false;
        mine.select();
        await navigator.clipboard?.writeText(mine.value).then(() => toast('Join code copied: send it to the owner'), () => {});
      } catch (e) {
        toast(e.message, 6000);
      } finally {
        make.disabled = false;
      }
    };
    return el(
      'div',
      { class: 'collab-codes-body' },
      el('p', { class: 'muted' }, 'Send your join code to the owner (chat, email); paste the answer code they send back.'),
      make,
      mine,
      answer,
      done,
    );
  }

  /** @private */
  meView() {
    const { el } = this.h.ui;
    const me = this.c.session.me;
    const name = el('input', { type: 'text', maxlength: '40', value: me.name, 'aria-label': 'Your name' });
    name.addEventListener('change', () => this.c.setMe({ name: name.value.trim() || me.name }));
    const swatches = el(
      'div',
      { class: 'collab-colors', role: 'radiogroup', 'aria-label': 'Your colour' },
      COLORS.map((c) =>
        el('button', { type: 'button', role: 'radio', 'aria-checked': String(c === me.color), 'aria-label': c, style: `background:${c}`, onclick: () => (this.c.setMe({ color: c }), this.render()) }),
      ),
    );
    return el('section', { class: 'agents-section' }, el('h3', {}, 'You'), name, swatches);
  }

  /** @private The user's TURN server (DESIGN.md section 6.6). */
  settingsView() {
    const { el, toast } = this.h.ui;
    let turn = {};
    try {
      turn = JSON.parse(localStorage.getItem('nymph.collab.turn') ?? '{}') ?? {};
    } catch {
      turn = {};
    }
    const urls = el('input', { type: 'text', value: turn.urls ?? '', placeholder: 'turns:turn.example.com:443', 'aria-label': 'TURN server' });
    const user = el('input', { type: 'text', value: turn.username ?? '', placeholder: 'user name', 'aria-label': 'TURN user name' });
    const pass = el('input', { type: 'password', value: turn.credential ?? '', placeholder: 'password', 'aria-label': 'TURN password' });
    return el(
      'details',
      { class: 'agents-section' },
      el('summary', {}, 'Connection settings'),
      el('p', { class: 'muted' }, 'Strict networks (offices, some phone networks) need a TURN server to relay the connection. It is used for the next session you start or join.'),
      urls,
      user,
      pass,
      el(
        'button',
        {
          type: 'button',
          class: 'btn small',
          onclick: () => {
            const value = urls.value.trim() ? { urls: urls.value.trim(), username: user.value, credential: pass.value } : null;
            try {
              if (value) {
                localStorage.setItem('nymph.collab.turn', JSON.stringify(value));
              } else {
                localStorage.removeItem('nymph.collab.turn');
              }
            } catch {
              // no storage
            }
            toast(value ? 'TURN server saved for the next session' : 'TURN server removed');
          },
        },
        'Save',
      ),
    );
  }

  /** @private */
  endView() {
    const { el } = this.h.ui;
    if (!this.c.active) {
      return el('section', { class: 'agents-section' }, el('button', { type: 'button', class: 'btn small', onclick: () => this.h.act('collab_stop') }, 'Close'));
    }
    const host = this.c.role === 'host';
    return el(
      'section',
      { class: 'agents-section' },
      el('button', { type: 'button', class: 'btn small danger', onclick: () => this.c.stop({ ask: true }) }, host ? 'End the session' : 'Leave the session'),
    );
  }

  // ── A guest's connecting dialog ────────────────────────────────────────

  /** Shows "Connecting to the chart…" until the first chart arrives. */
  connecting() {
    const { dialog, el } = this.h.ui;
    this.problems = [];
    dialog('Joining a live chart', (body, close) => {
      this.connectingBox = { body, close, status: el('p', { class: 'collab-stage' }, 'Looking for the owner of the chart…'), problem: el('p', { class: 'collab-problem', hidden: true }) };
      body.append(
        this.connectingBox.status,
        this.connectingBox.problem,
        el('details', { class: 'collab-codes' }, el('summary', {}, 'Join with codes instead'), this.guestCodes()),
        el('div', { class: 'row' }, el('button', { type: 'button', class: 'btn', onclick: () => close('cancel') }, 'Cancel')),
      );
    }, { cancelValue: 'cancel' }).then((v) => {
      this.connectingBox = null;
      if (v === 'cancel' && !this.c.opened) {
        this.c.stop({ ask: false });
      }
    });
  }

  /** The first chart arrived. */
  connected() {
    this.connectingBox?.close('done');
    this.connectingBox = null;
  }

  /** @private */
  renderConnecting() {
    const box = this.connectingBox;
    const s = this.c.session;
    if (!box || !s) {
      return;
    }
    box.status.textContent = s.state === 'open' ? 'Connected: waiting for the chart…' : STAGES[s.stage] ?? 'Looking for the owner of the chart…';
  }

  /** A connection problem the user can act on. */
  problem(e) {
    const message = e?.message ?? String(e);
    if (this.connectingBox) {
      this.connectingBox.problem.hidden = false;
      this.connectingBox.problem.textContent = message;
    } else if (this.c.role === 'guest' && this.c.session?.state !== 'open') {
      this.h.ui.toast(message, 8000);
    }
  }
}
