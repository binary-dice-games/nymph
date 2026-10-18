#!/usr/bin/env python3
# MIT License © 2026 Binary Dice Games
"""Throwaway MCP server for spec/06_claude_integration/PLAN.md step 1.

Answers, in a real Claude client, what the sandbox of an MCP App allows
(WebAssembly, WebGL2, eval, blob workers, the exact CSP) and whether the
nymph web viewer draws there. Python standard library only.

    python3 probe_server.py --log probe.log \
        [--png examples/dashboard.png] [--yaml examples/dashboard.yaml] \
        [--viewer build-web/web/nymph-viewer.single.js] \
        [--viewer-js build-web-js/web/nymph-viewer.single.js]

Every message the client sends and every report a probe page makes is
appended to the log as one JSON line. Delete this folder once step 1 is
answered.
"""

import argparse
import base64
import datetime
import json
import sys

PROTOCOL_VERSIONS = ["2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"]
UI_MIME = "text/html;profile=mcp-app"

args = None
log_file = None


def log(kind, payload):
    entry = {"t": datetime.datetime.now().isoformat(), "kind": kind, "data": payload}
    log_file.write(json.dumps(entry) + "\n")
    log_file.flush()


# ── Page parts ──────────────────────────────────────────────────────────────

BRIDGE_JS = r"""
const pending = new Map();
let nextId = 1;
const listeners = {};
function rpc(method, params) {
  const id = nextId++;
  window.parent.postMessage({jsonrpc: '2.0', id, method, params}, '*');
  return new Promise((resolve, reject) => {
    pending.set(id, {resolve, reject});
    setTimeout(() => { if (pending.delete(id)) reject(new Error('timeout: ' + method)); }, 10000);
  });
}
function notify(method, params) {
  window.parent.postMessage({jsonrpc: '2.0', method, params}, '*');
}
window.addEventListener('message', (e) => {
  const m = e.data;
  if (!m || m.jsonrpc !== '2.0') return;
  if (m.id !== undefined && pending.has(m.id) && !m.method) {
    const p = pending.get(m.id); pending.delete(m.id);
    m.error ? p.reject(new Error(JSON.stringify(m.error))) : p.resolve(m.result);
  } else if (m.method) {
    (listeners[m.method] || []).forEach((f) => f(m.params));
    if (m.id !== undefined) window.parent.postMessage({jsonrpc: '2.0', id: m.id, result: {}}, '*');
  }
});
function on(method, f) { (listeners[method] = listeners[method] || []).push(f); }
const violations = [];
document.addEventListener('securitypolicyviolation', (e) => {
  violations.push({directive: e.effectiveDirective, blocked: e.blockedURI, policy: e.originalPolicy});
});
const out = document.getElementById('out');
function show(o) { out.textContent = JSON.stringify(o, null, 2); }
async function report(probe, results) {
  results.violations = violations;
  show(results);
  try { await rpc('tools/call', {name: 'probe_report', arguments: {probe, results}}); }
  catch (err) { results.report_error = String(err); show(results); }
  try {
    await rpc('ui/update-model-context', {content: [{type: 'text',
      text: 'nymph probe "' + probe + '" results: ' + JSON.stringify(results)}]});
  } catch (err) { results.context_error = String(err); show(results); }
}
let host = null;
const toolResult = new Promise((resolve) => on('ui/notifications/tool-result', resolve));
async function start() {
  try {
    host = await rpc('ui/initialize', {protocolVersion: '2026-01-26',
      appInfo: {name: 'nymph-probe', version: '0'},
      appCapabilities: {availableDisplayModes: ['inline', 'fullscreen']}});
  } catch (err) { host = {error: String(err)}; }
  notify('ui/notifications/initialized', {});
  return host;
}
"""

SANDBOX_PAGE = r"""<!doctype html><html><head><meta charset="utf-8">
<style>body{font:12px monospace;margin:8px;background:#fff;color:#000}
pre{white-space:pre-wrap;word-break:break-all}</style></head><body>
<b>nymph sandbox probe</b><pre id="out">running...</pre>
<script>__BRIDGE__
// 8-byte header plus an exported add(i32, i32) function.
const ADD = new Uint8Array([0,97,115,109,1,0,0,0,1,7,1,96,2,127,127,1,127,3,2,1,0,
  7,7,1,3,97,100,100,0,0,10,9,1,7,0,32,0,32,1,106,11]);
async function attempt(f) {
  try { return {ok: true, value: await f()}; }
  catch (err) { return {ok: false, error: String(err)}; }
}
(async () => {
  const r = {};
  r.host = await start();
  r.location = String(location.href).slice(0, 200);
  r.origin = String(window.origin);
  r.user_agent = navigator.userAgent;
  r.wasm_compile = await attempt(async () => !!(await WebAssembly.compile(ADD)));
  r.wasm_add = await attempt(async () =>
    (await WebAssembly.instantiate(ADD)).instance.exports.add(2, 3));
  r.eval = await attempt(async () => new Function('return 1')());
  r.webgl2 = await attempt(async () => {
    const gl = document.createElement('canvas').getContext('webgl2');
    if (!gl) throw new Error('no webgl2 context');
    const dbg = gl.getExtension('WEBGL_debug_renderer_info');
    return dbg ? gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.VERSION);
  });
  r.blob_worker_wasm = await attempt(() => new Promise((resolve, reject) => {
    const src = 'onmessage=async(e)=>{try{const i=await WebAssembly.instantiate(e.data);' +
      'postMessage("ok "+i.instance.exports.add(2,3))}catch(x){postMessage("error "+x)}}';
    const w = new Worker(URL.createObjectURL(new Blob([src], {type: 'text/javascript'})));
    w.onmessage = (e) => resolve(e.data);
    w.onerror = (e) => reject(new Error(e.message || 'worker error'));
    w.postMessage(ADD);
    setTimeout(() => reject(new Error('worker timeout')), 5000);
  }));
  r.download_anchor = await attempt(async () => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob(['probe'], {type: 'text/plain'}));
    a.download = 'nymph-probe.txt';
    document.body.appendChild(a); a.click(); a.remove();
    return 'clicked (check whether a file was downloaded)';
  });
  r.tool_result = await Promise.race([toolResult.then((p) => Object.keys(p || {})),
    new Promise((res) => setTimeout(() => res('no tool-result within 5 s'), 5000))]);
  await report('sandbox', r);
})();
</script></body></html>"""

VIEWER_PAGE = r"""<!doctype html><html><head><meta charset="utf-8">
<style>body{font:12px monospace;margin:0;background:#fff;color:#000}
#box{width:100%;height:420px}pre{white-space:pre-wrap;word-break:break-all;margin:8px}</style>
</head><body><div id="box"></div><pre id="out">loading viewer...</pre>
<script type="module">__VIEWER__
;__BRIDGE__
(async () => {
  const r = {variant: '__VARIANT__', bytes: __BYTES__};
  r.host = await start();
  const t0 = performance.now();
  try {
    const params = await toolResult;
    const source = params.structuredContent.source;
    const viewer = await createViewer(document.getElementById('box'), {source, fit: 'width',
      // An inline page has no base URL for the default fonts/ folder.
      fontBaseUrl: 'https://fonts.invalid/'});
    await viewer.rendered();
    r.first_frame_ms = Math.round(performance.now() - t0);
    r.info = viewer.info();
    // Redraw cost: 20 resizes, each a full layout and frame.
    const t1 = performance.now();
    for (let i = 0; i < 20; i++) { viewer.resize(780 - (i % 2) * 40, 420, 1); await viewer.rendered(); }
    r.redraw_ms = Math.round((performance.now() - t1) / 20);
    notify('ui/notifications/size-changed', {height: document.body.scrollHeight});
  } catch (err) { r.error = String(err && err.stack || err); }
  await report('viewer-' + r.variant, r);
})();
</script></body></html>"""


def page(kind):
    if kind == "sandbox":
        return SANDBOX_PAGE.replace("__BRIDGE__", BRIDGE_JS)
    path = args.viewer if kind == "wasm" else args.viewer_js
    with open(path, encoding="utf-8") as f:
        viewer = f.read()
    return (VIEWER_PAGE.replace("__BRIDGE__", BRIDGE_JS)
            .replace("__VARIANT__", kind)
            .replace("__BYTES__", str(len(viewer)))
            .replace("__VIEWER__", viewer))


# ── MCP ─────────────────────────────────────────────────────────────────────

def ui_tool(name, title, uri):
    return {"name": name, "title": title,
            "description": title + " (nymph integration probe; no arguments).",
            "inputSchema": {"type": "object", "properties": {}},
            "_meta": {"ui": {"resourceUri": uri}}}


def tools():
    result = [ui_tool("probe_sandbox", "Probe the MCP App sandbox", "ui://nymph-probe/sandbox")]
    if args.viewer:
        result.append(ui_tool("probe_viewer", "Show a nymph chart with the WebAssembly viewer",
                              "ui://nymph-probe/viewer-wasm"))
    if args.viewer_js:
        result.append(ui_tool("probe_viewer_js", "Show a nymph chart with the JavaScript-only viewer",
                              "ui://nymph-probe/viewer-js"))
    result.append({"name": "probe_report", "description": "Records a probe page's results.",
                   "inputSchema": {"type": "object",
                                   "properties": {"probe": {"type": "string"},
                                                  "results": {"type": "object"}}},
                   "_meta": {"ui": {"visibility": ["app"]}}})
    return result


RESOURCES = {"ui://nymph-probe/sandbox": "sandbox",
             "ui://nymph-probe/viewer-wasm": "wasm",
             "ui://nymph-probe/viewer-js": "js"}


def call_tool(name, arguments):
    if name == "probe_report":
        log("report", arguments)
        return {"content": [{"type": "text", "text": "recorded"}]}
    if name not in ("probe_sandbox", "probe_viewer", "probe_viewer_js"):
        return {"isError": True, "content": [{"type": "text", "text": "unknown tool " + name}]}
    with open(args.png, "rb") as f:
        png = base64.b64encode(f.read()).decode()
    with open(args.yaml, encoding="utf-8") as f:
        source = f.read()
    return {"content": [{"type": "image", "data": png, "mimeType": "image/png"},
                        {"type": "text", "text": "Probe " + name + ": the image above is "
                         "examples/dashboard.png. Tell the user what the widget shows."}],
            "structuredContent": {"source": source}}


def handle(msg):
    method, params = msg.get("method"), msg.get("params") or {}
    if method == "initialize":
        asked = params.get("protocolVersion")
        version = asked if asked in PROTOCOL_VERSIONS else PROTOCOL_VERSIONS[0]
        return {"protocolVersion": version,
                "capabilities": {"tools": {}, "resources": {}},
                "serverInfo": {"name": "nymph-probe", "version": "0"},
                "instructions": "Integration probe for nymph. Run each probe_ tool once "
                                "when asked, and report what the widgets show."}
    if method == "ping":
        return {}
    if method == "tools/list":
        return {"tools": tools()}
    if method == "tools/call":
        return call_tool(params.get("name"), params.get("arguments") or {})
    if method == "resources/list":
        return {"resources": [{"uri": u, "name": k, "mimeType": UI_MIME}
                              for u, k in RESOURCES.items()
                              if k == "sandbox" or (k == "wasm" and args.viewer)
                              or (k == "js" and args.viewer_js)]}
    if method == "resources/read":
        uri = params.get("uri")
        if uri not in RESOURCES:
            raise KeyError("unknown resource " + str(uri))
        text = page(RESOURCES[uri])
        log("resource_read", {"uri": uri, "bytes": len(text)})
        return {"contents": [{"uri": uri, "mimeType": UI_MIME, "text": text,
                              "_meta": {"ui": {"prefersBorder": True}}}]}
    raise LookupError(method)


def main():
    global args, log_file
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", required=True)
    parser.add_argument("--png", default="examples/dashboard.png")
    parser.add_argument("--yaml", default="examples/dashboard.yaml")
    parser.add_argument("--viewer")
    parser.add_argument("--viewer-js")
    args = parser.parse_args()
    log_file = open(args.log, "a", encoding="utf-8")
    for line in sys.stdin:
        if not line.strip():
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError as err:
            log("bad_json", str(err))
            continue
        log("in", msg if msg.get("method") != "tools/call" else
            {"method": "tools/call", "params": msg.get("params")})
        if "id" not in msg or "method" not in msg:
            continue
        try:
            reply = {"jsonrpc": "2.0", "id": msg["id"], "result": handle(msg)}
        except LookupError as err:
            reply = {"jsonrpc": "2.0", "id": msg["id"],
                     "error": {"code": -32601 if not isinstance(err, KeyError) else -32602,
                               "message": str(err)}}
        sys.stdout.write(json.dumps(reply) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
