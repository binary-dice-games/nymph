# nymph — Release Pipeline: Implementation Plan

See [DESIGN.md](DESIGN.md) for the design this plan implements, and
[spec/06_claude_integration/DESIGN.md](../06_claude_integration/DESIGN.md)
section 6 for the release builds it automates.

Steps go from CI on the platforms nymph already claims (step 1), to the
new platform and the source install (steps 2-4), to the release itself
(step 5), to each distribution channel (steps 6-9), to the first real
release (step 10). Every step leaves the existing native, Node and
browser tests passing. Workflow steps are tested on a fork or a branch
with `workflow_dispatch` and a prerelease tag (`v<X.Y.Z>-rc.<n>`), which
publishes nothing to a channel. Each step records the runner images and
tool versions it was verified with.

## Steps

### 1. CI for the existing targets

**Goal:** every pull request builds and tests nymph on Linux x86-64 and
arm64, Windows (MSVC and MSYS2) and the web, and fails when any of them
fails.

**Deliverables:**
- `.github/workflows/build.yml`: a reusable workflow (`workflow_call`)
  with inputs `target` (one of the DESIGN section 4.2 jobs), `release`
  (bool) and `mcp_viewer_artifact`; it checks out with submodules,
  installs the target's tools, configures, builds, runs `ctest -j
  --output-on-failure`, and uploads `dist/` (release) or the web outputs
  as artifacts.
- `.github/workflows/ci.yml`: on `pull_request` and `push` to `main`,
  calls `build.yml` for linux-gcc, linux-arm64, windows-msvc,
  windows-msys2, web and release-smoke (which needs web's App page).
  `permissions: contents: read`; actions pinned by SHA; `concurrency`
  cancels superseded runs of a pull request.
- Fixes for whatever MSVC and MSYS2 builds reveal (neither was verified
  before: spec 01, 02, 03 status), each with a test where behavior
  changed.
- `spec/INDEX.md`: statuses of projects 01-04 and 06 updated where this
  step verifies MSVC or MSYS2.

**Tests:**
- All CI jobs green on a pull request.
- A pull request with a deliberate test failure turns its job red (then
  dropped).

### 2. macOS target

**Goal:** nymph builds and passes its tests on macOS arm64 and x86-64,
and `NYMPH_RELEASE` makes a signed, stripped executable that links only
system libraries.

**Deliverables:**
- `CMakeLists.txt`: `NYMPH_OPENGL` default OFF when `APPLE`;
  `CMAKE_OSX_DEPLOYMENT_TARGET` default (13.0, or the lowest that
  passes; record it).
- `cmake/NymphRelease.cmake`: the Darwin branch of DESIGN section 4.8
  (`-Wl,-dead_strip`, no `-s`, `strip -x` and `codesign --force -s -`
  as post-build steps of `nymph-cli`).
- `tests/release_static.cmake` renamed `release_linkage.cmake`: Linux
  as today (no `NEEDED`), macOS with `otool -L` (only `libSystem.B` and
  `libc++.1` under `/usr/lib`); `tests/CMakeLists.txt` registers it for
  both.
- `tests/release_examples.cmake`: the arm64 list of differing examples
  for AppleClang, if it differs from GCC's (measure on macos-15).
- `ci.yml`: macos-arm64 (Debug) and macos-x86_64 (Release) jobs;
  release-smoke on macos-15 as well.
- `CLAUDE.md` (Platform Support), `docs/building.md` (prerequisites:
  Xcode command line tools; a "macOS" section), `README.md` status line:
  macOS listed with Linux and Windows.

**Tests:**
- Full `ctest` green on macos-15 and macos-15-intel.
- `nymph rasterizers` on macOS reports the software rasterizer and
  "nymph was built without OpenGL support"; `nymph examples/dashboard.yaml`
  writes a PNG whose source extracts back byte for byte.
- `release_linkage` passes on the release build and fails on a build
  linked against an extra dylib (checked once by hand, recorded).
- `codesign -v` accepts the release executable on arm64.

### 3. Install from source

**Goal:** `cmake --install` installs a working `nymph` and its
documentation, and a tree without submodules fails with a clear message.

**Deliverables:**
- `cmake/NymphInstall.cmake` (included from the root `CMakeLists.txt`
  after the targets): option `NYMPH_INSTALL`, the rules of DESIGN
  section 4.3 with `GNUInstallDirs`, component `nymph`.
- `CMakeLists.txt`: before `NymphDependencies.cmake`, if
  `extern/imgui/imgui.h` is missing and there is no `.git`, a
  `FATAL_ERROR` naming the source tarball and `git clone --recursive`.
- `tests/install_check.cmake` and its `nymph_install` test (top-level
  native builds): `cmake --install <build> --prefix <work>/prefix
  --component nymph`, then the installed `nymph --version` prints the
  project version, it renders `examples/dashboard.yaml`, and every file
  of the table in DESIGN section 4.3 exists.
- `docs/building.md`: "Install" section; `README.md` Quick Build gains
  `cmake --install build`.
- `CHANGELOG.md`: `cmake --install` installs nymph.

**Tests:**
- `nymph_install` passes on every CI job (Linux, macOS, MSVC, MSYS2).
- With nymph added by `add_subdirectory` from a small host project
  (a test fixture project configured by a script test), the host's
  `cmake --install` installs nothing of nymph.
- A copy of the tree without `extern/` contents and `.git` fails to
  configure with the message (script test).

### 4. Source tarball

**Goal:** `nymph-<v>-src.tar.gz` builds, tests and installs offline on
Linux and macOS.

**Deliverables:**
- `cmake/NymphSourceDist.cmake` (script mode: `-DSOURCE_DIR -DVERSION
  -DOUTPUT`): DESIGN section 4.4; the exclusion list with the size it
  saves; fails if a submodule is not checked out at its pinned commit.
- A `nymph-source-dist` target (top-level, git checkout) writing
  `<build>/dist/nymph-<v>-src.tar.gz`.
- `ci.yml` source job: make the tarball; on ubuntu-24.04 extract it and
  configure, build, `ctest` and install under `unshare -rn` (no
  network); the same on macos-15 without the network guard. The tarball
  is uploaded as an artifact.
- `docs/building.md`: building from the source tarball;
  `CHANGELOG.md`: release source tarball with submodules.

**Tests:**
- The source job is green; the tarball has one top folder, no `.git`,
  no file with the executable bit set except scripts that are executable
  in the repository (`claude/plugin/bin/nymph`), and no `.wasm` or
  generated file.
- Its size is recorded (and the exclusion list is justified by it).
- Two runs on the same commit give the same file list.

### 5. Release workflow

**Goal:** pushing a `v*` tag produces a complete published GitHub
release with every file of DESIGN section 3, or no published release at
all.

**Deliverables:**
- `.github/workflows/release.yml`, on `push: tags: ['v*']` and
  `workflow_dispatch`: jobs `check` (tag = version, changelog section,
  prerelease flag, notes extracted to a file), `web`, `native` (five
  targets, `release: true`), `source`, `release` (download artifacts;
  `lipo -create` the macOS executables and run `cmake/NymphMcpb.cmake`
  in script mode for `nymph-<v>-macos-universal.mcpb`; drop the
  per-arch macOS `.mcpb`; rename the App page to
  `nymph-mcp-app-<v>.html`; one `SHA256SUMS` over all files; create the
  draft release with the notes, upload, publish, marked prerelease for
  `-` tags). The `release` job alone has `contents: write`, and it fails
  if the tag's release exists and is published.
- `docs/releasing.md` (new): the two release actions of DESIGN section
  6, what the workflow does, re-running a failed job, the repository
  settings table; `README.md` "Further Documentation" row.
- `docs/building.md` "Release executables": the manual steps now point
  to docs/releasing.md (kept for local builds).

**Tests:**
- `workflow_dispatch` on a branch: every file is a workflow artifact,
  no release is created.
- A prerelease tag `v<X.Y.Z>-rc.1` on a fork: a published prerelease
  with exactly the files of DESIGN section 3; `sha256sum -c SHA256SUMS`
  passes on the downloaded files; the universal `.mcpb` passes
  `tests/mcpb_check.cmake` on macOS arm64 and x86-64 runners.
- A tag whose version differs from `CMakeLists.txt` fails in `check`,
  with no release created.
- Re-running the workflow for an already published tag fails in
  `release` without changing any file.

### 6. Claude Code plugin deployment

**Goal:** a published release opens and merges a pull request that pins
its executables in the plugin, and the plugin's version always equals
the pinned release.

**Deliverables:**
- `cmake/NymphPluginSync.cmake`: the version rule of DESIGN section 4.5
  (`VERSION` from the pinned `SHA256SUMS` when given, else kept from the
  existing `plugin.json`; `0.0.0` when none).
- `tests/plugin_sync_check.cmake`: checks the skill copies (as today)
  and that `plugin.json` and `checksums.txt` name the same version.
- `release.yml` job `plugin` (needs `release`; not for prereleases;
  skipped with a warning without `NYMPH_RELEASE_TOKEN`): run the sync
  with the published `SHA256SUMS`, push branch
  `release/plugin-v<version>`, open the pull request, enable auto-merge;
  finds an existing one instead of failing.
- `docs/claude.md`: macOS in the platform list; updates arrive with
  `/plugin marketplace update`.
- `CHANGELOG.md`: plugin version follows the pinned release; macOS
  executables and universal `.mcpb`.

**Tests:**
- Script tests of the sync: with sums of `0.2.0`, `plugin.json` says
  `0.2.0` and `checksums.txt` lists five executables (macOS included);
  without sums both stay; mismatched files fail the check.
- On a fork's release: the pull request changes only the two files, its
  CI is green, it merges.
- After merge, the launcher with `NYMPH_LAUNCHER_INFO=1` names the new
  release on Linux, macOS and (Git Bash) Windows runners, and a real run
  downloads, checks and runs `nymph --version` on each.

### 7. Homebrew tap

**Goal:** each release updates `binary-dice-games/homebrew-nymph`, and
`brew install binary-dice-games/nymph/nymph` installs a working nymph on
macOS and Linux.

**Deliverables:**
- The tap repository (created by the owner) with a README.
- `packaging/homebrew/nymph.rb.in`: DESIGN section 4.6, with
  placeholders for version, tarball and App page URLs and SHA-256s.
- `release.yml` job `homebrew` (needs `release`; not for prereleases;
  skipped without `HOMEBREW_TAP_TOKEN`): fill the template from the
  published `SHA256SUMS`; on macos-15 and ubuntu-24.04 (Homebrew on
  Linux) `brew install --build-from-source`, `brew test`,
  `brew audit --strict --online`; then commit to the tap
  ("nymph <version>") unless it is already at that version.
- `README.md` and `docs/building.md`: the brew command.
- `CHANGELOG.md`: Homebrew tap.

**Tests:**
- On a fork with a test tap: the job pushes the formula; on a clean
  runner `brew install <test-tap>/nymph` then `nymph --version` prints
  the release version, `nymph mcp` answers `initialize` and lists the
  App resource (the App page was passed), and `brew test nymph` passes.
- Re-running the job is a no-op.

### 8. Chrome Web Store

**Goal:** each release uploads the extension to the Web Store and
submits it for review.

**Deliverables:**
- `packaging/chrome/publish.sh <zip>`: DESIGN section 4.7 (access token
  from the refresh token, upload, wait for the upload state, publish);
  prints the store's answer and exits non-zero on any refusal.
- `release.yml` job `chrome` (needs `release`; not for prereleases;
  skipped without `CHROME_EXTENSION_ID` and the secrets).
- `docs/releasing.md`: one-time store setup (developer account, item
  creation with a first manual upload, listing text, screenshots,
  privacy practices: no data collected, `<all_urls>` justification,
  the API credentials).
- `docs/extension.md`: install from the Web Store.
- `CHANGELOG.md`: Chrome extension published to the Web Store.

**Tests:**
- `publish.sh` against a stub HTTP server (Node, in the extension's
  Node tests): the right requests in order, failure on an error answer,
  no token in the output.
- On the real item, once: the job uploads, the dashboard shows the new
  version "pending review"; a second run reports the version already
  uploaded and does not fail the release.

### 9. macOS signing and Claude Desktop on macOS

**Goal:** macOS release files run without a Gatekeeper prompt in every
channel, and the universal `.mcpb` works in Claude Desktop on a Mac.

**Deliverables:**
- `release.yml` (macOS native jobs): when the `APPLE_*` secrets exist,
  import the certificate into a temporary keychain, sign with the
  Developer ID and hardened runtime, notarize with `notarytool`; the
  universal binary is signed after `lipo`. Without them, ad hoc as in
  step 2.
- `docs/releasing.md`: the Apple settings; `docs/claude.md`: Claude
  Desktop on macOS.
- Claude DESIGN section 10: the macOS question marked answered by this
  project.

**Tests:**
- `codesign -dv --verbose=2` shows the Developer ID (when configured);
  `spctl -a -t exec` accepts the executable downloaded with a browser
  (quarantined).
- By hand, recorded with versions: Claude Desktop on macOS (arm64)
  installs the universal `.mcpb`, the server starts, `render_chart`
  returns a PNG and the App shows it; with ad hoc signing only, record
  whether Gatekeeper blocks it.

### 10. First release

**Goal:** a real `v<X.Y.Z>` release reaches every channel and each one
installs and runs it.

**Deliverables:**
- The release commit (version, changelog), the tag.
- `release.yml` job `verify` (needs the channel jobs; runs on Linux,
  macOS and Windows): installs from each channel that ran and runs
  `nymph --version` and one render.
- Results recorded in this plan, statuses in DESIGN.md and
  `spec/INDEX.md`.

**Tests:**
- The `verify` job is green.
- By hand: the Web Store listing shows the version once review passes.

## Completion Criteria

- One tag push publishes a release with every file of DESIGN section 3
  and updates the plugin, the Homebrew tap and the Chrome Web Store,
  with no other manual action (beyond store review).
- CI builds and tests Linux (x86-64, arm64), macOS (arm64, x86-64),
  Windows (MSVC, MSYS2), the web build and the source tarball on every
  pull request, all green.
- The source tarball builds, tests and installs with no network, and
  `cmake --install` installs a working `nymph` on every platform.
- `brew install binary-dice-games/nymph/nymph` works on macOS and Linux.
- The plugin's version and checksums always name a published release.
- No published release file is ever replaced; prereleases and dry runs
  update no channel.
