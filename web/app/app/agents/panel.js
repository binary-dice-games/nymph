// MIT License © 2026 Binary Dice Games
/**
 * @file panel.js The Agents panel in the document screen's dock
 * (spec/17_agent_connectors/DESIGN.md section 5): who is connected, the
 * Connect an agent dialog, the WebMCP switch, and every change agents made,
 * with Undo. Everything an agent sent is shown as text, never as HTML.
 */

import { agentName } from '../commands.js';
import { icons } from '../icons.js';
import { dialog, dialogButtons, el, toast } from '../ui.js';

export class AgentsPanel {
  /**
   * @param {{dock: HTMLElement, button: HTMLElement, commands, activity,
   *          offers: {bridge: boolean, webmcp: boolean},
   *          webmcpEnabled: () => boolean, setWebmcp: (on: boolean) => void,
   *          pair: (code: string) => Promise<void>}} options
   */
  constructor(options) {
    Object.assign(this, options);
    /** Connected agents: id -> {name, route, disconnect?}. */
    this.connections = new Map();
    this.isOpen = false;
    /** Opened by itself once per chart, on an agent's first change. */
    this.autoOpened = false;
    this.button.hidden = !(this.offers.bridge || this.offers.webmcp);
    this.button.addEventListener('click', () => this.toggle());
    this.activity.onChange((entries, added) => {
      if (added && !this.isOpen && !this.autoOpened) {
        this.autoOpened = true;
        this.open();
      } else {
        this.render();
      }
    });
    this.commands.onAgentCall(() => this.renderStatus());
  }

  open() {
    this.isOpen = true;
    this.dock.hidden = false;
    this.button.setAttribute('aria-pressed', 'true');
    this.render();
  }

  close() {
    this.isOpen = false;
    this.dock.hidden = true;
    this.button.setAttribute('aria-pressed', 'false');
  }

  toggle() {
    if (this.isOpen) {
      this.close();
    } else {
      this.open();
    }
  }

  /** A new chart: its activity starts empty, and the panel may open again. */
  documentChanged() {
    this.autoOpened = false;
    this.render();
  }

  /** An agent connected (a bridge pairing, a WebMCP caller). */
  connected(id, info) {
    this.connections.set(id, info);
    this.render();
  }

  disconnected(id) {
    this.connections.delete(id);
    this.render();
  }

  render() {
    if (!this.isOpen) {
      return;
    }
    const head = el(
      'div',
      { class: 'agents-head' },
      el('h2', {}, 'Agents'),
      el('button', { type: 'button', class: 'btn icon ghost', 'aria-label': 'Close the agents panel', html: icons.close, onclick: () => this.close() }),
    );
    this.statusBox = el('div', { class: 'agents-status' });
    this.dock.replaceChildren(head, this.statusBox, this.connectionsView(), this.activityView());
    this.renderStatus();
  }

  /** "Claude Code is changing the chart · Stop", while a change group is open. */
  renderStatus() {
    if (!this.isOpen || !this.statusBox) {
      return;
    }
    const g = this.commands.group;
    if (!g) {
      this.statusBox.replaceChildren();
      this.statusBox.hidden = true;
      return;
    }
    this.statusBox.hidden = false;
    this.statusBox.replaceChildren(
      el('span', { class: 'pulse', 'aria-hidden': 'true' }),
      el('span', { class: 'text' }, `${agentName(g.origin)} is changing the chart: ${g.label}`),
      el(
        'button',
        {
          type: 'button',
          class: 'btn small',
          onclick: () => {
            this.commands.stopAgent(g.origin);
            toast(`Stopped ${agentName(g.origin)}. It will ask you before changing the chart again.`);
            this.render();
          },
        },
        'Stop',
      ),
    );
  }

  connectionsView() {
    const list = el('ul', { class: 'agents-list' });
    for (const [id, c] of this.connections) {
      list.append(
        el(
          'li',
          {},
          el('span', { class: 'dot', 'aria-hidden': 'true' }),
          el('span', { class: 'name' }, c.name),
          el('span', { class: 'route' }, c.route),
          c.disconnect && el('button', { type: 'button', class: 'link', onclick: () => c.disconnect(id) }, 'Disconnect'),
        ),
      );
    }
    const section = el('section', { class: 'agents-section', 'aria-label': 'Connected agents' });
    if (this.connections.size) {
      section.append(list);
    } else {
      section.append(
        el(
          'p',
          { class: 'muted' },
          this.offers.bridge
            ? 'No agent is connected. Ask Claude Code, Cursor or Gemini CLI to "connect to the nymph web app", then type the code it gives you.'
            : 'No agent is connected.',
        ),
      );
    }
    if (this.offers.bridge) {
      section.append(el('button', { type: 'button', class: 'btn', id: 'agents-connect', onclick: () => this.connectDialog() }, 'Connect an agent…'));
    }
    if (this.offers.webmcp) {
      const box = el('input', { type: 'checkbox', id: 'agents-webmcp', checked: this.webmcpEnabled() });
      box.addEventListener('change', () => this.setWebmcp(box.checked));
      section.append(el('label', { class: 'check' }, box, ' Let the browser\'s assistant use this chart'));
    }
    return section;
  }

  activityView() {
    const section = el('section', { class: 'agents-section activity', 'aria-label': 'Changes by agents' });
    const entries = [...this.activity.entries].reverse();
    if (!entries.length) {
      section.append(el('p', { class: 'muted' }, 'Changes agents make to this chart are listed here, each with Undo.'));
      return section;
    }
    const list = el('ol', { class: 'activity-list', 'aria-live': 'polite' });
    for (const e of entries) {
      list.append(this.entryView(e));
    }
    section.append(el('h3', {}, 'Changes'), list);
    return section;
  }

  entryView(e) {
    const when = new Date(e.at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    const how = this.activity.undoable(e);
    const status = e.kind === 'external' ? (e.declined ? 'declined' : 'allowed') : e.open ? 'in progress' : e.undone ? 'undone' : '';
    const details = el(
      'details',
      {},
      el('summary', {}, `${e.tools.length} ${e.tools.length === 1 ? 'step' : 'steps'}`),
      el(
        'ul',
        { class: 'tools' },
        e.tools.map((t) => el('li', { class: t.ok ? '' : 'failed' }, el('code', {}, t.name), t.ok ? '' : ` failed: ${t.message}`)),
      ),
    );
    return el(
      'li',
      { class: `entry ${e.undone ? 'undone' : ''}`, dataset: { entry: String(e.id) } },
      el(
        'div',
        { class: 'line' },
        el('span', { class: 'who' }, e.who),
        el('span', { class: 'label' }, e.label),
        el('time', {}, when),
      ),
      el(
        'div',
        { class: 'line' },
        details,
        status && el('span', { class: 'state' }, status),
        how &&
          el(
            'button',
            {
              type: 'button',
              class: 'btn small',
              'aria-label': `Undo: ${e.label}`,
              onclick: async () => {
                try {
                  await this.activity.undo(e);
                } catch (err) {
                  toast(err.message ?? String(err), 6000);
                }
              },
            },
            'Undo',
          ),
      ),
    );
  }

  /** Connect an agent: the code `web_app_pair` gave the agent. */
  async connectDialog() {
    const code = await dialog('Connect an agent', (body, close) => {
      const input = el('input', {
        type: 'text',
        id: 'pair-code',
        placeholder: '48213-K7QM-2XPD',
        autocomplete: 'off',
        spellcheck: 'false',
        'aria-label': 'Pairing code',
        autofocus: true,
      });
      const buttons = dialogButtons(close, [{ label: 'Cancel', value: undefined }]);
      buttons.append(el('button', { type: 'submit', class: 'btn primary' }, 'Connect'));
      const form = el('form', {}, input, buttons);
      form.onsubmit = (e) => (e.preventDefault(), close(input.value.trim()));
      body.append(
        el('p', {}, 'Ask your agent to connect to the nymph web app. It runs nymph (Claude Code, Claude Desktop, Cursor, Gemini CLI), and gives you a code like 48213-K7QM-2XPD. Type it here:'),
        form,
        el('p', { class: 'muted' }, 'The agent then sees this chart and can change it; every change is listed here, with Undo. Your browser may ask to let this site reach apps on this device: allow it.'),
      );
    });
    if (code) {
      try {
        await this.pair(code);
      } catch (e) {
        toast(e.message ?? String(e), 8000);
      }
    }
  }
}
