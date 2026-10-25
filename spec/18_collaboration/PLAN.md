# nymph — Collaboration: Plan

Design: [DESIGN.md](DESIGN.md). Each step is a reviewable deliverable; the
next starts after its tests pass. Web tests extend `web/tests/node/` and
`web/tests/browser/` (Playwright with the preinstalled Chromium, skipped
with 77 when Playwright is missing), serve the built site from a local
server on port 0, and wait on conditions, never fixed sleeps. No test
reaches a public tracker, STUN or TURN server: they use the local test
tracker (step 4) and host candidates only. A `CHANGELOG.md` entry is added
with each user-visible step (6 to 9).

## 1. Risk check: trackers, NATs and browser limits

**Goal:** every fact DESIGN.md takes on trust is checked, and the design
is corrected where one is false, before anything is built.

**Deliverables:** throwaway pages and scripts in the scratch build (not
committed), and the results written into this step's **Result**, item by
item:

1. Public WebTorrent trackers (at least five candidates): which accept a
   `wss` connection from a page on `https://binary-dice-games.github.io`
   (and from `http://localhost`), forward an offer whose SDP field is an
   opaque sealed string, and how long a join takes from opening the link
   to an open data channel. Sets the default `NYMPH_COLLAB_TRACKERS`. If
   fewer than two work, compare Nostr relays (ephemeral events) and
   correct DESIGN.md sections 3.4 and 8 before step 4.
2. NAT: a direct connection with STUN only between a home network and a
   phone hotspot (carrier-grade NAT), two home networks, and a corporate or
   university network if one is at hand; the same with a TURN server
   (a hosted free tier or a coturn on a VM). Sets the default
   `NYMPH_COLLAB_ICE_SERVERS`.
3. Data channels: largest message that arrives whole in Chromium, Firefox
   and Safari (confirms the 16 KiB frame), and `negotiated: true` channels
   between Chromium and Firefox, and Chromium and Safari.
4. CSP: the app's `connect-src` with `wss:` added lets tracker sockets
   open, and does not affect `RTCPeerConnection` in the three browsers.
5. `getStats()`: the selected candidate pair tells `direct` from `relay`
   in the three browsers.

**Tests:** none committed; each item records what was run, on which
version, and the outcome.

**Result (not done):** the build sandbox's network proxy refuses the
tracker hosts (`CONNECT` 403 for `tracker.openwebtorrent.com`, errors for
`tracker.webtorrent.dev`, `tracker.btorrent.xyz` and others) and has no UDP
for STUN, so items 1, 2 and 5 against real networks are open. The default
`NYMPH_COLLAB_TRACKERS` (openwebtorrent, webtorrent.dev, btorrent) and
`NYMPH_COLLAB_ICE_SERVERS` (Google and Cloudflare STUN) are therefore
unverified. Item 3 is covered for Chromium only (16 KiB frames between two
Chromium contexts, `negotiated: true` channels); item 4 for Chromium (the
app's CSP with `wss:` and loopback `ws:` lets the test tracker connect;
WebRTC is unaffected). Firefox and Safari not tried.

## 2. Whiteboard model

**Goal:** the whiteboard state converges on every peer whatever the order
of operations.

**Deliverables:** the model part of `web/collab/collab-whiteboard.js`
(exported for tests as `createModel`, not documented for embedders):
items (`ink`, `text`, `sticker`), ids, Lamport stamps, last-writer-wins
per item, tombstones, clear stamp, own-operation undo / redo, snapshot and
merge, point simplification and rounding, the limits of DESIGN.md
section 3.6, operation validation.

**Tests:** Node (`web/tests/node/whiteboard_model.mjs`): three models given
the same operations in 200 seeded random orders (with duplicates) end
equal; snapshot round trip; merging two diverged snapshots both ways gives
the same state; a delete then a late add keeps the item deleted; clear
removes only older items; undo / redo of own operations only; limits
refuse with a message; malformed operations are rejected.

**Result:** done. `web/tests/node/collab.mjs` (6 model tests; 200 seeded
rounds of three peers editing concurrently, then each applying every
operation in its own order with duplicates). Clear removes items stamped
before it outright (no tombstone needed: any later put with an older stamp
is ignored).

## 3. Session core

**Goal:** rooms, roster, relay and channels work over any link.

**Deliverables:** `web/collab/collab-session.js` without WebRTC: the token
and HKDF keys, sealing (AES-GCM), the link interface, an in-memory link
pair (exported for tests), the `broadcast` link and rendezvous, `hello`
and protocol check, roster and presence, host relay with `accept` and the
`from` stamp, `fast` drop on full buffers, framing (16 KiB frames,
compression over 4 KiB, 16 MB limit), validation and the malformed-frame
limit, `lock`, `remove`, `newInvite`, `close`, `state` events.

**Tests:** Node (`web/tests/node/session.mjs`, in-memory links): a host and
three guests see the same roster; a guest's broadcast reaches the others
once, with `from` set by the host, and not its sender; `accept` drops a
message; a 5 MB message arrives whole, a 20 MB one is refused; a guest
claiming another `from` is overridden; protocol mismatch refused with the
reason; removed guest's id refused; lock refuses new joins; a sealed
message altered by one byte is rejected; host close ends every guest with
`ended`.

**Result:** done, in the same file (9 session tests, plus one with the
model synced over a session). Rendezvous became "bring links"
(`start(context)` with `context.link()`), so the in-memory network, the
BroadcastChannel and WebRTC share one session path (DESIGN.md section 3.4
updated).

## 4. WebRTC links and rendezvous

**Goal:** two browsers on one machine join a room through a tracker, or by
manual codes.

**Deliverables:** the `webrtc` link (two negotiated channels, untrickled
offers with the 3 s gathering limit, ICE restart on `disconnected`,
`stats()` path), the `torrent` rendezvous (several trackers at once,
announce schedule and backoff, sealed SDP, role check), the `manual`
rendezvous and its codes, connection failure reasons (DESIGN.md section
7). `web/tests/browser/tracker.mjs`: a minimal WebTorrent-protocol tracker
in Node with no dependencies, on port 0. `web/collab/demo/index.html`:
two packages over a plain image, joining by a link.

**Tests:** Browser (`web/tests/browser/collab_session.mjs`), two browser
contexts with host candidates only: join through the test tracker, then
messages both ways on `reliable` and `fast`; join with the manual codes;
a guest that joins before the host connects once the host comes; a
stopped tracker after joining does not drop the link; the tracker sees no
readable SDP (checked in the test tracker's log); no tracker reachable
gives the "no tracker" reason within the timeout (shortened for tests).

**Result:** done. `web/tests/browser/tracker.mjs` and
`web/tests/browser/collab.mjs` (Chromium with
`--disable-features=WebRtcHideLocalIpsWithMdns`, so host candidates are
IP addresses). Found while building the app: the host answered all three
offers of one guest, and its "same peer, new link wins" rule then closed
the link the guest used, in a loop. Now the host only stays in the swarm,
guests offer, a guest takes the first answer and pauses its offers, and the
hello carries a per-tab instance id: a second link of the same instance is
dropped, another instance (a reload) replaces the old link (DESIGN.md
section 4).

## 5. Whiteboard overlay

**Goal:** people in a room see each other's cursors and draw, write and
place stickers over any element.

**Deliverables:** the overlay part of `collab-whiteboard.js`: shadow DOM,
SVG layer with the adapter's transform, default adapter, tools (pointer,
pen, text, sticker, eraser), live ink and cursors on `fast` (at most 30 a
second, hidden after 5 s idle or on leave), join snapshot from the hub
and both-ways merge, built-in stickers, optional toolbar, `strings`,
keys only outside pointer mode, `readOnly`, `toSvg()`. The demo page
uses it.

**Tests:** Node: an import check that `web/collab/` imports nothing outside
itself and the whiteboard does not import the session. Browser
(`web/tests/browser/whiteboard.mjs`, two tabs over `broadcast`, then two
contexts over the test tracker): a stroke drawn in one tab appears in the
other at the same content position with a different container size and a
different adapter scale; text from a peer containing `<b>` shows as text;
an unknown sticker name draws the placeholder; pointer mode lets a click
reach the element below; a late joiner receives the existing items;
eraser and undo propagate; a read-only whiteboard draws nothing.

**Result:** done (5 browser tests, plus the demo page served alone). A
whiteboard made after its session opened asks the host for its snapshot
(`sync` with `want: true`): the join event had already passed.

## 6. Viewer and editor calls

**Goal:** the app can place the overlay on the figure in both modules.

**Deliverables:** `figureBox()` and the `figure` event in
`nymph-viewer.js` (from its canvas and `state().geometry`) and
`nymph-editor.js` (from a new `figure` entry in `nymph_editor_state` in
`web/editor/nymph_editor_web.cpp`); `docs/viewer.md` rows.

**Tests:** Browser: `figureBox()` matches the canvas box and the layout
size times the CSS scale in the viewer, and in the editor moves right when
the divider is dragged and is `null` on a data tab (with a `figure`
event each time); the size budget test still passes.

**Result:** done; in `web/tests/browser/collab_app.mjs` (viewer box =
canvas box, a `figure` event on a source of another size, the editor's box
under its tab bar; the overlay's transform on the figure in both modes).
Dragging the divider and the data tab are not driven (no tool switches
tabs). Added after screenshots showed drawings off between the owner's
editor and the guests: `view_fit::natural` and `setFit()` (DESIGN.md
sections 5.3 and 6.4), tested natively (`test_live_view`,
`FitNaturalLaysOutAtTheSourceSizeAndNeverScales`) and in the app (the
owner's editor and a guest's viewer report the same layout size).

## 7. App: hosting and joining

**Goal:** the owner shares the open chart and guests see it live.

**Deliverables:** `web/app/app/collab/collab.js` and `adapter.js`; in
`main.js`, `document.js`, `commands.js` and `device.js`: the `collab_*`
commands (DESIGN.md section 6.5), the `#collab=` route, the guest document
(origin `collab`, read-only, no draft or recent entry), the doc channel
(section 6.3), Follow the owner, the error-tolerant guest update, the end
of session, host resume after a reload. `index.html`: `wss:` in the CSP.
`web/CMakeLists.txt`: `collab/` copied by `nymph-site`, the `config.json`
`collab` section from `NYMPH_COLLAB_TRACKERS` and
`NYMPH_COLLAB_ICE_SERVERS`; `web/size_budget.json` entries for both
packages (set from their size after step 5 plus 10 %).

**Tests:** Browser (`web/tests/browser/collab_app.mjs`, the built site, two
contexts, the test tracker through `config.json`): a guest opening the
invite sees the owner's chart; an edit in Edit mode and an agent-style
`document_set_source` both reach the guest within the debounce; the guest
keeps its zoom on unchanged elements; Follow the owner applies the owner's
pan and turns off on the guest's own pan; a source that does not bind
keeps the guest's last chart with the message; the guest has no Edit and
no draft; closing the owner's document ends the session after asking;
an owner reload resumes the room and the guest reconnects; nothing
connects to a tracker before Collaborate (checked by the test tracker
seeing no client). Node: the agent tool list includes `collab_state` and
not the other `collab_*` commands.

**Result:** done (app tests: sharing and every change reaching the guest
in View and Edit, the guest refused Edit / set source / doc messages,
Follow the owner and the guest's own view kept, a broken source keeping
the last chart, closing ending the session after asking, the owner's
reload resuming the room with its drawings, no tracker contacted before
Collaborate, the agent tool list). The first chart goes out as version 1
(a guest takes only versions above its own 0).

## 8. App: People panel and whiteboard

**Goal:** the collaboration is usable and understandable end to end.

**Deliverables:** `web/app/app/collab/panel.js`, `export.js` and `app.css`: the start
dialog (what is shared), name prompt, People panel (invite link, people,
colours, path, host controls: lock room, lock drawing, remove, new link,
end), connection settings (TURN, in `localStorage`), Download PNG with
drawings (DESIGN.md section 6.7), the whiteboard over the stage in View and Edit mode with its toolbar, guest status messages
and the failure messages with their advice.

**Tests:** Browser: the start dialog appears once per browser; lock drawing
makes the guest's whiteboard read-only and the host drops a forged
operation; remove closes the guest with a message; a new link refuses the
old token for new joins while connected guests stay; the overlay follows
the editor's divider; a TURN entry in the settings reaches
`RTCPeerConnection`'s configuration; an unreachable host shows the "no
answer" message after the (shortened) timeout.

**Result:** done (app tests: the start dialog once, a TURN entry in the
ICE servers, Only I can draw with a forged operation dropped, Remove, New
link keeping people in, PNG with drawings, typing "e" and "?" in a note in
View mode). The "no answer" message after the timeout is tested for the
session package (no tracker), not in the app. Fixed on the way: the app's
shortcut and paste handlers read `e.target`, which a shadow root hides; they
now read `composedPath()[0]`, so a note's text box counts as typing.

## 9. Documentation and status

**Goal:** users, embedders and site owners can use what was built.

**Deliverables:** `docs/collaboration.md` (starting and joining, what is
shared, NATs and TURN setup, embedding `collab-session` and
`collab-whiteboard` with the adapter and the demo); `docs/web-app.md`
(a Collaboration section and the keys); `docs/viewer.md` (step 6 calls);
`README.md` (one line and the Further Documentation row);
`CHANGELOG.md`; each step's **Result** here; the status in
`spec/INDEX.md` and DESIGN.md.

**Tests:** the doc links resolve (the existing link check, if any, else by
hand); the demo page's code in `docs/collaboration.md` matches
`web/collab/demo/index.html`.

**Result:** done: `docs/collaboration.md`, `docs/web-app.md`,
`docs/viewer.md`, `docs/agent-web-app.md`, `README.md`, `CHANGELOG.md`,
`spec/INDEX.md`.

## Completion Criteria

- Every test above passes in CI (the web job and `pages.yml`); the viewer
  and editor modules within their size budgets; the app without a session
  makes no new request.
- On the deployed site, by hand: an owner in Chrome and guests in
  Firefox, Safari and a phone browser join through the public trackers;
  a guest on a phone hotspot joins a host on a home network; a session
  with a configured TURN server connects where STUN alone did not, and
  shows "relayed"; the manual codes work with the trackers blocked.
- Guests see every owner change live and never change the chart; the
  whiteboard converges for everyone and is never in the saved file.
- `web/collab/demo/` works served alone, with no nymph file.
- `spec/INDEX.md`, `README.md`, `docs/collaboration.md`, `docs/web-app.md`,
  `docs/viewer.md` and `CHANGELOG.md` describe what was built.
