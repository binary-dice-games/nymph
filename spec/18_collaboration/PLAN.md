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

## 6. Viewer and editor calls

**Goal:** the app can place the overlay on the figure and follow the
owner's view.

**Deliverables:** `figureBox()`, the `figure` event and `viewState()` in
`nymph-viewer.js` and `nymph-editor.js`, `setViewState()` in the viewer,
with the C exports in `web/nymph_web.cpp` and
`web/editor/nymph_editor_web.cpp` (reusing the `view` key code);
`docs/viewer.md` rows.

**Tests:** Node (`web/tests/node/app.mjs`, `editor.mjs`): `viewState()`
equals the `view` key of `toYaml()`; `setViewState()` of another viewer's
state gives the same `viewState()` and leaves `source()` unchanged.
Browser: `figureBox()` matches the canvas box in the viewer, and in the
editor after moving the divider (with a `figure` event); the size budget
test still passes.

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

## 8. App: People panel and whiteboard

**Goal:** the collaboration is usable and understandable end to end.

**Deliverables:** `web/app/app/collab/panel.js` and `app.css`: the start
dialog (what is shared), name prompt, People panel (invite link, people,
colours, path, host controls: lock room, lock drawing, remove, new link,
end), connection settings (TURN, in `localStorage`), the whiteboard over
the stage in View and Edit mode with its toolbar, guest status messages
and the failure messages with their advice.

**Tests:** Browser: the start dialog appears once per browser; lock drawing
makes the guest's whiteboard read-only and the host drops a forged
operation; remove closes the guest with a message; a new link refuses the
old token for new joins while connected guests stay; the overlay follows
the editor's divider; a TURN entry in the settings reaches
`RTCPeerConnection`'s configuration; an unreachable host shows the "no
answer" message after the (shortened) timeout.

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
