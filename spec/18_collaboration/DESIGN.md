# nymph — Collaboration: Design

**Status: designed; nothing built.** [PLAN.md](PLAN.md) is the
implementation plan; its step 1 checks the facts this design takes on trust
(public trackers, NAT success rates, browser limits) before anything is
built. This document describes only what this project adds or changes; the
app it extends is [15_web_app](../15_web_app/DESIGN.md) (*app DESIGN*), its
command registry and agent rules are
[17_agent_connectors](../17_agent_connectors/DESIGN.md) (*agents DESIGN*),
and the viewer is [04_web_viewer](../04_web_viewer/DESIGN.md).

## 1. Purpose / Scope

Several people look at one chart together, live, in the web app. The
**owner** (the person who has the file open) is the only one who edits the
chart; the **guests** see the owner's chart as it is now, unsaved changes
included, in their own viewer. Over the chart, everyone shares a
**whiteboard**: each person's cursor, freehand ink, text notes and
stickers, as in Lucidchart's collaboration. The whiteboard helps the
conversation and is never written into the YAML.

There is no server of nymph's own. Browsers talk to each other directly
over **WebRTC data channels**. They find each other through public
**WebTorrent trackers** (or by copying two codes by hand), get through NATs
with **STUN**, and use a **TURN** relay only where one is configured.

The work is three pieces, two of them independent of nymph:

| Piece | What |
|---|---|
| `collab-session` (`web/collab/collab-session.js`) | Rooms, invite tokens, signaling, WebRTC peers, a host that relays for guests, presence (name, colour), named message channels. Knows nothing about charts or drawings. |
| `collab-whiteboard` (`web/collab/collab-whiteboard.js`) | The drawing overlay and its data model: cursors, pen, text, stickers, eraser, a pointer mode that lets input through to the page below. Placed over any element through a small adapter; synced over any session-like transport. |
| nymph glue (`web/app/app/collab/`) | The owner's chart sent to guests, guest documents, "Follow the owner", the nymph adapter, the People panel, commands, configuration. |

Both packages are meant to be embedded in other projects later: no nymph
imports, no framework, no build step, one ES module file each, served at
the stable paths `collab/collab-session.js` and
`collab/collab-whiteboard.js` of the site (as `nymph-viewer.js` is).

In scope:

- Live view of the owner's chart for up to about 20 guests per room,
  on computers and phones (guests only view, so phones are full guests).
- The whiteboard over the chart in View and Edit mode, for everyone.
- Signaling through public WebTorrent trackers, encrypted end to end;
  manual exchange (copy a code each way) as the fallback.
- STUN by default, TURN from `config.json` or the user's settings;
  the connection path (direct or relayed) shown per person.
- An owner tab reload resumes the room; a guest reload rejoins it.

Not in scope: more than one editor, or handing over editing (section 12);
a nymph-run signaling or TURN server; a full mesh between guests (guests
reach each other only through the host); a "refresh from Google Drive"
mode for networks where WebRTC fails (section 12); saving the whiteboard
into the chart, or anywhere outside the tab; voice, video and chat;
agents taking part in a session (section 12); the native `nymph view`
(no WebRTC there).

Files owned or changed:

| Path | What |
|---|---|
| `web/collab/collab-session.js` | new: the session package |
| `web/collab/collab-whiteboard.js` | new: the whiteboard package |
| `web/collab/demo/index.html` | new: both packages over a plain image, no nymph (the embedding example, and the independence test) |
| `web/app/app/collab/` | new: `collab.js` (commands, doc channel, lifecycle), `adapter.js` (nymph adapter), `panel.js` (People panel, whiteboard toolbar placement) |
| `web/app/app/main.js`, `document.js`, `commands.js`, `device.js` | guest documents, the `collab_*` commands, `#collab=` route, `config.json` `collab` section |
| `web/app/index.html`, `app.css` | `wss:` in the CSP's `connect-src`; Collaborate button, People panel |
| `web/js/nymph-viewer.js`, `web/js/nymph-editor.js` | `figureBox()`, `figure` event, `viewState()`, `setViewState()` |
| `web/nymph_web.cpp`, `web/editor/nymph_editor_web.cpp` | the C exports behind those calls |
| `web/CMakeLists.txt`, `web/size_budget.json` | `collab/` copied by `nymph-site`; budgets for both packages; `NYMPH_COLLAB_TRACKERS`, `NYMPH_COLLAB_ICE_SERVERS` |
| `web/tests/node/`, `web/tests/browser/` | model, session and app tests; a local test tracker |
| `docs/collaboration.md` | new: user guide, embedding the packages, setting up TURN |
| `docs/web-app.md`, `docs/viewer.md`, `README.md`, `CHANGELOG.md` | collaboration, the new viewer and editor calls |

## 2. Design Goals

1. **No server of nymph's own** (app DESIGN goal 5). Everything nymph
   ships is static files. The public helpers it relies on (trackers,
   STUN) only introduce peers or tell a browser its address; they never
   see a chart or a drawing in clear. TURN, the one helper that carries
   traffic, is optional and carries only encrypted traffic.
2. **One editor, no merging.** Only the owner changes the chart. Guests
   send nothing that can change it, so the chart needs no merging or
   conflict handling; the owner's text is the truth.
3. **The packages stand alone.** `collab-session` and `collab-whiteboard`
   import nothing from nymph and each other only through a documented
   interface; a test checks it. The whiteboard runs over any element
   through an adapter and over any transport that offers channels.
4. **Private by default, opt-in to share.** Nothing connects anywhere
   until the owner starts a session or a guest opens an invite. The
   invite is a secret in the URL fragment; signaling and data are
   encrypted with it or by WebRTC. Starting a session says what others
   will see (the chart, the drawings, your IP address).
5. **Viewers stay light.** The viewer and editor modules gain only small
   view and geometry calls (within today's size budgets); the packages are
   loaded only when a session starts or is joined.
6. **Clear failure.** Every way a connection can fail ends in a message
   that says what happened and what to do (another network, a TURN
   server, the manual codes), never a spinner without end.

## 3. Key Abstractions

### 3.1 Room and invite token

A room is named by a 128-bit random **token** made by the host, written in
the invite link's fragment: `https://…/nymph/#collab=<token base64url>`.
Every key comes from it with HKDF-SHA-256 (WebCrypto):

| Derived | Use |
|---|---|
| `topic` (20 bytes) | The tracker swarm (`info_hash`); also names the room in manual codes |
| `seal` (AES-GCM 256) | Encrypts every signaling message, so trackers see only opaque bytes |

Anyone with the token can join until the host locks the room or makes a
new invite (a new token; people already connected stay). The token is
never sent to any server, never logged, and is removed from the address
bar after joining (`history.replaceState`); the tab keeps it in
`sessionStorage` to rejoin on reload.

### 3.2 Peers and roles

Every tab in a room is a **peer** with a random 64-bit id (kept in
`sessionStorage` for the room, so a reload is the same peer), a name and a
colour (chosen once, kept in `localStorage`), and a role: one **host**, any
number of **guests**. Topology is a star: each guest has one WebRTC
connection, to the host. The host relays what a guest broadcasts to the
others and keeps the **roster** (who is here, with name, colour, role,
path). Guests never connect to each other, so only the guest-host path has
to cross NATs.

### 3.3 Link

A **link** is one connection to one other peer, with two data channels
negotiated up front (`negotiated: true`, fixed ids):

| Channel | Options | Carries |
|---|---|---|
| `reliable` | ordered, reliable | Chart sources, whiteboard operations, roster, control |
| `fast` | `ordered: false`, `maxRetransmits: 0` | Cursors, ink being drawn: newer replaces older, a lost one does not matter |

The session sees links through an interface (`send(frame)`, `on('frame')`,
`close()`, `stats()`), with two implementations: `webrtc` (the real one)
and `broadcast` (a `BroadcastChannel` between tabs of one browser, for the
demo and tests). Node tests use an in-memory pair.

### 3.4 Rendezvous

A **rendezvous** gets two peers' WebRTC offers and answers to each other.
Interface: `start(topic, seal, onSignal)`, `signal(toPeer, message)`,
`stop()`. Two back ends ship:

- **`torrent`**: the WebTorrent tracker protocol over `wss`, the protocol
  public trackers were built for (browser peers trading WebRTC offers
  inside a swarm). The session announces to every tracker in the list at
  once; the first answer wins. The SDP field carries the sealed message,
  not a readable SDP.
- **`manual`**: the guest gets an **offer code** to send to the host by any
  means (chat, email), the host pastes it and gets an **answer code** to
  send back. Codes are sealed, compressed and base64url (about 1 to 2 KB);
  slow, but it needs nothing on the network but STUN.

Other back ends (Nostr relays, MQTT brokers, a Drive file, a server of the
embedder's own) fit the same interface (section 12).

### 3.5 Session

The session joins it all: rendezvous, links, roster, relay, channels.

Invariants:

- A guest's message to everyone goes to the host, which delivers it
  locally and forwards it to the other guests, marked with the sender's
  id; the host's `accept(channel, message, from)` hook can drop it first.
  A guest cannot claim another sender: the host stamps `from` itself.
- The host never forwards a frame back to its sender, and drops `fast`
  frames for a guest whose channel buffer is over 256 KB (a slow guest
  never slows the others).
- A `reliable` message larger than 16 KiB is split into 16 KiB frames and
  reassembled in order; a message over 4 KiB is compressed (`deflate-raw`)
  first. A `fast` message must fit one frame (refused otherwise). Largest
  message: 16 MB, as the app's open limit.
- Every frame is validated (envelope shape, channel name, sizes) before
  any handler sees it; a malformed frame is dropped, and ten in a minute
  close that link.

### 3.6 Whiteboard model

The whiteboard state is a set of **items**:

| Kind | Fields |
|---|---|
| `ink` | points (content units, rounded to 0.1, simplified with Ramer-Douglas-Peucker), colour, width |
| `text` | position, text (at most 500 characters), colour, size |
| `sticker` | position, sticker name, size |

Each item has an id (`<peer id>.<counter>`), its author, and a stamp
`(clock, peer id)` from a Lamport clock. Every change (add, move, restyle,
edit text, delete) replaces the whole item, and an item keeps the change
with the larger stamp: a **last-writer-wins** register per item, a simple
conflict-free model. Deletes leave a tombstone so a late add cannot bring
an item back; **Clear** records a clear stamp that removes every item
stamped before it.

Invariants: applying the same operations in any order, any number of
times, gives the same state on every peer (the model test checks this
with random orders); a snapshot (items, tombstones, clear stamp, clock) is
itself mergeable, so two peers that were apart merge by swapping
snapshots. Limits: 2,000 live items, 5,000 points per ink, a 1 MB
snapshot; past them a new item is refused with a message.

The model does not need a host. The nymph session happens to have one,
which answers joiners' snapshot requests; the order the host relays in is
not needed for convergence.

### 3.7 Adapter

The whiteboard draws in **content units** that the host page defines (for
nymph: the figure's layout pixels). The adapter tells it how content maps
to the screen:

| Member | Meaning |
|---|---|
| `element` | The element the overlay covers (it is positioned over it, same box) |
| `transform()` | The affine map from content units to CSS pixels of `element`'s box: `{a, b, c, d, e, f}` as in `DOMMatrix` |
| `onChange(callback)` | Calls back when the transform changes (pan, zoom, resize, layout); returns an unsubscribe function |

`createWhiteboard` without an adapter uses the container's box as content
(one unit per CSS pixel at the size it had when the whiteboard was made).
Because items are stored in content units, people with different window
sizes, or zoomed differently, see them in the same place on the content.

## 4. Data Flow

**Starting.** The owner clicks **Collaborate**. The app shows what will be
shared, loads both packages, creates a host session (new token), a
whiteboard over the stage, and shows the invite link.

**Joining.**

```
guest tab                       trackers (wss)                 host tab
   │ open #collab=<token>            │                            │
   │ derive topic, seal              │                            │
   │── announce(topic, sealed offers)▶── forwards offers ────────▶│ unseal, check role
   │                                 │◀── announce(sealed answer)─│
   │◀── answer ──────────────────────│                            │
   │═══════ ICE (host / STUN / TURN candidates), DTLS ════════════│
   │── hello {peer id, name, colour, protocol} ──────────────────▶│ roster += guest
   │◀── roster, doc {v, source}, whiteboard snapshot ─────────────│
   │                                                              │
```

1. Every peer announces to each tracker with up to 5 sealed offers every
   5 seconds (doubling up to 60 s once connected and the room is quiet);
   the tracker forwards offers to other peers of the swarm. A host answers
   a guest's offer, a guest answers the host's; guest-guest pairs ignore
   each other (the role is inside the sealed part).
2. Offers are not trickled: a peer waits for ICE gathering to finish (at
   most 3 seconds) and seals the full SDP, so one tracker message carries
   one complete offer.
3. ICE tries host, server-reflexive (STUN) and relayed (TURN, if
   configured) candidates. DTLS secures the channels; the certificate
   fingerprint travels inside the sealed SDP, so a tracker cannot put
   itself in the middle.
4. Over `reliable`, the guest sends `hello`; the host answers with the
   roster, then each app channel's join data (for nymph: the chart and the
   whiteboard snapshot).

**Running.** The owner edits: on every document change (the app's own
`change` path, app DESIGN section 3.3), after 150 ms of quiet, the host
sends `doc {v, source}` to all guests; guests call
`viewer.setSource(source)`, which keeps their zoom on what did not change.
Pans and zooms of the owner go as `view {v, state}` to guests following
the owner. Whiteboard operations from anyone go to the host and out to
the others; cursors and ink in progress go over `fast`.

**Failing.** A link whose ICE state goes `disconnected` gets an ICE
restart over the same link (a network change); `failed` closes it, and
the guest goes back to announcing. A guest that cannot connect in 30
seconds sees why (section 7). A host tab that closes ends the room for
everyone 60 seconds later, unless the owner reloads and reopens the same
document in that tab, which resumes the room (the token, the document id
and the whiteboard snapshot are in `sessionStorage`).

## 5. Public API Contract

### 5.1 `collab-session`

```js
import { createSession } from './collab-session.js';

const session = await createSession({
  role: 'host' | 'guest',
  token,                 // guest: from the invite; host: omit for a new room
  protocol: 'nymph/1',   // peers with another protocol are refused, with a message
  me: { name, color },
  rendezvous: [{ kind: 'torrent', urls: [...] }] | [{ kind: 'manual' }]
            | [{ kind: 'broadcast' }],
  iceServers: [...],     // RTCIceServer list
  accept: (channel, message, from) => boolean,   // host only
});
```

| Member | Contract |
|---|---|
| `session.token`, `inviteFragment()` | The room token; `collab=<token>` for the host app to put in its link. |
| `session.me`, `session.role`, `session.peers()` | This peer; the roster (`{id, name, color, role, path: 'direct' \| 'relay' \| 'local' \| null}`). |
| `session.channel(name)` | A named channel: `send(message, {to, fast})` (JSON value; `to` a peer id, default everyone), `on('message', (message, from) => …)`, `on('join', (peer) => …)` (for join data). Names are 1-32 ASCII letters, digits and `-`. |
| `session.on(event, cb)` | `peers` (roster changed), `state` (`connecting`, `open`, `reconnecting`, `ended`, with a reason), `error`. Returns an unsubscribe function. |
| `session.setMe({name, color})` | Changes this peer's presence for everyone. |
| `session.lock(on)`, `session.remove(id)`, `session.newInvite()` | Host only: no new joins; close a guest's link and refuse its id; a new token (people already in stay). |
| `session.manual` | With the `manual` rendezvous: `offerCode()` (guest), `acceptOffer(code)` → answer code (host), `acceptAnswer(code)` (guest). |
| `session.close()` | Leaves (a host ends the room for everyone). |

Failures: `createSession` rejects on a malformed token or no WebRTC; a
`send` on a closed session throws; network failures come as `state` and
`error` events, never as throws from handlers.

### 5.2 `collab-whiteboard`

```js
import { createWhiteboard } from './collab-whiteboard.js';

const wb = createWhiteboard(container, {
  adapter,               // section 3.7; optional
  session,               // anything with me, peers(), on('peers'), channel(name)
  channel: 'wb',
  toolbar: true,         // false: the host builds its own controls
  tools: ['pointer', 'pen', 'text', 'sticker', 'eraser'],
  stickers: [{ name, svg }],   // extra stickers, from the host only
  strings: { … },        // labels, for translation
});
```

| Member | Contract |
|---|---|
| `wb.setTool(name)`, `wb.tool` | `pointer` lets every pointer event through to the page (the overlay has `pointer-events: none`); the others capture input over the content. |
| `wb.setStyle({color, width, size})` | The style of new items. |
| `wb.undo()`, `wb.redo()` | This peer's own changes only, as new operations. |
| `wb.clear()`, `wb.readOnly` | Clear for everyone; a read-only whiteboard shows everything and draws nothing. |
| `wb.snapshot()`, `wb.load(snapshot)` | A mergeable JSON snapshot (section 3.6); `load` merges, it does not replace. |
| `wb.toSvg()` | The items as a standalone SVG in content units. |
| `wb.on('change' \| 'tool', cb)`, `wb.destroy()` | |

Keys (`Ctrl+Z`, `Delete`, `Esc` back to pointer) are handled only while a
tool other than `pointer` is active, so the page's own shortcuts (the
editor's undo) keep working in pointer mode. Text from other peers is
rendered with `textContent` only; peers send sticker names, never SVG,
and an unknown name draws a placeholder.

### 5.3 Viewer and editor additions

| Call | Viewer | Editor | Contract |
|---|---|---|---|
| `figureBox()` | yes | yes | `{left, top, width, height, scale}`: where the figure's layout is drawn, in CSS pixels of the container, and CSS pixels per layout pixel. `null` with no figure. |
| `on('figure', cb)` | yes | yes | After the figure box changes (resize, a source with another size, the editor's panel or divider). |
| `viewState()` | yes | yes | The `view` key's content as a JSON object (what `toYaml()` would save). |
| `setViewState(state)` | yes | no | Shows that view, as a `setSource` with that `view` key would, without changing the source. |

## 6. nymph Glue

### 6.1 Owner

- **Collaborate** (in the document's toolbar; `collab_start`) needs an
  open document. The first time, a dialog says: people with the link see
  this chart and its unsaved changes live and can draw over it; their
  browsers and yours learn each other's IP address; public trackers help
  you find each other but see nothing of the chart. Then the People panel
  opens with the invite link (**Copy link**), the list of people, and the
  host's controls: Lock the room, Lock drawing, Remove, New link, End.
- The host's `accept` hook drops anything a guest sends on the `doc`
  channel and drops whiteboard operations from guests while drawing is
  locked. The doc channel is one way.
- The session follows the document: closing it, or opening another, asks
  "End the session?" first.
- View and Edit both work; the overlay is placed over the viewer's or the
  editor's figure box, and moves with the panel and the divider.

### 6.2 Guest

- `#collab=<token>` opens the app in **guest mode**: a name prompt (once),
  then "Connecting to the chart…", then the owner's chart in View.
- The guest's document has origin kind `collab`: read-only (no Edit, no
  Source pane, no theme change written), no draft, no recent entry; Save
  is Download (YAML or PNG of what is shown, without the drawings).
- **Follow the owner** (on by default) applies the owner's `view` messages
  with `setViewState`; panning or zooming by hand turns it off, and the
  button turns it back on.
- A `doc` source that does not bind (the owner is fixing an error) keeps
  the last good chart and shows "The owner's chart has an error; showing
  the last version that worked."
- When the room ends: "The session has ended", the last chart stays (with
  Download), and the whiteboard stays visible, read-only.

### 6.3 Doc channel messages

| Message | From | Meaning |
|---|---|---|
| `doc {v, source}` | host | The whole document source, version `v` (increasing). Sent on join and 150 ms after the last change. Guests ignore a `v` not above theirs. |
| `view {v, state}` | host | The owner's `viewState()` after a pan or zoom settles (the viewer's `view` event) and with every `doc`. |
| `meta {name, mode}` | host | The file name and the owner's mode, for the guest's title. |
| `lock {drawing}` | host | Drawing locked or not; guests set `wb.readOnly`. |

### 6.4 Adapter for nymph

`adapter.js` builds the transform from `figureBox()` of the viewer or the
editor (`a = d = scale`, `e = left`, `f = top`), listens to their `figure`
event and to the stage's resize, and swaps between them when the mode
changes. Content units are the figure's layout pixels: the same for every
person whatever their window. Plots pan and zoom inside a fixed layout, so
a note beside a plot stays beside it; a note over a data point does not
follow that point when someone zooms the plot (pinning to data is
section 12).

### 6.5 Commands

| Command | Scope | Effect | Agent |
|---|---|---|---|
| `collab_start` | document | external | no |
| `collab_join {token}` | app | external | no |
| `collab_stop` | app | read | no |
| `collab_state` | app | read | yes: role, people, path, whiteboard item count |

Agents cannot start or join a session (they would share the chart with
people the user did not choose); the owner's agent connectors keep working
during a session, and their changes reach guests as any other change.

### 6.6 Configuration

`config.json` gains:

```json
"collab": {
  "trackers": ["wss://…", "wss://…"],
  "iceServers": [{ "urls": "stun:stun.l.google.com:19302" }, …]
}
```

from the CMake cache variables `NYMPH_COLLAB_TRACKERS` (a list) and
`NYMPH_COLLAB_ICE_SERVERS` (JSON). The default lists are set by plan step
1 from what it measures. An empty tracker list leaves the manual codes
only; the Collaborate button is hidden only when WebRTC is missing. A
**TURN server** is added by the site owner in `NYMPH_COLLAB_ICE_SERVERS`,
or by a user in the People panel's **Connection settings** (kept in
`localStorage`, used for sessions this user joins or hosts).
`docs/collaboration.md` says how to run coturn or use a hosted TURN
service.

## 7. NAT and Connection Failure

| Situation | What happens |
|---|---|
| Same network, or one side has a public address | Host candidates or STUN: direct. |
| Both behind ordinary home NATs | STUN hole punching: direct (most cases). |
| One side behind symmetric NAT or carrier-grade NAT (many phone networks) | Usually still works when the other side is not; fails when both are. |
| Corporate firewall allowing only web traffic | Fails without TURN on 443 (`turns:` over TLS). |
| TURN configured | Relayed when direct fails; the People panel shows "relayed". |
| Trackers blocked or down | No offers arrive: after 30 s the guest is offered the manual codes. |

The guest's message after 30 seconds without a link names the stage that
failed: no tracker reachable, no answer from the host (wrong or old link,
host offline, room locked), or ICE failed (with the advice: another
network such as a phone hotspot, a TURN server, or ask the owner to send
the chart file). The path of each person (from the selected candidate
pair in `getStats()`) is shown in the People panel.

The star topology matters here: each guest needs a path to the host only.
Two guests on hostile networks can both be in the room as long as the host
is reachable to each.

## 8. Design Decisions

- **WebRTC data channels.** The only peer-to-peer channel a browser has. A
  page cannot accept a WebSocket, so "peer to peer over WebSockets" would
  need one side to be a native server, which brings back port forwarding
  and NATs without ICE to cross them. A shared file (Google Drive) works
  for the chart's state but cannot carry cursors and ink at tens of
  updates a second within Drive's latency and request quotas, and makes
  every guest sign in to Google. The agents DESIGN rejected WebRTC for
  pairing with a local `nymph mcp` because of signaling; here the peers
  are on other machines, so there is no way around it.
- **Public WebTorrent trackers for signaling**, over Nostr relays or MQTT
  brokers. The tracker protocol was made for browsers trading WebRTC
  offers, needs no client library (Nostr needs secp256k1 signatures),
  and there are several public trackers to use at once. Messages are
  sealed with the room key, so a tracker learns only that some browsers
  share a topic. Rejected: a signaling server of nymph's own (goal 1); a
  Cloudflare Worker (still a server to run and pay for). Cost: public
  trackers come and go, hence several at once, the list in
  `config.json`, and the manual codes. Step 1 confirms trackers accept
  sealed SDP and browser connections from the site.
- **A star with the host as hub**, over a full mesh. The owner's tab
  already holds the truth for the chart; guests need a path to it only,
  which halves the NAT problem and keeps connections at N, not N².
  Cost: the host's upload carries everything, which is small (chart text,
  cursors) for about 20 guests; and the room ends with the host, which is
  what "the owner's chart" means anyway.
- **The whole source on each change**, over text diffs. Charts are small
  (kilobytes), compression makes repeats cheap, and a full source cannot
  drift. Diffs are an optimisation if large charts need it.
- **Last-writer-wins per whiteboard item**, over a host-ordered log or a
  CRDT library (Yjs, Automerge). A host order would tie the whiteboard to
  nymph's star (goal 3); a library adds tens of kilobytes for features a
  whiteboard does not need (merging characters inside one note).
  Concurrent edits of the same note keep the later one: acceptable for
  sticky notes.
- **SVG for the overlay**, over a canvas. Pan and zoom become one
  `transform` on a group, hit testing for the eraser and text is the
  browser's, text is real text, and `toSvg()` is free. A canvas would be
  faster with thousands of strokes; the item limit keeps SVG fast enough.
- **Stickers as built-in SVG symbols** (check, cross, question, star,
  heart, thumbs up, warning, arrow), not emoji: emoji look different on
  every system, so two people would see different stickers.
- **Untrickled offers.** One sealed message per offer fits the tracker
  protocol (which carries one SDP per offer) and keeps tracker traffic
  low; the cost is up to 3 seconds of ICE gathering before an offer goes.
- **Content units from the host page**, over screen pixels or a
  normalised 0-1 box. Pixels differ per window; a 0-1 box stretches
  drawings when the figure's aspect changes. The figure's layout pixels
  are what the chart itself is laid out in.
- **Guests are viewers, never editors.** It removes merging from the
  chart (goal 2), lets phones join fully, and keeps every edit going
  through the owner's undo, attribution and agent rules (agents DESIGN).

## 9. Security Notes

- Everyone with the link is in the room: the link is the permission.
  The owner can lock the room, remove a person, and make a new link.
- Trackers see the topic hash and the connecting browsers' IP addresses,
  never the token, chart or drawings. Peers learn each other's IP
  addresses (WebRTC needs them); the start dialog says so.
- Guests' messages are untrusted: validated, size-limited and rate-limited
  (50 whiteboard operations a second per peer); they never reach the
  document, the command registry or HTML parsing.
- A TURN server sees only DTLS-encrypted traffic.

## 10. Constraints and Invariants

- No connection to a tracker, STUN or TURN server before the user starts
  or joins a session; leaving closes every socket and peer connection.
- The guest's document is never changed by anything but a `doc` message
  from the host; the host's document is never changed by any message.
- The whiteboard is never written into the YAML, the PNG, a draft or a
  recent entry; it lives in memory and the tab's `sessionStorage` only.
- `web/collab/` imports nothing outside itself; `collab-whiteboard.js`
  does not import `collab-session.js`.
- The viewer and editor modules stay within `web/size_budget.json`.
- Tests use port 0, a local test tracker, and wait on conditions, never
  fixed sleeps (CLAUDE.md, Tests).
- Code style: JavaScript as `web/app/` (ES modules, no framework, no
  build step), MIT header, JSDoc on the public API.

## 11. Integration Boundaries

- Depends on: the app's document, commands and stage (app DESIGN sections
  3.3, 3.11); the agent tool list rules (agents DESIGN section 4); the
  viewer's `setSource` keeping the view of unchanged elements and its
  `view` event ([04_web_viewer](../04_web_viewer/DESIGN.md)); the editor's
  figure box ([14_editor_workspace](../14_editor_workspace/DESIGN.md)).
- Used by: the app; later, any page embedding `collab/*.js`.
- Browser features: `RTCPeerConnection`, `crypto.subtle` (HKDF, AES-GCM),
  `CompressionStream`, `BroadcastChannel` (tests, demo). The CSP gains
  `wss:` in `connect-src`; WebRTC itself is not governed by `connect-src`.
- The service worker is unaffected (it serves same-origin GETs only).

## 12. Extension Points

- **Handing over editing**: the owner passes the doc role to a guest (the
  guest becomes the host; the session would need host migration).
- **Pinning drawings to data**: an optional `anchor` on items, resolved by
  an adapter `resolveAnchor(anchor)` (for nymph: a data point or diagram
  node from the viewer's picks), so a note follows a point as plots zoom.
- **Other rendezvous back ends**: Nostr, MQTT, a Drive file, an embedder's
  own server, behind the interface of section 3.4.
- **Mesh fallback**: a guest connected to the host relays for a guest that
  cannot reach it.
- **A Drive refresh mode** for networks where WebRTC fails: guests open the
  owner's Drive file and reload it on a new revision (no whiteboard).
- **Agents**: a read tool returning the whiteboard's notes so the owner's
  agent can act on what reviewers wrote.
- **Export with drawings**: the chart PNG with `toSvg()` drawn over it.
