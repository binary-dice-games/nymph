// MIT License © 2026 Binary Dice Games
/**
 * @file bridge.js The page side of the `nymph mcp` bridge
 * (spec/17_agent_connectors/DESIGN.md section 7.2). `nymph mcp` listens on
 * 127.0.0.1 after its agent called `web_app_pair`; the user types the code
 * it gave (or opens its `#pair=` link), the page connects, proves it with
 * the code, and then serves MCP over the socket: `initialize`,
 * `tools/list` (Commands.agentTools) and `tools/call` (Commands.callAgent
 * with the connector `mcp:<client>`). A resume token kept for this tab
 * reconnects after a reload while that `nymph mcp` runs.
 */

const PROTOCOLS = ['2025-11-25', '2025-06-18', '2025-03-26'];
const TOKENS = 'nymph.bridge.'; // sessionStorage: one per port

/** `48213-K7QM-2XPD` (any case, spaces dropped) -> {port, code}; null when it is not one. */
export function parseCode(text) {
  const t = String(text ?? '').replace(/\s+/g, '').toUpperCase();
  const m = /^(\d{2,5})-([0-9A-HJKMNP-TV-Z]{4})-([0-9A-HJKMNP-TV-Z]{4})$/.exec(t);
  if (!m || Number(m[1]) < 1 || Number(m[1]) > 65535) {
    return null;
  }
  return { port: Number(m[1]), code: t };
}

/** A client name as the registry takes it: `[a-z0-9-]`, at most 32. */
function connectorOf(client) {
  const name = String(client ?? '')
    .toLowerCase()
    .replace(/[^a-z0-9-]+/g, '-')
    .replace(/^-+|-+$/g, '')
    .slice(0, 32);
  return `mcp:${name || 'agent'}`;
}

function store(port, token) {
  try {
    if (token) {
      sessionStorage.setItem(TOKENS + port, token);
    } else {
      sessionStorage.removeItem(TOKENS + port);
    }
  } catch {
    // no storage: a reload pairs again
  }
}

export class Bridge {
  /**
   * @param {import('../commands.js').Commands} commands
   * @param {{documentName: () => string, version: string, instructions: string,
   *          connected: (id, info) => void, disconnected: (id) => void}} hooks
   */
  constructor(commands, hooks) {
    this.commands = commands;
    this.hooks = hooks;
    /** Open links: port -> {ws, connector}. */
    this.links = new Map();
    /** The socket URL; tests may point it elsewhere. */
    this.url = (port) => `ws://127.0.0.1:${port}/nymph`;
    window.addEventListener('focus', () => this.focus());
  }

  /** Pairs with the `nymph mcp` that gave `text` (a pairing code). */
  async pair(text) {
    const parsed = parseCode(text);
    if (!parsed) {
      throw new Error('That is not a pairing code: it looks like 48213-K7QM-2XPD.');
    }
    return this.connect(parsed.port, { pair: parsed.code });
  }

  /** Reconnects the links this tab had before a reload. */
  async resume() {
    let keys = [];
    try {
      keys = Object.keys(sessionStorage).filter((k) => k.startsWith(TOKENS));
    } catch {
      return;
    }
    for (const key of keys) {
      const port = Number(key.slice(TOKENS.length));
      const token = sessionStorage.getItem(key);
      try {
        await this.connect(port, { resume: token }, { quiet: true });
      } catch {
        store(port, null); // that nymph mcp is gone
      }
    }
  }

  /** Tells every paired `nymph mcp` that this tab has the focus (and its chart's name). */
  focus() {
    for (const link of this.links.values()) {
      this.send(link, { jsonrpc: '2.0', method: 'nymph/focus', params: { name: this.hooks.documentName() } });
    }
  }

  disconnect(port) {
    const link = this.links.get(port);
    store(port, null);
    link?.ws.close(1000);
  }

  connect(port, hello, { quiet = false } = {}) {
    return new Promise((resolve, reject) => {
      if (this.links.has(port)) {
        this.links.get(port).ws.close(1000);
        this.links.delete(port);
      }
      let ws;
      try {
        ws = new WebSocket(this.url(port));
      } catch (e) {
        reject(new Error(`Cannot connect: ${e.message}`));
        return;
      }
      let paired = false;
      let settled = false;
      const fail = (message) => {
        if (!settled) {
          settled = true;
          reject(new Error(message));
        }
      };
      ws.onopen = () => ws.send(JSON.stringify({ ...hello, name: this.hooks.documentName() }));
      ws.onerror = () =>
        fail(
          `Could not reach the agent on this computer (port ${port}). Check that it is still running and ask it for a new code. ` +
            'If your browser asked to let this site reach apps on this device, allow it and try again.',
        );
      ws.onclose = () => {
        fail('The agent closed the connection: ask it for a new code.');
        if (paired) {
          const link = this.links.get(port);
          this.links.delete(port);
          if (link) {
            this.commands.endChange(`agent:${link.connector}`);
          }
          this.hooks.disconnected(`bridge:${port}`);
        }
      };
      ws.onmessage = async (event) => {
        let message;
        try {
          message = JSON.parse(event.data);
        } catch {
          return;
        }
        if (!paired) {
          if (message.paired) {
            paired = true;
            settled = true;
            store(port, message.resume);
            const link = { ws, port, connector: 'mcp:agent' };
            this.links.set(port, link);
            this.hooks.connected(`bridge:${port}`, { name: 'An agent', route: 'through nymph mcp', disconnect: () => this.disconnect(port) });
            resolve();
          } else {
            if (quiet) {
              store(port, null);
            }
            fail(message.error ?? 'The agent refused the code: ask it for a new one.');
            ws.close();
          }
          return;
        }
        const link = this.links.get(port);
        if (link && message.method && message.id !== undefined) {
          this.send(link, await this.answer(link, message));
        }
      };
    });
  }

  send(link, message) {
    if (link.ws.readyState === WebSocket.OPEN) {
      link.ws.send(JSON.stringify(message));
    }
  }

  /** The page as an MCP server: one request, one response. */
  async answer(link, { id, method, params }) {
    const reply = (result) => ({ jsonrpc: '2.0', id, result });
    const error = (code, message) => ({ jsonrpc: '2.0', id, error: { code, message } });
    try {
      if (method === 'initialize') {
        const asked = params?.protocolVersion;
        return reply({
          protocolVersion: PROTOCOLS.includes(asked) ? asked : PROTOCOLS[0],
          capabilities: { tools: { listChanged: true } },
          serverInfo: { name: 'nymph-web-app', title: 'nymph web app', version: this.hooks.version },
          instructions: this.hooks.instructions,
        });
      }
      if (method === 'ping') {
        return reply({});
      }
      if (method === 'tools/list') {
        return reply({ tools: this.commands.agentTools() });
      }
      if (method === 'tools/call') {
        if (typeof params?.name !== 'string') {
          return error(-32602, 'tools/call needs "name"');
        }
        const connector = connectorOf(params?._meta?.['nymph/origin']);
        if (connector !== link.connector) {
          link.connector = connector;
          this.hooks.connected(`bridge:${link.port}`, {
            name: connector.slice(4),
            route: 'through nymph mcp',
            disconnect: () => this.disconnect(link.port),
          });
        }
        return reply(await this.commands.callAgent(params.name, params.arguments ?? {}, connector));
      }
      return error(-32601, `method not found: ${method}`);
    } catch (e) {
      return error(-32603, e.message ?? String(e));
    }
  }
}
