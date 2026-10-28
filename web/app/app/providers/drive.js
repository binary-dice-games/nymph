// MIT License © 2026 Binary Dice Games
/**
 * @file drive.js Google Drive as a place to open and save charts
 * (spec/15_web_app/DESIGN.md section 3.8). Scope `drive.file`: the app sees
 * only the files the user picks with it or saves with it. Google's scripts
 * load the first time a Drive action is used; the access token stays in
 * memory. Configured by config.json's `google` (client id, API key, Cloud
 * project number); without it the provider is not available and no Drive
 * control is shown.
 *
 * A provider has: label, available(), open(), openById(id), save(doc,
 * bytes, mime), saveAs(doc, bytes, name, mime).
 */

const SCOPE = 'https://www.googleapis.com/auth/drive.file';
const GIS = 'https://accounts.google.com/gsi/client';
const GAPI = 'https://apis.google.com/js/api.js';
const API = 'https://www.googleapis.com/drive/v3';
const UPLOAD = 'https://www.googleapis.com/upload/drive/v3';

export class DriveError extends Error {
  constructor(message, kind = 'drive') {
    super(message);
    this.kind = kind;
  }
}

const loaded = new Map();

/** Loads a script once. */
function loadScript(src) {
  if (!loaded.has(src)) {
    loaded.set(
      src,
      new Promise((resolve, reject) => {
        const s = document.createElement('script');
        s.src = src;
        s.async = true;
        s.onload = resolve;
        s.onerror = () => {
          loaded.delete(src);
          reject(new DriveError(`Google's script could not be loaded (${src})`, 'network'));
        };
        document.head.append(s);
      }),
    );
  }
  return loaded.get(src);
}

export class DriveProvider {
  /** @param {{clientId?: string, apiKey?: string, appId?: string}} config */
  constructor(config = {}) {
    this.config = config;
    this.label = 'Google Drive';
    this.token = null;
    this.tokenExpires = 0;
  }

  available() {
    return !!(this.config.clientId && this.config.apiKey);
  }

  /** @private Google's two scripts and the Picker. */
  async load() {
    await Promise.all([loadScript(GIS), loadScript(GAPI)]);
    await new Promise((resolve, reject) => window.gapi.load('picker', { callback: resolve, onerror: reject }));
  }

  /** @private An access token, asking the user to sign in when needed. */
  async signIn() {
    if (this.token && Date.now() < this.tokenExpires - 60000) {
      return this.token;
    }
    await this.load();
    return new Promise((resolve, reject) => {
      const client = window.google.accounts.oauth2.initTokenClient({
        client_id: this.config.clientId,
        scope: SCOPE,
        callback: (response) => {
          if (response.error) {
            reject(new DriveError(`Google sign-in failed: ${response.error}`, 'auth'));
            return;
          }
          this.token = response.access_token;
          this.tokenExpires = Date.now() + (Number(response.expires_in) || 3600) * 1000;
          resolve(this.token);
        },
        error_callback: (e) => reject(new DriveError(`Google sign-in was not completed (${e.type ?? 'closed'})`, 'auth')),
      });
      client.requestAccessToken({ prompt: this.token ? '' : 'consent' });
    });
  }

  /** @private The Picker: files (`folders` false) or a folder; resolves with the picked doc or null. */
  async pick(folders) {
    const token = await this.signIn();
    const p = window.google.picker;
    return new Promise((resolve) => {
      const view = folders
        ? new p.DocsView(p.ViewId.FOLDERS).setSelectFolderEnabled(true).setIncludeFolders(true)
        : new p.DocsView(p.ViewId.DOCS).setMimeTypes('application/x-yaml,text/yaml,text/plain,image/png,application/octet-stream');
      const builder = new p.PickerBuilder()
        .addView(view)
        .setOAuthToken(token)
        .setDeveloperKey(this.config.apiKey)
        .setCallback((data) => {
          if (data.action === p.Action.PICKED) {
            resolve(data.docs[0]);
          } else if (data.action === p.Action.CANCEL) {
            resolve(null);
          }
        });
      if (this.config.appId) {
        builder.setAppId(this.config.appId);
      }
      builder.build().setVisible(true);
    });
  }

  /** @private A Drive REST call with the token. */
  async request(url, init = {}) {
    const token = await this.signIn();
    const response = await fetch(url, { ...init, headers: { ...(init.headers ?? {}), authorization: `Bearer ${token}` } });
    if (response.status === 401) {
      this.token = null;
    }
    if (!response.ok) {
      let detail = '';
      try {
        detail = (await response.json()).error?.message ?? '';
      } catch {
        // no JSON body
      }
      throw new DriveError(`Google Drive answered ${response.status}${detail ? `: ${detail}` : ''}`, response.status === 404 ? 'missing' : 'http');
    }
    return response;
  }

  /** @private The file's name and current revision. */
  async meta(fileId) {
    const r = await this.request(`${API}/files/${encodeURIComponent(fileId)}?fields=id,name,mimeType,headRevisionId,size`);
    return r.json();
  }

  /** Picks a file and reads it: {bytes, name, origin} or null. */
  async open() {
    const picked = await this.pick(false);
    return picked ? this.openById(picked.id) : null;
  }

  /** Reads file `fileId` (the `?drive=` route). */
  async openById(fileId) {
    const m = await this.meta(fileId);
    const r = await this.request(`${API}/files/${encodeURIComponent(fileId)}?alt=media`);
    const bytes = new Uint8Array(await r.arrayBuffer());
    return { bytes, name: m.name, origin: { kind: 'drive', fileId, revision: m.headRevisionId } };
  }

  /**
   * Writes `bytes` over the file the document came from. Refuses with kind
   * "conflict" when the file changed in Drive since it was read.
   */
  async save(doc, bytes, mime, { overwrite = false } = {}) {
    const fileId = doc.origin.fileId;
    if (!overwrite) {
      const m = await this.meta(fileId);
      if (doc.origin.revision && m.headRevisionId && m.headRevisionId !== doc.origin.revision) {
        throw new DriveError('the file changed in Google Drive since you opened it', 'conflict');
      }
    }
    const r = await this.request(`${UPLOAD}/files/${encodeURIComponent(fileId)}?uploadType=media&fields=id,headRevisionId`, {
      method: 'PATCH',
      headers: { 'content-type': mime },
      body: bytes,
    });
    const out = await r.json();
    doc.origin.revision = out.headRevisionId;
    return out;
  }

  /** Creates a new file in a folder the user picks (or My Drive): the new origin. */
  async saveAs(doc, bytes, name, mime) {
    const folder = await this.pick(true);
    if (folder === null) {
      return null;
    }
    const boundary = `nymph${Math.random().toString(16).slice(2)}`;
    const metadata = { name, mimeType: mime, ...(folder?.id ? { parents: [folder.id] } : {}) };
    const head = new TextEncoder().encode(
      `--${boundary}\r\ncontent-type: application/json; charset=UTF-8\r\n\r\n${JSON.stringify(metadata)}\r\n--${boundary}\r\ncontent-type: ${mime}\r\n\r\n`,
    );
    const tail = new TextEncoder().encode(`\r\n--${boundary}--`);
    const data = typeof bytes === 'string' ? new TextEncoder().encode(bytes) : bytes;
    const body = new Uint8Array(head.length + data.length + tail.length);
    body.set(head);
    body.set(data, head.length);
    body.set(tail, head.length + data.length);
    const r = await this.request(`${UPLOAD}/files?uploadType=multipart&fields=id,name,headRevisionId`, {
      method: 'POST',
      headers: { 'content-type': `multipart/related; boundary=${boundary}` },
      body,
    });
    const out = await r.json();
    return { kind: 'drive', fileId: out.id, revision: out.headRevisionId, name: out.name };
  }
}
