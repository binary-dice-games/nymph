// MIT License © 2026 Binary Dice Games
// Writes the editor tool definitions as the web app lists them to agents
// before the editor module is loaded (spec/17_agent_connectors/DESIGN.md
// section 4.2). Run after a web build, and commit the result:
//
//   node web/tools/dump-editor-tools.mjs <build>/web > web/app/editor-tools.json
//
// nymph_web_app_node fails when the committed file is stale.

import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [webDir] = process.argv.slice(2);
if (!webDir) {
  console.error('usage: node dump-editor-tools.mjs <build>/web');
  process.exit(2);
}
const { createEditor } = await import(pathToFileURL(join(webDir, 'editor', 'nymph-editor.js')).href);
const fetchFont = async (name) => new Uint8Array(await readFile(join(webDir, 'fonts', name)));
const editor = await createEditor(null, { headless: true, fetchFont, width: 800, height: 600 });
process.stdout.write(`${JSON.stringify(editor.toolDefinitions(), null, 1)}\n`);
editor.destroy();
