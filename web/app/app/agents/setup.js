// MIT License © 2026 Binary Dice Games
/**
 * @file setup.js The setup guide of the Connect an agent dialog
 * (spec/17_agent_connectors/DESIGN.md section 5.1), free of DOM code: which
 * system the visitor has, and for each agent the one thing to do so that it
 * runs `nymph mcp`, as text to copy. The guide reaches the user's agent
 * through the npm launcher (`npx -y @binary-dice-games/nymph mcp`,
 * docs/agents.md) or the agent's own nymph package.
 */

/** The npm package whose launcher runs `nymph mcp` (spec/07_release_pipeline section 4.11). */
export const PACKAGE = '@binary-dice-games/nymph';

/** What the user asks the agent, once it runs nymph. */
export const ASK = 'Connect to the nymph web app';

/** Where every agent's setup is described in full. */
export const DOCS_URL = 'https://github.com/binary-dice-games/nymph/blob/main/docs/agents.md';

/** The latest release, with the Claude Desktop bundles. */
export const RELEASES_URL = 'https://github.com/binary-dice-games/nymph/releases/latest';

/** Node.js, which the npm launcher needs. */
export const NODE_URL = 'https://nodejs.org/';

/** The agents the guide covers, in the order it lists them. */
export const AGENTS = Object.freeze([
  { id: 'claude-code', name: 'Claude Code' },
  { id: 'claude-desktop', name: 'Claude Desktop' },
  { id: 'cursor', name: 'Cursor' },
  { id: 'vscode', name: 'VS Code' },
  { id: 'gemini-cli', name: 'Gemini CLI' },
  { id: 'codex', name: 'Codex CLI' },
  { id: 'other', name: 'Other' },
]);

/** The systems the guide has instructions for. */
export const SYSTEMS = Object.freeze([
  { id: 'windows', name: 'Windows' },
  { id: 'mac', name: 'macOS' },
  { id: 'linux', name: 'Linux' },
]);

/**
 * The visitor's system from the browser's user agent and platform; phones
 * and tablets are `other` (they run no agent with local tools).
 */
export function detectSystem(userAgent, platform = '') {
  const ua = `${userAgent ?? ''} ${platform ?? ''}`;
  if (/Android|iPhone|iPad|iPod|Mobile/i.test(ua)) {
    return 'other';
  }
  if (/Windows|Win32|Win64/i.test(ua)) {
    return 'windows';
  }
  if (/Macintosh|Mac OS X|MacIntel/i.test(ua)) {
    return 'mac';
  }
  if (/Linux|X11|CrOS/i.test(ua)) {
    return 'linux';
  }
  return 'other';
}

/**
 * The command and arguments that start `nymph mcp` through npx. On Windows
 * through `cmd /c`: clients start servers without a shell, and npx is a
 * `.cmd` file there.
 */
export function serverCommand(system) {
  const args = ['-y', PACKAGE, 'mcp'];
  return system === 'windows' ? { command: 'cmd', args: ['/c', 'npx', ...args] } : { command: 'npx', args };
}

function commandLine(system) {
  const { command, args } = serverCommand(system);
  return [command, ...args].join(' ');
}

/** A file in the user's home folder, written the system's way. */
function homeFile(system, ...parts) {
  return system === 'windows' ? `%USERPROFILE%\\${parts.join('\\')}` : `~/${parts.join('/')}`;
}

/** JSON indented by two, with arrays of strings on one line (as configurations are shown). */
function json(value) {
  return JSON.stringify(value, null, 2).replace(/\[\s*("(?:[^"\\]|\\.)*"(?:,\s*"(?:[^"\\]|\\.)*")*)\s*\]/g, (_, items) =>
    `[${items.split(/,\s*/).join(', ')}]`,
  );
}

/** Cursor's one-click install link for an MCP server (docs/cursor.md). */
export function cursorLink(system) {
  const config = btoa(JSON.stringify(serverCommand(system)));
  return `cursor://anysphere.cursor-deeplink/mcp/install?name=nymph&config=${encodeURIComponent(config)}`;
}

/**
 * What to do so that `agent` runs nymph on `system`:
 * - `intro`: one sentence saying where the text goes;
 * - `text`: what to copy (a command or a configuration), or null;
 * - `link`: {href, label} a button opens, or null;
 * - `after`: the step after (a restart, a check), or '';
 * - `node`: whether the step needs Node.js (the npm launcher).
 *
 * @param {string} agent an id of AGENTS
 * @param {string} system an id of SYSTEMS ('other' is taken as Linux)
 */
export function setupFor(agent, system) {
  const sys = SYSTEMS.some((s) => s.id === system) ? system : 'linux';
  const mcpServers = { mcpServers: { nymph: serverCommand(sys) } };
  switch (agent) {
    case 'claude-code':
      return {
        intro: 'Type this in Claude Code. It installs the nymph plugin; nothing else is needed.',
        text: '/plugin install nymph --marketplace binary-dice-games/nymph',
        link: null,
        after: 'Then start a new Claude Code session.',
        node: false,
      };
    case 'claude-desktop':
      return {
        intro: `Download the nymph extension for ${sys === 'linux' ? 'your system' : SYSTEMS.find((s) => s.id === sys).name} from the latest release (the .mcpb file${
          sys === 'mac' ? ' ending in macos-universal.mcpb' : sys === 'windows' ? ' ending in windows-x86_64.mcpb' : ''
        }) and open it. Claude Desktop asks you to confirm.`,
        text: null,
        link: { href: RELEASES_URL, label: 'Open the latest release' },
        after: 'Then use the Code tab, where nymph\'s tools run.',
        node: false,
      };
    case 'cursor':
      return {
        intro: `Press Add to Cursor, or add this to ${homeFile(sys, '.cursor', 'mcp.json')}:`,
        text: json(mcpServers),
        link: { href: cursorLink(sys), label: 'Add to Cursor' },
        after: 'Cursor lists nymph under Settings, MCP.',
        node: true,
      };
    case 'vscode':
      return {
        intro: 'Add this to .vscode/mcp.json in your project (or run "MCP: Open User Configuration" for every project):',
        text: json({ servers: { nymph: { type: 'stdio', ...serverCommand(sys) } } }),
        link: null,
        after: 'Then use Copilot Chat in Agent mode.',
        node: true,
      };
    case 'gemini-cli':
      return {
        intro: `Add this to ${homeFile(sys, '.gemini', 'settings.json')} (merge it with what is there):`,
        text: json(mcpServers),
        link: null,
        after: 'Then start Gemini CLI again; /mcp lists nymph.',
        node: true,
      };
    case 'codex':
      return {
        intro: 'Run this in a terminal:',
        text: `codex mcp add nymph -- ${commandLine(sys)}`,
        link: null,
        after: 'Then start Codex again; /mcp lists nymph.',
        node: true,
      };
    default:
      return {
        intro: 'Add nymph to your agent\'s MCP servers. Most agents take this form:',
        text: json(mcpServers),
        link: null,
        after: 'Then start the agent again.',
        node: true,
      };
  }
}
