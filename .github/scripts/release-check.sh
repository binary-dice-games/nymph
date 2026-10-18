#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# The release's version checks (spec/07_release_pipeline/DESIGN.md section
# 4.1): prints "version=", "tag=", "prerelease=" lines (for $GITHUB_OUTPUT)
# and writes the release notes (the CHANGELOG.md section of the version)
# to NOTES.
#
#   sh release-check.sh <tag or empty> <notes file>
#
# With a tag: it must be v<X.Y.Z> or v<X.Y.Z>-<pre>, X.Y.Z must be the
# project VERSION in CMakeLists.txt, and CHANGELOG.md must have a
# "## [X.Y.Z]" section. Without a tag (a dry run) a missing section is only
# a warning. Exits 1 with one line on standard error otherwise.

set -eu
tag=$1
notes=$2

fail() {
  echo "release check: $*" >&2
  exit 1
}

version=$(sed -n 's/^  VERSION \([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)$/\1/p' CMakeLists.txt | head -n 1)
[ -n "$version" ] || fail "no project VERSION X.Y.Z in CMakeLists.txt"

prerelease=false
if [ -n "$tag" ]; then
  case $tag in
    "v$version") ;;
    "v$version"-*)
      suffix=${tag#"v$version"-}
      printf '%s' "$suffix" | grep -Eq '^[0-9A-Za-z.]+$' || fail "tag $tag: bad prerelease suffix"
      prerelease=true
      ;;
    *) fail "tag $tag does not match the project VERSION $version in CMakeLists.txt" ;;
  esac
fi

# The changelog section: from "## [X.Y.Z]" to the next "## [".
awk -v v="$version" '
  index($0, "## [" v "]") == 1 { found = 1; next }
  found && /^## \[/ { exit }
  found { print }' CHANGELOG.md | sed -e '/./,$!d' > "$notes"
if [ ! -s "$notes" ]; then
  if [ -n "$tag" ]; then
    fail "CHANGELOG.md has no '## [$version]' section with text: rename [Unreleased] first (docs/releasing.md)"
  fi
  echo "::warning::CHANGELOG.md has no '## [$version]' section (dry run)" >&2
  echo "Dry run of nymph $version." > "$notes"
fi

echo "version=$version"
echo "tag=${tag:-v$version}"
echo "prerelease=$prerelease"
