#!/bin/sh
# MIT License © 2026 Binary Dice Games
#
# Configure, build and test one target, for CI and releases alike
# (.github/workflows/build.yml; spec/07_release_pipeline/DESIGN.md
# section 4.2). Runs in bash on Linux, macOS and Windows (Git Bash, MSVC
# environment set up beforehand) and in an MSYS2 MINGW64 shell.
#
# Environment:
#   TARGET       linux-x86_64 | linux-arm64 | macos-arm64 | macos-x86_64 |
#                windows-msvc | windows-msys2 | web
#   RELEASE      true: NYMPH_RELEASE build, outputs in build/dist/
#   BUILD_TYPE   CMake build type of a non-release build (default Release)
#   VIEWER       the MCP App page for a release build (from the web target)
#   CODESIGN_IDENTITY   macOS release: a Developer ID ("-" or empty: ad hoc)
#   PATH_CHECK   true: after the tests, install for real into a scratch
#                prefix and check a new login shell (or the user Path)
#                finds nymph
#
# Outputs: build/dist/* (release), build-web/claude/viewer.html and
# build-web/extension/*.zip (web).

set -eu

: "${TARGET:?TARGET is not set}"
RELEASE=${RELEASE:-false}
BUILD_TYPE=${BUILD_TYPE:-Release}
JOBS=${JOBS:-4}

step() {
  echo "::group::$*"
}
end_step() {
  echo "::endgroup::"
}

# ── Web ───────────────────────────────────────────────────────────────────────

if [ "$TARGET" = web ]; then
  step "configure (Emscripten)"
  emcmake cmake -S . -B build-web -G Ninja -DNYMPH_WEB=ON -DCMAKE_BUILD_TYPE=MinSizeRel
  end_step
  step "build"
  cmake --build build-web --parallel "$JOBS"
  end_step
  step "test"
  ctest --test-dir build-web --output-on-failure
  end_step
  ls -l build-web/claude/viewer.html build-web/extension/*.zip
  exit 0
fi

# ── Native ────────────────────────────────────────────────────────────────────

set -- -S . -B build -G Ninja
if [ "$RELEASE" = true ]; then
  [ -n "${VIEWER:-}" ] || { echo "a release build needs VIEWER (the MCP App page)" >&2; exit 1; }
  set -- "$@" -DNYMPH_RELEASE=ON "-DNYMPH_MCP_VIEWER=$VIEWER"
  case $TARGET in
    macos-*) set -- "$@" "-DNYMPH_CODESIGN_IDENTITY=${CODESIGN_IDENTITY:--}" ;;
  esac
else
  set -- "$@" "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
fi
case $TARGET in
  macos-x86_64) set -- "$@" -DCMAKE_OSX_ARCHITECTURES=x86_64 ;;
  macos-arm64) set -- "$@" -DCMAKE_OSX_ARCHITECTURES=arm64 ;;
esac

step "configure: $*"
cmake "$@"
end_step
step "build"
cmake --build build --parallel "$JOBS"
end_step
step "test"
ctest --test-dir build --output-on-failure -j "$JOBS"
end_step

if [ "$RELEASE" = true ]; then
  ls -l build/dist
  cat build/dist/SHA256SUMS
fi

# ── nymph on PATH after a real install (spec 07 PLAN step 3) ─────────────────

if [ "${PATH_CHECK:-false}" = true ]; then
  step "install and PATH check"
  prefix=$HOME/nymph-ci-prefix
  case $TARGET in
    macos-*) shell=/bin/zsh ;;
    *) shell=/bin/bash ;;
  esac
  SHELL=$shell cmake --install build --prefix "$prefix"
  case $TARGET in
    windows-msvc)
      # A new console reads the user Path from the registry.
      win_bin=$(cygpath -w "$prefix/bin")
      powershell -NoProfile -Command "
        \$want = '$win_bin'
        \$user = [Environment]::GetEnvironmentVariable('Path', 'User')
        if (-not ((\$user -split ';') -contains \$want)) { throw \"\$want is not on the user Path: \$user\" }
        & (Join-Path \$want 'nymph.exe') --version"
      ;;
    macos-*)
      found=$(env -i HOME="$HOME" PATH=/usr/bin:/bin zsh -lc 'command -v nymph')
      [ "$found" = "$prefix/bin/nymph" ] || { echo "a new zsh finds '$found'" >&2; exit 1; }
      ;;
    *)
      found=$(env -i HOME="$HOME" PATH=/usr/bin:/bin bash -lc 'command -v nymph')
      case $found in
        "$prefix/bin/nymph" | "$prefix/bin/nymph.exe") ;;
        *) echo "a new bash finds '$found', not $prefix/bin/nymph" >&2; exit 1 ;;
      esac
      ;;
  esac
  echo "a new console finds nymph"
  end_step
fi
