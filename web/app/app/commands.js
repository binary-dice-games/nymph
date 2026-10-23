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
      out.push({
        name: c.name,
        title: c.title ?? c.name,
        description: c.description,
        inputSchema: c.inputSchema,
        annotations: { readOnlyHint: c.effect === 'read', openWorldHint: c.effect === 'external', destructiveHint: false },
        _meta: { scope: c.scope, effect: c.effect },
      });
    }
    return out;
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
      if (c.effect === 'external' && origin !== 'user' && !(await this.confirm(c, args, origin))) {
        throw new CommandError('the user declined');
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

/** The default confirmation of an external command an agent asked for. */
function confirmExternal(command, args, origin) {
  const who = origin.startsWith('agent:') ? origin.slice(6) : origin;
  return dialog(
    'Allow this?',
    (body, close) => {
      body.append(
        el('p', {}, `${who} asks to ${command.title ?? command.name}: ${command.description}`),
        el('code', { class: 'args' }, JSON.stringify(args, null, 2)),
        dialogButtons(close, [
          { label: 'Cancel', value: false },
          { label: 'Allow', value: true, kind: 'primary', focus: true },
        ]),
      );
    },
    { cancelValue: false },
  );
}
