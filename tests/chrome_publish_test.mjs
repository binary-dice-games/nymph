// MIT License © 2026 Binary Dice Games
//
// spec/07_release_pipeline/PLAN.md step 8: packaging/chrome/publish.sh
// against a stub of the Chrome Web Store API and OAuth endpoints: the
// requests in order, polling an upload in progress, failure on a refused
// upload, success when the store already has the version, and no
// credential in the output.
//
//   node chrome_publish_test.mjs <publish.sh> <work dir>

import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { join } from 'node:path';

const [script, work] = process.argv.slice(2);
mkdirSync(work, { recursive: true });
const zip = join(work, 'nymph-extension-9.8.7.zip');
writeFileSync(zip, 'PK fake zip');

const secrets = { id: 'client-id-SECRET1', secret: 'client-secret-SECRET2', refresh: 'refresh-SECRET3', token: 'access-SECRET4' };
let scenario = 'ok';
let requests = [];
let statusChecks = 0;

const server = createServer((req, res) => {
  let body = '';
  req.on('data', (c) => (body += c));
  req.on('end', () => {
    requests.push(`${req.method} ${req.url}`);
    const send = (code, json) => {
      res.writeHead(code, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify(json));
    };
    if (req.url === '/token') {
      const ok = body.includes(secrets.refresh) && body.includes(secrets.secret);
      return ok ? send(200, { access_token: secrets.token }) : send(401, { error: 'invalid_grant' });
    }
    if (req.headers.authorization !== `Bearer ${secrets.token}`) return send(401, { error: 'unauthenticated' });
    if (req.url.endsWith(':upload')) {
      if (scenario === 'refused') return send(400, { error: { message: 'Invalid package: manifest missing' } });
      if (scenario === 'same-version') {
        return send(400, { error: { message: 'The version of the uploaded package must be greater than 9.8.7' } });
      }
      return send(200, { uploadState: 'IN_PROGRESS', crxVersion: '9.8.7' });
    }
    if (req.url.endsWith(':fetchStatus')) {
      statusChecks++;
      return send(200, { lastAsyncUploadState: statusChecks < 2 ? 'IN_PROGRESS' : 'SUCCEEDED' });
    }
    if (req.url.endsWith(':publish')) return send(200, { state: 'PENDING_REVIEW' });
    send(404, { error: 'not found' });
  });
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const base = `http://127.0.0.1:${server.address().port}`;

function run() {
  requests = [];
  statusChecks = 0;
  // Asynchronous: a synchronous child would block the stub server.
  return new Promise((resolve) => {
    {
      const child = spawn('sh', [script, zip], {
        env: {
          PATH: process.env.PATH,
          NO_PROXY: '127.0.0.1', no_proxy: '127.0.0.1',
          CHROME_EXTENSION_ID: 'ITEM', CHROME_PUBLISHER_ID: 'PUB',
          CHROME_CLIENT_ID: secrets.id, CHROME_CLIENT_SECRET: secrets.secret, CHROME_REFRESH_TOKEN: secrets.refresh,
          CHROME_API_BASE: base, CHROME_UPLOAD_BASE: `${base}/upload`, CHROME_TOKEN_URL: `${base}/token`,
          CHROME_POLL_SECONDS: '0',
        },
      });
      let output = '';
      child.stdout.on('data', (d) => (output += d));
      child.stderr.on('data', (d) => (output += d));
      child.on('close', (code) => resolve({ code, output }));
    }
  });
}

let failures = 0;
let last = { code: null, output: '' };
function check(description, ok) {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${description}`);
  if (!ok) {
    failures++;
    console.log(`     exit ${last.code}; requests ${JSON.stringify(requests)}; output:
${last.output.replace(/^/gm, '     ')}`);
  }
}
const noSecret = (text) => !Object.values(secrets).some((s) => text.includes(s));

let r = (last = await run());
check('publishes: exit 0', r.code === 0);
check('requests in order: token, upload, status until done, publish', JSON.stringify(requests) === JSON.stringify([
  'POST /token',
  'POST /upload/v2/publishers/PUB/items/ITEM:upload',
  'GET /v2/publishers/PUB/items/ITEM:fetchStatus',
  'GET /v2/publishers/PUB/items/ITEM:fetchStatus',
  'POST /v2/publishers/PUB/items/ITEM:publish',
]));
check('says it was submitted for review', r.output.includes('PENDING_REVIEW'));
check('no credential in the output', noSecret(r.output));

scenario = 'refused';
r = (last = await run());
check('a refused upload exits 1', r.code === 1);
check('with the store answer', r.output.includes('manifest missing'));
check('and never publishes', !requests.some((q) => q.endsWith(':publish')));
check('no credential in the output (refused)', noSecret(r.output));

scenario = 'same-version';
r = (last = await run());
check('the same version again is not an error', r.code === 0 && r.output.includes('already has this version'));

server.close();
if (failures) process.exit(1);
console.log('publish.sh: all checks passed');
