# nymph — Releasing

A release is one tag push. GitHub Actions (`.github/workflows/release.yml`)
builds and tests nymph on every platform, publishes the GitHub release and
updates every channel users install from. Design:
[spec/07_release_pipeline](../spec/07_release_pipeline/DESIGN.md).

## Make a release

1. In one commit on `main`, with CI green:
   - set the version in `CMakeLists.txt`: `project(Nymph VERSION X.Y.Z ...)`;
   - in `CHANGELOG.md`, rename `## [Unreleased]` to `## [X.Y.Z] - <date>`
     and add a new, empty `## [Unreleased]` above it.
2. Tag that commit and push the tag:

   ```sh
   git tag vX.Y.Z
   git push origin vX.Y.Z
   ```

That is all. The workflow fails at once, before building anything, if
the tag is not `v` plus the `CMakeLists.txt` version or the changelog has
no section for it.

- **Prerelease:** a tag `vX.Y.Z-<suffix>` (`v0.2.0-rc.1`) makes a GitHub
  prerelease with every file, and updates no channel. Its files carry the
  plain `X.Y.Z` in their names.
- **Dry run:** run the `release` workflow by hand (Actions, "Run
  workflow"): everything is built and tested, the files are left as the
  run's `release-files` artifact, and nothing is published.

## What the workflow does

| Job | Does |
|---|---|
| `check` | Tag = version, changelog section (the release notes) |
| `web` | Emscripten build: the MCP App page and the Chrome extension, with their Node and browser tests |
| `native` | Release builds with their tests: Linux x86-64 and arm64, Windows x86-64 (MSYS2), macOS arm64 and x86-64 |
| `source` | The source tarball, built, tested and installed with no network (Linux) and on macOS |
| `macos-universal` | One executable for both Macs, in `nymph-X.Y.Z-macos-universal.mcpb` |
| `release` | Gathers the files, writes `SHA256SUMS`, creates the release as a draft, uploads, publishes |
| `plugin` | Opens a pull request pinning the release in the Claude Code and Cursor plugins, set to merge when CI passes |
| `homebrew-test`, `homebrew` | Builds and tests the formula on macOS and Linux, then pushes it to the tap |
| `chrome` | Uploads the extension to the Chrome Web Store and submits it for review |
| `verify` | On Linux, macOS and Windows: the install scripts, a new console, the plugin launcher |

The channel jobs run only after the release is published, and not for
prereleases. Each one is independent and can be re-run alone ("Re-run
failed jobs") once its problem is fixed; re-running does nothing a second
time (an open pin pull request, a tap already at the version, a version
the store already has). A published release's files are never replaced:
the plugin and the formula pin their SHA-256, so re-running the `release`
job of a published tag fails. To fix a bad release, publish the next
version.

The release files:

| File | For |
|---|---|
| `nymph-X.Y.Z-<os>-<arch>[.exe]` | The executable (linux, macos: x86_64, arm64; windows: x86_64) |
| `install.sh`, `install.ps1` | Install the executable and put it on `PATH` (README, "Install") |
| `nymph-X.Y.Z-<os>-<arch>.mcpb`, `nymph-X.Y.Z-macos-universal.mcpb` | Claude Desktop |
| `nymph-skill-X.Y.Z.zip` | claude.ai |
| `nymph-extension-X.Y.Z.zip` | The Chrome extension, unpacked install |
| `nymph-mcp-app-X.Y.Z.html` | The live chart page, for source builds and Homebrew |
| `nymph-X.Y.Z-src.tar.gz` | The sources with every submodule |
| `SHA256SUMS` | All of the above |

## One-time setup

Each channel is skipped, with a warning in the run, until its settings
exist (repository Settings, "Secrets and variables", "Actions"). The
release itself needs none.

| Name | Kind | For |
|---|---|---|
| `NYMPH_RELEASE_TOKEN` | secret | Plugin pin pull request: a fine-grained token (or GitHub App token) with Contents and Pull requests write on this repository. One made with the default `GITHUB_TOKEN` would start no CI. |
| `HOMEBREW_TAP_TOKEN` | secret | Homebrew: Contents write on `binary-dice-games/homebrew-nymph` |
| `CHROME_EXTENSION_ID`, `CHROME_PUBLISHER_ID` | variables | Chrome Web Store item and publisher |
| `CHROME_CLIENT_ID`, `CHROME_CLIENT_SECRET`, `CHROME_REFRESH_TOKEN` | secrets | Chrome Web Store API (OAuth) |
| `APPLE_CERTIFICATE_P12` (base64), `APPLE_CERTIFICATE_PASSWORD` | secrets | macOS: Developer ID Application certificate |
| `APPLE_API_KEY` (the `.p8` text), `APPLE_API_KEY_ID`, `APPLE_API_ISSUER` | secrets | macOS: App Store Connect API key for notarization |

- **Public downloads.** The plugin launcher, the install scripts and
  Homebrew download release files without a token: the repository (or a
  public mirror of its releases) must be public for them to work.
- **Pull request auto-merge.** Turn on "Allow auto-merge" (Settings,
  General) so the plugin pin merges by itself when CI passes; otherwise
  merge it by hand.
- **Homebrew tap.** Create the public repository
  `binary-dice-games/homebrew-nymph` with a README; the workflow adds
  `Formula/nymph.rb`. Users then run
  `brew install binary-dice-games/nymph/nymph`.
- **Chrome Web Store.** With a developer account: create the item by
  uploading `nymph-extension-X.Y.Z.zip` once by hand, fill the listing
  (description, screenshots, the 128 px icon `extension/icons/on-128.png`),
  the privacy practices (no data collected) and the justification for
  access to all sites (the extension looks for nymph PNGs on any page;
  [extension.md](extension.md)). Then create OAuth credentials with the
  `https://www.googleapis.com/auth/chromewebstore` scope and set the
  settings above. Each release is reviewed before it reaches users; access
  to all sites means an in-depth review.
- **Apple.** Without the Apple settings, macOS files are signed ad hoc:
  the plugin launcher, the install script and Homebrew run them (files
  downloaded by `curl` or built by Homebrew are not quarantined), but a
  Mac may refuse one downloaded with a browser until it is allowed in
  System Settings, "Privacy & Security". With them, the executables are
  signed with the Developer ID and notarized.

## Checks before the first real release

The workflows are tested by CI, but the store and tap steps only run on a
real release. Push a prerelease tag first (`vX.Y.Z-rc.1`) to see every
build and the release files, then the release tag.
