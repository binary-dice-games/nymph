// MIT License © 2026 Binary Dice Games
// Plays the host side of MCP Apps for probe_server.py in Chromium, inside
// an iframe sandboxed and CSP'd like the spec's default, so the probe's
// pages can be checked without a Claude client. Prints each probe's report.
//
//   node fake_host.mjs <probe_server.py args...> [--wasm-csp]
//
// --wasm-csp adds 'wasm-unsafe-eval' to script-src (a host that allows it);
// --claude-csp uses the policy users reported for Claude's MCP Apps
// (anthropics/claude-ai-mcp issue 40, Claude.app 1.34493.1, August 2026).

import { spawn, execSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { createInterface } from 'node:readline';

const argv = process.argv.slice(2);
const wasmCsp = argv.includes('--wasm-csp');
const claudeCsp = argv.includes('--claude-csp');
const serverArgs = argv.filter((a) => a !== '--wasm-csp' && a !== '--claude-csp');

const root = execSync('npm root -g').toString().trim();
const { chromium } = createRequire(join(root, 'noop.js'))('playwright');

// ── MCP client over stdio ──
const server = spawn('python3', [new URL('probe_server.py', import.meta.url).pathname, ...serverArgs],
  { stdio: ['pipe', 'pipe', 'inherit'] });
const waiting = new Map();
let nextId = 1;
createInterface({ input: server.stdout }).on('line', (line) => {
  const m = JSON.parse(line);
  const w = waiting.get(m.id);
  if (w) { waiting.delete(m.id); w(m); }
});
function request(method, params) {
  const id = nextId++;
  server.stdin.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
  return new Promise((resolve) => waiting.set(id, resolve));
}

await request('initialize', { protocolVersion: '2025-06-18', capabilities: {
  extensions: { 'io.modelcontextprotocol/ui': { mimeTypes: ['text/html;profile=mcp-app'] } } },
  clientInfo: { name: 'nymph-fake-host', version: '0' } });
const tools = (await request('tools/list', {})).result.tools.filter((t) => t._meta?.ui?.resourceUri);

const csp = ["default-src 'none'",
  "script-src 'self' 'unsafe-inline'" + (wasmCsp ? " 'wasm-unsafe-eval'" : ''),
  "style-src 'self' 'unsafe-inline'", "img-src 'self' data:", "media-src 'self' data:",
  "connect-src 'none'", "frame-src 'none'", "base-uri 'self'", "object-src 'none'"].join('; ');
const policy = claudeCsp ? ["default-src 'self'",
  "script-src 'self' 'unsafe-inline' 'unsafe-eval' blob: data: https://assets.claude.ai",
  "style-src 'self' 'unsafe-inline' https://assets.claude.ai", "img-src 'self' data: blob: https://assets.claude.ai",
  "connect-src 'self'", "font-src 'self' https://assets.claude.ai", "media-src 'self' blob: data: https://assets.claude.ai",
  "worker-src 'self' blob: https://assets.claude.ai", "frame-src 'self' blob: data:", "base-uri 'self'",
  "object-src 'none'", "form-action 'self'"].join('; ') : csp;
const policyName = claudeCsp ? 'claude-csp' : wasmCsp ? 'wasm-csp' : 'default-csp';

const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
for (const tool of tools) {
  const result = (await request('tools/call', { name: tool.name, arguments: {} })).result;
  const html = (await request('resources/read', { uri: tool._meta.ui.resourceUri })).result.contents[0].text;
  const doc = html.replace('<head>', `<head><meta http-equiv="Content-Security-Policy" content="${policy}">`);
  const page = await browser.newPage();
  const reports = [];
  // The page's messages to its parent arrive here; replies go back through the iframe.
  await page.exposeFunction('hostReceive', async (m) => {
    const reply = (r) => page.evaluate((x) => document.querySelector('iframe').contentWindow.postMessage(x, '*'),
      { jsonrpc: '2.0', id: m.id, ...r });
    if (m.method === 'ui/initialize') {
      await reply({ result: { protocolVersion: '2026-01-26', hostInfo: { name: 'nymph-fake-host', version: '0' },
        hostCapabilities: {}, hostContext: { theme: 'light', displayMode: 'inline' } } });
    } else if (m.method === 'ui/notifications/initialized') {
      await page.evaluate((x) => document.querySelector('iframe').contentWindow.postMessage(x, '*'),
        { jsonrpc: '2.0', method: 'ui/notifications/tool-result', params: result });
    } else if (m.method === 'tools/call') {
      const r = await request('tools/call', m.params);
      if (m.params.name === 'probe_report') reports.push(m.params.arguments);
      await reply({ result: r.result });
    } else if (m.id !== undefined) {
      await reply({ result: {} });
    }
  });
  await page.setContent('<iframe sandbox="allow-scripts" style="width:800px;height:600px"></iframe>' +
    '<script>addEventListener("message", (e) => hostReceive(e.data));</script>');
  await page.evaluate((d) => { document.querySelector('iframe').srcdoc = d; }, doc);
  const t0 = Date.now();
  while (!reports.length && Date.now() - t0 < 60000) await new Promise((r) => setTimeout(r, 100));
  if (process.env.PROBE_SHOTS) {
    await page.screenshot({ path: `${process.env.PROBE_SHOTS}/${tool.name}-${policyName}.png` });
  }
  console.log(JSON.stringify({ tool: tool.name, policy: policyName, report: reports[0] ?? 'no report in 60 s' }));
  await page.close();
}
await browser.close();
server.stdin.end();
