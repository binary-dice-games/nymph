// MIT License © 2026 Binary Dice Games
// spec/15_web_app plan step 1: the modules stay within their gzip budgets
// (web/size_budget.json).
//
//   node size.mjs <build>/web <nymph source dir>

import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { gzipSync } from 'node:zlib';

const [webDir, sourceDir] = process.argv.slice(2);
const budget = JSON.parse(await readFile(join(sourceDir, 'web', 'size_budget.json'), 'utf8'));
let failed = 0;
for (const [file, limit] of Object.entries(budget)) {
  if (file.startsWith('_')) {
    continue;
  }
  const size = gzipSync(await readFile(join(webDir, file)), { level: 9 }).length;
  const ok = size <= limit;
  failed += ok ? 0 : 1;
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${file}: ${size} bytes gzip (budget ${limit})`);
}
process.exit(failed ? 1 : 0);
