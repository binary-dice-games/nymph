// MIT License © 2026 Binary Dice Games
/**
 * @file store.js Recent files and drafts, in this browser only (IndexedDB;
 * spec/15_web_app/DESIGN.md section 3.5). Every call fails soft: a browser
 * without storage (a private window) still runs the app.
 */

const DB = 'nymph-app';
const VERSION = 1;
const MAX_RECENT = 20;
/** Sources kept in a recent entry, for origins that cannot be fetched again. */
const MAX_KEPT = 1024 * 1024;

let dbPromise = null;

function openDb() {
  if (!dbPromise) {
    dbPromise = new Promise((resolve, reject) => {
      if (typeof indexedDB === 'undefined') {
        reject(new Error('no IndexedDB'));
        return;
      }
      const req = indexedDB.open(DB, VERSION);
      req.onupgradeneeded = () => {
        const db = req.result;
        db.createObjectStore('recent', { keyPath: 'id' });
        db.createObjectStore('drafts', { keyPath: 'id' });
      };
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });
    dbPromise.catch(() => {});
  }
  return dbPromise;
}

async function tx(store, mode, fn) {
  try {
    const db = await openDb();
    return await new Promise((resolve, reject) => {
      const t = db.transaction(store, mode);
      const s = t.objectStore(store);
      let result;
      Promise.resolve(fn(s)).then((r) => (result = r));
      t.oncomplete = () => resolve(result);
      t.onerror = () => reject(t.error);
      t.onabort = () => reject(t.error);
    });
  } catch {
    return undefined;
  }
}

const req = (r) => new Promise((resolve, reject) => {
  r.onsuccess = () => resolve(r.result);
  r.onerror = () => reject(r.error);
});

/** The recent documents, newest first. */
export async function recent() {
  const all = (await tx('recent', 'readonly', (s) => req(s.getAll()))) ?? [];
  return all.sort((a, b) => b.openedAt - a.openedAt);
}

/**
 * Records `doc` as opened now. What reopens it: a file handle, the URL, the
 * Drive id, the example name, or (for the rest, under 1 MB) the source.
 */
export async function remember(doc, thumb) {
  const o = doc.origin;
  const entry = {
    id: doc.id,
    name: doc.name,
    kind: o.kind,
    url: o.url,
    fileId: o.fileId,
    example: o.example,
    handle: o.handle ?? undefined,
    png: doc.png,
    openedAt: Date.now(),
    thumb,
  };
  if (!['url', 'drive', 'example'].includes(o.kind) && !o.handle && doc.savedSource.length < MAX_KEPT) {
    entry.source = doc.savedSource;
  }
  const put = (e) =>
    tx('recent', 'readwrite', async (s) => {
      const old = await req(s.get(e.id));
      if (old && !thumb) {
        e.thumb = old.thumb;
      }
      s.put(e);
    });
  try {
    structuredClone(entry); // a handle some browsers cannot store
  } catch {
    delete entry.handle;
  }
  await put(entry);
  const all = await recent();
  if (all.length > MAX_RECENT) {
    await tx('recent', 'readwrite', (s) => all.slice(MAX_RECENT).forEach((e) => s.delete(e.id)));
  }
}

export async function forgetRecent(id) {
  await tx('recent', 'readwrite', (s) => s.delete(id));
}

/** Keeps the unsaved source of `doc`. */
export async function saveDraft(doc) {
  await tx('drafts', 'readwrite', (s) =>
    s.put({ id: doc.id, name: doc.name, source: doc.source, base: doc.savedSource, savedAt: Date.now() }),
  );
}

export async function draft(id) {
  return tx('drafts', 'readonly', (s) => req(s.get(id)));
}

export async function dropDraft(id) {
  await tx('drafts', 'readwrite', (s) => s.delete(id));
}

/** Empties both stores. */
export async function clearAll() {
  await tx('recent', 'readwrite', (s) => s.clear());
  await tx('drafts', 'readwrite', (s) => s.clear());
}
