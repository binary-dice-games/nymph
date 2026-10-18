#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# After a release, on Linux, macOS and Windows (Git Bash): the release's
# install.sh installs this platform's executable from the release files
# and a new login shell runs it; the Claude Code plugin's launcher, pinned
# to the release's SHA256SUMS, downloads, checks and runs it
# (spec/07_release_pipeline/PLAN.md step 10).
#
#   sh release-verify.sh <folder with the release files> <version>

set -eu
files=$(cd "$1" && pwd)
version=$2
root=$(cd "$(dirname "$0")/../.." && pwd)
work=${RUNNER_TEMP:-${TMPDIR:-/tmp}}/nymph-verify
rm -rf "$work"
mkdir -p "$work/home"

fail() {
  echo "release verify: $*" >&2
  exit 1
}
# Windows console output may end lines with CR.
no_cr() { tr -d '\r'; }

case $(uname -s) in
  MINGW* | MSYS* | CYGWIN*) url="file:///$(cygpath -m "$files")" ;;
  *) url="file://$files" ;;
esac

# ── install.sh ───────────────────────────────────────────────────────────────
HOME="$work/home" SHELL=/bin/bash NYMPH_RELEASE_URL="$url" \
  sh "$files/install.sh" --version "$version" --bin-dir "$work/bin"
found=$(env -i HOME="$work/home" PATH=/usr/bin:/bin bash -lc 'command -v nymph') ||
  fail "a new login shell does not find nymph"
echo "release verify: a new login shell finds $found"
out=$("$found" --version | no_cr)
[ "$out" = "nymph $version" ] || fail "the installed nymph says '$out'"
cat > "$work/chart.yaml" << 'YAML'
root:
  type: Plot
  title: Revenue
  children:
    - {type: PlotLine, label: revenue, ys: $revenue}
data: {revenue: [12, 15, 11, 18]}
YAML
"$found" render "$work/chart.yaml" -o "$work/chart.png" --rasterizer software
[ "$("$found" "$work/chart.png" | no_cr)" = "$(cat "$work/chart.yaml")" ] || fail "the source did not come back from the PNG"

# ── The plugin launcher, pinned to this release ──────────────────────────────
mkdir -p "$work/plugin/bin"
cp "$root/claude/plugin/bin/nymph" "$work/plugin/bin/nymph"
grep -E "  nymph-$version-[a-z]+-[a-z0-9_]+(\\.exe)?\$" "$files/SHA256SUMS" > "$work/plugin/bin/checksums.txt"
out=$(HOME="$work/home" NYMPH_CACHE="$work/cache" NYMPH_RELEASE_URL="$url" sh "$work/plugin/bin/nymph" --version | no_cr)
[ "$out" = "nymph $version" ] || fail "the plugin launcher ran '$out'"
echo "release verify: install.sh, login shell and plugin launcher work with nymph $version"
