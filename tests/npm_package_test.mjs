// MIT License © 2026 Binary Dice Games
//
// spec/07_release_pipeline/DESIGN.md section 4.11: .github/scripts/npm-package.sh
// writes the npm package and the MCP Registry server.json of a release, and
// the package's launcher (packaging/npm/bin/nymph.js) downloads the pinned
// executable (following a redirect, as GitHub's downloads do), checks its
// SHA-256, caches and runs it; it never runs or keeps a file whose SHA-256
// differs, and writes nothing on standard output but the program's.
//
//   node npm_package_test.mjs <npm-package.sh> <work dir>

import { spawn, spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { appendFileSync, chmodSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { join } from 'node:path';

const [script, work] = process.argv.slice(2);
rmSync(work, { recursive: true, force: true });
mkdirSync(join(work, 'release'), { recursive: true });

let failures = 0;
function check(description, condition) {
  console.log(`${condition ? 'ok  ' : 'FAIL'} ${description}`);
  if (!condition) failures++;
}
const sha = (file) => createHash('sha256').update(readFileSync(file)).digest('hex');

// ── The package from a release's SHA256SUMS ─────────────────────────────────

const version = '9.8.7';
const fake = join(work, 'fake');
writeFileSync(fake, '#!/bin/sh\necho "fake nymph $*"\nexit 3\n');
chmodSync(fake, 0o755);
const platforms = ['linux-x86_64', 'linux-arm64', 'macos-x86_64', 'macos-arm64', 'windows-x86_64.exe'];
const sums = platforms.map((p) => `${sha(fake)}  nymph-${version}-${p}`);
sums.push(`${'1'.repeat(64)}  nymph-${version}-linux-x86_64.mcpb`, `${'2'.repeat(64)}  nymph-${version}-src.tar.gz`);
writeFileSync(join(work, 'SHA256SUMS'), sums.join('\n') + '\n');
for (const p of platforms) writeFileSync(join(work, 'release', `nymph-${version}-${p}`), readFileSync(fake));

const out = join(work, 'out');
const made = spawnSync('sh', [script, version, join(work, 'SHA256SUMS'), out], { encoding: 'utf8' });
check('npm-package.sh succeeds', made.status === 0);
const pkg_dir = join(out, 'package');
const pkg = JSON.parse(readFileSync(join(pkg_dir, 'package.json'), 'utf8'));
const server = JSON.parse(readFileSync(join(out, 'server.json'), 'utf8'));
check('package.json has the version', pkg.version === version);
check('package.json runs bin/nymph.js as nymph', pkg.bin.nymph === 'bin/nymph.js' && existsSync(join(pkg_dir, 'bin/nymph.js')));
check('the files it lists are there', pkg.files.every((f) => existsSync(join(pkg_dir, f))));
check('server.json names the package, its version and `mcp`',
  server.name === pkg.mcpName && server.version === version && server.packages[0].identifier === pkg.name &&
  server.packages[0].version === version && server.packages[0].transport.type === 'stdio' &&
  server.packages[0].packageArguments[0].value === 'mcp');
check('server.json description fits the registry (100 characters)', server.description.length <= 100);
const pins = readFileSync(join(pkg_dir, 'checksums.txt'), 'utf8').split('\n').filter((l) => l && !l.startsWith('#'));
check('checksums.txt pins the executables only', pins.length === platforms.length && pins.every((l) => !/mcpb|tar/.test(l)));
const bad = spawnSync('sh', [script, '1.0.0', join(work, 'SHA256SUMS'), join(work, 'bad')], { encoding: 'utf8' });
check('a version with no executable fails', bad.status !== 0 && /no nymph-1.0.0 executable/.test(bad.stderr));

// ── The launcher against a stand-in release server ──────────────────────────

let requests = 0;
const http = createServer((req, res) => {
  requests++;
  if (req.url.startsWith('/download/')) {
    res.writeHead(302, { location: req.url.replace('/download/', '/objects/') });
    return res.end();
  }
  const file = join(work, 'release', req.url.replace('/objects/', ''));
  if (!existsSync(file)) {
    res.writeHead(404);
    return res.end();
  }
  res.writeHead(200);
  res.end(readFileSync(file));
});
await new Promise((resolve) => http.listen(0, '127.0.0.1', resolve));
const base = `http://127.0.0.1:${http.address().port}/download`;
const home = join(work, 'home');

/// Runs the launcher with a clean environment (asynchronously: the server
/// answers on this process's event loop).
function launch(env = {}, input = '') {
  return new Promise((resolve) => {
    const child = spawn(process.execPath, [join(pkg_dir, 'bin/nymph.js'), 'mcp', '--check'], {
      env: { PATH: process.env.PATH, HOME: home, USERPROFILE: home, NYMPH_RELEASE_URL: base, ...env },
    });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (d) => (stdout += d));
    child.stderr.on('data', (d) => (stderr += d));
    child.on('close', (status) => resolve({ status, stdout, stderr }));
    child.stdin.end(input);
  });
}

const info = (await launch({ NYMPH_LAUNCHER_INFO: '1' })).stdout.trim().split('\n');
const [name, target] = info;
check('info names this platform and the cache shared with the plugin',
  /^nymph-9\.8\.7-/.test(name) && target === join(home, '.cache', 'nymph', version, name));

const own = join(work, 'own');
writeFileSync(own, '#!/bin/sh\necho "own nymph $*"\n');
chmodSync(own, 0o755);
let r = await launch({ NYMPH_BIN: own });
check('NYMPH_BIN is run and nothing downloaded', r.stdout === 'own nymph mcp --check\n' && requests === 0 && !existsSync(target));

const echo = join(work, 'echo');
writeFileSync(echo, '#!/bin/sh\ncat\n');
chmodSync(echo, 0o755);
r = await launch({ NYMPH_BIN: echo }, '{"jsonrpc":"2.0"}\n');
check('standard input reaches the program (the MCP channel)', r.stdout === '{"jsonrpc":"2.0"}\n');

r = await launch();
check('downloads through a redirect and runs the release', r.stdout === 'fake nymph mcp --check\n' && requests === 2);
check('passes on the exit code', r.status === 3);
check('caches it, executable', existsSync(target) && (readFileSync(target).length > 0) && sha(target) === sha(fake));

requests = 0;
r = await launch();
check('runs from the cache without a request', r.stdout === 'fake nymph mcp --check\n' && requests === 0);

appendFileSync(target, 'echo tampered\n');
rmSync(join(work, 'release', name));
r = await launch();
check('a changed cached file is not run', r.status === 1 && !r.stdout.includes('tampered'));

writeFileSync(join(work, 'release', name), readFileSync(fake));
r = await launch();
check('and is replaced by the release', r.stdout === 'fake nymph mcp --check\n' && sha(target) === sha(fake));

rmSync(join(home, '.cache'), { recursive: true });
appendFileSync(join(work, 'release', name), 'echo evil\n');
r = await launch();
const errors = r.stderr.trim().split('\n').filter((l) => !l.includes('downloading'));
check('a tampered release exits 1, says why, prints nothing on stdout',
  r.status === 1 && r.stdout === '' && errors.length === 1 && errors[0].includes('not run'));
check('and keeps no file', readdirSync(join(home, '.cache', 'nymph', version)).length === 0);

writeFileSync(join(pkg_dir, 'checksums.txt'), '# none\n');
r = await launch();
check('no pinned release: exit 1, asks for NYMPH_BIN', r.status === 1 && r.stderr.includes('NYMPH_BIN'));

http.close();
if (failures) process.exit(1);
console.log('npm package: all checks passed');
