// MIT License © 2026 Binary Dice Games
/**
 * @file commands.js Every action of the app is a command in one registry,
 * shaped as an MCP tool (spec/15_web_app/DESIGN.md section 3.11): buttons,
 * menus, shortcuts and routes call it here, and the same list is what an
 * agent will be given (section 11).
 *
 * A command: {name, title, description, inputSchema, scope, effect, run}.
 *   scope  'app' (always), 'document' (a document is open, either module),
 *          'editor' (Edit mode only)
 *   effect 'read', 'edit' (changes the document, undoable), 'external'
 *          (leaves the tab: opens, saves, downloads, copies)
 * run(args, context) returns a value (made a CallToolResult here) or throws.
 * An 'external' command called by anyone but the user waits for the user's
 * confirmation; the registry enforces it, not the callers.
 *
 * Agents (spec/17_agent_connectors/DESIGN.md section 4) reach it only
 * through callAgent(), which gives their calls an `agent:<connector>`
 * origin, and see agentTools(): every command not marked `agent: false`,
 * plus the editor tools on a desktop, the same list for the page's whole
 * life. Change groups make an agent's several calls one undo step.
 */

import { dialog, dialogButtons, el } from './ui.js';

export class CommandError extends Error {}

export class Commands {
  /**
   * @param {() => {hasDocument: boolean, mode: string}} state what is open now
   * @param {{confirm?: (command, args, origin) => Promise<boolean>}} options
   */
  constructor(state, options = {}) {
    this.state = state;
    this.commands = new Map();
    this.confirm = options.confirm ?? confirmExternal;
    /** Calls, for tests: [{name, origin}]. */
    this.log = [];
    this.listeners = new Set();
    // ── Agents ──
    /** The editor tools' definitions, listed before the editor loads. */
    this.editorTools = [];
    /** Whether this device can run the editor (lists the editor tools). */
    this.editing = options.editing ?? true;
    /** `origin|command` pairs the user allowed for this document. */
    this.allowed = new Set();
    /** The open change group: {origin, label, timer} or null. */
    this.group = null;
    /** Origins the user stopped: their writes are refused until a new group. */
    this.stopped = new Set();
    /** A change group ends after this long without a call from its agent. */
    this.groupIdleMs = 60000;
    /** Hooks the shell sets: editor groups, snapshots for the activity list. */
    this.hooks = {
      beginGroup: () => {},
      endGroup: () => {},
      snapshot: () => null,
    };
    this.agentListeners = new Set();
  }

  /** Adds or replaces a command. */
  define(command) {
    const c = {
      scope: 'app',
      effect: 'read',
      inputSchema: { type: 'object', properties: {}, additionalProperties: false },
      available: () => true,
      ...command,
    };
    if (!c.name || !c.run || !c.description) {
      throw new Error(`command ${c.name} needs a name, a description and run()`);
    }
    this.commands.set(c.name, c);
    return c;
  }

  /** Removes the commands whose name passes `test` (the editor's on destroy). */
  remove(test) {
    for (const name of [...this.commands.keys()]) {
      if (test(name)) {
        this.commands.delete(name);
      }
    }
  }

  /** Whether `name` can run now. */
  available(name) {
    const c = this.commands.get(name);
    if (!c) {
      return false;
    }
    const s = this.state();
    if (c.scope === 'document' && !s.hasDocument) {
      return false;
    }
    if (c.scope === 'editor' && s.mode !== 'edit') {
      return false;
    }
    return c.available();
  }

  /**
   * The commands available now, as `tools/list` entries.
   * @param {{effect?: string[]}} filter
   */
  list(filter = {}) {
    const out = [];
    for (const c of this.commands.values()) {
      if (!this.available(c.name) || (filter.effect && !filter.effect.includes(c.effect))) {
        continue;
      }
      out.push(entry(c));
    }
    return out;
  }

  // ── Agents (spec/17_agent_connectors) ─────────────────────────────────

  /**
   * The tools agents see: the same for the page's life, whatever the mode
   * (DESIGN.md section 4.2), as `tools/list` entries.
   */
  agentTools() {
    const out = [];
    const seen = new Set();
    for (const c of this.commands.values()) {
      if (c.agent === false || (c.scope === 'editor' && !this.editing)) {
        continue;
      }
      if (c.name.startsWith('editor_') && this.editorTools.some((t) => t.name === c.name)) {
        continue; // listed from editorTools below, as before the editor loaded
      }
      seen.add(c.name);
      out.push(entry(c));
    }
    if (this.editing) {
      for (const t of this.editorTools) {
        if (!seen.has(t.name)) {
          out.push({ ...t, _meta: { scope: 'editor', effect: t.annotations?.readOnlyHint ? 'read' : 'edit' } });
        }
      }
    }
    return out;
  }

  /** Listens to agents' calls: cb({name, args, origin, result, effect, before, after, group}). */
  onAgentCall(cb) {
    this.agentListeners.add(cb);
    return () => this.agentListeners.delete(cb);
  }

  /**
   * Runs a command for an agent. `connector` is `webmcp` or `mcp:<client>`;
   * the call's origin is `agent:<connector>`, never `user`.
   * @returns a CallToolResult; never throws for the command's own failures.
   */
  async callAgent(name, args = {}, connector) {
    if (typeof connector !== 'string' || !/^(webmcp|mcp:[a-z0-9-]{1,32})$/.test(connector)) {
      throw new CommandError(`'${connector}' is not an agent connector`);
    }
    const origin = `agent:${connector}`;
    const c = this.commands.get(name);
    const failure = (text) => ({ content: [{ type: 'text', text }], isError: true });
    if (c?.agent === false) {
      return failure(`'${name}' is for the user's own controls, not for agents`);
    }
    if (!c || !this.available(name)) {
      const why = this.unavailableReason(name);
      if (why) {
        return failure(why);
      }
    }
    if (this.group && this.group.origin === origin) {
      this.touchGroup();
    }
    const effect = c?.effect ?? 'read';
    const writes = effect === 'edit';
    if (writes && this.stopped.has(origin)) {
      return failure('the user stopped your changes: ask them before changing the chart again');
    }
    if (writes && this.group && this.group.origin !== origin) {
      return failure('another agent is changing the chart now: try again in a moment');
    }
    const group = writes && this.group?.origin === origin ? this.group : null;
    if (group) {
      this.hooks.beginGroup(group.label, origin); // again, after a user's change ended it
    }
    const before = writes ? this.hooks.snapshot() : null;
    const result = await this.call(name, args, origin);
    const after = writes ? this.hooks.snapshot() : null;
    for (const cb of this.agentListeners) {
      cb({ name, args, origin, result, effect, before, after, group: group?.id ?? null, label: group?.label ?? c?.title ?? name });
    }
    return result;
  }

  /** Why an agent cannot run `name` now, with what to do instead; '' when it can. */
  unavailableReason(name) {
    const c = this.commands.get(name);
    const s = this.state();
    const editorTool = name.startsWith('editor_') && this.editorTools.some((t) => t.name === name);
    if (!c && !editorTool) {
      return `unknown tool '${name}'`;
    }
    if (!s.hasDocument && (editorTool || c?.scope !== 'app')) {
      return 'no chart is open: open one (app_open_example, app_open_url) or make one (app_new_document)';
    }
    if (editorTool || c?.scope === 'editor') {
      if (!this.editing) {
        return 'this device has no editor (a phone or tablet): use document_set_source';
      }
      if (s.mode !== 'edit') {
        return 'the editor is not open: call app_set_mode with mode edit, or use document_set_source';
      }
    }
    if (c && !this.available(name)) {
      return `'${name}' is not available now`;
    }
    return '';
  }

  /** Opens a change group for `origin` (document_begin_change). */
  beginChange(origin, label) {
    if (this.group && this.group.origin !== origin) {
      throw new CommandError('another agent is changing the chart: try again once it is done');
    }
    this.stopped.delete(origin);
    if (!this.group) {
      this.group = { id: `g${Date.now().toString(36)}${Math.random().toString(36).slice(2, 6)}`, origin, label: label || 'Agent change' };
      for (const cb of this.agentListeners) {
        cb({ name: 'document_begin_change', origin, group: this.group.id, label: this.group.label, effect: 'group' });
      }
    }
    this.touchGroup();
    return { group: this.group.id };
  }

  /** Ends the open change group (of `origin`, or any when omitted). */
  endChange(origin) {
    const g = this.group;
    if (!g || (origin && g.origin !== origin)) {
      return false;
    }
    clearTimeout(g.timer);
    this.group = null;
    this.hooks.endGroup();
    for (const cb of this.agentListeners) {
      cb({ name: 'document_end_change', origin: g.origin, group: g.id, label: g.label, effect: 'group' });
    }
    return true;
  }

  /** The user's Stop: ends the agent's group and refuses its writes until it starts a new one. */
  stopAgent(origin) {
    this.endChange(origin);
    this.stopped.add(origin);
  }

  touchGroup() {
    const g = this.group;
    clearTimeout(g.timer);
    g.timer = setTimeout(() => this.endChange(g.origin), this.groupIdleMs);
  }

  /** The document closed: forget what the user allowed for it, end groups. */
  documentClosed() {
    this.allowed.clear();
    this.endChange();
    this.stopped.clear();
  }

  /** Listens to every call: cb({name, args, origin, result}). */
  onCall(cb) {
    this.listeners.add(cb);
    return () => this.listeners.delete(cb);
  }

  /**
   * Runs a command. @returns a CallToolResult ({content, structuredContent,
   * isError}); never throws for the command's own failures.
   */
  async call(name, args = {}, origin = 'user') {
    this.log.push({ name, origin });
    const c = this.commands.get(name);
    let result;
    try {
      if (!c) {
        throw new CommandError(`unknown command '${name}'`);
      }
      if (!this.available(name)) {
        throw new CommandError(`'${name}' is not available now`);
      }
      checkArgs(c.inputSchema, args ?? {});
      if (c.effect === 'external' && origin !== 'user' && !this.allowed.has(`${origin}|${name}`)) {
        const answer = await this.confirm(c, args, origin);
        const allow = answer === true || answer?.allow === true;
        if (!allow) {
          throw new CommandError('the user declined');
        }
        if (answer?.always) {
          this.allowed.add(`${origin}|${name}`);
        }
      }
      const value = await c.run(args ?? {}, { origin });
      result = toResult(value);
    } catch (e) {
      result = { content: [{ type: 'text', text: e.message ?? String(e) }], isError: true };
    }
    for (const cb of this.listeners) {
      cb({ name, args, origin, result });
    }
    return result;
  }

  /** call() for the UI: shows nothing on success, returns the value. */
  async run(name, args = {}) {
    const r = await this.call(name, args, 'user');
    if (r.isError) {
      throw new CommandError(r.content?.[0]?.text ?? 'failed');
    }
    return r.structuredContent ?? r.content?.[0]?.text;
  }
}

/** A command as a `tools/list` entry. */
function entry(c) {
  return {
    name: c.name,
    title: c.title ?? c.name,
    description: c.description,
    inputSchema: c.inputSchema,
    annotations: { readOnlyHint: c.effect === 'read', openWorldHint: c.effect === 'external', destructiveHint: false },
    _meta: { scope: c.scope, effect: c.effect },
  };
}

function toResult(value) {
  if (value === undefined || value === null) {
    return { content: [{ type: 'text', text: 'done' }] };
  }
  if (typeof value === 'string') {
    return { content: [{ type: 'text', text: value }] };
  }
  if (value.content) {
    return value; // already a CallToolResult
  }
  return { content: [{ type: 'text', text: JSON.stringify(value) }], structuredContent: value };
}

/** A small check of the arguments against the schema: types and required. */
function checkArgs(schema, args) {
  if (typeof args !== 'object' || Array.isArray(args)) {
    throw new CommandError('the arguments must be an object');
  }
  for (const key of schema.required ?? []) {
    if (!(key in args)) {
      throw new CommandError(`missing argument '${key}'`);
    }
  }
  const props = schema.properties ?? {};
  for (const [key, value] of Object.entries(args)) {
    const p = props[key];
    if (!p) {
      if (schema.additionalProperties === false) {
        throw new CommandError(`unknown argument '${key}'`);
      }
      continue;
    }
    const type = Array.isArray(value) ? 'array' : value === null ? 'null' : typeof value;
    const want = p.type === 'integer' ? 'number' : p.type;
    if (want && type !== want) {
      throw new CommandError(`'${key}' must be ${p.type === 'integer' ? 'an integer' : `a ${p.type}`}`);
    }
    if (p.enum && !p.enum.includes(value)) {
      throw new CommandError(`'${key}' must be one of ${p.enum.join(', ')}`);
    }
  }
}

/** An agent origin as the user reads it: `mcp:claude-code` is "claude-code". */
export function agentName(origin) {
  const who = origin.startsWith('agent:') ? origin.slice(6) : origin;
  return who.startsWith('mcp:') ? who.slice(4) : who === 'webmcp' ? 'the browser\'s assistant' : who;
}

/**
 * The default confirmation of an external command an agent asked for.
 * @returns {Promise<{allow: boolean, always: boolean}>}
 */
function confirmExternal(command, args, origin) {
  const who = agentName(origin);
  const always = el('input', { type: 'checkbox', id: 'allow-always' });
  return dialog(
    'Allow this?',
    (body, close) => {
      body.append(
        el('p', {}, `${who} asks to ${command.title ?? command.name}: ${command.description}`),
        el('code', { class: 'args' }, JSON.stringify(args, null, 2)),
        el('label', { class: 'check' }, always, ` Allow ${who} to do this again without asking, until this chart is closed`),
        dialogButtons(close, [
          { label: 'Cancel', value: false },
          { label: 'Allow', value: true, kind: 'primary', focus: true },
        ]),
      );
    },
    { cancelValue: false },
  ).then((allow) => ({ allow: allow === true, always: allow === true && always.checked }));
}
