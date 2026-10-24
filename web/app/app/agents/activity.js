// MIT License © 2026 Binary Dice Games
/**
 * @file activity.js What agents did to the open chart, with Undo
 * (spec/17_agent_connectors/DESIGN.md section 5). One entry per change
 * group, or per write outside a group, and one per `external` action. Each
 * write entry keeps the source before and after it, because View mode has
 * no undo history; in Edit mode the editor's own undo is used while the
 * entry is still its last step.
 */

import { agentName } from '../commands.js';
import { withoutView } from '../document.js';

/** The most entries kept for the open chart. */
const MAX_ENTRIES = 50;

export class Activity {
  /**
   * @param {import('../commands.js').Commands} commands
   * @param {{snapshot: () => object|null, restore: (source: string) => Promise<void>,
   *          undoEditor: () => void, confirm: (message: string) => Promise<boolean>}} hooks
   */
  constructor(commands, hooks) {
    this.commands = commands;
    this.hooks = hooks;
    /** Newest last. */
    this.entries = [];
    this.listeners = new Set();
    this.nextId = 1;
    commands.onAgentCall((call) => this.record(call));
  }

  /** Listens to changes of the list: cb(entries, added). */
  onChange(cb) {
    this.listeners.add(cb);
    return () => this.listeners.delete(cb);
  }

  emit(added = null) {
    for (const cb of this.listeners) {
      cb(this.entries, added);
    }
  }

  /** The chart closed: its changes can no longer be undone. */
  clear() {
    this.entries = [];
    this.emit();
  }

  /** One agent call (Commands.onAgentCall). */
  record(call) {
    const who = agentName(call.origin);
    if (call.effect === 'group') {
      if (call.name === 'document_begin_change') {
        this.add({ kind: 'change', origin: call.origin, who, label: call.label, group: call.group, tools: [], open: true });
      } else {
        const e = this.entries.find((x) => x.group === call.group);
        if (e) {
          e.open = false;
          if (!e.after) {
            this.entries.splice(this.entries.indexOf(e), 1); // nothing was changed
          }
          this.emit();
        }
      }
      return;
    }
    if (call.effect === 'read') {
      return;
    }
    const ok = !call.result?.isError;
    const tool = { name: call.name, args: call.args, ok, message: ok ? '' : call.result?.content?.[0]?.text ?? '' };
    if (call.effect === 'external') {
      this.add({ kind: 'external', origin: call.origin, who, label: call.label, tools: [tool], declined: !ok });
      return;
    }
    let e = call.group ? this.entries.find((x) => x.group === call.group) : null;
    if (!e) {
      if (!ok) {
        return; // a refused write outside a group changed nothing
      }
      e = this.add({ kind: 'change', origin: call.origin, who, label: call.label, tools: [] }, false);
    }
    e.tools.push(tool);
    if (ok && call.before && call.after && withoutView(call.before.source) !== withoutView(call.after.source)) {
      e.before ??= call.before;
      e.after = call.after;
    }
    if (!e.group && !e.after) {
      this.entries.splice(this.entries.indexOf(e), 1);
    }
    this.emit(e);
  }

  add(entry, emit = true) {
    const e = { id: this.nextId++, at: Date.now(), undone: false, before: null, after: null, ...entry };
    this.entries.push(e);
    if (this.entries.length > MAX_ENTRIES) {
      this.entries.shift();
    }
    if (emit) {
      this.emit(e);
    }
    return e;
  }

  /** Whether `entry` can be undone now, and how. */
  undoable(entry) {
    if (entry.kind !== 'change' || entry.undone || entry.open || !entry.before || !entry.after) {
      return null;
    }
    const now = this.hooks.snapshot();
    if (!now || now.doc !== entry.after.doc || entry.before.doc !== entry.after.doc) {
      return null;
    }
    if (now.mode === 'edit' && entry.after.mode === 'edit' && now.version !== null && now.version === entry.after.version) {
      return 'editor'; // still the editor's last step
    }
    return withoutView(now.source) === withoutView(entry.after.source) ? 'restore' : 'restore-later';
  }

  /** Undoes `entry`. @returns whether it was undone. */
  async undo(entry) {
    const how = this.undoable(entry);
    if (!how) {
      return false;
    }
    if (how === 'editor') {
      this.hooks.undoEditor();
    } else {
      if (how === 'restore-later' && !(await this.hooks.confirm('Restore the chart from before this change? Later changes are lost.'))) {
        return false;
      }
      await this.hooks.restore(entry.before.source);
    }
    entry.undone = true;
    this.emit();
    return true;
  }
}
