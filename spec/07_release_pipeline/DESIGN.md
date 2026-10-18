# nymph — Release Pipeline: Design

**Status: implemented (plan steps 1-9 in code; step 10, the first real
release, waits on the owner's setup). Verified on Linux only: everything
that runs on Linux passes there, including the source tarball built
offline. The workflows have not run on GitHub yet, so macOS, Windows,
Homebrew and the Chrome Web Store are untested; see [PLAN.md](PLAN.md)
for each step's result.** [PLAN.md](PLAN.md) is the
ordered implementation plan. This document describes only what this
project adds or changes; the rest is
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md)
(*core DESIGN*), [spec/02_opengl_rasterizer/DESIGN.md](../02_opengl_rasterizer/DESIGN.md)
(*GL DESIGN*), [spec/05_browser_extension/DESIGN.md](../05_browser_extension/DESIGN.md)
(*extension DESIGN*) and
[spec/06_claude_integration/DESIGN.md](../06_claude_integration/DESIGN.md)
(*Claude DESIGN*), whose release builds (section 6) and plugin (section
4.6) this project automates.

## 1. Purpose / Scope

Today a nymph release is a sequence of commands run by hand on one Linux
machine (docs/building.md, "Release executables"): cross builds, a manual
upload, a manual plugin pin. Nothing builds or tests on macOS, MSVC or
MSYS2, and nothing publishes the Chrome extension. This project makes a
release **one tag push**:

```
git tag v0.2.0 && git push origin v0.2.0
```

and GitHub Actions builds, tests and publishes everything a user can
install nymph from.

In scope:

1. **A GitHub release per build**: each release tag creates a new
   GitHub release `v<version>` with the executables and packages for
   Windows, Linux and macOS (section 3).
2. **macOS as a target**, on the same footing as Linux and Windows:
   built and tested in CI, release executables for arm64 and x86-64, a
   Claude Desktop bundle.
3. **Install from source**: `cmake --install` installs the `nymph`
   command; a release source tarball carries every submodule, so a build
   needs no network and no third-party binary.
4. **`nymph` on `PATH`**: every way of installing nymph leaves `nymph`
   runnable from a new console, on Windows, Linux and macOS (section
   4.9). Release install scripts (`install.sh`, `install.ps1`) install
   the release executable for users who neither build nor use Homebrew.
5. **Homebrew**: a tap, `binary-dice-games/homebrew-nymph`, whose formula
   builds the release source tarball; each release updates it.
6. **Chrome extension**: each release uploads the extension to the Chrome
   Web Store and submits it for review.
7. **Claude Code plugin**: each release pins its executables in
   `claude/plugin/bin/checksums.txt` and bumps the plugin version.
8. **CI** on every pull request for every target, so a release never
   finds a broken platform.

Out of scope: a Homebrew bottle (section 10), homebrew-core, Linux
distribution packages (`.deb`, `.rpm`), installing the library and
headers (`find_package(nymph)`; `add_subdirectory` stays the way to use
the library, core DESIGN), Windows arm64, an OpenGL rasterizer on macOS
(section 8, decision 4), other browsers' stores, the claude.ai Skill and
Claude Desktop bundle beyond attaching them to the release (they have no
store to publish to).

New and changed files:

| Path | What |
|---|---|
| `.github/workflows/build.yml` | Reusable: set up a runner, build and test one target (CI and release share it) |
| `.github/workflows/source.yml` | Reusable: the source tarball, then its offline build |
| `.github/workflows/ci.yml` | Build and test every target on pull requests and `main` |
| `.github/workflows/release.yml` | The release, from a `v*` tag |
| `.github/scripts/` | `ci-build.sh` (configure, build, test, `PATH` check), `release-check.sh` (tag, version, notes), `release-assemble.sh` (the release files), `homebrew-formula.sh`, `release-verify.sh` |
| `CMakeLists.txt`, `cmake/NymphInstall.cmake`, `cmake/NymphInstallPath.cmake` | `install()` rules, `NYMPH_INSTALL` and `NYMPH_INSTALL_UPDATE_PATH` options, Windows default prefix, the install-time `PATH` step, missing-`extern/` error, macOS defaults |
| `src/core/charconv.*` | `from_chars_double()`: `std::from_chars` for `double`, with a portable fallback for Apple's libc++ |
| `app/CMakeLists.txt` | macOS release: strip and sign after the link (`NYMPH_CODESIGN_IDENTITY`) |
| `packaging/install/install.sh`, `install.ps1` | Install the release executable and put it on `PATH`; also called by `cmake --install` for `PATH` only |
| `cmake/NymphRelease.cmake` | macOS release flags; `nymph-dist` on macOS |
| `cmake/NymphSourceDist.cmake` | Script mode: the source tarball |
| `cmake/NymphPluginSync.cmake` | Plugin version follows the pinned release (section 4.5) |
| `packaging/homebrew/nymph.rb.in` | The formula template |
| `packaging/chrome/publish.sh` | Chrome Web Store upload and publish |
| `tests/` | `install_check.cmake`, `install_subproject.cmake`, `install_script.sh` / `.ps1`, `release_linkage.cmake` (was `release_static.cmake`), `source_dist_check.cmake`, `plugin_sync_versions.cmake`, `chrome_publish_test.mjs`, `test_charconv.cpp` |
| `docs/building.md`, `docs/releasing.md` (new), `docs/extension.md`, `docs/claude.md`, `README.md`, `CLAUDE.md` | macOS, installing, releasing |

## 2. Design Goals

1. **One trigger.** Pushing a tag `v<version>` is the only manual act of
   a release, after a commit that sets the version and the changelog.
   Everything after it is the workflow's.
2. **Nothing half published.** A user never sees a release with missing
   files, a plugin pinned to a file that does not exist, a formula whose
   tarball is missing, or an extension newer than the release. Every
   distribution channel is updated only after the GitHub release is
   complete and published.
3. **Published files never change.** Once a release is published its
   files are never replaced: the plugin and the formula pin their
   SHA-256.
4. **Every channel installs the same tested build.** The files a channel
   ships are the ones the release's tests ran on; the formula builds the
   same source tarball the release tested.
5. **Source builds need no network and no binary.** The source tarball
   configures, builds, tests and installs offline with only CMake and a
   C++20 compiler.
6. **macOS is a target like the others**: same tests, same packages,
   same release files, in CI on every pull request.
7. **Installed means on `PATH`.** After any install, a new console finds
   `nymph`, with no step left to the user; the change is per user,
   never machine-wide, and can be undone.

## 3. Release Files

Every release `v<version>` has these files (names from Claude DESIGN
section 6, `nymph-<version>-<os>-<arch>`):

| File | Built on | For |
|---|---|---|
| `nymph-<v>-linux-x86_64` | ubuntu-24.04 | Plugin launcher, direct download |
| `nymph-<v>-linux-arm64` | ubuntu-24.04-arm | Same |
| `nymph-<v>-windows-x86_64.exe` | windows-latest, MSYS2 MINGW64 | Same |
| `nymph-<v>-macos-arm64` | macos-15 | Same |
| `nymph-<v>-macos-x86_64` | macos-15-intel | Same |
| `nymph-<v>-linux-x86_64.mcpb`, `-linux-arm64.mcpb`, `-windows-x86_64.mcpb` | as the executable | Claude Desktop |
| `nymph-<v>-macos-universal.mcpb` | the release job (`lipo` of both macOS executables) | Claude Desktop on any Mac |
| `nymph-skill-<v>.zip` | ubuntu-24.04 | claude.ai |
| `nymph-extension-<v>.zip` | ubuntu-24.04, Emscripten | Chrome Web Store, "Load unpacked" |
| `nymph-mcp-app-<v>.html` | same | The MCP App page for source builds (section 4.3) |
| `nymph-<v>-src.tar.gz` | ubuntu-24.04 | Source builds, Homebrew |
| `install.sh`, `install.ps1` | copied from `packaging/install/` | Installing the release executable on `PATH` (section 4.9) |
| `SHA256SUMS` | the release job | Every file above, `sha256sum` format |

The per-architecture macOS `.mcpb` files that `nymph-dist` makes are not
published: an MCPB manifest cannot name a CPU (Claude DESIGN section
4.7), so a Mac user could pick the wrong one. The universal bundle has
one executable for both.

## 4. Key Abstractions

### 4.1 The release tag and version

- Tag `v<X.Y.Z>`: a release. Tag `v<X.Y.Z>-<pre>` (e.g. `v0.2.0-rc.1`):
  a prerelease, built and published as a GitHub prerelease, but no
  channel (plugin, tap, Web Store) is updated.
- `<X.Y.Z>` must equal `project(... VERSION)` in `CMakeLists.txt`, and
  `CHANGELOG.md` must have a `## [X.Y.Z]` section; the workflow fails
  first thing otherwise. That section's text is the release notes.
- Files keep the plain `X.Y.Z` in their names, prereleases too
  (the Chrome manifest only takes numbers, and the names stay what
  `NYMPH_RELEASE` builds produce).
- `workflow_dispatch` runs the same build without a tag and without
  publishing: the files are workflow artifacts only (a dry run).

### 4.2 CI workflow (`ci.yml`)

On pull requests and pushes to `main`. One job per target, each running
the full `ctest`:

| Job | Runner | Build |
|---|---|---|
| linux-gcc | ubuntu-24.04 | Debug, `NYMPH_OPENGL` on (Mesa EGL installed, GL tests run on llvmpipe) |
| linux-arm64 | ubuntu-24.04-arm | Release |
| macos-arm64 | macos-15 | Debug |
| macos-x86_64 | macos-15-intel | Release |
| windows-msvc | windows-latest, MSVC + Ninja | Debug |
| windows-msys2 | windows-latest, MSYS2 MINGW64 | Release |
| web | ubuntu-24.04, Emscripten 6.0.12 | `NYMPH_WEB`, Node and Playwright tests |
| release-smoke | ubuntu-24.04 | `NYMPH_RELEASE` with the web job's App page, the release tests |
| source | ubuntu-24.04 | Source tarball, offline build and install (section 4.4) |

The release workflow calls the same build steps (a reusable workflow,
`workflow_call`), so a release builds exactly what CI tests.

### 4.3 Install from source (`cmake/NymphInstall.cmake`)

`cmake --install <build> [--prefix <dir>]` installs, with
`GNUInstallDirs`:

| Path | What |
|---|---|
| `bin/nymph` (`nymph.exe`) | The executable |
| `share/nymph/install.sh` (`install.ps1` on Windows) | The `PATH` script of section 4.9, for `--uninstall` later |
| `share/doc/nymph/` | `README.md`, `CHANGELOG.md`, `LICENSE`, `docs/agent-guide.md`, `docs/cli.md`, `docs/format.md`, `docs/elements.md` |
| `share/licenses/nymph/` | `LICENSE` and the embedded fonts' licences (`fonts/*.txt`) |

- On Windows, when the prefix was not given, it is
  `%LOCALAPPDATA%/Programs/nymph` (no administrator rights needed, and
  the user `PATH` of section 4.9 fits it), not CMake's
  `C:/Program Files/Nymph`. Elsewhere CMake's default (`/usr/local`)
  stays.
- After the files, `cmake --install` puts `<prefix>/bin` on `PATH`
  (section 4.9) unless `NYMPH_INSTALL_UPDATE_PATH` is OFF (default ON)
  or `DESTDIR` is set (a staging install for a package).
- Option `NYMPH_INSTALL`, default ON when nymph is the top-level project
  and not `NYMPH_WEB`. Off when nymph is a subdirectory, so it adds
  nothing to a host project's install.
- The rules are component `nymph` only; the dependencies have no install
  rules (core DESIGN builds them from sources here).
- A source build with no MCP App page (`NYMPH_MCP_VIEWER` empty) serves
  PNGs only, as Claude DESIGN section 6 says. To get the App, build the
  web viewer with Emscripten (docs/building.md) or pass the release's
  `nymph-mcp-app-<v>.html`, which is generated, not source.
- Configuring a tree whose `extern/imgui` is empty and which is not a git
  checkout fails with one message naming the two ways to get the sources
  (the release tarball, or `git clone --recursive`), instead of a missing
  file error. GitHub's own "Source code" archives are such trees.

### 4.4 The source tarball (`cmake/NymphSourceDist.cmake`)

Script mode, run on a git checkout with initialized submodules:
`nymph-<v>-src.tar.gz` with one top folder `nymph-<v>/`, holding
`git archive` of the repository and of each submodule at its pinned
commit, minus large folders a build never reads (an exclusion list in
the script: e.g. `extern/json`'s tests and docs; the step measures).
It has no `.git`, so `nymph_ensure_submodules()` runs no git command.
It contains no executable and no generated file; fonts and example PNGs
are data, as in the repository.

Its test (CI `source` job, and the release): extract it in an empty
folder, configure with no network (`unshare -rn` on Linux), build, run
`ctest`, `cmake --install` to a temporary prefix, then the installed
`nymph` renders `examples/dashboard.yaml` and extracts its source back.

### 4.5 Plugin deployment

Claude DESIGN section 4.6 stays as it is, with one change: **the
plugin's version is the version of the release it pins**, not
`PROJECT_VERSION`. Claude Code offers a plugin update when its version
changes; if the version moved with the source, users could update to a
plugin that says `0.2.0` but still pins `0.1.0` and never get the pin.
So `nymph-plugin-sync` without `NYMPH_PLUGIN_SUMS` keeps the version in
`plugin.json`, with it writes the pinned version, and the staleness test
checks that `plugin.json` and `checksums.txt` name the same version
(`0.0.0` while nothing is pinned).

After the release is published, the `plugin` job runs
`nymph-plugin-sync` with the release's `SHA256SUMS` and opens a pull
request `release/plugin-v<version>` that changes only
`claude/plugin/.claude-plugin/plugin.json` and
`claude/plugin/bin/checksums.txt`, set to merge automatically when CI
passes. A pull request, not a push: it works with branch protection,
CI checks the pin, and the history shows each pin.

### 4.6 Homebrew tap

- Repository `binary-dice-games/homebrew-nymph`, file `Formula/nymph.rb`.
  Users: `brew install binary-dice-games/nymph/nymph`.
- The formula builds from source: `url` the release's
  `nymph-<v>-src.tar.gz` and its `sha256`; `depends_on "cmake" =>
  :build`; a `resource` for `nymph-mcp-app-<v>.html` (with its `sha256`)
  passed as `NYMPH_MCP_VIEWER`, so `nymph mcp` shows the live chart;
  `NYMPH_BUILD_TESTS=OFF`, `NYMPH_INSTALL_UPDATE_PATH=OFF` (Homebrew
  links `nymph` onto `PATH` itself); `cmake --install`. Its `test do` renders a
  chart and reads its source back. The same formula works with Homebrew
  on Linux.
- `packaging/homebrew/nymph.rb.in` in this repository is the template,
  filled by `.github/scripts/homebrew-formula.sh` from the release's
  `SHA256SUMS`. The `homebrew-test` job fills it with `file://` URLs of
  the release files (so it works while downloads are private) and runs
  `brew install --build-from-source`, `brew test` and
  `brew audit --strict` from a local tap on macOS arm64 and Linux; the
  `homebrew` job then fills it with the release URLs and pushes it to the
  tap's default branch.

### 4.7 Chrome Web Store

- The `chrome` job uploads `nymph-extension-<v>.zip` to the store item
  (`CHROME_EXTENSION_ID` repository variable) and publishes it, which
  submits it for review; the store releases it when review passes
  (`<all_urls>` host access means an in-depth review: days, not hours).
- `packaging/chrome/publish.sh` calls the Chrome Web Store API v2 with
  curl: an access token from the OAuth refresh token, `:upload` to
  `publishers/<CHROME_PUBLISHER_ID>/items/<CHROME_EXTENSION_ID>`,
  `:fetchStatus` while the upload is in progress, then `:publish`. It
  fails when the store refuses the upload; a refusal because the store
  already has the version is success (a re-run). It never prints a
  credential. The endpoints are settable for its stub test; that the
  real API matches is checked on the first release (PLAN step 8).
- The item, its listing, screenshots, privacy answers and the
  `<all_urls>` justification are created by hand once
  (docs/releasing.md); the workflow only updates the package.

### 4.8 macOS target

- Builds with AppleClang (Xcode on the runner), C++20,
  `CMAKE_OSX_DEPLOYMENT_TARGET` default 13.3 (below; `MACOSX_DEPLOYMENT_TARGET`
  in the environment wins, as Homebrew sets it).
- `NYMPH_OPENGL` defaults OFF on macOS (decision 4). `nymph rasterizers`
  then says "nymph was built without OpenGL support"; the CLI's default
  `--rasterizer opengl` falls back silently, as on any machine without
  OpenGL (GL DESIGN).
- Apple's libc++ lacks `std::from_chars` for `double` (and has
  `std::to_chars` for `double` only from macOS 13.3, hence the 13.3
  default). `from_chars_double()` (`src/core/charconv.hpp`) uses the
  standard overload when a configure check finds it
  (`NYMPH_HAS_FLOAT_FROM_CHARS`) and otherwise a portable parser with
  the same results: a scanner for `from_chars`'s grammar, then a
  classic-locale stream conversion. The fallback is built and tested on
  every platform.
- `NYMPH_RELEASE` on macOS: MinSizeRel, `-Wl,-dead_strip`, no `-static`
  (Apple supports no static executables), stripped with `strip -x`
  after link, then signed (`codesign`; arm64 runs no unsigned code, and
  `strip` invalidates the linker's signature): ad hoc by default, or
  with the Developer ID that `NYMPH_CODESIGN_IDENTITY` names (hardened
  runtime, timestamp), as the release workflow sets when the Apple
  secrets exist. The
  executable links only `/usr/lib/libSystem.B.dylib` and
  `/usr/lib/libc++.1.dylib`, both part of macOS; the release linkage
  test checks that list with `otool -L`, as `release_linkage` checks the
  Linux executable.
- Developer ID signing and notarization run only when the `APPLE_*`
  secrets exist (section 9); without them the files are signed ad hoc.
  A file downloaded by curl (the plugin launcher) or built by Homebrew
  has no quarantine attribute, so Gatekeeper does not check it; the
  `.mcpb`'s executable depends on how Claude Desktop extracts it
  (PLAN step 9 checks).
- The example PNGs were drawn on x86-64 Linux. `release_examples` is
  exact on x86-64 (macOS included, if AppleClang contracts no
  floating-point operation differently; the step checks) and lists the
  differing examples on arm64, as for Linux arm64 (Claude DESIGN section
  6). AppleClang contracts `a*b+c` into FMA by default on arm64
  (`-ffp-contract=on`), so its list may differ from GCC's.

### 4.9 `nymph` on `PATH`

| Install route | How `nymph` gets on `PATH` |
|---|---|
| Homebrew | Homebrew links `bin/nymph` into its prefix, already on `PATH`; the formula passes `NYMPH_INSTALL_UPDATE_PATH=OFF` |
| `cmake --install` | The install runs the `PATH` step below for `<prefix>/bin` |
| `install.sh` / `install.ps1` | They download the release executable for the OS and CPU, check it against the release's `SHA256SUMS`, copy it to `~/.local/bin/nymph` (Windows: `%LOCALAPPDATA%\Programs\nymph\bin\nymph.exe`), then run the `PATH` step |
| Claude plugin, `.mcpb`, Skill | Unchanged: Claude's own `PATH` (Claude DESIGN section 4.6); they are not console installs |

The `PATH` step, for a folder `<bin>`:

1. If `<bin>` is already on `PATH`, nothing changes (the usual case for
   `/usr/local/bin`, `/opt/homebrew/bin` and, on most Linux systems,
   `~/.local/bin`).
2. **Windows** (also MSYS2): `<bin>` is appended to the **user** `Path`
   (`HKCU\Environment`, through
   `[Environment]::SetEnvironmentVariable(..., 'User')`, which tells
   running programs the environment changed). Never the machine `Path`.
3. **Linux, macOS**: a block between `# >>> nymph >>>` and
   `# <<< nymph <<<` that prepends `<bin>` to `PATH` is written, as
   rustup does, to `~/.profile` (login shells, desktop sessions),
   `~/.bash_profile` when it exists (bash then skips `~/.profile`),
   `~/.bashrc` for bash users or when it exists (interactive non-login
   bash) and `$ZDOTDIR/.zshenv` for zsh users or when `~/.zshrc` exists
   (every zsh, login or not; macOS' `path_helper` keeps the entry). For
   fish, `fish_add_path -U <bin>` is run (the folder is made first:
   fish skips a missing one). An older block is replaced, so one file
   never has two.
4. Run as root (`sudo cmake --install` into a prefix not on `PATH`), no
   file is written: root's profile is not the user's. The line to add
   is printed.
5. It prints what it changed and that a **new** console is needed (a
   process cannot change its parent's environment).

Running it twice changes nothing (the block or the `Path` entry is
found). Two test-only overrides keep tests off the real machine:
`NYMPH_PATH_STORE` (a file standing in for the user `Path`) and
`NYMPH_INSTALL_UID` (the user id the root check sees). `install.sh --uninstall` / `install.ps1 -Uninstall` removes the
executable it installed, the block or the `Path` entry; the copy in
`<prefix>/share/nymph/` does the same for a CMake install (removing the
`PATH` entry; CMake has no uninstall of files). `NYMPH_NO_MODIFY_PATH=1`
(environment) makes the scripts print instead of write, for users who
manage their own dotfiles.

The scripts are the one implementation: `cmake/NymphInstall.cmake`
runs the installed copy with `--path-only <bin>` (`-PathOnly <bin>`)
from `install(CODE)`, so CMake, release users and CI share the same
logic and tests. `install.sh` is POSIX `sh` (like the plugin launcher,
whose OS, CPU and SHA-256 code it reuses); `install.ps1` runs in
Windows PowerShell 5.1 and PowerShell 7.

## 5. Data Flow

```
tag v0.2.0 pushed
  │
  ▼
check ──── tag = project VERSION, CHANGELOG has [0.2.0], prerelease?
  │
  ├─► web ──────────── Emscripten: App page, extension zip, Node/browser tests
  │     │
  │     ▼
  ├─► native ×5 ────── NYMPH_RELEASE with the App page, ctest, dist/
  │     (linux x86_64, linux arm64, windows x86_64, macos arm64, macos x86_64)
  │
  ├─► source ───────── tarball, offline build, test, install
  │
  ▼
release ───────────── universal macOS .mcpb, one SHA256SUMS, draft release
  │                   with notes, all files uploaded, then published
  │
  ▼ (not for prereleases; each job independent)
  ├─► plugin ──────── pull request pinning the release, auto-merge
  ├─► homebrew ────── formula built and tested, pushed to the tap
  └─► chrome ──────── upload and publish to the Web Store
  │
  ▼
verify ────────────── on linux, macOS, windows, from the release files:
                      install.sh (and install.ps1) install, a new console
                      finds nymph, the plugin launcher pinned to the new
                      SHA256SUMS downloads, checks and runs it
```

A failed channel job does not undo the release or the other channels;
it is re-run by itself (GitHub's "Re-run failed jobs") once fixed, and
each channel job is idempotent: the plugin job finds its pull request
already open or merged, the tap job finds the formula already at that
version, the store job finds the version already uploaded.

## 6. Public Contract

For maintainers (docs/releasing.md):

1. One commit: set `project(... VERSION X.Y.Z)`, rename
   `## [Unreleased]` in `CHANGELOG.md` to `## [X.Y.Z] - <date>` with a
   new empty `## [Unreleased]` above it. CI green.
2. `git tag vX.Y.Z <that commit>` and push the tag.

For users:

| Channel | Command |
|---|---|
| Homebrew (macOS, Linux) | `brew install binary-dice-games/nymph/nymph` |
| Source | download `nymph-<v>-src.tar.gz`, then `cmake -S nymph-<v> -B build && cmake --build build && cmake --install build` (puts `nymph` on `PATH`) |
| Release executable, Linux / macOS | `curl -fsSL https://github.com/binary-dice-games/nymph/releases/latest/download/install.sh \| sh` |
| Release executable, Windows | `irm https://github.com/binary-dice-games/nymph/releases/latest/download/install.ps1 \| iex` |
| Claude Code | `/plugin install nymph --marketplace binary-dice-games/nymph` (unchanged) |
| Claude Desktop | the `.mcpb` for the OS (one universal file for macOS) |
| Chrome | the Chrome Web Store listing |
| Direct | the executable for the OS and CPU, checked with `SHA256SUMS` |

Repository settings the workflows need (all optional: a channel whose
secret is missing is skipped with a warning, as wish's APT publish is):

| Name | Kind | Used by |
|---|---|---|
| `NYMPH_RELEASE_TOKEN` | secret: token with contents and pull-request write on this repository | plugin pull request (one made with `GITHUB_TOKEN` starts no CI) |
| `HOMEBREW_TAP_TOKEN` | secret: contents write on the tap | homebrew |
| `CHROME_EXTENSION_ID`, `CHROME_PUBLISHER_ID` | variables | chrome |
| `CHROME_CLIENT_ID`, `CHROME_CLIENT_SECRET`, `CHROME_REFRESH_TOKEN` | secrets | chrome |
| `APPLE_CERTIFICATE_P12`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_API_KEY`, `APPLE_API_KEY_ID`, `APPLE_API_ISSUER` | secrets | macOS signing and notarization |

## 7. Constraints and Invariants

- A published release's files are never uploaded again: the release job
  uploads only to its own draft and refuses a tag whose release is
  already published. Re-running is safe until publication.
- No channel job runs before the release is published, and none runs for
  a prerelease or a dry run.
- The plugin never names a version whose files are not all published
  (it pins from the published `SHA256SUMS`).
- The formula's tarball and App page are the release's files, pinned by
  SHA-256.
- Third-party actions are pinned to a commit SHA. Workflows get
  `permissions: contents: read` by default; only the release job has
  `contents: write`. Secrets are passed only to the step that needs them.
- The release build is the CI build: both call the same reusable
  workflow with the same options.
- A source build never needs the network: no `FetchContent`, no download
  in CMake; `nymph_ensure_submodules()` runs git only in a git checkout.
- Installing changes `PATH` only per user, only by adding one entry
  (never reordering or removing others), only once, and never in a
  staging (`DESTDIR`) or package-manager install; it can be undone with
  `--uninstall`.
- Platform code follows CLAUDE.md: macOS shares the POSIX files
  (`gl_device_posix.cpp` is simply not built when `NYMPH_OPENGL` is off);
  no `_mac` file.

## 8. Design Decisions

1. **A tag, not a branch or a GitHub release, starts it.** A branch
   moves; a tag names one commit, and the GitHub release is tied to it.
   Starting from a published GitHub release (as wish does) publishes
   the release before its files exist (goal 2). The workflow creates the
   release itself, as a draft, and publishes it when complete.
2. **The formula builds from source; no binary formula.** A formula
   that downloads the release executables would install faster, but
   Homebrew's own rules prefer source, the source formula tests the
   source tarball on every release for free (goal 4), and it works on
   every CPU and macOS version Homebrew supports. Bottles can be added
   later without changing the formula's source (section 10).
3. **The MCP App page is a release file, not part of the source
   tarball.** It is generated by Emscripten (WebAssembly inside), so
   putting it in the "source" breaks goal 5's promise to environments
   that forbid binaries. Homebrew fetches it as a pinned resource;
   source users choose.
4. **No OpenGL on macOS.** macOS has no EGL, and Apple deprecated
   OpenGL in 2018; a CGL or Metal backend is a project of its own. The
   software rasterizer draws an example in well under 0.1 s (Claude
   DESIGN section 6). Building the POSIX EGL backend on macOS would only
   produce a confusing "libEGL.so.1 cannot be loaded" fallback reason.
5. **Two macOS executables, one universal bundle.** The plugin launcher
   and `checksums.txt` pick a file by OS and CPU (Claude DESIGN section
   4.6), so per-architecture executables fit them unchanged and are half
   the download. The `.mcpb` cannot name a CPU, so it carries a
   universal binary.
6. **Windows releases stay MinGW (MSYS2), built natively.** The
   MinGW-w64 static build is the one Claude DESIGN verified; building it
   in MSYS2 on a Windows runner runs its tests natively instead of under
   Wine. MSVC is built and tested in CI so it stays a valid target, but
   is not what ships.
7. **Native arm64 runners, not QEMU.** GitHub's arm64 Linux and macOS
   runners run the tests at native speed; the cross toolchain files stay
   for local use.
8. **The plugin pin is a pull request after the release.** The tag's
   commit cannot contain the checksums of files built from it, so the pin
   is always a later commit; making it a pull request keeps branch
   protection and runs CI on it.
9. **Installing sets `PATH` by default, per user.** The request is
   that an install leaves `nymph` usable from the console. CMake itself
   never touches `PATH`, so the install step does it, opt-out
   (`NYMPH_INSTALL_UPDATE_PATH`, `NYMPH_NO_MODIFY_PATH`). Rejected: a
   machine-wide `PATH` (needs administrator rights, affects other
   users); only printing instructions (leaves the step to the user,
   which is the problem); a Windows installer (`.msi`) for this alone
   (a signed installer is a larger project; the PowerShell script
   covers it). Windows' default prefix moves to `%LOCALAPPDATA%` so the
   user-level install needs no elevation.
10. **Changelog section as release notes.** The changelog already exists
   in Keep a Changelog form; the release notes are its section for the
   version, so there is one place to write them.

## 9. Integration Boundaries

- **Claude DESIGN** section 6 (release builds, `nymph-dist`,
  `SHA256SUMS`, cross toolchains) and 4.6 to 4.8 (plugin, `.mcpb`,
  Skill): reused as they are, except the plugin version rule (section
  4.5) and the macOS universal bundle. Its open question "macOS" is
  answered here; its open question "private repository" is this
  project's precondition (section 10).
- **Extension DESIGN** section 6: the extension zip is unchanged; the
  manifest version is `PROJECT_VERSION`, so the store's "newer version"
  rule means every store update is a nymph release.
- **GL DESIGN**: `NYMPH_OPENGL` default per platform (section 4.8).
- **CLAUDE.md** and docs/building.md: the platform list gains macOS.
- External: GitHub Actions runners (`ubuntu-24.04`, `ubuntu-24.04-arm`,
  `windows-latest`, `macos-15`, `macos-15-intel`), the tap repository,
  the Chrome Web Store API and developer account, Apple's notary
  service (optional).

## 10. Open Questions and Extension Points

- **Private repository (blocking for channels).** The plugin launcher,
  Homebrew and direct downloads fetch release files without a token, so
  the repository (or a public releases-only mirror) must be public
  before the first channel deployment. The build and the release itself
  work while it is private.
- **Apple Developer ID.** Notarization needs a paid Apple developer
  account. Without it, files are signed ad hoc; whether Claude Desktop
  then runs the `.mcpb`'s executable on a Mac without a Gatekeeper
  prompt is checked in PLAN step 9.
- **Chrome Web Store account.** A developer account and the first
  listing are made by hand; the owner provides them.
- **Homebrew bottles.** If building from source (a few minutes) proves
  too slow, the `homebrew` job can build bottles for macOS arm64 and
  Linux x86-64 with `brew install --build-bottle` and `brew bottle`, and
  attach them to the release; the formula gains a `bottle do` block.
- **macOS x86-64 runners.** `macos-15-intel` is GitHub's last Intel
  image. When it goes, the x86-64 executable is built on arm64 with
  `CMAKE_OSX_ARCHITECTURES=x86_64` and tested under Rosetta 2.
- **homebrew-core.** Once nymph is public and notable enough, the same
  formula can be submitted there.
