# Live collaboration

Share the chart open in the [web app](web-app.md) with other people, live:
you edit it, they see every change as you make it (unsaved changes
included), and everyone draws on a whiteboard over it: cursors, freehand
ink, text notes and stickers. There is no nymph server: the browsers
connect to each other directly (WebRTC). Design:
[spec/18_collaboration/DESIGN.md](../spec/18_collaboration/DESIGN.md).

## Sharing a chart

1. Open the chart, then **Collaborate** in the top bar (or `Ctrl+Shift+L`,
   or **Share > Share live…**). The first time, the app says what is
   shared and asks for the name others will see.
2. **Copy link** in the **Live session** panel and send it to the people
   you mean. Anyone with the link can watch and draw, until you **Lock the
   session** or make a **New link** (people already in stay).
3. Edit as usual, in View or Edit mode. Your AI agents keep working; their
   changes reach everyone too. While you share, the editor shows the chart
   at its own size, as View and everyone else do, so drawings land on the
   same spot for all; drag the divider if it does not fit.

Only you change the chart. The panel lists who is here and how each is
connected (*direct*, or *relayed* through a TURN server), and has
**Remove**, **Only I can draw**, **Clear all** and **End the session**.
Closing the chart, or opening another, asks first and ends the session.
Reloading your tab and reopening the same chart resumes it: the people in
it reconnect, and the drawings come back.

## Joining

Open the link. The app asks your name, connects, and shows the owner's
chart in View: pan, zoom and hover as in any chart. **Follow the owner**
(on at the start) shows what the owner sees; your own pan or zoom turns it
off, and the People panel turns it back on. You cannot edit the chart,
only download it (YAML, PNG, or PNG with drawings). When the owner ends
the session, the last chart and the drawings stay on your screen.

Phones and tablets join as full guests: they view, and draw with a finger.

## Drawing

The tools are at the top-left of the chart:

| Tool | What |
|------|------|
| Pointer | Use the chart under the drawings (pan, zoom, hover, edit). `Esc` from any tool. |
| Pen | Freehand ink in the chosen colour. Others see it as you draw. |
| Text | Click to write a note (`Enter` ends it, `Shift+Enter` a new line); click a note to change it, drag it to move it. |
| Sticker | Check, cross, question, star, heart, thumbs up, warning, arrow; drag one to move it. |
| Eraser | Click or drag over a drawing to remove it. |

`Ctrl+Z` / `Ctrl+Y` undo and redo your own drawings while a drawing tool is
active (in pointer mode they are the editor's). Drawings sit on the
figure: they keep their place on it whatever each person's window size.
They belong to the session only: they are never written into the chart's
YAML. **Download PNG with drawings** (Share or Save menu, or the panel)
gives a picture with them, which still opens in nymph as the chart without
them.

## Privacy

- Nothing connects anywhere until you start or join a session.
- The chart and the drawings go only to the people in the session, over
  encrypted WebRTC connections.
- To find each other, the browsers use public WebTorrent trackers. What
  they pass on is encrypted with a key from the invite link (which never
  leaves the browsers: it is in the link's `#` part), so they see that some
  browsers met, not the chart. STUN servers tell each browser its public
  address.
- People in a session learn each other's IP address, as in any direct
  connection.

## When people cannot connect

Most home and office networks connect directly. If someone sees "could not
connect directly" (strict firewalls, some phone networks):

- **Join codes**: under *Join with codes* (guest) and *Someone cannot
  connect?* (owner) in the panel, the guest makes a join code, sends it to
  the owner by any chat, and pastes the answer code the owner sends back.
  This also works when the trackers are blocked.
- **Another network**: a phone hotspot often works where an office network
  does not.
- **A TURN server** relays the connection where nothing else works:
  **Connection settings** in the panel takes its address
  (`turns:turn.example.com:443`), user name and password, for the next
  session you start or join. Run [coturn](https://github.com/coturn/coturn)
  on any server, or use a hosted TURN service. A TURN server only sees
  encrypted traffic.

A site owner can set the trackers and the STUN / TURN servers for everyone
(below).

## Hosting and setup

The site's `config.json` has a `collab` section:

```json
"collab": {
  "trackers": ["wss://tracker.openwebtorrent.com", "wss://tracker.webtorrent.dev"],
  "iceServers": [{ "urls": ["stun:stun.l.google.com:19302"] }]
}
```

set at build time by the CMake cache variables `NYMPH_COLLAB_TRACKERS` (a
list; empty leaves the join codes only) and `NYMPH_COLLAB_ICE_SERVERS` (a
JSON `RTCIceServer` list; add a TURN server here for everyone, knowing its
credentials are then public).

## Embedding the packages

The session and the whiteboard are two standalone ES modules, with no
nymph code and no dependencies, at `collab/collab-session.js` and
`collab/collab-whiteboard.js` of the site (`web/collab/` in the
repository). [`web/collab/demo/index.html`](../web/collab/demo/index.html)
puts a whiteboard over a plain picture:

```js
import { createSession } from './collab-session.js';
import { createWhiteboard } from './collab-whiteboard.js';

const session = await createSession({
  role: 'host',                     // or 'guest' with the token of an invite
  me: { name: 'Ana', color: '#e5484d' },
  protocol: 'my-app/1',
  rendezvous: [{ kind: 'torrent', urls: ['wss://tracker.openwebtorrent.com'] }, { kind: 'manual' }],
  iceServers: [{ urls: 'stun:stun.l.google.com:19302' }],
});
location.hash = session.inviteFragment();          // collab=<token>
session.channel('chat').on('message', (m, from) => console.log(from, m));

// Content units: here the picture's own 800 x 450 pixels.
const adapter = {
  element: picture,
  transform: () => {
    const s = img.getBoundingClientRect().width / 800;
    return { a: s, b: 0, c: 0, d: s, e: 0, f: 0 };
  },
  onChange: (cb) => {
    const ro = new ResizeObserver(cb);
    ro.observe(img);
    return () => ro.disconnect();
  },
};
const wb = createWhiteboard(picture, { session, adapter });
```

**Session.** One host and its guests in a star: guests connect to the host,
which relays what they send. `channel(name)` gives `send(message, {to,
fast})` (JSON values; `fast` for lossy, unordered messages such as cursors)
and `on('message' | 'join')`. `peers()`, `on('peers' | 'state' | 'status'
| 'error')`, `setMe()`; the host has `lock()`, `remove(id)`, `newInvite()`
and an `accept(channel, message, from)` hook that drops what a guest may not
send; `session.manual` has the join codes. Rendezvous kinds: `torrent`,
`manual`, `broadcast` (tabs of one browser), or an object of your own with
`start(context)` and `stop()`.

**Whiteboard.** `setTool()`, `setStyle()`, `readOnly`, `undo()`, `redo()`,
`clear()`, `items()`, `snapshot()` / `load()` (mergeable),
`toSvg({x, y, width, height})`, `on('change' | 'tool' | 'error')`,
`destroy()`; options `toolbar`, `tools`, `stickers` (`{name, svg}` of your
own), `strings` (labels). The adapter's `transform()` may return
`hidden: true` while the content is not shown, and an optional `clip()`
returns the rectangle of the element drawings stay inside. Restyle it from the page
with the custom properties `--wb-bg`, `--wb-fg`, `--wb-border`,
`--wb-accent` and `--wb-hover` on `collab-whiteboard`.
