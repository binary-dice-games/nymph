#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 8: the Claude Code plugin's
# launcher (claude/plugin/bin/nymph) against a fake release served from a
# local folder: it downloads, checks the SHA-256, caches and runs the
# executable; it never runs or keeps a file whose SHA-256 differs.
#
#   sh plugin_launcher.sh <claude/plugin/bin/nymph> <work dir>
#
# Exits 77 (CTest's skip) without curl, which file:// downloads need.

set -u
launcher=$1
work=$2

command -v curl >/dev/null 2>&1 || { echo "no curl: skipped"; exit 77; }

failures=0
check() { # <description> <condition...>
  description=$1
  shift
  if "$@"; then
    echo "ok   $description"
  else
    echo "FAIL $description"
    failures=$((failures + 1))
  fi
}
sha() { sha256sum "$1" | cut -d ' ' -f 1; }

rm -rf "$work"
mkdir -p "$work/plugin/bin" "$work/release" "$work/home"
cp "$launcher" "$work/plugin/bin/nymph"
run() { # runs the launcher copy with a clean environment
  env -i PATH="$PATH" HOME="$work/home" NYMPH_RELEASE_URL="file://$work/release" "$@" sh "$work/plugin/bin/nymph" mcp --check
}

# A stand-in executable, published under every platform's name.
printf '#!/bin/sh\necho "fake nymph $*"\n' > "$work/fake"
chmod +x "$work/fake"
: > "$work/plugin/bin/checksums.txt"
for platform in linux-x86_64 linux-arm64 macos-x86_64 macos-arm64 windows-x86_64.exe windows-arm64.exe; do
  cp "$work/fake" "$work/release/nymph-9.8.7-$platform"
  echo "$(sha "$work/fake")  nymph-9.8.7-$platform" >> "$work/plugin/bin/checksums.txt"
done
info=$(run NYMPH_LAUNCHER_INFO=1)
name=$(echo "$info" | sed -n 1p)
target=$(echo "$info" | sed -n 2p)
check "info names this platform's release and the cache" [ -n "$name" ] && [ "$target" = "$work/home/.cache/nymph/9.8.7/$name" ]

# 1. NYMPH_BIN wins.
printf '#!/bin/sh\necho "own nymph $*"\n' > "$work/own"
chmod +x "$work/own"
check "NYMPH_BIN is run" [ "$(run NYMPH_BIN="$work/own")" = "own nymph mcp --check" ]
check "NYMPH_BIN downloads nothing" [ ! -e "$target" ]

# 2. Downloaded, checked, cached, run.
check "downloads and runs the release" [ "$(run)" = "fake nymph mcp --check" ]
check "caches it, executable" [ -x "$target" ]

# 3. The cache is used without the release.
mv "$work/release" "$work/release.away"
check "runs from the cache" [ "$(run)" = "fake nymph mcp --check" ]

# 4. A changed cached file is not run; with the release back it is replaced.
echo 'echo tampered' >> "$target"
out=$(run 2>&1)
code=$?
check "a changed cached file is not run" [ $code -eq 1 ] && ! echo "$out" | grep -q tampered
mv "$work/release.away" "$work/release"
check "and is replaced by the release" [ "$(run)" = "fake nymph mcp --check" ] && [ "$(sha "$target")" = "$(sha "$work/fake")" ]

# 5. A release whose SHA-256 differs is refused, and nothing is kept.
rm -rf "$work/home/.cache"
echo 'echo evil' >> "$work/release/$name"
out=$(run 2>&1 >/dev/null)
code=$?
check "a tampered release exits 1" [ $code -eq 1 ]
check "with one line on stderr naming the check" [ "$(echo "$out" | wc -l)" -eq 1 ] && echo "$out" | grep -q "not run"
check "and keeps no file" [ ! -e "$target" ] && [ -z "$(ls "$work/home/.cache/nymph/9.8.7" 2>/dev/null)" ]

# 6. No release for this platform.
echo "# none" > "$work/plugin/bin/checksums.txt"
out=$(run 2>&1)
code=$?
check "no pinned release: exit 1, asks for NYMPH_BIN" [ $code -eq 1 ] && echo "$out" | grep -q NYMPH_BIN

[ $failures -eq 0 ] || exit 1
echo "launcher: all checks passed"
