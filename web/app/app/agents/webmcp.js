// MIT License © 2026 Binary Dice Games
/**
 * @file webmcp.js The app's agent tools registered with the browser's own
 * agent through WebMCP (`navigator.modelContext`, a draft of the W3C Web
 * Machine Learning community group; spec/17_agent_connectors/DESIGN.md
 * section 7.1). Registered while a chart is open, removed on the start
 * screen; every call goes through Commands.callAgent with the `webmcp`
 * connector. The draft has two ways to register, both handled: one tool
 * at a time (`registerTool`, returning a handle to unregister) or the whole
 * list (`provideContext`, cleared with `clearContext`).
 */

const KEY = 'nymph.webmcp';

export class WebMcp {
  /**
   * @param {import('../commands.js').Commands} commands
   * @param {{onCall?: () => void}} options
   */
  constructor(commands, options = {}) {
    this.commands = commands;
    this.onCall = options.onCall ?? (() => {});
    this.mc = typeof navigator !== 'undefined' ? navigator.modelContext : undefined;
    this.handles = [];
    this.registered = false;
  }

  /** Whether the browser has WebMCP. */
  get available() {
    return !!this.mc && (typeof this.mc.registerTool === 'function' || typeof this.mc.provideContext === 'function');
  }

  /** The user's switch (Agents panel); on unless turned off in this browser. */
  get enabled() {
    try {
      return localStorage.getItem(KEY) !== 'off';
    } catch {
      return true;
    }
  }

  setEnabled(on, hasDocument) {
    try {
      localStorage.setItem(KEY, on ? 'on' : 'off');
    } catch {
      // the switch lasts for the page
    }
    this.update(hasDocument);
  }

  /** Registers the tools while a chart is open and the switch is on. */
  update(hasDocument) {
    const want = this.available && this.enabled && hasDocument;
    if (want && !this.registered) {
      this.register();
    } else if (!want && this.registered) {
      this.unregister();
    }
  }

  tools() {
    return this.commands.agentTools().map((t) => ({
      name: t.name,
      description: t.description,
      inputSchema: t.inputSchema,
      annotations: t.annotations,
      execute: async (input) => {
        this.onCall();
        return this.commands.callAgent(t.name, input ?? {}, 'webmcp');
      },
    }));
  }

  register() {
    const tools = this.tools();
    try {
      if (typeof this.mc.registerTool === 'function') {
        this.handles = tools.map((t) => this.mc.registerTool(t));
      } else {
        this.mc.provideContext({ tools });
      }
      this.registered = true;
    } catch (e) {
      console.warn('nymph: WebMCP registration failed', e);
    }
  }

  unregister() {
    try {
      if (this.handles.length) {
        for (const h of this.handles) {
          h?.unregister?.();
        }
        this.handles = [];
      } else if (typeof this.mc.clearContext === 'function') {
        this.mc.clearContext();
      } else if (typeof this.mc.provideContext === 'function') {
        this.mc.provideContext({ tools: [] });
      }
    } catch (e) {
      console.warn('nymph: WebMCP unregistration failed', e);
    }
    this.registered = false;
  }
}
